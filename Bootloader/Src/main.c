#include <stdint.h>
#include "stm32f4xx.h"
#include "flash_map.h"
#include "boot_metadata.h"
#include "flash_driver.h"
#include "uart_driver.h"
#include "uart_protocol.h"
#include "crc32.h"
#include "timebase.h"

#define UART_BAUD          115200U
#define UPDATE_WINDOW_MS   2000U   // time given to the host to request an update

/* Values written to RTC->BKP0R, a battery-backed register that
 * survives resets (but not full power loss without VBAT). This is
 * how the application tells the bootloader "I'm alive and healthy"
 * without any shared flash-writing code between the two binaries.
 */
#define COMMIT_MAGIC       0x600DC0DEUL // set by app_commit() in the app
#define PENDING_MAGIC      0xBAADF00DUL // set by bootloader before jump

static boot_metadata_t g_meta;

// Backup-domain "commit flag" helpers
static void enable_backup_domain(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR      |= PWR_CR_DBP; // unlock write access to RTC backup regs
}

static uint32_t read_commit_flag(void)
{
    return RTC->BKP0R;
}
static void write_commit_flag(uint32_t v)
{
    RTC->BKP0R = v;
}

static int reset_was_watchdog(void)
{
    int wdg = (RCC->CSR & (RCC_CSR_IWDGRSTF | RCC_CSR_WWDGRSTF)) != 0;
    RCC->CSR |= RCC_CSR_RMVF; // clear flags so next boot reads cleanly
    return wdg;
}

/* Firmware validation
 * Sanity-check a candidate vector table before trusting it at all:
 * the initial stack pointer must land in SRAM and the reset handler
 * must be a Thumb address inside the slot it came from. Cheap, but
 * catches "erased flash" / garbage / wrong-slot images immediately,
 * before we even bother with the (slower) CRC pass.
 */
static int vectors_look_sane(uint32_t slot)
{
    const uint32_t *vtor = (const uint32_t *)slot_base(slot);
    uint32_t initial_sp = vtor[0];
    uint32_t reset_handler = vtor[1];

    if (initial_sp < SRAM_BASE || initial_sp > SRAM_END)
    {
        return 0;
    }

    if ((reset_handler & 0x1U) == 0U)
    {
        return 0; // Thumb bit must be set
    }
    uint32_t base = slot_base(slot);
    uint32_t top  = base + slot_capacity(slot);

    if (reset_handler < base || reset_handler >= top)
    {
        return 0;
    }
    return 1;
}

static int slot_crc_ok(uint32_t slot, uint32_t size, uint32_t expected_crc)
{
    if (size == 0U || size > slot_capacity(slot))
    {
        return 0;
    }

    uint32_t computed = crc32_compute((const uint8_t *)slot_base(slot), size);
    return computed == expected_crc;
}

/* Full validation = state is boot-eligible AND CRC32 matches AND
 * vector table passes the sanity check above.
 */
static int validate_slot(uint32_t slot)
{
    slot_info_t *info = &g_meta.slot[slot];
    if (info->state != SLOT_STATE_VALID &&
        info->state != SLOT_STATE_NEW &&
        info->state != SLOT_STATE_TESTING) {
        return 0;
    }
    if (!slot_crc_ok(slot, info->size, info->crc32)) {
        return 0;
    }
    if (!vectors_look_sane(slot)) {
        return 0;
    }
    return 1;
}

// Vector table relocation + jump
static void __attribute__((noreturn)) jump_to_app(uint32_t slot)
{
    uint32_t base = slot_base(slot);
    const uint32_t *vtor = (const uint32_t *)base;
    uint32_t initial_sp = vtor[0];
    uint32_t reset_handler = vtor[1];

    __disable_irq();

    /* Relocate the vector table so the application's own interrupt
     * and exception handlers are used from this point on.
     */
    SCB->VTOR = base;
    __DSB();
    __ISB();

    __set_MSP(initial_sp);

    typedef void (*reset_fn_t)(void);
    reset_fn_t app_reset = (reset_fn_t)reset_handler;

    __enable_irq();
    app_reset();

    for (;;) { } // never reached
}

// Metadata / update flow helpers
static void init_default_metadata(void)
{
    g_meta.magic       = BOOT_META_MAGIC;
    g_meta.seq         = 0;
    g_meta.active_slot = SLOT_A;
    for (uint32_t i = 0; i < NUM_SLOTS; i++)
    {
        g_meta.slot[i].size          = 0;
        g_meta.slot[i].crc32         = 0;
        g_meta.slot[i].state         = SLOT_STATE_EMPTY;
        g_meta.slot[i].boot_attempts = 0;
    }
}

static void do_update_flow(void)
{
    uint32_t target = g_meta.active_slot ^ 1U; // always flash the inactive slot
    uint32_t size, crc;

    if (uart_recv_firmware(target, &size, &crc))
    {
        g_meta.slot[target].size          = size;
        g_meta.slot[target].crc32         = crc;
        g_meta.slot[target].state         = SLOT_STATE_NEW;
        g_meta.slot[target].boot_attempts = 0;
        g_meta.active_slot = target; // boot the new image on next reset
        g_meta.seq++;
        boot_metadata_save(&g_meta);
    }
    /* On failure the inactive slot is left erased/unmarked and the
     * previously active, already-validated firmware keeps running.
     */
}

int main(void)
{
    timebase_init(16000000UL); // HSI @ 16 MHz, no PLL - keep bootloader minimal
    uart_init(UART_BAUD);
    enable_backup_domain();

    int watchdog_reset = reset_was_watchdog();
    uint32_t commit_flag = read_commit_flag();
    write_commit_flag(0);

    if (!boot_metadata_load(&g_meta))
    {
        /* Blank chip / corrupted log: assume a valid image was
         * pre-flashed into Slot A at production time.
         */
        init_default_metadata();
        g_meta.slot[SLOT_A].state = SLOT_STATE_VALID;
        g_meta.slot[SLOT_A].size = 0; // production flow must fill this in
        boot_metadata_save(&g_meta);
    }

    // Give the host a short window to request a firmware update.
    if (uart_wait_for_update_request(UPDATE_WINDOW_MS))
    {
        do_update_flow();
    }

    uint32_t active = g_meta.active_slot;
    slot_info_t *info = &g_meta.slot[active];

    // Resolve the outcome of a previous "on probation" boot.
    if (info->state == SLOT_STATE_TESTING)
    {
        if (commit_flag == COMMIT_MAGIC)
        {
            info->state = SLOT_STATE_VALID;
            info->boot_attempts = 0;
        }
        else if (watchdog_reset || commit_flag == PENDING_MAGIC)
        {
            /* App hung, crashed, or was reset without ever calling
             * boot_commit() - treat this as a failed image outright,
             * even if its CRC still checks out.
             */
            info->state = SLOT_STATE_INVALID;
        }
    }

    int booted = 0;

    if (info->state != SLOT_STATE_INVALID && validate_slot(active))
    {
        if (info->state == SLOT_STATE_NEW)
        {
            info->state = SLOT_STATE_TESTING;
            info->boot_attempts = 1;
        }
        else if (info->state == SLOT_STATE_TESTING)
        {
            info->boot_attempts++;
            if (info->boot_attempts > MAX_BOOT_ATTEMPTS)
            {
                info->state = SLOT_STATE_INVALID;
            }
        }

        if (info->state != SLOT_STATE_INVALID)
        {
            g_meta.seq++;
            boot_metadata_save(&g_meta);
            if (info->state == SLOT_STATE_TESTING)
            {
                write_commit_flag(PENDING_MAGIC);
            }
            booted = 1;
            jump_to_app(active); // does not return
        }
    } else {
        info->state = SLOT_STATE_INVALID;
    }

    if (!booted)
    {
        // Rollback: fall back to the other slot
        uint32_t other = active ^ 1U;
        slot_info_t *other_info = &g_meta.slot[other];

        g_meta.seq++;
        boot_metadata_save(&g_meta); // persist that `active` is now INVALID

        if (other_info->state != SLOT_STATE_INVALID && validate_slot(other))
        {
            g_meta.active_slot = other;
            other_info->boot_attempts = 0;
            if (other_info->state == SLOT_STATE_NEW)
            {
                other_info->state = SLOT_STATE_TESTING;
                other_info->boot_attempts = 1;
            }
            g_meta.seq++;
            boot_metadata_save(&g_meta);
            if (other_info->state == SLOT_STATE_TESTING)
            {
                write_commit_flag(PENDING_MAGIC);
            }
            jump_to_app(other); // does not return
        }
    }

    /* Recovery mode
     * No valid firmware anywhere. Sit here and wait indefinitely
     * for a firmware update instead of bricking.
     */
    for (;;)
    {
        if (uart_wait_for_update_request(UPDATE_WINDOW_MS))
        {
            do_update_flow();
            NVIC_SystemReset();
        }
    }
}
