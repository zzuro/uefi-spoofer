#include <prepatch.h>
#include <PiHob.h>
#include <HobLib.h>

static SMBIOS_TABLE_ENTRY_POINT    *EntryPoint;     //SmbiosEps original
//static SMBIOS_TABLE_ENTRY_POINT    *SmbiosEpsNew;   //new SmbiosEps

BOOLEAN IsEntryPointStructureValid(IN SMBIOS_TABLE_ENTRY_POINT *EntryPointStructure)
{
    UINTN I;
    UINT8 Length;
    UINT8 Checksum = 0;
    UINT8 *BytePtr;

    if (!EntryPointStructure)
    {
        return FALSE;
    }

    BytePtr = (UINT8 *)EntryPointStructure;
    Length = EntryPointStructure->EntryPointLength;

    for (I = 0; I < Length; I++)
    {
        Checksum = Checksum + (UINT8)BytePtr[I];
    }

    // a valid SMBIOS EPS must have checksum of 0
    return (Checksum == 0);
}

VOID *FindOemSMBIOSPtr(VOID)
{
    for (UINTN address = SMBIOS_START_ADDRESS; address < SMBIOS_END_ADDRESS; address += 0x10)
    {
        if (*(UINT32 *)address == SMBIOS_PTR && IsEntryPointStructureValid((SMBIOS_TABLE_ENTRY_POINT *)address))
        {
            return (VOID *)address;
        }
    }
    return NULL;
}

VOID *GetSmbiosTablesFromHob(VOID)
{
    EFI_PHYSICAL_ADDRESS *Table;
    EFI_PEI_HOB_POINTERS GuidHob;

    GuidHob.Raw = GetFirstGuidHob(&gEfiSmbiosTableGuid);
    if (GuidHob.Raw != NULL)
    {
        Table = (EFI_PHYSICAL_ADDRESS *)GET_GUID_HOB_DATA(GuidHob.Guid);
        if (Table != NULL)
        {
            return (VOID *)(UINTN)*Table;
        }
    }
    GuidHob.Raw = GetFirstGuidHob(&gEfiSmbios3TableGuid);
    if (GuidHob.Raw != NULL)
    {
        Table = (EFI_PHYSICAL_ADDRESS *)GET_GUID_HOB_DATA(GuidHob.Guid);
        if (Table != NULL)
        {
            return (VOID *)(UINTN)*Table;
        }
    }
    return NULL;
}

VOID *GetSmbiosTablesFromConfigTables(VOID)
{
    EFI_STATUS Status;
    EFI_PHYSICAL_ADDRESS *Table;

    Status = LibGetSystemConfigurationTable(&gEfiSmbiosTableGuid, (VOID **)&Table);
    if (EFI_ERROR(Status) || Table == NULL)
    {
        Table = NULL;
        Status = LibGetSystemConfigurationTable(&gEfiSmbios3TableGuid, (VOID **)&Table);
        if (EFI_ERROR(Status))
        {
            Table = NULL;
        }
    }

    return Table;
}

VOID *FindSmbios(VOID)
{

    VOID *SmbiosEntryPoint;
    SmbiosEntryPoint = FindOemSMBIOSPtr();
    if (!SmbiosEntryPoint)
    {
        SmbiosEntryPoint = GetSmbiosTablesFromHob();
        if (!SmbiosEntryPoint)
        {
            SmbiosEntryPoint = GetSmbiosTablesFromConfigTables();
            if (!SmbiosEntryPoint)
            {
                return NULL;
            }
        }
    }

    return SmbiosEntryPoint;
}


EFI_STATUS PrepatchSmbios(VOID* SmbiosEntryPoint)
{
    if (!SmbiosEntryPoint)
    {
        return EFI_NOT_FOUND;
    }

    EntryPoint = (SMBIOS_TABLE_ENTRY_POINT *)SmbiosEntryPoint;
    Print(L"[Spoofer] Found SMBIOS Entry Point at: 0x%08x\n", EntryPoint->TableAddress);

    return EFI_SUCCESS;
}