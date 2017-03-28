#include "DiceSha256.h"
#include "DiceHmac.h"
#include "DiceKdf.h"
#include "DiceEcc.h"
#include "DiceStatus.h"

#define DICEMAJORVERSION            (0x0001)
#define DICEMINORVERSION            (0x0001)
#define DICEDATE                    (1490281840)
#define DICEMAGIC                   (0x45434944) //'DICE'
#define DICEPROVISIONEDID           (0x44494345) //'ECID'
#define DICEMARKER                  (0x4B52414D45434944)
#define DICEDATAEEPROMSTART         (0x08080000)
#define DICEDATAEEPROMSIZE          (0x00000100)
#define DICEFUSEAREA                ((PDiceFuse_t)0x08080000)
#define DICERAMSTART                (0x20000000)
#define DICERAMSIZE                 (0x00000200)
#define DICEWIPERAMSTART            (DICERAMSTART + DICERAMSIZE)
#define DICEWIPERAMSIZE             (0x00005000 - DICERAMSIZE)
#define DICEAPPLICATIONOFFSET       (0x00005400)
#define DICEAPPLICATIONAREASTART    (0x08000000 + DICEAPPLICATIONOFFSET)
#define DICEAPPLICATIONAREASIZE     (0x00030000 - DICEAPPLICATIONOFFSET)
#define DICECOMPOUNDDERIVATIONLABEL "DiceCompoundKey"
#define DICEDATAPTR                 ((PDiceData_t)DICERAMSTART)

#define DICEBLINKRESETME            (1)
#define DICEBLINKDFU                (2)
#define DICEBLINKERROR              (3)

#ifndef SILENTDICE
#define EPRINTF(...) fprintf(&__stderr, __VA_ARGS__)
#define EPRINTFHEXSTRING(__p, __l) for(uint32_t n = 0; n < (__l); n++) EPRINTF("%02x", ((uint8_t*)(__p))[n]);
#define EPRINTFBYTESTRING(__p, __l) for(uint32_t n = 0; n < (__l); n++) EPRINTF("%02x ", ((uint8_t*)(__p))[n]);
#else
#define EPRINTF(...)
#define EPRINTFHEXSTRING(__p, __l)
#define EPRINTFBYTESTRING(__p, __l)
#endif

typedef struct
{
    uint32_t importedSeed:1;
    uint32_t noClear:1;
    uint32_t noBootNonce:1;
    uint32_t inheritedAuthority:1;
} DiceProperties_t, *PDiceProperties_t;

typedef struct
{
    uint32_t magic;                 // 4
    DiceProperties_t properties;    // 4
    uint32_t rollBackProtection;    // 4
    ecc_publickey authorityPub;     // 19 * 4 = 76
    ecc_publickey devicePub;        // 19 * 4 = 76
} DicePublic_t, *PDicePublic_t;     // Size = 164

typedef union {
    struct {
        DicePublic_t deviceInfo;   // 164
        ecc_privatekey devicePrv;  // 9 * 4 = 36
    } s;                           // Used Size = 200
    uint32_t raw32[64];
    uint8_t raw8[256];             // Final Size = 256
} DiceFuse_t, *PDiceFuse_t;

typedef struct
{
    struct
    {
        DicePublic_t deviceInfo;                 // 164
        uint32_t codeSize;                       // 4
        uint32_t issueDate;                      // 4
        uint8_t codeName[SHA256_DIGEST_LENGTH];  // 32
        ecc_publickey compoundPub;               // 19 * 4 = 76
        ecc_publickey alternatePub;              // 19 * 4 = 76
        uint8_t bootNonce[SHA256_DIGEST_LENGTH]; // 32
    } signData;
    ecc_signature signature;                     // 18 * 4 = 72
} DiceCert_t, *PDiceCert_t;                      // Size = 460

typedef struct
{
    uint64_t startMarker;
    struct
    {
        uint32_t codeSize;
        uint32_t issueDate;
        uint8_t codeDigest[SHA256_DIGEST_LENGTH];
        uint8_t alternateDigest[SHA256_DIGEST_LENGTH];
    } signedData;
    ecc_signature signature;
    uint64_t endMarker;
} DiceEmbeddedSignature_t, *PDiceEmbeddedSignature_t;

typedef union
{
    struct
    {
        DiceCert_t cert;                             // 460
        ecc_privatekey compoundPrv;                  // 9 * 4 = 36
        ecc_privatekey alternatePrv;                 // 9 * 4 = 36
        PDiceEmbeddedSignature_t codeSignaturePtr;   // 4
    } s;                                             // 536
    uint32_t raw32[136];
    uint8_t raw8[544];
} DiceData_t, *PDiceData_t;

extern DiceData_t DiceData;
extern const DiceEmbeddedSignature_t trailer;
extern volatile uint32_t touch;

DICE_STATUS DiceLockDown(void);
DICE_STATUS DiceInitialize(void);
DICE_STATUS DiceSecure(void);
void DiceTouchData(void);
bool DiceNullCheck(void* dataPtr, uint32_t dataSize);
void DiceGetRandom(uint8_t* entropyPtr, uint32_t entropySize);
void DicePrintInfo(void);
void DicePrintInfoHex(char* varName, void* dataPtr, uint32_t dataSize);
bool DiceVerifyDeviceCertificate(void);
void DiceBlink(uint32_t info);
