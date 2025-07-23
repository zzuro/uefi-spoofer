#pragma once
#include <efi/efi.h>
#include <efi/efilib.h>
#include <efiprot.h>


typedef struct
{
    UINT32 TimeLow;
    UINT16 TimeMid;
    UINT16 TimeHiAndVersion;
    UINT8 ClockSeqHiAndReserved;
    UINT8 ClockSeqLow;
    UINT8 Node[6];
} __attribute__((packed)) GUID;


EFI_STATUS spoof_GUID(GUID *guid);
UINT8 pseudo_random_byte();