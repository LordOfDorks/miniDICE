#ifndef _DICE_X509_BLDR_H
#define _DICE_X509_BLDR_H

#ifdef __cplusplus
extern "C" {
#endif

#define DICE_X509_SNUM_LEN  0x10

// Const x509 "to be signed" data
typedef struct
{
    uint8_t CertSerialNum[DICE_X509_SNUM_LEN];
    const char *CommonName;
    const char *Org;
    const char *Country;
    ecc_publickey *PubKey;
    ecc_privatekey *PrvKey;
} DICE_ENTITY_INFO;

typedef struct
{
    DICE_ENTITY_INFO Issuer;
    uint32_t ValidFrom;
    uint32_t ValidTo;
    DICE_ENTITY_INFO Subject;
} DICE_X509_TBS_DATA;

int
X509MakeCertBody(
    DERBuilderContext   *Tbs,
    DICE_X509_TBS_DATA  *TbsData
);

int
X509CompleteCert(
    DERBuilderContext   *Tbs,
    DICE_X509_TBS_DATA  *TbsData
);

int
X509MakeDeviceCert(
    DERBuilderContext   *Tbs,
    uint32_t            DiceVersion,
    uint32_t            DiceTimeStamp,
    uint32_t            DiceProperties,
    ecc_publickey       *AppAuthorityPub
);

int
X509MakeCompoundCert(
    DERBuilderContext *Tbs,
    PDicePublicInfo_t DevInfo,
    PDiceEmbeddedSignature_t AppInfo
);

#ifdef __cplusplus
}
#endif
#endif
