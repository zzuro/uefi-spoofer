#pragma once
#include <efi/efi.h>
#include <efi/efilib.h>
#include <smbios.h>

#define SMBIOS_START_ADDRESS 0xF0000
#define SMBIOS_END_ADDRESS   0xFFFFF

#define SMBIOS_PTR        EFI_SIGNATURE_32('_','S','M','_')

VOID* FindOemSMBIOSPtr(VOID);
VOID* GetSmbiosTablesFromHob (VOID);
VOID* GetSmbiosTablesFromConfigTables (VOID);

BOOLEAN IsEntryPointStructureValid (IN SMBIOS_TABLE_ENTRY_POINT *EntryPointStructure);

VOID* FindSmbios(VOID);
EFI_STATUS PrepatchSmbios(VOID* SmbiosEntryPoint);