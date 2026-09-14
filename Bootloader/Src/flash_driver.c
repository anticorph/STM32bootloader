#include "stm32f4xx.h"
#include "flash_driver.h"
#include "flash_map.h"

#define FLASH_KEY1  0x45670123UL
#define FLASH_KEY2  0xCDEF89ABUL

static void flash_wait_busy(void)
{
    while (FLASH->SR & FLASH_SR_BSY)
    {
        // spin
    }
}

void flash_unlock(void)
{
    if (FLASH->CR & FLASH_CR_LOCK)
    {
        FLASH->KEYR = FLASH_KEY1;
        FLASH->KEYR = FLASH_KEY2;
    }
}

void flash_lock(void)
{
    FLASH->CR |= FLASH_CR_LOCK;
}

static void clear_error_flags(void)
{
    FLASH->SR |= (FLASH_SR_EOP | FLASH_SR_OPERR | FLASH_SR_WRPERR | FLASH_SR_PGAERR | FLASH_SR_PGPERR | FLASH_SR_PGSERR);
}

int flash_erase_sector(uint32_t sector)
{
    flash_wait_busy();
    flash_unlock();
    clear_error_flags();

    FLASH->CR &= ~FLASH_CR_SNB;
    FLASH->CR |=  (sector << FLASH_CR_SNB_Pos) & FLASH_CR_SNB;
    FLASH->CR |=  FLASH_CR_SER;
    FLASH->CR |=  FLASH_CR_PSIZE_1; // PSIZE=10b: 32-bit width, VDD 2.7-3.6V
    FLASH->CR |=  FLASH_CR_STRT;

    flash_wait_busy();

    int ok = (FLASH->SR & (FLASH_SR_OPERR | FLASH_SR_WRPERR)) == 0;

    FLASH->CR &= ~FLASH_CR_SER;
    flash_lock();
    return ok;
}

int flash_erase_slot(uint32_t slot)
{
    for (uint32_t s = slot_first_sector(slot); s <= slot_last_sector(slot); s++)
    {
        if (!flash_erase_sector(s))
        {
            return 0;
        }
    }
    return 1;
}

int flash_program(uint32_t dest, const uint8_t *src, uint32_t len)
{
    flash_wait_busy();
    flash_unlock();
    clear_error_flags();

    FLASH->CR &= ~FLASH_CR_PSIZE;
    FLASH->CR |=  FLASH_CR_PSIZE_1; // 32-bit program width
    FLASH->CR |=  FLASH_CR_PG;

    uint32_t i = 0;
    int ok = 1;

    while (i < len && ok)
    {
        uint32_t remaining = len - i;
        uint32_t chunk = (remaining >= 4U) ? 4U : remaining;
        uint32_t word = 0xFFFFFFFFUL;

        for (uint32_t b = 0; b < chunk; b++)
        {
            word &= ~(0xFFUL << (8U * b));
            word |= ((uint32_t)src[i + b]) << (8U * b);
        }

        *(volatile uint32_t *)(dest + i) = word;
        flash_wait_busy();

        if (FLASH->SR & (FLASH_SR_OPERR | FLASH_SR_WRPERR | FLASH_SR_PGAERR | FLASH_SR_PGPERR | FLASH_SR_PGSERR))
        {
            ok = 0;
            break;
        }
        if (*(volatile uint32_t *)(dest + i) != word)
        {
            ok = 0; // read-back verification failed
            break;
        }
        i += 4U;
    }

    FLASH->CR &= ~FLASH_CR_PG;
    flash_lock();
    return ok;
}
