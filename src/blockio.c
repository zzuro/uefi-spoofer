#include <blockio.h>
#include <gpt.h>

EFI_HANDLE ImageHandle = NULL;

EFI_STATUS get_mediaID(UINT32 *mediaID) {
    EFI_STATUS status = EFI_SUCCESS;

    //OpenProtocol chapter 7.3.9
    EFI_LOADED_IMAGE_PROTOCOL *loadedImage = NULL;
    status = uefi_call_wrapper(BS->OpenProtocol, 6, ImageHandle, &gEfiLoadedImageProtocolGuid, (VOID **)&loadedImage, ImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if(EFI_ERROR(status)) {
        Print(L"Error opening Loaded Image Protocol\n");
        return status;
    }

    EFI_BLOCK_IO_PROTOCOL *blockIo = NULL;
    status = uefi_call_wrapper(BS->OpenProtocol, 6, loadedImage->DeviceHandle, &gEfiBlockIoProtocolGuid, (VOID **)&blockIo, ImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if (EFI_ERROR(status)) {
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
    if (EFI_ERROR(Status)) {
        Print(L"Error locating Block IO handles \n");
        return Status;
    }

    for(UINTN i = 0; i < num_handles; i++) 
    {
        Status = uefi_call_wrapper(BS->OpenProtocol, 6, handle_buffer[i], &gEfiBlockIoProtocolGuid, (VOID **)&blockIo, ImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
        if (EFI_ERROR(Status)) {
            Print(L"Error opening Block IO Protocol for handle %d\n", i);
            continue;
        }

        if(blockIo->Media->LastBlock == 0) {
            continue;
        }

        EFI_GUID gEfiPartitionInfoProtocolGuid = EFI_PARTITION_INFO_PROTOCOL_GUID;

        if(blockIo->Media->LogicalPartition) {
            EFI_PARTITION_INFO_PROTOCOL *partitionInfo = NULL;
            Status = uefi_call_wrapper(BS->OpenProtocol, 6, handle_buffer[i], &gEfiPartitionInfoProtocolGuid, (VOID **)&partitionInfo, ImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
            if (EFI_ERROR(Status)) {
                Print(L"Error opening Partition Info Protocol for handle %d\n", i);
                continue;
            }

            if(partitionInfo->Type == PARTITION_TYPE_GPT) {
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

EFI_STATUS spoof_partition()
{
    EFI_STATUS Status = EFI_SUCCESS;
    Status = print_partition();
    if (EFI_ERROR(Status)) {
        return Status;
    }

    UINTN num_handles = 0;
    EFI_HANDLE *handle_buffer = NULL;
    EFI_BLOCK_IO_PROTOCOL *blockIo = NULL;

    Status = uefi_call_wrapper(BS->LocateHandleBuffer, 5, ByProtocol, &gEfiBlockIoProtocolGuid, NULL, &num_handles, &handle_buffer);
    if (EFI_ERROR(Status)) {
        Print(L"Error locating Block IO handles \n");
        return Status;
    }

    UINT32 last_mediaID = -1;
    for(UINTN i = 0; i < num_handles; i++) 
    {
        Status = uefi_call_wrapper(BS->OpenProtocol, 6, handle_buffer[i], &gEfiBlockIoProtocolGuid, (VOID **)&blockIo, ImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
        if (EFI_ERROR(Status)) {
            Print(L"Error opening Block IO Protocol for handle %d\n", i);
            continue;
        }

        if(blockIo->Media->LastBlock == 0 || blockIo->Media->MediaId == last_mediaID) {
            continue;
        }

        last_mediaID = blockIo->Media->MediaId;

        GPT_HEADER *gptHeader = (GPT_HEADER *)AllocateZeroPool(sizeof(GPT_HEADER));
        Status = uefi_call_wrapper(blockIo->ReadBlocks, 5, blockIo, blockIo->Media->MediaId, 1, sizeof(GPT_HEADER), gptHeader);
        if (EFI_ERROR(Status)) {
            Print(L"Error reading GPT header for handle %d\n", i); 
        }

        EFI_GUID guid = gptHeader->DiskGUID;

        Print(L"Disk GUID: %08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
                    guid.Data1, guid.Data2, guid.Data3,
                    guid.Data4[0], guid.Data4[1],
                    guid.Data4[2], guid.Data4[3],
                    guid.Data4[4], guid.Data4[5],
                    guid.Data4[6], guid.Data4[7]);

        void *buf = (GPT_PARTITION_ENTRY *)AllocateZeroPool(LBA_SIZE);
        BOOLEAN done = FALSE;
        for(int j = 0; j < (gptHeader->NumberOfPartitionEntries * gptHeader->SizeOfPartitionEntry)/LBA_SIZE; j++) {
            Status = uefi_call_wrapper(blockIo->ReadBlocks, 5, blockIo, blockIo->Media->MediaId, gptHeader->PartitionEntryLBA + j * gptHeader->SizeOfPartitionEntry, LBA_SIZE, buf);
            if (EFI_ERROR(Status)) {
                Print(L"Error reading GPT entry %d for handle %d\n", j, i);
                break;
            }

            for(int k = 0; k < LBA_SIZE / sizeof(GPT_PARTITION_ENTRY); k++) {
                GPT_PARTITION_ENTRY *gptEntry = (GPT_PARTITION_ENTRY *)buf + k;
                if(gptEntry->UniquePartitionGUID.Data1 == 0) {
                    done = TRUE;
                    break;
                }

                //Overwrite the partition GUID
            }
            if(done) {
                break;
            }
        }
        FreePool(buf);
        FreePool(gptHeader);
    }

    
    return Status;
}