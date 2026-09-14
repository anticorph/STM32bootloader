#include "stm32f4xx.h"
#include "app_commit.h"

// Must match COMMIT_MAGIC in Bootloader/Src/main.c exactly
#define COMMIT_MAGIC 0x600DC0DEUL

void boot_commit(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR      |= PWR_CR_DBP; // unlock RTC backup register writes
    RTC->BKP0R    = COMMIT_MAGIC;
}
