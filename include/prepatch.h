#pragma once
#include <efi/efi.h>
#include <efi/efilib.h>
#include <smbios.h>

#define SMBIOS_START_ADDRESS 0xF0000
#define SMBIOS_END_ADDRESS   0xFFFFF

#define EFI_SYSTEM_TABLE_MAX_ADDRESS 0xFFFFFFFF

#define SMBIOS_PTR        EFI_SIGNATURE_32('_','S','M','_')

// Find the SMBIOS entry point structure
VOID* FindOemSMBIOSPtr(VOID);
VOID* GetSmbiosTablesFromHob (VOID);
VOID* GetSmbiosTablesFromConfigTables (VOID);
BOOLEAN IsEntryPointStructureValid (IN SMBIOS_TABLE_ENTRY_POINT *EntryPointStructure);

// Get table from type
SMBIOS_STRUCTURE_POINTER GetSmbiosTableFromType(SMBIOS_TABLE_ENTRY_POINT *SmbiosPoint, UINT8 SmbiosType, UINTN IndexTable);
UINT16 SmbiosTableLength (SMBIOS_STRUCTURE_POINTER SmbiosTable);

// Get string from table
CHAR8 *GetSmbiosString(SMBIOS_STRUCTURE_POINTER SmbiosTableN, SMBIOS_TABLE_STRING StringN);
EFI_STATUS UpdateSmbiosString(OUT SMBIOS_STRUCTURE_POINTER SmbiosTableN, SMBIOS_STRING *Field, IN CONST CHAR8 *Buffer);
UINTN iStrLen(CONST CHAR8 *String, UINTN MaxLen);
CHAR16 *AsciiToUnicode(IN CONST CHAR8 *AsciiString);


VOID* FindSmbios(VOID);
EFI_STATUS PrepatchSmbios(VOID* SmbiosEntryPoint);
VOID AddSmbiosEndOfTable();

VOID edit_random_string(OUT SMBIOS_STRUCTURE_POINTER SmbiosTableN, SMBIOS_STRING *Field);
VOID LogSmbiosTable (SMBIOS_STRUCTURE_POINTER SmbiosTableN);

EFI_STATUS PatchAll();
EFI_STATUS PatchTableType0();
EFI_STATUS PatchTableType1();
EFI_STATUS PatchTableType2();
EFI_STATUS PatchTableType3();


EFI_STATUS CopyTable(UINT8 SmbiosType);
