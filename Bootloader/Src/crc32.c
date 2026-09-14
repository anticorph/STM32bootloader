#include "crc32.h"

static uint32_t crc_table[256];
static int table_ready = 0;

// Building a table
static void build_table(void)
{
    for (uint32_t i = 0; i < 256U; i++)
    {
        uint32_t c = i;
        for (int k = 0; k < 8; k++)
        {
            c = (c & 1U) ? (0xEDB88320UL ^ (c >> 1)) : (c >> 1);
        }
        crc_table[i] = c;
    }
    table_ready = 1;
}

uint32_t crc32_update(uint32_t crc, const uint8_t *data, uint32_t len)
{
    if (!table_ready)
    {
        build_table();
    }
    uint32_t c = crc ^ 0xFFFFFFFFUL;
    for (uint32_t i = 0; i < len; i++)
    {
        c = crc_table[(c ^ data[i]) & 0xFFU] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFUL;
}

uint32_t crc32_compute(const uint8_t *data, uint32_t len)
{
    return crc32_update(0, data, len);
}
