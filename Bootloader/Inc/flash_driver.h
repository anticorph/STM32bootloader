#ifndef FLASH_DRIVER_H
#define FLASH_DRIVER_H

#include <stdint.h>

void flash_unlock(void);
void flash_lock(void);

// Erases one physical flash sector (0-11 on STM32F407)
int flash_erase_sector(uint32_t sector);

// Erases every sector that backs the given application slot
int flash_erase_slot(uint32_t slot);

/* Programs `len` bytes from `src` into flash starting at `dest`.
 * Not required to be 4-byte aligned in length; the final partial word
 * is padded with 0xFF. Verifies every word by read-back. Blocking.
 * Returns 1 on success, 0 on any flash error or verify mismatch.
 */
int flash_program(uint32_t dest, const uint8_t *src, uint32_t len);

#endif // FLASH_DRIVER_H
