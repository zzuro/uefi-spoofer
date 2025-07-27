#include <prepatch.h>
#include <PiHob.h>
#include <HobLib.h>
#include <crc32.h>
#include <random.h>

static SMBIOS_TABLE_ENTRY_POINT *Smbios;
static SMBIOS_TABLE_ENTRY_POINT *EntryPoint;
static SMBIOS_TABLE_ENTRY_POINT *SmbiosEpsNew; // new SmbiosEps
static UINT8 *current;                         // pointer to the current end of tables
static SMBIOS_STRUCTURE_POINTER newSmbiosTable;
static UINT16 NumberOfRecords;
static UINT16 MaxStructureSize;

static EFI_GUID *gTableGuidArray[] = {&gEfiSmbiosTableGuid, &gEfiSmbios3TableGuid};

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

UINT16 SmbiosTableLength(SMBIOS_STRUCTURE_POINTER SmbiosTable)
{
    CHAR8 *AChar;
    UINT16 Length;

    AChar = (CHAR8 *)(SmbiosTable.Raw + SmbiosTable.Hdr->Length);
    while ((*AChar != 0) || (*(AChar + 1) != 0))
    { // area is /0/0 terminated
        AChar++;
    }
    Length = (UINT16)((UINTN)AChar - (UINTN)SmbiosTable.Raw + 2); // +2 for the /0/0 termination
    return Length;
}

CHAR8 *GetSmbiosString(SMBIOS_STRUCTURE_POINTER SmbiosTableN, SMBIOS_TABLE_STRING StringN)
{
    CHAR8 *AString;
    UINT8 Ind;

    Ind = 1;
    AString = (CHAR8 *)(SmbiosTableN.Raw + SmbiosTableN.Hdr->Length); // first string
    while (Ind != StringN)
    {
        while (*AString != 0)
        {
            AString++;
        }
        AString++; // skip zero ending
        if (*AString == 0)
        {
            return AString; // this is end of the table
        }
        Ind++;
    }

    return AString; // return pointer to Ascii string
}

EFI_STATUS UpdateSmbiosString(OUT SMBIOS_STRUCTURE_POINTER SmbiosTableN,
                              SMBIOS_STRING *Field, IN CONST CHAR8 *Buffer)
{
    CHAR8 *AString;
    CHAR8 *C1;
    CHAR8 *C2;
    UINTN Length = SmbiosTableLength(SmbiosTableN);
    UINTN ALength, BLength;
    UINT8 IndexStr = 1;

    if ((SmbiosTableN.Raw == NULL) || !Buffer || !Field)
    {
        return EFI_NOT_FOUND;
    }

    AString = (CHAR8 *)(SmbiosTableN.Raw + SmbiosTableN.Hdr->Length); // first string
    while (IndexStr != *Field)
    {
        if (*AString)
        {
            IndexStr++;
        }
        while (*AString != 0)
            AString++; // skip string at index
        AString++;     // next string
        if (*AString == 0)
        {
            // this is end of the table
            if (*Field == 0)
            {
                AString[1] = 0; // one more zero
            }
            *Field = IndexStr; // index of the next string that  is empty
            if (IndexStr == 1)
            {
                AString--; // first string has no leading zero
            }
            break;
        }
    }

    // AString is at place to copy
    ALength = iStrLen(AString, 0);
    BLength = iStrLen(Buffer, MAX_OEM_STRING);

    if (BLength > ALength)
    {
        // Shift right
        C1 = (CHAR8 *)SmbiosTableN.Raw + Length; // old end
        C2 = C1 + BLength - ALength;             // new end
        *C2 = 0;
        while (C1 != AString)
            *(--C2) = *(--C1);
    }
    else if (BLength < ALength)
    {
        // Shift left
        C1 = AString + ALength; // old start
        C2 = AString + BLength; // new start
        while (C1 != ((CHAR8 *)SmbiosTableN.Raw + Length))
        {
            *C2++ = *C1++;
        }
        *C2 = 0;
        *(--C2) = 0; // end of table
    }
    CopyMem(AString, Buffer, BLength);
    *(AString + BLength) = 0; // not sure there is 0

    return EFI_SUCCESS;
}

UINTN iStrLen(CONST CHAR8 *String, UINTN MaxLen)
{
    UINTN Len = 0;
    CONST CHAR8 *BA;
    if (MaxLen > 0)
    {
        for (Len = 0; Len < MaxLen; Len++)
        {
            if (String[Len] == 0)
            {
                break;
            }
        }
        BA = &String[Len - 1];
        while ((Len != 0) && ((*BA == ' ') || (*BA == 0)))
        {
            BA--;
            Len--;
        }
    }
    else
    {
        BA = String;
        while (*BA)
        {
            BA++;
            Len++;
        }
    }
    return Len;
}

CHAR16 *
AsciiToUnicode(
    IN CONST CHAR8 *AsciiString)
{
    UINTN Length = iStrLen(AsciiString, 0);
    CHAR16 *UnicodeString = AllocateZeroPool((Length + 1) * sizeof(CHAR16));
    if (UnicodeString == NULL)
    {
        return NULL;
    }

    for (UINTN i = 0; i < Length; i++)
    {
        UnicodeString[i] = (CHAR16)AsciiString[i];
    }

    return UnicodeString;
}

SMBIOS_STRUCTURE_POINTER GetSmbiosTableFromType(SMBIOS_TABLE_ENTRY_POINT *SmbiosPoint, UINT8 SmbiosType, UINTN IndexTable)
{
    SMBIOS_STRUCTURE_POINTER SmbiosTable;
    UINTN SmbiosTypeIndex = 0;

    SmbiosTable.Raw = (UINT8 *)(UINTN)SmbiosPoint->TableAddress;
    if (SmbiosTable.Raw == NULL)
    {
        return SmbiosTable;
    }

    while ((SmbiosTypeIndex != IndexTable) || (SmbiosTable.Hdr->Type != SmbiosType))
    {
        if (SmbiosTable.Hdr->Type == SMBIOS_TYPE_END_OF_TABLE)
        {
            SmbiosTable.Raw = NULL;
            return SmbiosTable;
        }

        if (SmbiosTable.Hdr->Type == SmbiosType)
        {
            SmbiosTypeIndex++;
        }

        SmbiosTable.Raw = (UINT8 *)(SmbiosTable.Raw + SmbiosTableLength(SmbiosTable));
    }

    return SmbiosTable;
}

VOID LogSmbiosTable(SMBIOS_STRUCTURE_POINTER SmbiosTableN)
{
    UINT16 Length = SmbiosTableLength(SmbiosTableN);
    if (Length > MaxStructureSize)
    {
        MaxStructureSize = Length;
    }

    CopyMem(current, SmbiosTableN.Raw, Length);
    current += Length;
    NumberOfRecords++;
}

VOID AddSmbiosEndOfTable()
{
    SMBIOS_STRUCTURE *StructurePtr = (SMBIOS_STRUCTURE *)current;
    StructurePtr->Type = SMBIOS_TYPE_END_OF_TABLE;
    StructurePtr->Length = sizeof(SMBIOS_STRUCTURE);
    StructurePtr->Handle = SMBIOS_TYPE_INACTIVE; // spec 2.7 p.120
    current += sizeof(SMBIOS_STRUCTURE);
    *current++ = 0;
    *current++ = 0; // double 0 at the end
    NumberOfRecords++;
}

EFI_STATUS PrepatchSmbios(VOID *SmbiosEntryPoint)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_PHYSICAL_ADDRESS BufferPtr;
    UINTN BufferLen;
    if (!SmbiosEntryPoint)
    {
        return EFI_NOT_FOUND;
    }

    EntryPoint = (SMBIOS_TABLE_ENTRY_POINT *)SmbiosEntryPoint;
    BufferLen = 0x20 + Smbios->TableLength + 64 * 10;

    BufferPtr = EFI_SYSTEM_TABLE_MAX_ADDRESS;
    Status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateMaxAddress, EfiACPIMemoryNVS, EFI_SIZE_TO_PAGES(BufferLen), &BufferPtr);
    if (EFI_ERROR(Status))
    {
        Print(L"[Spoofer] Failed to allocate memory for new SMBIOS Entry Point: %r", Status);
        return Status;
    }

    SmbiosEpsNew = (SMBIOS_TABLE_ENTRY_POINT *)(UINTN)BufferPtr;

    ZeroMem(SmbiosEpsNew, BufferLen);
    NumberOfRecords = 0;
    MaxStructureSize = 0;
    CopyMem((VOID *)SmbiosEpsNew, (VOID *)EntryPoint, sizeof(SMBIOS_TABLE_ENTRY_POINT));

    Smbios = (VOID *)(SmbiosEpsNew + 1); // put tables after the entry point structure
    current = (UINT8 *)Smbios;
    SmbiosEpsNew->TableAddress = (UINT32)(UINTN)current;
    SmbiosEpsNew->EntryPointLength = sizeof(SMBIOS_TABLE_ENTRY_POINT);
    return Status;
}

VOID edit_random_string(OUT SMBIOS_STRUCTURE_POINTER SmbiosTableN, SMBIOS_STRING *Field)
{
    CHAR8 *RandomString = AllocateZeroPool(MAX_OEM_STRING);
    if (RandomString == NULL)
    {
        Print(L"[Spoofer] Failed to allocate memory for random string.\n");
        return;
    }

    random_string(RandomString, iStrLen(GetSmbiosString(SmbiosTableN, *Field), 64) + 1);
    EFI_STATUS Status = UpdateSmbiosString(SmbiosTableN, Field, RandomString);
    if (EFI_ERROR(Status))
    {
        Print(L"[Spoofer] Failed to update SMBIOS string: %r\n", Status);
    }

    FreePool(RandomString);
}

EFI_STATUS PatchAll()
{
    EFI_STATUS Status = EFI_SUCCESS;
    newSmbiosTable.Raw = (UINT8 *)AllocateZeroPool(MAX_TABLE_SIZE);
    Status = PatchTableType0();
    if (EFI_ERROR(Status))
    {
        Print(L"[Spoofer] Failed to patch SMBIOS Type 0 table\n");
        return Status;
    }

    Status = PatchTableType1();
    if (EFI_ERROR(Status))
    {
        Print(L"[Spoofer] Failed to patch SMBIOS Type 1 table\n");
    }

    Status = PatchTableType2();
    if (EFI_ERROR(Status))
    {
        Print(L"[Spoofer] Failed to patch SMBIOS Type 2 table\n");
    }

    Status = PatchTableType3();
    if (EFI_ERROR(Status))
    {
        Print(L"[Spoofer] Failed to patch SMBIOS Type 3 table\n");
    }

    Status = PatchTableType4();
    if (EFI_ERROR(Status))
    {
        Print(L"[Spoofer] Failed to patch SMBIOS Type 4 table\n");
    }

    AddSmbiosEndOfTable();

    SmbiosEpsNew->TableLength = (UINT16)((UINT32)(UINTN)current - (UINT32)(UINTN)Smbios);
    SmbiosEpsNew->NumberOfSmbiosStructures = NumberOfRecords;
    SmbiosEpsNew->MaxStructureSize = MaxStructureSize;
    SmbiosEpsNew->IntermediateChecksum = 0;
    SmbiosEpsNew->IntermediateChecksum = (UINT8)(256 - Checksum8((UINT8 *)SmbiosEpsNew + 0x10, SmbiosEpsNew->EntryPointLength - 0x10));
    SmbiosEpsNew->EntryPointStructureChecksum = 0;
    SmbiosEpsNew->EntryPointStructureChecksum = (UINT8)(256 - Checksum8((UINT8 *)SmbiosEpsNew, SmbiosEpsNew->EntryPointLength));

    if (EFI_ERROR(Status))
    {
        Print(L"[Spoofer] Failed to calculate CRC32: %r", Status);
        return Status;
    }

    EFI_PEI_HOB_POINTERS GuidHob;
    EFI_PEI_HOB_POINTERS HobStart;
    EFI_PHYSICAL_ADDRESS *Table = NULL;

    HobStart.Raw = GetHobList();
    if (HobStart.Raw != NULL)
    {
        for (UINTN Index = 0; Index < sizeof(gTableGuidArray) / sizeof(*gTableGuidArray); ++Index)
        {
            GuidHob.Raw = GetNextGuidHob(gTableGuidArray[Index], HobStart.Raw);
            if (GuidHob.Raw != NULL)
            {
                Table = GET_GUID_HOB_DATA(GuidHob.Guid);
                if (Table != NULL)
                {
                    if (Index != 0)
                    {
                        Print(L"[Spoofer] Found SMBIOS3 Table\n");
                    }
                    break;
                }
            }
        }
    }

    Status = uefi_call_wrapper(BS->InstallConfigurationTable, 2, &gEfiSmbiosTableGuid, (VOID *)SmbiosEpsNew);
    if (EFI_ERROR(Status))
    {
        Print(L"[Spoofer] Failed to install SMBIOS configuration table: %r", Status);
        return Status;
    }

    ST->Hdr.CRC32 = 0;
    Status = uefi_call_wrapper(BS->CalculateCrc32, 3, (UINT8 *)&ST->Hdr, ST->Hdr.HeaderSize, &ST->Hdr.CRC32);

    if (Table != NULL)
    {
        *Table = (EFI_PHYSICAL_ADDRESS)(UINTN)SmbiosEpsNew;
        Print(L"[Spoofer] Updated SMBIOS Table in HOB\n");
    }

    FreePool((VOID *)newSmbiosTable.Raw);
    return Status;
}

EFI_STATUS CopyTable(UINT8 SmbiosType)
{
    EFI_STATUS Status = EFI_SUCCESS;
    SMBIOS_STRUCTURE_POINTER SmbiosTable = GetSmbiosTableFromType(EntryPoint, SmbiosType, 0);
    if (SmbiosTable.Raw == NULL)
    {
        return EFI_NOT_FOUND;
    }

    ZeroMem((VOID *)newSmbiosTable.Raw, MAX_TABLE_SIZE);
    CopyMem((VOID *)newSmbiosTable.Raw, (VOID *)SmbiosTable.Raw, SmbiosTableLength(SmbiosTable));

    LogSmbiosTable(newSmbiosTable);

    return Status;
}

EFI_STATUS PatchTableType0()
{
    EFI_STATUS Status = EFI_SUCCESS;
    SMBIOS_STRUCTURE_POINTER SmbiosTable = GetSmbiosTableFromType(EntryPoint, SMBIOS_TYPE_BIOS_INFORMATION, 0);
    if (SmbiosTable.Raw == NULL || SmbiosTable.Hdr->Type != SMBIOS_TYPE_BIOS_INFORMATION)
    {
        Print(L"SMBIOS Type 0 table not found.\n");
        return EFI_NOT_FOUND;
    }

    ZeroMem((VOID *)newSmbiosTable.Type0, MAX_TABLE_SIZE);
    CopyMem((VOID *)newSmbiosTable.Type0, (VOID *)SmbiosTable.Type0, SmbiosTableLength(SmbiosTable));

    edit_random_string(newSmbiosTable, &newSmbiosTable.Type0->Vendor);
    edit_random_string(newSmbiosTable, &newSmbiosTable.Type0->BiosVersion);
    edit_random_string(newSmbiosTable, &newSmbiosTable.Type0->BiosReleaseDate);

    LogSmbiosTable(newSmbiosTable);

    return Status;
}

EFI_STATUS PatchTableType1()
{
    EFI_STATUS Status = EFI_SUCCESS;
    SMBIOS_STRUCTURE_POINTER SmbiosTable = GetSmbiosTableFromType(EntryPoint, SMBIOS_TYPE_SYSTEM_INFORMATION, 0);
    if (SmbiosTable.Raw == NULL || SmbiosTable.Hdr->Type != SMBIOS_TYPE_SYSTEM_INFORMATION)
    {
        Print(L"SMBIOS Type 1 table not found.\n");
        return EFI_NOT_FOUND;
    }

    ZeroMem((VOID *)newSmbiosTable.Type1, MAX_TABLE_SIZE);
    CopyMem((VOID *)newSmbiosTable.Type1, (VOID *)SmbiosTable.Type1, SmbiosTableLength(SmbiosTable));

    edit_random_string(newSmbiosTable, &newSmbiosTable.Type1->Manufacturer);
    edit_random_string(newSmbiosTable, &newSmbiosTable.Type1->ProductName);
    edit_random_string(newSmbiosTable, &newSmbiosTable.Type1->Version);
    edit_random_string(newSmbiosTable, &newSmbiosTable.Type1->SerialNumber);

    LogSmbiosTable(newSmbiosTable);
    return Status;
}

EFI_STATUS PatchTableType2()
{
    EFI_STATUS Status = EFI_SUCCESS;
    SMBIOS_STRUCTURE_POINTER SmbiosTable = GetSmbiosTableFromType(EntryPoint, SMBIOS_TYPE_BASEBOARD_INFORMATION, 0);
    if (SmbiosTable.Raw == NULL || SmbiosTable.Hdr->Type != SMBIOS_TYPE_BASEBOARD_INFORMATION)
    {
        Print(L"SMBIOS Type 2 table not found.\n");
        return EFI_NOT_FOUND;
    }

    ZeroMem((VOID *)newSmbiosTable.Type2, MAX_TABLE_SIZE);
    CopyMem((VOID *)newSmbiosTable.Type2, (VOID *)SmbiosTable.Type2, SmbiosTableLength(SmbiosTable));

    edit_random_string(newSmbiosTable, &newSmbiosTable.Type2->Manufacturer);
    edit_random_string(newSmbiosTable, &newSmbiosTable.Type2->ProductName);
    edit_random_string(newSmbiosTable, &newSmbiosTable.Type2->Version);
    edit_random_string(newSmbiosTable, &newSmbiosTable.Type2->SerialNumber);

    LogSmbiosTable(newSmbiosTable);
    return Status;
}

EFI_STATUS PatchTableType3()
{
    EFI_STATUS Status = EFI_SUCCESS;
    SMBIOS_STRUCTURE_POINTER SmbiosTable = GetSmbiosTableFromType(EntryPoint, SMBIOS_TYPE_SYSTEM_ENCLOSURE, 0);
    if (SmbiosTable.Raw == NULL || SmbiosTable.Hdr->Type != SMBIOS_TYPE_SYSTEM_ENCLOSURE)
    {
        Print(L"SMBIOS Type 3 table not found.\n");
        return EFI_NOT_FOUND;
    }

    ZeroMem((VOID *)newSmbiosTable.Type3, MAX_TABLE_SIZE);
    CopyMem((VOID *)newSmbiosTable.Type3, (VOID *)SmbiosTable.Type3, SmbiosTableLength(SmbiosTable));

    edit_random_string(newSmbiosTable, &newSmbiosTable.Type3->Manufacturer);
    edit_random_string(newSmbiosTable, &newSmbiosTable.Type3->Version);
    edit_random_string(newSmbiosTable, &newSmbiosTable.Type3->SerialNumber);
    edit_random_string(newSmbiosTable, &newSmbiosTable.Type3->AssetTag);

    LogSmbiosTable(newSmbiosTable);
    return Status;
}

EFI_STATUS PatchTableType4()
{
    EFI_STATUS Status = EFI_SUCCESS;
    SMBIOS_STRUCTURE_POINTER SmbiosTable = GetSmbiosTableFromType(EntryPoint, SMBIOS_TYPE_PROCESSOR_INFORMATION, 0);
    if (SmbiosTable.Raw == NULL || SmbiosTable.Hdr->Type != SMBIOS_TYPE_PROCESSOR_INFORMATION)
    {
        Print(L"SMBIOS Type 4 table not found.\n");
        return EFI_NOT_FOUND;
    }

    ZeroMem((VOID *)newSmbiosTable.Type4, MAX_TABLE_SIZE);
    CopyMem((VOID *)newSmbiosTable.Type4, (VOID *)SmbiosTable.Type4, SmbiosTableLength(SmbiosTable));

    edit_random_string(newSmbiosTable, &newSmbiosTable.Type4->ProcessorManufacture);
    edit_random_string(newSmbiosTable, &newSmbiosTable.Type4->ProcessorVersion);
    
    for(UINTN i = 0; i < 8; i++)
    {
        newSmbiosTable.Type4->ProcessorId[i] = (UINTN)pseudo_random_byte(0, 255);
    }

    LogSmbiosTable(newSmbiosTable);
    return Status;
}