#include <string.h>
#include "uart_protocol.h"
#include "uart_driver.h"
#include "flash_driver.h"
#include "flash_map.h"
#include "crc32.h"
#include "timebase.h"

int uart_wait_for_update_request(uint32_t window_ms)
{
    const char *magic = UART_MAGIC_STR;
    uint32_t magic_len = 0;
    while (magic[magic_len] != '\0')
    {
        magic_len++;
    }

    uint32_t match_len = 0;
    uint32_t start = millis();
    uint8_t b;

    while ((millis() - start) < window_ms)
    {
        if (uart_recv_byte_timeout(&b, 20))
        {
            if (b == (uint8_t)magic[match_len])
            {
                match_len++;
                if (match_len == magic_len)
                {
                    uart_send_byte(UART_ACK);
                    return 1;
                }
            } else {
                match_len = (b == (uint8_t)magic[0]) ? 1U : 0U;
            }
        }
    }
    return 0;
}

static int recv_u32(uint32_t *out, uint32_t timeout_ms)
{
    uint8_t bytes[4];
    for (int i = 0; i < 4; i++)
    {
        if (!uart_recv_byte_timeout(&bytes[i], timeout_ms))
        {
            return 0;
        }
    }
    *out = (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
    return 1;
}

int uart_recv_firmware(uint32_t target_slot, uint32_t *out_size, uint32_t *out_crc)
{
    uint32_t size = 0, expected_crc = 0, reserved = 0;

    // Header: size (u32 LE), crc32 (u32 LE), reserved (u32 LE)
    if (!recv_u32(&size, 3000) || !recv_u32(&expected_crc, 3000) || !recv_u32(&reserved, 3000))
    {
        uart_send_byte(UART_NAK);
        return 0;
    }
    (void)reserved;

    if (size == 0U || size > slot_capacity(target_slot))
    {
        uart_send_byte(UART_NAK);
        return 0;
    }
    uart_send_byte(UART_ACK);

    if (!flash_erase_slot(target_slot))
    {
        uart_send_byte(UART_NAK);
        return 0;
    }

    uint32_t base = slot_base(target_slot);
    uint32_t received = 0;
    uint8_t packet[PKT_MAX_PAYLOAD];

    while (received < size)
    {
        uint32_t this_len = (size - received) > PKT_MAX_PAYLOAD ? PKT_MAX_PAYLOAD : (size - received);
        int accepted = 0;

        for (uint32_t attempt = 0; attempt < PKT_MAX_RETRIES && !accepted; attempt++)
        {
            uint32_t got = 0;
            while (got < this_len)
            {
                if (!uart_recv_byte_timeout(&packet[got], 2000))
                {
                    break;
                }
                got++;
            }

            uint32_t pkt_crc = 0;
            int have_crc = (got == this_len) && recv_u32(&pkt_crc, 2000);

            if (have_crc && crc32_compute(packet, this_len) == pkt_crc)
            {
                if (!flash_program(base + received, packet, this_len))
                {
                    uart_send_byte(UART_NAK);
                    return 0; // flash failure is not worth retrying
                }
                uart_send_byte(UART_ACK);
                accepted = 1;
            } else {
                uart_send_byte(UART_NAK); // ask host to resend this packet
            }
        }

        if (!accepted)
        {
            return 0; // too many retries on this packet, abort transfer
        }
        received += this_len;
    }

    uint32_t final_crc = crc32_compute((const uint8_t *)base, size);
    if (final_crc != expected_crc)
    {
        uart_send_byte(UART_NAK);
        return 0;
    }

    uart_send_byte(UART_ACK);
    *out_size = size;
    *out_crc = final_crc;
    return 1;
}
