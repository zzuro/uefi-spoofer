#include <efi.h>
#include <efilib.h>
#include <smbios.h>
#include <prepatch.h>
#include <blockio.h>

EFI_STATUS EFIAPI efi_main(EFI_HANDLE In_ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {

    EFI_STATUS Status = EFI_SUCCESS;

    InitializeLib(In_ImageHandle, SystemTable);
    ImageHandle = In_ImageHandle;

    uefi_call_wrapper(SystemTable->ConOut->ClearScreen, 1, SystemTable->ConOut);

    Print(L"[Spoofer] Initialization complete.\n");
    Print(L"[Spoofer] Searching for SMBIO Entry Point\n");

    VOID* SmbiosEntryPoint = FindSmbios();
    if (SmbiosEntryPoint == NULL) {
        Print(L"[Spoofer] Failed to prepatch SMBIOS table.\n");
        return EFI_NOT_FOUND;
    }

    Print(L"[Spoofer] SMBIOS Entry Point found at: 0x%08x\n", SmbiosEntryPoint);

    Status = PrepatchSmbios(SmbiosEntryPoint);
    if (EFI_ERROR(Status)) {
        Print(L"[Spoofer] Failed to prepatch SMBIOS");
        return Status;
    }

    Status = spoof_partition();
    if (EFI_ERROR(Status)) {
        Print(L"[Spoofer] Failed to print partition information\n");
        return Status;
    }


    return Status;
}