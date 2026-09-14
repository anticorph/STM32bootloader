#include <string.h>
#include <stddef.h>
#include "boot_metadata.h"
#include "flash_driver.h"
#include "crc32.h"

/* Records are padded out to a fixed stride so we can scan the sector
 * without needing a separate length field, and so future growth of
 * boot_metadata_t has headroom without a layout migration.
 */
#define RECORD_STRIDE 64U
#define RECORDS_PER_SECTOR (METADATA_SIZE / RECORD_STRIDE)

static uint32_t compute_meta_crc(const boot_metadata_t *m)
{
    return crc32_compute((const uint8_t *)m, (uint32_t)offsetof(boot_metadata_t, crc32));
}

static const boot_metadata_t *record_at(uint32_t index)
{
    return (const boot_metadata_t *)(METADATA_BASE + index * RECORD_STRIDE);
}

int boot_metadata_load(boot_metadata_t *out)
{
    int found = 0;
    uint32_t best_seq = 0;

    for (uint32_t i = 0; i < RECORDS_PER_SECTOR; i++)
    {
        const boot_metadata_t *rec = record_at(i);

        if (rec->magic != BOOT_META_MAGIC)
        {
            continue; // erased (0xFFFFFFFF) or garbage
        }
        if (compute_meta_crc(rec) != rec->crc32)
        {
            continue; // torn write from a power-loss mid-program
        }
        if (!found || rec->seq >= best_seq)
        {
            best_seq = rec->seq;
            memcpy(out, rec, sizeof(*out));
            found = 1;
        }
    }
    return found;
}

static int find_free_slot_index(uint32_t *index_out)
{
    for (uint32_t i = 0; i < RECORDS_PER_SECTOR; i++)
    {
        if (record_at(i)->magic == 0xFFFFFFFFUL)
        {
            *index_out = i;
            return 1;
        }
    }
    return 0;
}

int boot_metadata_save(boot_metadata_t *meta)
{
    meta->magic = BOOT_META_MAGIC;
    meta->crc32 = compute_meta_crc(meta);

    uint32_t idx;
    if (!find_free_slot_index(&idx))
    {
        /* Log full: erase and start over. `seq` keeps counting up so
         * boot_metadata_load()'s "highest seq wins" logic still holds
         * even though physical index 0 is being reused.
         */
        if (!flash_erase_sector(METADATA_SECTOR))
        {
            return 0;
        }
        idx = 0;
    }

    uint32_t dest = METADATA_BASE + idx * RECORD_STRIDE;
    return flash_program(dest, (const uint8_t *)meta, sizeof(*meta));
}
