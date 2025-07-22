#pragma once
#include <efi/efi.h>
#include <efi/efilib.h>

typedef struct
{
    UINT8 Signature[8];
    UINT32 Revision;
    UINT32 HeaderSize;
    UINT32 HeaderCRC32;
    UINT32 Reserved;
    UINT64 MyLBA;
    UINT64 AlternateLBA;
    UINT64 FirstUsableLBA;
    UINT64 LastUsableLBA;
    EFI_GUID DiskGUID;
    UINT64 PartitionEntryLBA;
    UINT32 NumberOfPartitionEntries;
    UINT32 SizeOfPartitionEntry;
    UINT32 PartitionEntryArrayCRC32;
    UINT8 Reserved2[420];
} __attribute__((packed)) GPT_HEADER;


typedef struct
{
    EFI_GUID PartitionTypeGUID;
    EFI_GUID UniquePartitionGUID;
    UINT64 StartingLBA;
    UINT64 EndingLBA;
    UINT64 Attributes;
    UINT16 PartitionName[36]; 
} __attribute__((packed)) GPT_PARTITION_ENTRY;