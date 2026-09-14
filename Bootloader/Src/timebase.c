#include "stm32f4xx.h"
#include "timebase.h"

static volatile uint32_t g_ms_ticks = 0;

void SysTick_Handler(void)
{
    g_ms_ticks++;
}

void timebase_init(uint32_t core_clock_hz)
{
    SysTick_Config(core_clock_hz / 1000U); // Interrupts each 1ms
}

uint32_t millis(void)
{
    return g_ms_ticks;
}

void delay_ms(uint32_t ms)
{
    uint32_t start = millis();
    while ((millis() - start) < ms) { __NOP(); }
}
