#include <crc32.h>

static UINT32 crc_table[256];

VOID make_crc_table()
{
    UINT32 c;
    for (int n = 0; n < 256; n++)
    {
        c = (UINT32)n;
        for (int k = 0; k < 8; k++)
        {
            if (c & 1)
            {
                c = 0xedb88320L ^ (c >> 1);
            }
            else
            {
                c = c >> 1;
            }
        }
        crc_table[n] = c;
    }
}

UINT32 crc32(UINT8 *buf, UINT64 len)
{
    static BOOLEAN crc_table_computed = 0;
    if (!crc_table_computed)
    {
        make_crc_table();
        crc_table_computed = 1;
    }

    UINT32 c = 0xFFFFFFFFL;
    for (UINT64 n = 0; n < len; n++)
    {
        c = crc_table[(c ^ buf[n]) & 0xFF] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFL;
}

UINT8 Checksum8(void* startPtr, UINT32 len)
{
  UINT8 Value = 0;
  UINT8 *ptr = (UINT8*)startPtr;
  UINT8 *endPtr = ptr + len;
  while (ptr < endPtr)
    Value += *ptr++;
  return Value;
}
