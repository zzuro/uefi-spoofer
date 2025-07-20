#include <efi.h>
#include <efilib.h>
#include <smbios.h>
#include <prepatch.h>

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    InitializeLib(ImageHandle, SystemTable);

    
    Print(L"[Spoofer] Initialization complete.\n");
    Print(L"[Spoofer] Searching for SMBIO Entry Point\n");

    VOID* SmbiosEntryPoint = FindSmbios();
    if (SmbiosEntryPoint == NULL) {
        Print(L"[Spoofer] Failed to prepatch SMBIOS table.\n");
        return EFI_NOT_FOUND;
    }

    Print(L"[Spoofer] SMBIOS Entry Point found at: 0x%08x\n", SmbiosEntryPoint);

    EFI_STATUS Status = PrepatchSmbios(SmbiosEntryPoint);
    if (EFI_ERROR(Status)) {
        Print(L"[Spoofer] Failed to prepatch SMBIOS");
        return Status;
    }

    return EFI_SUCCESS;
}