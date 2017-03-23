#include <string.h>
#include <time.h>
#include "main.h"
#include "stm32l0xx_hal.h"
#include "DiceCore.h"

extern RNG_HandleTypeDef hrng;
const uint32_t diceReleaseDate = DICEDATE;
const struct{
    uint16_t major;
    uint16_t minor;
} diceReleaseVersion = {DICEMAJORVERSION, DICEMINORVERSION};
volatile uint32_t touch;

void DiceTouchData(void)
{
    // Make sure this data does not get optimized out
    touch = ((uint32_t*)DICEDATAPTR)[0];
//    touch = ((uint32_t*)&trailer)[0];
}

bool DiceNullCheck(void* dataPtr, uint32_t dataSize)
{
    for(uint32_t n = 0; n < dataSize; n++)
    {
        if(((uint8_t*)dataPtr)[n] != 0x00) return false;
    }
    return true;
}

void DiceGetRandom(uint8_t* entropyPtr, uint32_t entropySize)
{
    for(uint32_t n = 0; n < entropySize; n += sizeof(uint32_t))
    {
        uint32_t entropy;
        HAL_RNG_GenerateRandomNumber(&hrng, &entropy);
        memcpy(&entropyPtr[n], &entropy, MIN(sizeof(entropy), entropySize - n));
    }
}

void DicePrintInfo(void)
{
    uint8_t num[32];

    EPRINTF("\r\n--DICE-Info------------------------------------------------------------------------------");
    EPRINTF("\r\nReleaseDate:        %s", asctime(localtime((time_t*)&diceReleaseDate)));
    EPRINTF("\rVersion:            %d.%d", diceReleaseVersion.major, diceReleaseVersion.minor);
    EPRINTF("\r\n--DICE-Certificate-START-----------------------------------------------------------------");
    EPRINTF("\r\nMagic:              %c%c%c%c", (DICEDATAPTR->s.cert.signData.deviceInfo.magic & 0x000000FF), ((DICEDATAPTR->s.cert.signData.deviceInfo.magic >> 8) & 0x000000FF), ((DICEDATAPTR->s.cert.signData.deviceInfo.magic >> 16) & 0x000000FF), ((DICEDATAPTR->s.cert.signData.deviceInfo.magic >> 24) & 0x000000FF));
    EPRINTF("\r\nProperties:         %s%s%s%s", (DICEDATAPTR->s.cert.signData.deviceInfo.properties.importedSeed) ? "importedSeed " : "", (DICEDATAPTR->s.cert.signData.deviceInfo.properties.noClear) ? "noClear " : "", (DICEDATAPTR->s.cert.signData.deviceInfo.properties.noBootNonce) ? "noBootNonce " : "", (DICEDATAPTR->s.cert.signData.deviceInfo.properties.inheritedAuthority) ? "inheritedAuthority " : "");
    EPRINTF("\r\nRollBackProtection: %d", DICEDATAPTR->s.cert.signData.deviceInfo.rollBackProtection);
    EPRINTF("\rDevicePub.x:        0x");
    BigValToBigInt(num, &DICEDATAPTR->s.cert.signData.deviceInfo.devicePub.x);
    EPRINTFHEXSTRING(num, sizeof(num));
    EPRINTF("\r\nDevicePub.y:        0x");
    BigValToBigInt(num, &DICEDATAPTR->s.cert.signData.deviceInfo.devicePub.y);
    EPRINTFHEXSTRING(num, sizeof(num));
    EPRINTF("\r\nAuthorityPub.x:     0x");
    BigValToBigInt(num, &DICEDATAPTR->s.cert.signData.deviceInfo.authorityPub.x);
    EPRINTFHEXSTRING(num, sizeof(num));
    EPRINTF("\r\nAuthorityPub.y:     0x");
    BigValToBigInt(num, &DICEDATAPTR->s.cert.signData.deviceInfo.authorityPub.y);
    EPRINTFHEXSTRING(num, sizeof(num));
    EPRINTF("\r\nCodeSize:           %d bytes (0x%08x)", DICEDATAPTR->s.cert.signData.codeSize, DICEDATAPTR->s.cert.signData.codeSize);
    EPRINTF("\r\nCodeIssuanceDate:   %d - %s", DICEDATAPTR->s.cert.signData.issueDate, asctime(localtime((time_t*)&DICEDATAPTR->s.cert.signData.issueDate)));
    EPRINTF("\rCodeName:           0x");
    EPRINTFHEXSTRING(DICEDATAPTR->s.cert.signData.codeName, sizeof(DICEDATAPTR->s.cert.signData.codeName));
    if((DICEDATAPTR->s.codeSignaturePtr != NULL) && (!DiceNullCheck(DICEDATAPTR->s.codeSignaturePtr->signedData.alternateDigest, sizeof(DICEDATAPTR->s.codeSignaturePtr->signedData.alternateDigest))))
    {
        EPRINTF("\r\nCodeAlternateName:  0x");
        EPRINTFHEXSTRING(DICEDATAPTR->s.codeSignaturePtr->signedData.alternateDigest, sizeof(DICEDATAPTR->s.codeSignaturePtr->signedData.alternateDigest));
    }
    EPRINTF("\r\nCompoundPub.x:      0x");
    BigValToBigInt(num, &DICEDATAPTR->s.cert.signData.compoundPub.x);
    EPRINTFHEXSTRING(num, sizeof(num));
    EPRINTF("\r\nCompoundPub.y:      0x");
    BigValToBigInt(num, &DICEDATAPTR->s.cert.signData.compoundPub.y);
    EPRINTFHEXSTRING(num, sizeof(num));
    if(!DiceNullCheck(&DICEDATAPTR->s.cert.signData.alternatePub, sizeof(DICEDATAPTR->s.cert.signData.alternatePub)))
    {
        EPRINTF("\r\nAlternatePub.x:     0x");
        BigValToBigInt(num, &DICEDATAPTR->s.cert.signData.alternatePub.x);
        EPRINTFHEXSTRING(num, sizeof(num));
        EPRINTF("\r\nAlternatePub.y:     0x");
        BigValToBigInt(num, &DICEDATAPTR->s.cert.signData.alternatePub.y);
        EPRINTFHEXSTRING(num, sizeof(num));
    }
    EPRINTF("\r\nBootNonce:          0x");
    EPRINTFHEXSTRING(DICEDATAPTR->s.cert.signData.bootNonce, sizeof(DICEDATAPTR->s.cert.signData.bootNonce));
    EPRINTF("\r\n--DICE-Certificate-Signature-------------------------------------------------------------");
    EPRINTF("\r\nSignature.r:        0x");
    BigValToBigInt(num, &DICEDATAPTR->s.cert.signature.r);
    EPRINTFHEXSTRING(num, sizeof(num));
    EPRINTF("\r\nSignature.s:        0x");
    BigValToBigInt(num, &DICEDATAPTR->s.cert.signature.s);
    EPRINTFHEXSTRING(num, sizeof(num));
    EPRINTF("\r\n--DICE-Certificate-END-------------------------------------------------------------------\r\n");
    if(DICEDATAPTR->s.codeSignaturePtr != NULL)
    {
        EPRINTF("\r\n--DICE-Application-----------------------------------------------------------------------");
        EPRINTF("\r\nCodeSignature.r:    0x");
        BigValToBigInt(num, &DICEDATAPTR->s.codeSignaturePtr->signature.r);
        EPRINTFHEXSTRING(num, sizeof(num));
        EPRINTF("\r\nCodeSignature.s:    0x");
        BigValToBigInt(num, &DICEDATAPTR->s.codeSignaturePtr->signature.s);
        EPRINTFHEXSTRING(num, sizeof(num));
        EPRINTF("\r\n--DICE-Application-----------------------------------------------------------------------\r\n");
    }
    EPRINTF("\r\n");
}

void DicePrintInfoHex(char* varName, void* dataPtr, uint32_t dataSize)
{
    EPRINTF("uint8_t %s[%d] = {", varName, dataSize);
    for(uint32_t n = 0; n < sizeof(DiceData_t); n++)
    {
        if((n < dataSize) && (n != 0)) EPRINTF(",");
        if((n % 0x10) != 0) EPRINTF(" ");
        else EPRINTF("\r\n");
        EPRINTF("0x%02x", ((uint8_t*)dataPtr)[n]);
    }
    EPRINTF("\r\n};\r\n\r\n");
}

bool DiceVerifyDeviceCertificate(void)
{
    return (Dice_DSAVerify((uint8_t*)&DICEDATAPTR->s.cert.signData,
                           sizeof(DICEDATAPTR->s.cert.signData),
                           &DICEDATAPTR->s.cert.signature,
                           &DICEDATAPTR->s.cert.signData.deviceInfo.devicePub) == DICE_SUCCESS);
}
