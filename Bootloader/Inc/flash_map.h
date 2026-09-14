#ifndef FLASH_MAP_H
#define FLASH_MAP_H

#include <stdint.h>

/* ------------------------------------------------------------------
 * Memory layout for STM32F407VG (1 MB flash, sector-based).
 * Sector sizes: 0-3 = 16 KB, 4 = 64 KB, 5-11 = 128 KB.
 *
 *  0x08000000 +----------------------+  sector 0-1
 *             |     Bootloader       |  32 KB
 *  0x08008000 +----------------------+  sector 2
 *             |  Metadata log (A/B)  |  16 KB
 *  0x0800C000 +----------------------+  sector 3
 *             |  Reserved (keys/cfg) |  16 KB
 *  0x08010000 +----------------------+  sectors 4-7
 *             |     Slot A (app)     |  448 KB
 *  0x08080000 +----------------------+  sectors 8-11
 *             |     Slot B (app)     |  512 KB
 *  0x08100000 +----------------------+
 *
 * Adjust these constants if you target a different STM32.
 * ------------------------------------------------------------------
 */

#define FLASH_BASE_ADDR        0x08000000UL

#define BOOTLOADER_BASE        0x08000000UL
#define BOOTLOADER_SIZE        (32U * 1024U)

#define METADATA_BASE          0x08008000UL
#define METADATA_SIZE          (16U * 1024U)
#define METADATA_SECTOR        2U

#define RESERVED_BASE          0x0800C000UL
#define RESERVED_SIZE          (16U * 1024U)

#define SLOT_A_BASE            0x08010000UL
#define SLOT_A_SIZE            (448U * 1024U)
#define SLOT_A_FIRST_SECTOR    4U
#define SLOT_A_LAST_SECTOR     7U

#define SLOT_B_BASE            0x08080000UL
#define SLOT_B_SIZE            (512U * 1024U)
#define SLOT_B_FIRST_SECTOR    8U
#define SLOT_B_LAST_SECTOR     11U

#define NUM_SLOTS              2U
#define SLOT_A                 0U
#define SLOT_B                 1U

// Largest image guaranteed to fit in either slot
#define MAX_APP_SIZE           SLOT_A_SIZE

static inline uint32_t slot_base(uint32_t slot)
{
    return (slot == SLOT_A) ? SLOT_A_BASE : SLOT_B_BASE;
}

static inline uint32_t slot_capacity(uint32_t slot)
{
    return (slot == SLOT_A) ? SLOT_A_SIZE : SLOT_B_SIZE;
}

static inline uint32_t slot_first_sector(uint32_t slot)
{
    return (slot == SLOT_A) ? SLOT_A_FIRST_SECTOR : SLOT_B_FIRST_SECTOR;
}

static inline uint32_t slot_last_sector(uint32_t slot)
{
    return (slot == SLOT_A) ? SLOT_A_LAST_SECTOR : SLOT_B_LAST_SECTOR;
}

/* SRAM bounds used to sanity-check a firmware image's initial stack
 * pointer before jumping to it (STM32F407: 128 KB SRAM at 0x20000000).
 */
#define SRAM_BASE              0x20000000UL
#define SRAM_SIZE              (128U * 1024U)
#define SRAM_END               (SRAM_BASE + SRAM_SIZE)

#endif // FLASH_MAP_H
