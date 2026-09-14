#ifndef TIMEBASE_H
#define TIMEBASE_H

#include <stdint.h>

void timebase_init(uint32_t core_clock_hz);
uint32_t millis(void);
void delay_ms(uint32_t ms);

#endif // TIMEBASE_H
