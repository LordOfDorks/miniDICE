/*(Copyright)

Microsoft Copyright 2017
Confidential Information

*/
#include <stdint.h>
#include <stdbool.h>
#ifndef WIN32
#include "main.h"
#include "stm32l0xx_hal.h"
#endif
#include "DiceCore.h"

// OIDs.  Note that the encoder expects a -1 sentinel.
static int diceVersionOID[] =               { 1,3,6,1,4,1,311,89,3,1,2,-1 };
static int diceTimeStampOID[] =             { 1,3,6,1,4,1,311,89,3,1,3,-1 };
static int dicePropertiesOID[] =            { 1,3,6,1,4,1,311,89,3,1,4,-1 };
static int diceAppAuthorityOID[] =          { 1,3,6,1,4,1,311,89,3,1,5,-1 };
static int diceAppDigestOID[] =             { 1,3,6,1,4,1,311,89,3,1,7,-1 };
static int diceAppAlternateDigestOID[] =    { 1,3,6,1,4,1,311,89,3,1,8,-1 };
static int diceAppVersionOID[] =            { 1,3,6,1,4,1,311,89,3,1,9,-1 };
static int diceAppSizeOID[] =               { 1,3,6,1,4,1,311,89,3,1,10,-1 };
static int diceAppTimeStampOID[] =          { 1,3,6,1,4,1,311,89,3,1,11,-1 };
static int diceBootCounterOID[] =           { 1,3,6,1,4,1,311,89,3,1,12,-1 };
static int ecdsaWithSHA256OID[] =           { 1,2,840,10045,4,3,2,-1 };
static int authorityKeyIdentifierOID[] =    { 2,5,29,1,-1 };
static int subjectKeyIdentifierOID[] =      { 2,5,29,14,-1 };
static int keyUsageOID[] =                  { 2,5,29,15,-1 };
//static int subjectAltNameOID[] =            { 2,5,29,17,-1 };
static int basicConstraintsOID[] =          { 2,5,29,19,-1 };
//static int authorityKeyIdentifierOID[] =    { 2,5,29,35,-1 };
static int extKeyUsageOID[] =               { 2,5,29,37,-1 };
static int serverAuthOID[] =                { 1,3,6,1,5,5,7,3,1,-1 };
static int clientAuthOID[] =                { 1,3,6,1,5,5,7,3,2,-1 };
//static int sha256OID[] =                    { 2,16,840,1,101,3,4,2,1,-1 };
static int commonNameOID[] =                { 2,5,4,3,-1 };
static int countryNameOID[] =               { 2,5,4,6,-1 };
static int orgNameOID[] =                   { 2,5,4,10,-1 };

static int
X509AddX501Name(
    DERBuilderContext   *Context,
    const char          *CommonName,
    const char          *OrgName,
    const char          *CountryName
)
{
    CHK(DERStartSequenceOrSet(Context, true));
    CHK(    DERStartSequenceOrSet(Context, false));
    CHK(        DERStartSequenceOrSet(Context, true));
    CHK(            DERAddOID(Context, commonNameOID));
    CHK(            DERAddUTF8String(Context, CommonName));
    CHK(        DERPopNesting(Context));
    CHK(    DERPopNesting(Context));
    CHK(    DERStartSequenceOrSet(Context, false));
    CHK(        DERStartSequenceOrSet(Context, true));
    CHK(            DERAddOID(Context, countryNameOID));
    CHK(            DERAddUTF8String(Context, CountryName));
    CHK(        DERPopNesting(Context));
    CHK(    DERPopNesting(Context));
    CHK(    DERStartSequenceOrSet(Context, false));
    CHK(        DERStartSequenceOrSet(Context, true));
    CHK(            DERAddOID(Context, orgNameOID));
    CHK(            DERAddUTF8String(Context, OrgName));
    CHK(        DERPopNesting(Context));
    CHK(    DERPopNesting(Context));
    CHK(DERPopNesting(Context));

    return 0;

Error:
    return -1;
}

int
X509MakeCertBody(
    DERBuilderContext   *Tbs,
    DICE_X509_TBS_DATA  *TbsData
)
{
    uint8_t     encBuffer[65];
    uint32_t    encBufferLen;
    uint8_t     digest[SHA256_DIGEST_LENGTH];

    CHK(DERStartSequenceOrSet(Tbs, true));
    CHK(    DERAddShortExplicitInteger(Tbs, 2));
    CHK(    DERAddIntegerFromArray(Tbs, TbsData->Subject.CertSerialNum, sizeof(TbsData->Subject.CertSerialNum)));
    CHK(    DERStartSequenceOrSet(Tbs, true));
    CHK(        DERAddOID(Tbs, ecdsaWithSHA256OID));
    CHK(    DERPopNesting(Tbs));
    CHK(    X509AddX501Name(Tbs, TbsData->Issuer.CommonName, TbsData->Issuer.Org, TbsData->Issuer.Country));
    CHK(    DERStartSequenceOrSet(Tbs, true));
    CHK(        DERAddUTCTime(Tbs, TbsData->ValidFrom));
    CHK(        DERAddUTCTime(Tbs, TbsData->ValidTo));
    CHK(    DERPopNesting(Tbs));
    CHK(    X509AddX501Name(Tbs, TbsData->Subject.CommonName, TbsData->Subject.Org, TbsData->Subject.Country));
    CHK(    DERGetEccPub(Tbs, TbsData->Subject.PubKey));
    CHK(    DERStartExplicit(Tbs, 3));
    CHK(        DERStartSequenceOrSet(Tbs, true));
    CHK(            DERStartSequenceOrSet(Tbs, true));
    CHK(                DERAddOID(Tbs, authorityKeyIdentifierOID));
    CHK(                DERStartEnvelopingOctetString(Tbs));
    CHK(                    DERStartSequenceOrSet(Tbs, true));
                                DERExportEccPub(TbsData->Issuer.PubKey, encBuffer, &encBufferLen);
                                Dice_SHA256_Block(encBuffer, encBufferLen, digest);
    CHK(                        DERAddSequenceOctets(Tbs, 0, digest, sizeof(digest)));
    CHK(                    DERPopNesting(Tbs));
    CHK(                DERPopNesting(Tbs));
    CHK(            DERPopNesting(Tbs));
    CHK(            DERStartSequenceOrSet(Tbs, true));
    CHK(                DERAddOID(Tbs, subjectKeyIdentifierOID));
    CHK(                DERStartEnvelopingOctetString(Tbs));
                            DERExportEccPub(TbsData->Subject.PubKey, encBuffer, &encBufferLen);
                            Dice_SHA256_Block(encBuffer, encBufferLen, digest);
    CHK(                    DERAddOctetString(Tbs, digest, sizeof(digest)));
    CHK(                DERPopNesting(Tbs));
    CHK(            DERPopNesting(Tbs));
    return 0;

Error:
    return -1;
}

int
X509CompleteCert(
    DERBuilderContext   *Tbs,
    DICE_X509_TBS_DATA  *TbsData
)
{
    uint8_t     encBuffer[65] = {0};
    uint32_t    encBufferLen = 0;
    ecc_signature sig;

    CHK(        DERPopNesting(Tbs));
    CHK(    DERPopNesting(Tbs));
    CHK(DERPopNesting(Tbs));

    Dice_DSASign(Tbs->Buffer, Tbs->Position, TbsData->Issuer.PrvKey, &sig);
    if(Dice_DSAVerify(Tbs->Buffer, Tbs->Position, &sig, TbsData->Issuer.PubKey) != DICE_SUCCESS)
    {
        return -1;
    }

    CHK(DERTbsToCert(Tbs));
    CHK(    DERStartSequenceOrSet(Tbs, true));
    CHK(        DERAddOID(Tbs, ecdsaWithSHA256OID));
    CHK(    DERPopNesting(Tbs));
    CHK(    DERStartEnvelopingBitString(Tbs));
    CHK(        DERStartSequenceOrSet(Tbs, true));
                    encBufferLen = BigValToBigInt(encBuffer, &sig.r);
    CHK(            DERAddIntegerFromArray(Tbs, encBuffer, encBufferLen));
                    encBufferLen = BigValToBigInt(encBuffer, &sig.s);
    CHK(            DERAddIntegerFromArray(Tbs, encBuffer, encBufferLen));
    CHK(        DERPopNesting(Tbs));
    CHK(    DERPopNesting(Tbs));
    CHK(DERPopNesting(Tbs));

    ASRT(DERGetNestingDepth(Tbs) == 0);
    return 0;

Error:
    return -1;
}

int
X509MakeDeviceCert(
    DERBuilderContext   *Tbs,
    uint32_t            DiceVersion,
    uint32_t            DiceTimeStamp,
    uint32_t            DiceProperties,
    ecc_publickey       *AppAuthorityPub
)
{
    uint8_t keyUsage = 0x43; //digitalSignature, keyCertSign, cRLSign

    CHK(            DERStartSequenceOrSet(Tbs, true));
    CHK(                DERAddOID(Tbs, basicConstraintsOID));
    CHK(                DERAddBoolean(Tbs, true));
    CHK(                DERStartEnvelopingOctetString(Tbs));
    CHK(                    DERStartSequenceOrSet(Tbs, true));
    CHK(                        DERAddBoolean(Tbs, true));
    CHK(                        DERAddInteger(Tbs, 1));
    CHK(                    DERPopNesting(Tbs));
    CHK(                DERPopNesting(Tbs));
    CHK(            DERPopNesting(Tbs));
    CHK(            DERStartSequenceOrSet(Tbs, true));
    CHK(                DERAddOID(Tbs, keyUsageOID));
    CHK(                DERAddBoolean(Tbs, true));
    CHK(                DERStartEnvelopingOctetString(Tbs));
    CHK(                    DERAddBitString(Tbs, &keyUsage, 7));
    CHK(                DERPopNesting(Tbs));
    CHK(            DERPopNesting(Tbs));
    CHK(            DERStartSequenceOrSet(Tbs, true));
    CHK(                DERAddOID(Tbs, diceVersionOID));
    CHK(                DERStartEnvelopingOctetString(Tbs));
    CHK(                    DERAddInteger(Tbs, DiceVersion));
    CHK(                DERPopNesting(Tbs));
    CHK(            DERPopNesting(Tbs));
    CHK(            DERStartSequenceOrSet(Tbs, true));
    CHK(                DERAddOID(Tbs, diceTimeStampOID));
    CHK(                DERStartEnvelopingOctetString(Tbs));
    CHK(                    DERAddUTCTime(Tbs, DiceTimeStamp));
    CHK(                DERPopNesting(Tbs));
    CHK(            DERPopNesting(Tbs));
    if(DiceProperties)
    {
        CHK(            DERStartSequenceOrSet(Tbs, true));
        CHK(                DERAddOID(Tbs, dicePropertiesOID));
        CHK(                DERStartEnvelopingOctetString(Tbs));
        CHK(                    DERAddInteger(Tbs, DiceProperties));
        CHK(                DERPopNesting(Tbs));
        CHK(            DERPopNesting(Tbs));
    }
    if(AppAuthorityPub)
    {
        CHK(            DERStartSequenceOrSet(Tbs, true));
        CHK(                DERAddOID(Tbs, diceAppAuthorityOID));
        CHK(                DERStartEnvelopingOctetString(Tbs));
        CHK(                    DERGetEccPub(Tbs, AppAuthorityPub));
        CHK(                DERPopNesting(Tbs));
        CHK(            DERPopNesting(Tbs));
    }
    return 0;

Error:
    return -1;
}

int
X509MakeCompoundCert(
    DERBuilderContext *Tbs,
    PDicePublicInfo_t DevInfo,
    PDiceEmbeddedSignature_t AppInfo
)
{
    uint8_t keyUsage = 0x07; //Digital Signature, Non-Repudiation, Key Encipherment (e0)

    CHK(            DERStartSequenceOrSet(Tbs, true));
    CHK(                DERAddOID(Tbs, basicConstraintsOID));
    CHK(                DERAddBoolean(Tbs, true));
    CHK(                DERStartEnvelopingOctetString(Tbs));
    CHK(                    DERStartSequenceOrSet(Tbs, true));
    CHK(                        DERAddBoolean(Tbs, false));
    CHK(                    DERPopNesting(Tbs));
    CHK(                DERPopNesting(Tbs));
    CHK(            DERPopNesting(Tbs));
    CHK(            DERStartSequenceOrSet(Tbs, true));
    CHK(                DERAddOID(Tbs, keyUsageOID));
    CHK(                DERAddBoolean(Tbs, true));
    CHK(                DERStartEnvelopingOctetString(Tbs));
    CHK(                    DERAddBitString(Tbs, &keyUsage, 3));
    CHK(                DERPopNesting(Tbs));
    CHK(            DERPopNesting(Tbs));
    CHK(            DERStartSequenceOrSet(Tbs, true));
    CHK(                DERAddOID(Tbs, extKeyUsageOID));
    CHK(                DERAddBoolean(Tbs, true));
    CHK(                DERStartEnvelopingOctetString(Tbs));
    CHK(                    DERStartSequenceOrSet(Tbs, true));
    CHK(                        DERAddOID(Tbs, clientAuthOID));
    CHK(                        DERAddOID(Tbs, serverAuthOID));
    CHK(                    DERPopNesting(Tbs));
    CHK(                DERPopNesting(Tbs));
    CHK(            DERPopNesting(Tbs));
    CHK(            DERStartSequenceOrSet(Tbs, true));
    CHK(                DERAddOID(Tbs, diceAppSizeOID));
    CHK(                DERStartEnvelopingOctetString(Tbs));
    CHK(                    DERAddInteger(Tbs, AppInfo->s.sign.codeSize));
    CHK(                DERPopNesting(Tbs));
    CHK(            DERPopNesting(Tbs));
    CHK(            DERStartSequenceOrSet(Tbs, true));
    CHK(                DERAddOID(Tbs, diceAppVersionOID));
    CHK(                DERStartEnvelopingOctetString(Tbs));
    CHK(                    DERAddInteger(Tbs, AppInfo->s.sign.version));
    CHK(                DERPopNesting(Tbs));
    CHK(            DERPopNesting(Tbs));
    CHK(            DERStartSequenceOrSet(Tbs, true));
    CHK(                DERAddOID(Tbs, diceAppTimeStampOID));
    CHK(                DERStartEnvelopingOctetString(Tbs));
    CHK(                    DERAddUTCTime(Tbs, AppInfo->s.sign.issueDate));
    CHK(                DERPopNesting(Tbs));
    CHK(            DERPopNesting(Tbs));
    CHK(            DERStartSequenceOrSet(Tbs, true));
    CHK(                DERAddOID(Tbs, diceAppDigestOID));
    CHK(                DERStartEnvelopingOctetString(Tbs));
    CHK(                    DERAddOctetString(Tbs, AppInfo->s.sign.appDigest, sizeof(AppInfo->s.sign.appDigest)));
    CHK(                DERPopNesting(Tbs));
    CHK(            DERPopNesting(Tbs));
    if(!DiceNullCheck(AppInfo->s.sign.alternateDigest, sizeof(AppInfo->s.sign.alternateDigest)))
    {
        CHK(            DERStartSequenceOrSet(Tbs, true));
        CHK(                DERAddOID(Tbs, diceAppAlternateDigestOID));
        CHK(                DERStartEnvelopingOctetString(Tbs));
        CHK(                    DERAddOctetString(Tbs, AppInfo->s.sign.alternateDigest, sizeof(AppInfo->s.sign.alternateDigest)));
        CHK(                DERPopNesting(Tbs));
        CHK(            DERPopNesting(Tbs));
    }
    CHK(            DERStartSequenceOrSet(Tbs, true));
    CHK(                DERAddOID(Tbs, diceBootCounterOID));
    CHK(                DERStartEnvelopingOctetString(Tbs));
    CHK(                    DERAddInteger(Tbs, DevInfo->bootCounter));
    CHK(                DERPopNesting(Tbs));
    CHK(            DERPopNesting(Tbs));
    return 0;

Error:
    return -1;
}
