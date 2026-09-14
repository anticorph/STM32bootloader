#ifndef UART_DRIVER_H
#define UART_DRIVER_H

#include <stdint.h>

void uart_init(uint32_t baudrate);
void uart_send_byte(uint8_t b);
void uart_send_buf(const uint8_t *buf, uint32_t len);

// Returns 1 and fills *b if a byte arrives within timeout_ms, else 0
int uart_recv_byte_timeout(uint8_t *b, uint32_t timeout_ms);

#endif // UART_DRIVER_H
