#include <random.h>
#include <blockio.h>

UINT8 pseudo_random_byte()
{
    EFI_TIME Time;
    EFI_STATUS Status;
    UINT64 seed = 0;

    Status = uefi_call_wrapper(RT->GetTime, 2, &Time, NULL);
    if (!EFI_ERROR(Status)) {
        seed ^= (UINT64)Time.Second |
                ((UINT64)Time.Minute << 8) |
                ((UINT64)Time.Hour << 16) |
                ((UINT64)Time.Day << 24) |
                ((UINT64)Time.Month << 32) |
                ((UINT64)Time.Year << 40);
    }

    for (int i = 0; i < 10000; i++) {
        UINT64 t;
        uefi_call_wrapper(BS->Stall, 1, 1); 
        uefi_call_wrapper(BS->GetNextMonotonicCount, 1, &t);
        seed ^= t ^ (seed << 5) ^ (seed >> 3);
    }

    return (UINT8)(seed ^ (seed >> 11) ^ (seed >> 19));
}


EFI_STATUS spoof_GUID(GUID *guid) {
    EFI_STATUS status = EFI_SUCCESS;
    
    if (guid == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    UINT8 randomBytes[16];
    for (int i = 0; i < 16; i++) {
        randomBytes[i] = pseudo_random_byte();
    }

    guid->TimeLow = *(UINT32 *)&randomBytes[0];
    guid->TimeMid = *(UINT16 *)&randomBytes[4];
    guid->TimeHiAndVersion = ((*(UINT16 *)&randomBytes[6]) & 0x0FFF) | 0x4000;
    guid->ClockSeqHiAndReserved = (randomBytes[8] & 0x80) | 0xBF; 
    guid->ClockSeqLow = randomBytes[9];

    for (int i = 0; i < 6; i++) {
        guid->Node[i] = randomBytes[10 + i];
    }

    return status;
}