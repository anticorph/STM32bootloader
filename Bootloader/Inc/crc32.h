#ifndef CRC32_H
#define CRC32_H

#include <stdint.h>

/* Standard CRC-32 (poly 0xEDB88320, reflected) - identical to
 * Python's zlib.crc32()/binascii.crc32(), so the host tool and the
 * firmware always agree without any extra glue code.
 */
uint32_t crc32_compute(const uint8_t *data, uint32_t len);
uint32_t crc32_update(uint32_t crc, const uint8_t *data, uint32_t len);

#endif // CRC32_H 
