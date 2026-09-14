#ifndef UART_PROTOCOL_H
#define UART_PROTOCOL_H

#include <stdint.h>

#define UART_MAGIC_STR    "BLUPDATE"
#define UART_ACK          0x06U
#define UART_NAK          0x15U
#define PKT_MAX_PAYLOAD   256U
#define PKT_MAX_RETRIES   5U

/* Listens for the update-request magic string for up to window_ms.
 * Sends UART_ACK and returns 1 if the host requested an update.
 */
int uart_wait_for_update_request(uint32_t window_ms);

/* Receives and flashes a firmware image into target_slot, protected
 * by a per-packet CRC32 (retried) and a whole-image CRC32 (final
 * accept/reject). Returns 1 on success and fills out_size/out_crc.
 */
int uart_recv_firmware(uint32_t target_slot, uint32_t *out_size, uint32_t *out_crc);

#endif // UART_PROTOCOL_H
