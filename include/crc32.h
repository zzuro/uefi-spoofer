#pragma once
#include <efi/efi.h>
#include <efi/efilib.h>

VOID make_crc_table();
UINT32 crc32(UINT8 *buf, UINT64 len);

UINT8 Checksum8(void* startPtr, UINT32 len);