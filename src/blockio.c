#include <blockio.h>
#include <gpt.h>
#include <random.h>
#include <crc32.h>

EFI_HANDLE ImageHandle = NULL;

EFI_STATUS get_mediaID(UINT32 *mediaID)
{
    EFI_STATUS status = EFI_SUCCESS;

    // OpenProtocol chapter 7.3.9
    EFI_LOADED_IMAGE_PROTOCOL *loadedImage = NULL;
    status = uefi_call_wrapper(BS->OpenProtocol, 6, ImageHandle, &gEfiLoadedImageProtocolGuid, (VOID **)&loadedImage, ImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if (EFI_ERROR(status))
    {
        Print(L"Error opening Loaded Image Protocol\n");
        return status;
    }

    EFI_BLOCK_IO_PROTOCOL *blockIo = NULL;
    status = uefi_call_wrapper(BS->OpenProtocol, 6, loadedImage->DeviceHandle, &gEfiBlockIoProtocolGuid, (VOID **)&blockIo, ImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if (EFI_ERROR(status))
    {
        Print(L"Error opening Block IO Protocol\n");
    }

    *mediaID = blockIo->Media->MediaId;
    Print(L"Media ID: %d\n", *mediaID);

    return status;
}

EFI_STATUS print_partition()
{
    EFI_STATUS Status = EFI_SUCCESS;

    UINTN num_handles = 0;
    EFI_HANDLE *handle_buffer = NULL;
    EFI_BLOCK_IO_PROTOCOL *blockIo = NULL;

    Status = uefi_call_wrapper(BS->LocateHandleBuffer, 5, ByProtocol, &gEfiBlockIoProtocolGuid, NULL, &num_handles, &handle_buffer);
    if (EFI_ERROR(Status))
    {
        Print(L"Error locating Block IO handles \n");
        return Status;
    }

    for (UINTN i = 0; i < num_handles; i++)
    {
        Status = uefi_call_wrapper(BS->OpenProtocol, 6, handle_buffer[i], &gEfiBlockIoProtocolGuid, (VOID **)&blockIo, ImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
        if (EFI_ERROR(Status))
        {
            Print(L"Error opening Block IO Protocol for handle %d\n", i);
            continue;
        }

        if (blockIo->Media->LastBlock == 0)
        {
            continue;
        }

        EFI_GUID gEfiPartitionInfoProtocolGuid = EFI_PARTITION_INFO_PROTOCOL_GUID;

        if (blockIo->Media->LogicalPartition)
        {
            EFI_PARTITION_INFO_PROTOCOL *partitionInfo = NULL;
            Status = uefi_call_wrapper(BS->OpenProtocol, 6, handle_buffer[i], &gEfiPartitionInfoProtocolGuid, (VOID **)&partitionInfo, ImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
            if (EFI_ERROR(Status))
            {
                Print(L"Error opening Partition Info Protocol for handle %d\n", i);
                continue;
            }

            if (partitionInfo->Type == PARTITION_TYPE_GPT)
            {
                EFI_GUID guid = partitionInfo->Info.Gpt.UniquePartitionGUID;
                Print(L"GUID: %08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
                      guid.Data1, guid.Data2, guid.Data3,
                      guid.Data4[0], guid.Data4[1],
                      guid.Data4[2], guid.Data4[3],
                      guid.Data4[4], guid.Data4[5],
                      guid.Data4[6], guid.Data4[7]);
            }
        }
    }

    return EFI_SUCCESS;
}

static EFI_STATUS spoof_partition_entries(GPT_HEADER *gpt_header, EFI_BLOCK_IO_PROTOCOL *blockIo)
{
    EFI_STATUS Status = EFI_SUCCESS;
    void *partition_entries = (GPT_PARTITION_ENTRY *)AllocateZeroPool(gpt_header->NumberOfPartitionEntries * gpt_header->SizeOfPartitionEntry);
    Status = uefi_call_wrapper(blockIo->ReadBlocks, 5, blockIo, blockIo->Media->MediaId, gpt_header->PartitionEntryLBA, gpt_header->NumberOfPartitionEntries * gpt_header->SizeOfPartitionEntry, partition_entries);
    if (EFI_ERROR(Status))
    {
        return Status;
    }

    for (int j = 0; j < gpt_header->NumberOfPartitionEntries; j++)
    {
        GPT_PARTITION_ENTRY *gptEntry = (GPT_PARTITION_ENTRY *)partition_entries + j;
        if (gptEntry->UniquePartitionGUID.Data1 == 0)
        {
            continue;
        }

        spoof_GUID((GUID *)&gptEntry->UniquePartitionGUID);
    }

    uefi_call_wrapper(blockIo->WriteBlocks, 5, blockIo, blockIo->Media->MediaId, gpt_header->PartitionEntryLBA, gpt_header->NumberOfPartitionEntries * gpt_header->SizeOfPartitionEntry, partition_entries);
    if (EFI_ERROR(Status))
    {
        return Status;
    }

    gpt_header->PartitionEntryArrayCRC32 = crc32((UINT8 *)partition_entries, gpt_header->NumberOfPartitionEntries * gpt_header->SizeOfPartitionEntry);
    gpt_header->HeaderCRC32 = 0;
    gpt_header->HeaderCRC32 = crc32((UINT8 *)gpt_header, gpt_header->HeaderSize);

    Status = uefi_call_wrapper(blockIo->WriteBlocks, 5, blockIo, blockIo->Media->MediaId, gpt_header->MyLBA, sizeof(GPT_HEADER), gpt_header);
    if (EFI_ERROR(Status))
    {
        return Status;
    }

    FreePool(partition_entries);
    return Status;
}

EFI_STATUS spoof_partition()
{
    EFI_STATUS Status = EFI_SUCCESS;
    Status = print_partition();
    if (EFI_ERROR(Status))
    {
        return Status;
    }

    UINTN num_handles = 0;
    EFI_HANDLE *handle_buffer = NULL;
    EFI_BLOCK_IO_PROTOCOL *blockIo = NULL;

    Status = uefi_call_wrapper(BS->LocateHandleBuffer, 5, ByProtocol, &gEfiBlockIoProtocolGuid, NULL, &num_handles, &handle_buffer);
    if (EFI_ERROR(Status))
    {
        Print(L"Error locating Block IO handles \n");
        return Status;
    }

    UINT32 last_mediaID = -1;
    for (UINTN i = 0; i < num_handles; i++)
    {
        Status = uefi_call_wrapper(BS->OpenProtocol, 6, handle_buffer[i], &gEfiBlockIoProtocolGuid, (VOID **)&blockIo, ImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
        if (EFI_ERROR(Status))
        {
            Print(L"Error opening Block IO Protocol for handle %d\n", i);
            continue;
        }

        if (blockIo->Media->LastBlock == 0 || blockIo->Media->MediaId == last_mediaID)
        {
            continue;
        }

        last_mediaID = blockIo->Media->MediaId;

        // read primary GPT header
        GPT_HEADER *primary_gpt_header = (GPT_HEADER *)AllocateZeroPool(sizeof(GPT_HEADER));
        Status = uefi_call_wrapper(blockIo->ReadBlocks, 5, blockIo, blockIo->Media->MediaId, 1, sizeof(GPT_HEADER), primary_gpt_header);
        if (EFI_ERROR(Status))
        {
            Print(L"Error reading GPT header for handle %d\n", i);
            continue;
        }

        // read backup GPT header
        GPT_HEADER *backup_gpt_header = (GPT_HEADER *)AllocateZeroPool(sizeof(GPT_HEADER));
        Status = uefi_call_wrapper(blockIo->ReadBlocks, 5, blockIo, blockIo->Media->MediaId, primary_gpt_header->AlternateLBA, sizeof(GPT_HEADER), backup_gpt_header);
        if (EFI_ERROR(Status))
        {
            Print(L"Error reading backup GPT header for handle %d\n", i);
            continue;
        }

        Print(L"Disk GUID: %08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
              primary_gpt_header->DiskGUID.Data1, primary_gpt_header->DiskGUID.Data2, primary_gpt_header->DiskGUID.Data3,
              primary_gpt_header->DiskGUID.Data4[0], primary_gpt_header->DiskGUID.Data4[1],
              primary_gpt_header->DiskGUID.Data4[2], primary_gpt_header->DiskGUID.Data4[3],
              primary_gpt_header->DiskGUID.Data4[4], primary_gpt_header->DiskGUID.Data4[5],
              primary_gpt_header->DiskGUID.Data4[6], primary_gpt_header->DiskGUID.Data4[7]);

        spoof_GUID((GUID *)&primary_gpt_header->DiskGUID);
        CopyMem(&backup_gpt_header->DiskGUID, &primary_gpt_header->DiskGUID, sizeof(EFI_GUID));

        Status = spoof_partition_entries(primary_gpt_header, blockIo);
        if (EFI_ERROR(Status))
        {
            Print(L"Error spoofing partition entries for handle %d\n", i);
            continue;
        }

        Status = spoof_partition_entries(backup_gpt_header, blockIo);
        if (EFI_ERROR(Status))
        {
            Print(L"Error spoofing backup partition entries for handle %d\n", i);
            continue;
        }

        Status = uefi_call_wrapper(blockIo->FlushBlocks, 1, blockIo);
        if (EFI_ERROR(Status))
        {
            Print(L"Error flushing blocks for handle %d\n", i);
            continue;
        }

        FreePool(primary_gpt_header);
        FreePool(backup_gpt_header);
    }

    Print(L"[Spoofer] Partition spoofing completed.\n");
    return Status;
}