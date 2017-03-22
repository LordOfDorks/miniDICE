#include <string.h>
#include <time.h>
#include "main.h"
#include "stm32l0xx_hal.h"
#include "DiceCore.h"

//PDiceFuse_t pDiceEEProm = (PDiceFuse_t)DICEDATAEEPROMSTART;
FIREWALL_InitTypeDef fw_init = {0};

DICE_STATUS DiceLockDown(void)
{
    DICE_STATUS retVal = DICE_SUCCESS;
    FLASH_OBProgramInitTypeDef ob = {0};
    HAL_FLASHEx_OBGetConfig(&ob);

    if((ob.RDPLevel != OB_RDP_LEVEL_0) &&
       (ob.WRPSector != (OB_WRP_Pages0to31 | OB_WRP_Pages32to63 | OB_WRP_Pages64to95 | OB_WRP_Pages96to127 | OB_WRP_Pages128to159 | OB_WRP_Pages160to191 | OB_WRP_Pages192to223 | OB_WRP_Pages224to255)))
    {
        EPRINTF("ERROR: Option bytes are in bad state.\r\n");
        EPRINTF("OptionType:     0x%08x\r\n", ob.OptionType);
        EPRINTF("WRPState:       0x%08x\r\n", ob.WRPState);
        EPRINTF("WRPSector:      0x%08x%08x\r\n", ob.WRPSector2, ob.WRPSector);
        EPRINTF("RDPLevel:       0x%02x\r\n", ob.RDPLevel);
        EPRINTF("BORLevel:       0x%02x\r\n", ob.BORLevel);
        EPRINTF("USERConfig:     0x%02x\r\n", ob.USERConfig);
        EPRINTF("BOOTBit1Config: 0x%02x\r\n", ob.BOOTBit1Config);
        retVal = DICE_FAILURE;
        goto Cleanup;
    }
    else if(ob.RDPLevel == OB_RDP_LEVEL_0)
    {
        memset(&ob, 0x00, sizeof(ob));
        ob.OptionType = OPTIONBYTE_WRP | OPTIONBYTE_RDP;
        ob.WRPState = OB_WRPSTATE_ENABLE;
        ob.WRPSector = OB_WRP_Pages0to31 | OB_WRP_Pages32to63 | OB_WRP_Pages64to95 | OB_WRP_Pages96to127 | OB_WRP_Pages128to159 | OB_WRP_Pages160to191 | OB_WRP_Pages192to223 | OB_WRP_Pages224to255;
#ifndef IRREVERSIBLELOCKDOWN
        ob.RDPLevel = OB_RDP_LEVEL_1;
#else
        ob.RDPLevel = OB_RDP_LEVEL_2;
#endif

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
        for(;;);
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

DICE_STATUS DiceInitialize(void)
{
    DICE_STATUS retVal = DICE_SUCCESS;
    bigval_t seed = {0};

    DiceTouchData();

    if(__HAL_FIREWALL_IS_ENABLED())
    {
        EPRINTF("ERROR: Firewall already engaged!\r\n");
        retVal = DICE_FAILURE;
        goto Cleanup;
    }
    
    touch = 0;
    if(touch != 0)
    {
        HAL_FLASH_Unlock();
        for(uint32_t n = 0; n < sizeof(DiceFuse_t) / sizeof(uint32_t); n++) HAL_FLASHEx_DATAEEPROM_Erase((uint32_t)(&DICEFUSEAREA->raw32[n]));
        HAL_FLASH_Lock();
    }

    if(DICEFUSEAREA->s.deviceInfo.magic != DICEMAGIC)
    {
        DiceFuse_t staging = {0};
        if(DICEFUSEAREA->s.deviceInfo.magic == DICEPROVISIONEDID)
        {
            memcpy(&staging, DICEFUSEAREA, sizeof(staging));
        }
        else
        {
            staging.s.deviceInfo.properties.importedSeed = 0;
            staging.s.deviceInfo.properties.noClear = 0;
            staging.s.deviceInfo.properties.noBootNonce = 0;

            // If there is alreay an authority key here - we keep that
            if(!DiceNullCheck(&(DICEFUSEAREA->s.deviceInfo.authorityPub), sizeof(DICEFUSEAREA->s.deviceInfo.authorityPub)))
            {
                staging.s.deviceInfo.properties.inheritedAuthority = 1;
                memcpy(&staging.s.deviceInfo.authorityPub, &DICEFUSEAREA->s.deviceInfo.authorityPub, sizeof(staging.s.deviceInfo.authorityPub));
            }
        }
        staging.s.deviceInfo.magic = DICEMAGIC;
        Dice_GenerateDSAKeyPair(&staging.s.deviceInfo.devicePub, &staging.s.devicePrv);

        // Write the data to the EEPROM
        if(HAL_FLASH_Unlock() != HAL_OK)
        {
            EPRINTF("ERROR: HAL_FLASH_Unlock() failed!\r\n");
            retVal = DICE_FAILURE;
            goto Cleanup;
        }
        for(uint32_t n = 0; n < (sizeof(staging.raw32) / sizeof(uint32_t)); n++)
        {
            if(DICEFUSEAREA->raw32[n] != staging.raw32[n])
            {
                if(DICEFUSEAREA->raw32[n] != 0)
                {
                    while(HAL_FLASHEx_DATAEEPROM_Erase((uint32_t)(&DICEFUSEAREA->raw32[n])) != HAL_OK);
                }
                if(staging.raw32[n])
                {
                    while(HAL_FLASHEx_DATAEEPROM_Program(FLASH_TYPEPROGRAM_WORD, (uint32_t)(&DICEFUSEAREA->raw32[n]), staging.raw32[n]) != HAL_OK);
                }
            }
        }
        if(HAL_FLASH_Lock() != HAL_OK)
        {
            EPRINTF("ERROR: HAL_FLASH_Lock() failed!\r\n");
            retVal = DICE_FAILURE;
            goto Cleanup;
        }
    }

    // Put the diceData together
    memset(DiceData.raw8, 0x00, sizeof(DiceData.raw8));
    // Copy the device information over
    memcpy(&DiceData.s.cert.signData.deviceInfo, &DICEFUSEAREA->s.deviceInfo, sizeof(DiceData.s.cert.signData.deviceInfo));
    
    // Get some salt for the cert
    if(!DiceData.s.cert.signData.deviceInfo.properties.noBootNonce)
    {
        DiceGetRandom(DiceData.s.cert.signData.bootNonce, sizeof(DiceData.s.cert.signData.bootNonce));
    }

    // No code - no boot
    if(DiceNullCheck((uint8_t*)DICEAPPLICATIONAREASTART, 1024))
    {
        EPRINTF("WARNING: The application area is NULL, we go direcrtly to DFU mode!\r\n");
        retVal = DICE_LOAD_MODULE_FAILED;
        goto Cleanup;
    }

    // Find the embedded signature block
    for(uint32_t n = 0; n < (DICEAPPLICATIONAREASIZE - sizeof(DiceEmbeddedSignature_t)); n += sizeof(uint64_t))
    {
        PDiceEmbeddedSignature_t signatureBlock = (PDiceEmbeddedSignature_t)&(((uint8_t*)DICEAPPLICATIONAREASTART)[n]);
        if((signatureBlock->startMarker == DICEMARKER) && (signatureBlock->endMarker == DICEMARKER))
        {
            DiceData.s.codeSignaturePtr = signatureBlock;
            DiceData.s.cert.signData.codeSize = signatureBlock->signedData.codeSize;
            DiceData.s.cert.signData.issueDate = signatureBlock->signedData.issueDate;
            break;
        }
    }
    if(DiceData.s.codeSignaturePtr == NULL)
    {
        EPRINTF("WARNING: No embedded signature trailer was found, we go direcrtly to DFU mode!\r\n");
        retVal = DICE_LOAD_MODULE_FAILED;
        goto Cleanup;
    }
    if(DiceData.s.codeSignaturePtr->signedData.issueDate < DICEFUSEAREA->s.deviceInfo.rollBackProtection)
    {
        EPRINTF("RollBackProtection: %s\r", asctime(localtime((time_t*)&DICEFUSEAREA->s.deviceInfo.rollBackProtection)));
        EPRINTF("CodeIssuanceDate:   %s\r", asctime(localtime((time_t*)&DiceData.s.codeSignaturePtr->signedData.issueDate)));
        EPRINTF("WARNING: Application rollback was detected, we go direcrtly to DFU mode!\r\n");
        retVal = DICE_LOAD_MODULE_FAILED;
        goto Cleanup;
    }
    
    // If the current application is newer than the previous one, crank the monotonic counter forward
    if((!DiceNullCheck(&DiceData.s.cert.signData.deviceInfo.authorityPub, sizeof(ecc_publickey))) &&
       (DiceData.s.codeSignaturePtr->signedData.issueDate > DICEFUSEAREA->s.deviceInfo.rollBackProtection))
    {
        if((HAL_FLASH_Unlock() != HAL_OK) ||
           (HAL_FLASHEx_DATAEEPROM_Erase((uint32_t)(&DICEFUSEAREA->s.deviceInfo.rollBackProtection)) != HAL_OK) ||
           (HAL_FLASHEx_DATAEEPROM_Program(FLASH_TYPEPROGRAM_WORD, (uint32_t)(&DICEFUSEAREA->s.deviceInfo.rollBackProtection), DiceData.s.codeSignaturePtr->signedData.issueDate)) ||
           (HAL_FLASH_Lock() != HAL_OK))
        {
            retVal = DICE_FAILURE;
            EPRINTF("ERROR: Compound key derivation failed!\r\n");
            goto Cleanup;
        }
        DiceData.s.codeSignaturePtr->signedData.issueDate = DICEFUSEAREA->s.deviceInfo.rollBackProtection;
        EPRINTF("INFO: Application update detected. Rollback protection adjusted.\r\n");
    }
    
    // Measure everything up to the embedded signature trailer and make sure the digest matches the trailer
    Dice_SHA256_Block((uint8_t*)DICEAPPLICATIONAREASTART, DiceData.s.cert.signData.codeSize, DiceData.s.cert.signData.codeName);
    if(memcmp(DiceData.s.cert.signData.codeName, DiceData.s.codeSignaturePtr->signedData.codeDigest, sizeof(DiceData.s.cert.signData.codeName)))
    {
        EPRINTF("WARNING: Application digest does not match with the embedded signature trailer, we go direcrtly to DFU mode!\r\n");
        retVal = DICE_LOAD_MODULE_FAILED;
        goto Cleanup;
    }

    // Verify the code signature if we have an authorityPub
    if(!DiceNullCheck(&DiceData.s.cert.signData.deviceInfo.authorityPub, sizeof(ecc_publickey)))
    {
        if((retVal = Dice_DSAVerify((uint8_t*)&DiceData.s.codeSignaturePtr->signedData, sizeof(DiceData.s.codeSignaturePtr->signedData), &DiceData.s.codeSignaturePtr->signature, &DiceData.s.cert.signData.deviceInfo.authorityPub)) != DICE_SUCCESS)
        {
            EPRINTF("WARNING: Authority signature verification of embedded signature trailer failed, we go direcrtly to DFU mode!\r\n");
            retVal = DICE_LOAD_MODULE_FAILED;
            goto Cleanup;
        }
    }
    else
    {
        EPRINTF("INFO: No authority set, signature verification is skipped.\r\n");
    }

    // Derive the compundDevice key
    Dice_HMAC_SHA256_Block((uint8_t*)&DICEFUSEAREA->s.devicePrv, sizeof(DICEFUSEAREA->s.devicePrv), DiceData.s.cert.signData.codeName, sizeof(DiceData.s.cert.signData.codeName), (uint8_t*)&seed);
    if((retVal = Dice_DeriveDsaKeyPair(&DiceData.s.cert.signData.compoundPub, &DiceData.s.compoundPrv, &seed, (uint8_t*)DICECOMPOUNDDERIVATIONLABEL, sizeof(DICECOMPOUNDDERIVATIONLABEL) - 1)) != DICE_SUCCESS)
    {
        EPRINTF("ERROR: Compound key derivation failed!\r\n");
        goto Cleanup;
    }
    
    // Derive the alternate key if we are supposed to
    if(!DiceNullCheck(DiceData.s.codeSignaturePtr->signedData.alternateDigest, sizeof(DiceData.s.codeSignaturePtr->signedData.alternateDigest)))
    {
        Dice_HMAC_SHA256_Block((uint8_t*)&DICEFUSEAREA->s.devicePrv, sizeof(DICEFUSEAREA->s.devicePrv), DiceData.s.codeSignaturePtr->signedData.alternateDigest, sizeof(DiceData.s.codeSignaturePtr->signedData.alternateDigest), (uint8_t*)&seed);
        if((retVal = Dice_DeriveDsaKeyPair(&DiceData.s.cert.signData.alternatePub, &DiceData.s.alternatePrv, &seed, (uint8_t*)DICECOMPOUNDDERIVATIONLABEL, sizeof(DICECOMPOUNDDERIVATIONLABEL) - 1)) != DICE_SUCCESS)
        {
            EPRINTF("ERROR: Alternate key derivation failed!\r\n");
            goto Cleanup;
        }
    }
    
    // Sign the DiceData with the device key
    if((retVal = Dice_DSASign((uint8_t*)&DiceData.s.cert.signData, sizeof(DiceData.s.cert.signData), &DICEFUSEAREA->s.devicePrv, &DiceData.s.cert.signature)) != DICE_SUCCESS)
    {
        EPRINTF("ERROR: Device certificate signing failed!\r\n");
        goto Cleanup;
    }

Cleanup:
    DicePrintInfo();
    DiceSecure();
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
    
Cleanup:
    return retVal;
}
