#pragma once
#include <efi/efi.h>
#include <efi/efilib.h>

VOID make_crc_table();
UINT32 crc32(UINT8 *buf, UINT64 len);