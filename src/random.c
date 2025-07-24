#include <random.h>
#include <blockio.h>


static UINT8 pseudo_random_byte_fallback()
{
    EFI_TIME Time;
    EFI_STATUS Status;

    static int lastRandom = 0;

    Status = uefi_call_wrapper(RT->GetTime, 2, &Time, NULL);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to get time: %r\n", Status);
        return 0;
    }

    if (lastRandom == 0) 
    {
        lastRandom = Time.Day + Time.Hour + Time.Minute + Time.Second + Time.Nanosecond;
    }
    
    lastRandom += Time.Minute;


    return lastRandom % 256; 
}

VOID random_string(CHAR8* str, CONST UINTN length)
{
    if (str == NULL || length == 0) {
        return;
    }

    for (UINTN i = 0; i < length - 1; i++) {
        str[i] = pseudo_random_byte();
    }
    
    str[length - 1] = '\0'; 
}



UINT8 pseudo_random_byte()
{
    EFI_GUID rngProto = EFI_RNG_PROTOCOL_GUID;
    EFI_RNG_PROTOCOL *rng;
    EFI_STATUS Status = uefi_call_wrapper(BS->LocateProtocol, 3, &rngProto, NULL, (void**)&rng);

    if (EFI_ERROR(Status)) {
        return pseudo_random_byte_fallback();
    }

    UINT8 rand;
    Status = uefi_call_wrapper(rng->GetRNG, 3, rng, NULL, sizeof(rand), &rand);
    if (EFI_ERROR(Status)) {
        return pseudo_random_byte_fallback();
    }

    return rand;
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