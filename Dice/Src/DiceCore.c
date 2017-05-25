#include <string.h>
#include <time.h>
#include "main.h"
#include "stm32l0xx_hal.h"
#include "usbd_dfu_if.h"
#include "DiceCore.h"

FIREWALL_InitTypeDef fw_init = {0};
volatile uint8_t manualReprovision = 0;

DICE_STATUS DiceLockDown(void)
{
    DICE_STATUS retVal = DICE_SUCCESS;
    FLASH_OBProgramInitTypeDef ob = {0};
    HAL_FLASHEx_OBGetConfig(&ob);

//    EPRINTF("OptionType:     0x%08x\r\n", ob.OptionType);
//    EPRINTF("WRPState:       0x%08x\r\n", ob.WRPState);
//    EPRINTF("WRPSector:      0x%08x%08x\r\n", ob.WRPSector2, ob.WRPSector);
//    EPRINTF("RDPLevel:       0x%02x\r\n", ob.RDPLevel);
//    EPRINTF("BORLevel:       0x%02x\r\n", ob.BORLevel);
//    EPRINTF("USERConfig:     0x%02x\r\n", ob.USERConfig);
//    EPRINTF("BOOTBit1Config: 0x%02x\r\n", ob.BOOTBit1Config);

    if(ob.RDPLevel == OB_RDP_LEVEL_0)
    {
        memset(&ob, 0x00, sizeof(ob));
        ob.OptionType = OPTIONBYTE_WRP | OPTIONBYTE_RDP;
        ob.WRPState = OB_WRPSTATE_ENABLE;
        uint64_t pageMask = 0x0000000000000000;
        for(uint32_t n = 0; n < (DICEBOOTLOADERSIZE / 4096); n++)
        {
            pageMask |= (0x0000000000000001 << n);
        }
        ob.WRPSector = (uint32_t)(pageMask & 0x00000000ffffffff);
        ob.WRPSector2 = (uint32_t)((pageMask >> 32) & 0x00000000ffffffff);
//        ob.WRPSector = OB_WRP_Pages0to31 | OB_WRP_Pages32to63 | OB_WRP_Pages64to95 | OB_WRP_Pages96to127 | OB_WRP_Pages128to159 | OB_WRP_Pages160to191 | OB_WRP_Pages192to223 | OB_WRP_Pages224to255;
#ifndef IRREVERSIBLELOCKDOWN
        ob.RDPLevel = OB_RDP_LEVEL_1;
#else
        ob.RDPLevel = OB_RDP_LEVEL_2;
#endif

#ifdef TOUCHOPTIONBYTES
        if((HAL_FLASH_OB_Unlock() != HAL_OK) ||
           (HAL_FLASHEx_OBProgram(&ob) != HAL_OK) ||
           (HAL_FLASH_OB_Lock() != HAL_OK))
        {
            EPRINTF("ERROR: Programming option bytes failed.\r\n");
            retVal = DICE_FAILURE;
            goto Cleanup;
        }
        memset(&ob, 0x00, sizeof(ob));
        HAL_FLASHEx_OBGetConfig(&ob);
        EPRINTF("INFO: Option bytes written. Power-cycle required to apply them!\r\n");
        for(;;)
        {
            DiceBlink(DICEBLINKRESETME);
        }
#else
        EPRINTF("WARNING: Write access to option bytes disabled!\r\n");
#endif
    }
    else if(ob.RDPLevel == OB_RDP_LEVEL_1)
    {
        EPRINTF("WARNING: Non-Permanent lockdown deteted (RDPLevel = OB_RDP_LEVEL_1).\r\n");
    }
    else if(ob.RDPLevel == OB_RDP_LEVEL_2)
    {
        EPRINTF("INFO: miniDICE is fully secured.\r\n");
    }
    else
    {
        EPRINTF("ERROR: RDPLevel = 0x%08x.\r\n", ob.RDPLevel);
        retVal = DICE_FAILURE;
        goto Cleanup;
    }
    
Cleanup:
    
    return retVal;
}

char dfuStr[0x100] = {0};
void DiceGenerateDFUString(void)
{
    uint32_t cursor = sprintf(dfuStr, "@miniDICE ");
    
    for(uint32_t n = DICEBOOTLOADERSIZE; n < DICEFLASHSIZE; n += MIN(99 * 512, DICEFLASHSIZE - n))
    {
        cursor += sprintf(&dfuStr[cursor],
                "/0x%08lx/%02lu*512Bf",
                (DICEFLASHSTART + n),
                MIN(99, (DICEFLASHSIZE - n) / 512));
    }

    if((!DICERAMAREA->info.properties.noClear) && (!__HAL_FIREWALL_IS_ENABLED()))
    {
        uint32_t readWriteStart = (uint32_t)&DICEFUSEAREA->info;
        uint32_t readWritePages = (DICEFUSEAREA->info.size + 63) / 64;
        uint32_t writeStart = readWriteStart + readWritePages * 64;
        uint32_t writePages = (DICEDATAEEPROMSTART + DICEDATAEEPROMSIZE - writeStart) /64;
        cursor += sprintf(&dfuStr[cursor],
                "/0x%08lx/%02lu*064Bg/0x%08lx/%02lu*064Bf",
                readWriteStart,
                readWritePages,
                writeStart,
                writePages);
    }
    else
    {
        cursor += sprintf(&dfuStr[cursor],
                "/0x%08lx/%02lu*064Ba",
                (uint32_t)&DICERAMAREA->info,
                ((sizeof(DicePublicInfo_t) - 1 + DICERAMAREA->info.certBagLen + 63) / 64));
    }
    USBD_DFU_fops_FS.pStrDesc = (uint8_t*)dfuStr;
}

static DICE_STATUS DiceWriteDataEEProm(uint32_t* destination, uint32_t* dataWords, uint32_t numDataWords)
{
    if(HAL_FLASH_Unlock() != HAL_OK) return DICE_FAILURE;
    for(uint32_t n = 0; n < numDataWords; n++)
    {
        if(n > 0) HAL_Delay(1); // Fast bulk erase has shown errors
        if(destination[n] != dataWords[n])
        {
            if(destination[n] != 0)
            {
                if(HAL_FLASHEx_DATAEEPROM_Erase((uint32_t)(&destination[n])) != HAL_OK) return DICE_FAILURE;
            }
            if(dataWords[n] != 0)
            {
                if(HAL_FLASHEx_DATAEEPROM_Program(FLASH_TYPEPROGRAM_WORD, (uint32_t)(&destination[n]), dataWords[n]) != HAL_OK) return DICE_FAILURE;
            }
        }
    }
    if(HAL_FLASH_Lock() != HAL_OK) return DICE_FAILURE;

    return DICE_SUCCESS;
}

DICE_STATUS DiceInitialize(void)
{
    DICE_STATUS retVal = DICE_SUCCESS;
    bigval_t seed = {0};

    EPRINTF("INFO: Initializing.\r\n");
    if(__HAL_FIREWALL_IS_ENABLED())
    {
        EPRINTF("ERROR: Firewall already engaged!\r\n");
        retVal = DICE_FAILURE;
        goto Cleanup;
    }

    // Initial Device Identity Provisioning
    if((manualReprovision) || (DICEFUSEAREA->magic != DICEMAGIC))
    {
        EPRINTF("INFO: Generating new identity\r\n");
        // Initilaize the staging area
        uint32_t* stagingBuffer = (uint32_t*)malloc(DICEFUSEDATASIZE);
        if(stagingBuffer == NULL)
        {
            EPRINTF("ERROR: No Memory!\r\n");
            retVal = DICE_FAILURE;
            goto Cleanup;
        }
        memset(stagingBuffer, 0x00, DICEFUSEDATASIZE);
        PDicePersistedData_t staging = (PDicePersistedData_t)stagingBuffer;
        staging->magic = DICEMAGIC;
        staging->info.magic = DICEMAGIC;
        staging->info.rollBackProtection = 0;
        staging->info.properties.noClear = 0;
        staging->info.bootCounter = 0;
        staging->info.certBagLen = DICEFUSEDATASIZE - sizeof(DicePersistedData_t) + 1;
        Dice_GenerateDSAKeyPair(&staging->info.devicePub, &staging->devicePrv);

        // Generate a selfsigned device cert
        DICE_X509_TBS_DATA x509DeviceCertData = {{{0},
                                                  "DICECore", "MSFT", "US",
                                                  &staging->info.devicePub, &staging->devicePrv
                                                 },
                                                 1483228800, 2145830400,
                                                 {{0},
                                                  "DICECore", "MSFT", "US",
                                                  &staging->info.devicePub, &staging->devicePrv
                                                 }};
        Dice_KDF_SHA256_Seed(x509DeviceCertData.Issuer.CertSerialNum,
                             sizeof(x509DeviceCertData.Issuer.CertSerialNum),
                             (const uint8_t*)&staging->info.devicePub,
                             sizeof(staging->info.devicePub),
                             NULL,
                             (const uint8_t*)x509DeviceCertData.Issuer.CommonName,
                             strlen(x509DeviceCertData.Issuer.CommonName));
        x509DeviceCertData.Issuer.CertSerialNum[0] &= 0x7F; // Make sure the serial number is always positive
        memcpy(x509DeviceCertData.Subject.CertSerialNum, x509DeviceCertData.Issuer.CertSerialNum, sizeof(x509DeviceCertData.Subject.CertSerialNum));

        uint8_t* cerBuffer = (uint8_t*)malloc(DER_MAX_TBS);
        if(cerBuffer == NULL)
        {
            EPRINTF("ERROR: No Memory!\r\n");
            retVal = DICE_FAILURE;
            goto Cleanup;
        }
        DERBuilderContext cerCtx = {0};
        DERInitContext(&cerCtx, cerBuffer, DER_MAX_TBS);
        X509MakeCertBody(&cerCtx, &x509DeviceCertData);
        uint32_t* pProperties = (uint32_t*)&staging->info.properties;
        X509MakeDeviceCert(&cerCtx, DICEVERSION, DICETIMESTAMP, *pProperties, NULL);
        X509CompleteCert(&cerCtx, &x509DeviceCertData);
        staging->info.certBagLen = DERtoPEM(&cerCtx, 0, staging->info.certBag, staging->info.certBagLen);
        
        staging->info.size = sizeof(DicePublicInfo_t) - 1 + staging->info.certBagLen;
        staging->info.dontTouchSize = (((sizeof(DicePersistedData_t) - 1 + staging->info.certBagLen + 0xFF) / 0x100) * 0x100);

        // Write the staging area to the data EEPROM
        if((retVal = DiceWriteDataEEProm((uint32_t*)DICEDATAEEPROMSTART, stagingBuffer, (staging->info.dontTouchSize / sizeof(uint32_t)))) != DICE_SUCCESS)
        {
            goto Cleanup;
        }
        free(cerBuffer);
        free(stagingBuffer);
    }

    // Increment the BootCounter
    {
        uint32_t BootCtr = DICEFUSEAREA->info.bootCounter + 1;
        if((retVal = DiceWriteDataEEProm(&DICEFUSEAREA->info.bootCounter, &BootCtr, 1)) != DICE_SUCCESS)
        {
            EPRINTF("ERROR: BootCounter update failed!\r\n");
            retVal = DICE_FAILURE;
            goto Cleanup;
        }
    }

    // Copy the device information over
    DICERAMAREA->magic = DICEMAGIC;
    memset(&DICERAMAREA->compoundPrv, 0x00, sizeof(DICERAMAREA->compoundPrv));
    memset(&DICERAMAREA->alternatePrv, 0x00, sizeof(DICERAMAREA->alternatePrv));
    memset(&DICERAMAREA->compoundPub, 0x00, sizeof(DICERAMAREA->compoundPub));
    memset(&DICERAMAREA->alternatePub, 0x00, sizeof(DICERAMAREA->alternatePub));
    memcpy(&DICERAMAREA->info, &DICEFUSEAREA->info, sizeof(DICEFUSEAREA->info) + DICEFUSEAREA->info.certBagLen - 1);

    // No code - no boot
    if(DICEAPPHDR->s.sign.magic != DICEMAGIC)
    {
        EPRINTF("WARNING: The application area is NULL, we go directly to DFU mode!\r\n");
        retVal = DICE_LOAD_MODULE_FAILED;
        goto Cleanup;
    }

    // Verify the App digest To this point it is still an App integrity check
    uint8_t referenceDigest[SHA256_DIGEST_LENGTH];
    Dice_SHA256_Block((uint8_t*)DICEAPPENTRY, DICEAPPHDR->s.sign.codeSize, referenceDigest);
    if(memcmp(DICEAPPHDR->s.sign.appDigest, referenceDigest, SHA256_DIGEST_LENGTH))
    {
        EPRINTF("WARNING: Application digest does not match with the embedded signature header, we go directly to DFU mode!\r\n");
        retVal = DICE_LOAD_MODULE_FAILED;
        goto Cleanup;
    }

    // If we have an authority do full authorization check
    if(!DiceNullCheck(&DICERAMAREA->info.authorityPub, sizeof(ecc_publickey)))
    {
        if(DICEAPPHDR->s.sign.issueDate < DICERAMAREA->info.rollBackProtection)
        {
            EPRINTF("TimeStamp latest issued App: %s\r", asctime(localtime((time_t*)&DICERAMAREA->info.rollBackProtection)));
            EPRINTF("TimeStamp of the installed App:   %s\r", asctime(localtime((time_t*)&DICEAPPHDR->s.sign.issueDate)));
            EPRINTF("WARNING: Application rollback was detected, we go directly to DFU mode!\r\n");
            retVal = DICE_LOAD_MODULE_FAILED;
            goto Cleanup;
        }

        // If the current application is newer than the previous one, crank the monotonic counter forward
        if(DICEAPPHDR->s.sign.issueDate > DICERAMAREA->info.rollBackProtection)
        {
            if((retVal = DiceWriteDataEEProm(&DICEFUSEAREA->info.rollBackProtection, &DICEAPPHDR->s.sign.issueDate, 1)) != DICE_SUCCESS)
            {
                EPRINTF("ERROR: Rollback protection could not be updated!\r\n");
                retVal = DICE_FAILURE;
                goto Cleanup;
            }
            DICERAMAREA->info.rollBackProtection = DICEAPPHDR->s.sign.issueDate;
            EPRINTF("INFO: Rollback protection updated to %s\r" , asctime(localtime((time_t*)&DICEAPPHDR->s.sign.issueDate)));
        }

        // Verify the App header signature
        if((retVal = Dice_DSAVerify((uint8_t*)&DICEAPPHDR->s.sign, sizeof(DICEAPPHDR->s.sign), &DICEAPPHDR->s.signature, &DICERAMAREA->info.authorityPub)) != DICE_SUCCESS)
        {
            EPRINTF("WARNING: Authority signature verification of App signature failed, we go directly to DFU mode!\r\n");
            retVal = DICE_LOAD_MODULE_FAILED;
            goto Cleanup;
        }
    }
    else
    {
        EPRINTF("INFO: No authority set, App verification is skipped.\r\n");
    }

    // Derive the compundDevice key
    Dice_HMAC_SHA256_Block((uint8_t*)&DICEFUSEAREA->devicePrv, sizeof(DICEFUSEAREA->devicePrv), DICEAPPHDR->s.sign.appDigest, sizeof(DICEAPPHDR->s.sign.appDigest), (uint8_t*)&seed);
    if((retVal = Dice_DeriveDsaKeyPair(&DICERAMAREA->compoundPub, &DICERAMAREA->compoundPrv, &seed, (uint8_t*)DICECOMPOUNDDERIVATIONLABEL, sizeof(DICECOMPOUNDDERIVATIONLABEL) - 1)) != DICE_SUCCESS)
    {
        EPRINTF("ERROR: Compound key derivation failed!\r\n");
        goto Cleanup;
    }
    
    // Derive the alternate key if we are supposed to
    if(!DiceNullCheck(DICEAPPHDR->s.sign.alternateDigest, sizeof(DICEAPPHDR->s.sign.alternateDigest)))
    {
        Dice_HMAC_SHA256_Block((uint8_t*)&DICEFUSEAREA->devicePrv, sizeof(DICEFUSEAREA->devicePrv), DICEAPPHDR->s.sign.alternateDigest, sizeof(DICEAPPHDR->s.sign.alternateDigest), (uint8_t*)&seed);
        if((retVal = Dice_DeriveDsaKeyPair(&DICERAMAREA->alternatePub, &DICERAMAREA->alternatePrv, &seed, (uint8_t*)DICECOMPOUNDDERIVATIONLABEL, sizeof(DICECOMPOUNDDERIVATIONLABEL) - 1)) != DICE_SUCCESS)
        {
            EPRINTF("ERROR: Alternate key derivation failed!\r\n");
            goto Cleanup;
        }
    }
    
    // Issue the compound cert
    {
        DICE_X509_TBS_DATA x509DeviceCertData = {{{0},
                                                  "DICECore", "MSFT", "US",
                                                  &DICEFUSEAREA->info.devicePub, &DICEFUSEAREA->devicePrv
                                                 },
                                                 1483228800, 2145830400,
                                                 {{0},
                                                  "DICEApp", "OEM", "US",
                                                  &DICERAMAREA->compoundPub, &DICERAMAREA->compoundPrv
                                                 }};
        Dice_KDF_SHA256_Seed(x509DeviceCertData.Issuer.CertSerialNum,
                             sizeof(x509DeviceCertData.Issuer.CertSerialNum),
                             (const uint8_t*)&DICEFUSEAREA->info.devicePub,
                             sizeof(DICEFUSEAREA->info.devicePub),
                             NULL,
                             (const uint8_t*)x509DeviceCertData.Issuer.CommonName,
                             strlen(x509DeviceCertData.Issuer.CommonName));
        x509DeviceCertData.Issuer.CertSerialNum[0] &= 0x7F; // Make sure the serial number is always positive
        Dice_KDF_SHA256_Seed(x509DeviceCertData.Subject.CertSerialNum,
                             sizeof(x509DeviceCertData.Subject.CertSerialNum),
                             (const uint8_t*)&DICERAMAREA->compoundPub,
                             sizeof(DICERAMAREA->compoundPrv),
                             NULL,
                             (const uint8_t*)x509DeviceCertData.Subject.CommonName,
                             strlen(x509DeviceCertData.Subject.CommonName));
        x509DeviceCertData.Subject.CertSerialNum[0] &= 0x7F; // Make sure the serial number is always positive

        uint8_t* cerBuffer = (uint8_t*)malloc(DER_MAX_TBS);
        if(cerBuffer == NULL)
        {
            EPRINTF("ERROR: No Memory!\r\n");
            retVal = DICE_FAILURE;
            goto Cleanup;
        }
        DERBuilderContext cerCtx = {0};
        DERInitContext(&cerCtx, cerBuffer, DER_MAX_TBS);
        X509MakeCertBody(&cerCtx, &x509DeviceCertData);
        X509MakeCompoundCert(&cerCtx, &DICERAMAREA->info, DICEAPPHDR);
        X509CompleteCert(&cerCtx, &x509DeviceCertData);
        DICERAMAREA->info.certBagLen += DERtoPEM(&cerCtx,
                                                 0,
                                                 &DICERAMAREA->info.certBag[DICERAMAREA->info.certBagLen],
                                                 DICERAMDATASIZE - ((uint32_t)&DICERAMAREA->info.certBag[DICERAMAREA->info.certBagLen] - DICERAMSTART));
        DICERAMAREA->info.size = sizeof(DicePublicInfo_t) - 1 + DICERAMAREA->info.certBagLen;
    }

Cleanup:
//    DicePrintInfo();
    return retVal;
}

DICE_STATUS DiceSecure(void)
{
    DICE_STATUS retVal = DICE_SUCCESS;

    // If we got here because of a firewall violation tell the world
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_FWRST) != RESET)
    {
        EPRINTF("WARNING: Reboot due to firewall violation detected.\r\n");
        __HAL_RCC_CLEAR_RESET_FLAGS();
    }

    // Raise the firewall around the data EEProm
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    fw_init.CodeSegmentStartAddress = 0;
    fw_init.CodeSegmentLength = 0;
    fw_init.NonVDataSegmentStartAddress = DICEDATAEEPROMSTART;
    fw_init.NonVDataSegmentLength = DICEDATAEEPROMSIZE;
    fw_init.VDataSegmentStartAddress = 0;
    fw_init.VDataSegmentLength = 0;
    fw_init.VolatileDataExecution = FIREWALL_VOLATILEDATA_NOT_EXECUTABLE;
    fw_init.VolatileDataShared = FIREWALL_VOLATILEDATA_NOT_SHARED;
    if(HAL_FIREWALL_Config(&fw_init) != HAL_OK)
    {
        retVal = DICE_FAILURE;
        goto Cleanup;
    }
    HAL_FIREWALL_EnableFirewall();
    if(!__HAL_FIREWALL_IS_ENABLED())
    {
        EPRINTF("ERROR: Firewall did not engage.\r\n");
        retVal = DICE_FAILURE;
        goto Cleanup;
    }
    EPRINTF("INFO: Firewall is UP!\r\n");
    
Cleanup:
    return retVal;
}
