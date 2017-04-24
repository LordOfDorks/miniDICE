#include <string.h>
#include <time.h>
#ifndef WIN32
#include "main.h"
#include "stm32l0xx_hal.h"
#endif
#include "DiceCore.h"
#include "DiceBase64.h"

#ifndef WIN32
extern RNG_HandleTypeDef hrng;
#endif
const uint32_t diceReleaseDate = DICETIMESTAMP;
const union
{
    uint32_t raw;
    struct{
        uint16_t minor;
        uint16_t major;
    } s;
} diceReleaseVersion = {DICEVERSION};
//volatile uint32_t touch;

#ifndef WIN32
void DiceBlink(uint32_t info)
{
    HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_SET);
    HAL_Delay(250);
    HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_RESET);
    HAL_Delay(500);
    for(uint32_t n = 0; n < info; n++)
    {
        HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_SET);
        HAL_Delay(1000);
        HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_RESET);
        HAL_Delay(500);
    }
    HAL_Delay(1500);
}
#endif

bool DiceNullCheck(void* dataPtr, uint32_t dataSize)
{
    for(uint32_t n = 0; n < dataSize; n++)
    {
        if(((uint8_t*)dataPtr)[n] != 0x00) return false;
    }
    return true;
}

#ifndef WIN32
void DiceGetRandom(uint8_t* entropyPtr, uint32_t entropySize)
{
    for(uint32_t n = 0; n < entropySize; n += sizeof(uint32_t))
    {
        uint32_t entropy;
        HAL_RNG_GenerateRandomNumber(&hrng, &entropy);
        memcpy(&entropyPtr[n], &entropy, MIN(sizeof(entropy), entropySize - n));
    }
}
#endif

#ifdef WIN32
#define SILENTDICE
#endif

#ifndef SILENTDICE
void DicePrintInfo(void)
{
    uint8_t num[128];
    char pem[256];
    uint32_t pemSize = sizeof(pem);
    char* dateStr;
    DERBuilderContext cerCtx = {0};

    EPRINTF("\r\n=====BEGIN DICE INFORMATION=====");
    dateStr = asctime(localtime((time_t*)&diceReleaseDate));
    dateStr[24] = '\0';
    EPRINTF("\r\nReleaseDate:        %s", dateStr);
    EPRINTF("\r\nVersion:            %d.%d", diceReleaseVersion.s.major, diceReleaseVersion.s.minor);
    if(DICERAMAREA->info.rollBackProtection > 0)
    {
        dateStr = asctime(localtime((time_t*)&DICERAMAREA->info.rollBackProtection));
        dateStr[24] = '\0';
        EPRINTF("\r\nRollBackProtection: %s", dateStr);
    }
    if(*((uint32_t*)&DICERAMAREA->info.properties) != 0)
    {
        EPRINTF("\r\nProperties:         %s", (DICERAMAREA->info.properties.noClear) ? "noClear " : "");
    }
    EPRINTF("\r\nProtectedFLASH:     0x%04x@0x%08x", DICERAMAREA->info.dontTouchSize, DICEDATAEEPROMSTART);
    EPRINTF("\r\nOccupiedRAM:        0x%04x@0x%08x", sizeof(DiceData_t) - 1 + DICERAMAREA->info.certBagLen, DICERAMSTART);

    DERInitContext(&cerCtx, num, sizeof(num));
    DERGetEccPub(&cerCtx, &DICERAMAREA->info.devicePub);
    pemSize = DERtoPEM(&cerCtx, 1, pem, sizeof(pem));
    EPRINTF("\r\nDevicePub(%d):\r\n%s", pemSize, pem);

    if(!DiceNullCheck(&DICERAMAREA->info.authorityPub, sizeof(DICERAMAREA->info.authorityPub)))
    {
        DERInitContext(&cerCtx, num, sizeof(num));
        DERGetEccPub(&cerCtx, &DICERAMAREA->info.authorityPub);
        pemSize = DERtoPEM(&cerCtx, 1, pem, sizeof(pem));
        EPRINTF("AuthorityPub(%d):\r\n%s", pemSize, pem);
    }

    if(DICERAMAREA->info.certBagLen > 0)
    {
        EPRINTF("CertificateBag(%d):\r\n%s", DICERAMAREA->info.certBagLen, DICERAMAREA->info.certBag);
    }
    EPRINTF("=====END DICE INFORMATION=====\r\n");

    if(DICEAPPHDR->s.sign.magic == DICEMAGIC)
    {
        EPRINTF("\r\n=====BEGIN APPLICATION=====");
        EPRINTF("\r\nAppSize:            %d bytes (0x%08x)", DICEAPPHDR->s.sign.codeSize, DICEAPPHDR->s.sign.codeSize);
        EPRINTF("\r\nAppVersion:         %d.%d", (DICEAPPHDR->s.sign.version >> 16), (DICEAPPHDR->s.sign.version & 0x0000ffff));
        dateStr = asctime(localtime((time_t*)&DICERAMAREA->info.rollBackProtection));
        dateStr[24] = '\0';
        EPRINTF("\r\nAppIssuanceDate:    %s (%d)", dateStr, DICEAPPHDR->s.sign.issueDate);
        EPRINTF("\r\nAppName:            %s", DICEAPPHDR->s.sign.name);
        EPRINTF("\r\nAppDigest:          0x");
        EPRINTFHEXSTRING(DICEAPPHDR->s.sign.appDigest, sizeof(DICEAPPHDR->s.sign.appDigest));
        EPRINTF("\r\nAlternateDigest:    0x");
        EPRINTFHEXSTRING(DICEAPPHDR->s.sign.alternateDigest, sizeof(DICEAPPHDR->s.sign.alternateDigest));
        DERInitContext(&cerCtx, num, sizeof(num));
        DERGetEccPub(&cerCtx, &DICERAMAREA->compoundPub);
        pemSize = DERtoPEM(&cerCtx, 1, pem, sizeof(pem));
        EPRINTF("\r\nCompoundPub(%d):\r\n%s", pemSize, pem);
        if(!DiceNullCheck(&DICERAMAREA->alternatePub, sizeof(DICERAMAREA->alternatePub)))
        {
            DERInitContext(&cerCtx, num, sizeof(num));
            DERGetEccPub(&cerCtx, &DICERAMAREA->alternatePub);
            pemSize = DERtoPEM(&cerCtx, 1, pem, sizeof(pem));
            EPRINTF("AlternatePub(%d):\r\n%s", pemSize, pem);
        }
        EPRINTF("=====END APPLICATION=====\r\n");
    }
}

void DicePrintInfoHex(char* varName, void* dataPtr, uint32_t dataSize)
{
    EPRINTF("uint8_t %s[%d] = {", varName, dataSize);
    for(uint32_t n = 0; n < dataSize; n++)
    {
        if((n < dataSize) && (n != 0)) EPRINTF(",");
        if((n % 0x10) != 0) EPRINTF(" ");
        else EPRINTF("\r\n");
        EPRINTF("0x%02x", ((uint8_t*)dataPtr)[n]);
    }
    EPRINTF("\r\n};\r\n\r\n");
}

#else
void DicePrintInfo(void)
{
}

void DicePrintInfoHex(char* varName, void* dataPtr, uint32_t dataSize)
{
}

#endif
