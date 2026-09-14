#ifndef BOOT_METADATA_H
#define BOOT_METADATA_H

#include <stdint.h>
#include "flash_map.h"

#define BOOT_META_MAGIC   0xB007CAFEUL

/* Deliberately "random-looking" 32-bit values (not 0x00000000 or
 * 0xFFFFFFFF) so a torn/half-erased write is unlikely to look valid.
 */
typedef enum {
    SLOT_STATE_EMPTY    = 0xFFFFFFFFUL, // erased flash, nothing written
    SLOT_STATE_NEW      = 0x5AA5A55AUL, // freshly programmed, unverified
    SLOT_STATE_TESTING  = 0x33CC33CCUL, // booted once, awaiting commit
    SLOT_STATE_VALID    = 0xAA55AA55UL, // application confirmed itself
    SLOT_STATE_INVALID  = 0x00000000UL  // failed validation / rolled back
} slot_state_t;

typedef struct {
    uint32_t size;          // firmware image size in bytes
    uint32_t crc32;         // CRC32 (zlib/PKZIP polynomial) of image
    uint32_t state;         // slot_state_t
    uint32_t boot_attempts; // consecutive un-committed boot attempts
} slot_info_t;

typedef struct {
    uint32_t     magic;
    uint32_t     seq;             // monotonically increasing record index
    uint32_t     active_slot;     // SLOT_A or SLOT_B
    slot_info_t  slot[NUM_SLOTS];
    uint32_t     crc32;           // CRC over every field above
} boot_metadata_t;

#define MAX_BOOT_ATTEMPTS 3U

/* Loads the most recent valid metadata record from the ping-pong log
 * in the metadata sector. Returns 1 on success, 0 if no valid record
 * was found (fresh/blank chip) - caller should fall back to defaults.
 */
int boot_metadata_load(boot_metadata_t *out);

/* Appends a new metadata record to the log, erasing/wrapping the
 * sector once it fills up. Returns 1 on success, 0 on flash error.
 */
int boot_metadata_save(boot_metadata_t *meta);

#endif // BOOT_METADATA_H
