#include <random.h>
#include <blockio.h>
static CHAR8 charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

static UINT8 pseudo_random_byte_fallback(UINT8 l, UINT8 h)
{
    EFI_TIME Time;
    EFI_STATUS Status;

    static UINT32 lastRandom = 0;

    if (l > h) {
        UINT8 temp = l;
        l = h;
        h = temp;
    }

    Status = uefi_call_wrapper(RT->GetTime, 2, &Time, NULL);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to get time: %r\n", Status);
        return l; 
    }

    lastRandom ^= (Time.Second * 1000000 + Time.Minute * 10000 +
                   Time.Hour * 100 + Time.Day + Time.Nanosecond);
    lastRandom = (lastRandom * 1103515245 + 12345) & 0x7fffffff; 

    UINT16 range = (UINT16)h - (UINT16)l + 1;
    UINT8 num = (UINT8)(lastRandom % range);

    return (UINT8)(num + l);
}


VOID random_string(CHAR8* str, CONST UINTN length)
{
    if (str == NULL || length == 0) {
        return;
    }

    for (UINTN i = 0; i < length - 1; i++) {
        str[i] = charset[pseudo_random_byte(0, sizeof(charset) - 1)]; 
    }
    
    str[length - 1] = '\0'; 
}



UINT8 pseudo_random_byte(UINT8 l, UINT8 h)
{
    EFI_GUID rngProto = EFI_RNG_PROTOCOL_GUID;
    EFI_RNG_PROTOCOL *rng;
    EFI_STATUS Status = uefi_call_wrapper(BS->LocateProtocol, 3, &rngProto, NULL, (void**)&rng);

    if (EFI_ERROR(Status)) {
        return pseudo_random_byte_fallback(l, h);
    }

    UINT8 rand;
    Status = uefi_call_wrapper(rng->GetRNG, 3, rng, NULL, sizeof(rand), &rand);
    if (EFI_ERROR(Status)) {
        return pseudo_random_byte_fallback(l, h);
    }

    UINT16 range = (UINT16)h - (UINT16)l + 1;
    UINT8 num = (UINT8)(rand % range);

    return (UINT8)(num + l);
}


EFI_STATUS spoof_GUID(GUID *guid) {
    EFI_STATUS status = EFI_SUCCESS;
    
    if (guid == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    UINT8 randomBytes[16];
    for (int i = 0; i < 16; i++) {
        randomBytes[i] = pseudo_random_byte(0, 255);
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