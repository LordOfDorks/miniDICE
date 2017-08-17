#include <stdbool.h>
#include "DiceSha256.h"
#include "DiceHmac.h"
#include "DiceKdf.h"
#include "DiceEcc.h"
#include "DiceStatus.h"
#include "DiceDerEnc.h"

#define DICEVERSION                 (0x00010004)
#define DICETIMESTAMP               (1502841825)
#define DICEMAGIC                   (0x45434944) //'DICE'
#ifndef SILENTDICE
#define DICEBOOTLOADERSIZE          (0x00010000)
#else
#define DICEBOOTLOADERSIZE          (0x00009400)
#endif
#define DICEDATAEEPROMSTART         (0x08080000)
#define DICEDATAEEPROMSIZE          (0x00001800)
#define DICERAMSTART                (0x20000000)
#define DICERAMSIZE                 (0x00005000)
#define DICEFLASHSTART              (0x08000000)
#define DICEFLASHSIZE               (0x00030000)

#define DICEFUSEDATASIZE            (0x600)
#define DICEAPPSIGNATUREHDRSIZE     (0x200)
#define DICERAMDATASIZE             (0x1000)
#define DICEMAXPERSISTEDCERT        (DICEFUSEDATASIZE - (sizeof(DicePersistedData_t) - 1))

#define DICEFUSEAREA                ((PDicePersistedData_t)DICEDATAEEPROMSTART)
#define DICEREQUESTAREA             ((PDicePersistedData_t)(DICEDATAEEPROMSTART + DICEFUSEDATASIZE))
#define DICERAMAREA                 ((PDiceData_t)DICERAMSTART)
#define DICEAPPHDR                  ((PDiceEmbeddedSignature_t)(DICEFLASHSTART + DICEBOOTLOADERSIZE))
#define DICEAPPENTRY                (DICEFLASHSTART + DICEBOOTLOADERSIZE + sizeof(DiceEmbeddedSignature_t))
#define DICEAPPMAXSIZE              (DICEFLASHSIZE - DICEBOOTLOADERSIZE)
#define DICEWIPERAMSTARTOFFSET      (sizeof(DiceData_t) - 1 + DICERAMAREA->info.certBagLen)
#define DICECOMPOUNDDERIVATIONLABEL "DiceCompoundKey"

#define DICEBLINKERROR              (1)
#define DICEBLINKRESETME            (2)
#define DICEBLINKDFU                (3)
#define DICEBLINKEARLYDFU           (4)

#ifndef SILENTDICE
#define EPRINTF(...) fprintf(stderr, __VA_ARGS__)
#define EPRINTFHEXSTRING(__p, __l) for(uint32_t n = 0; n < (__l); n++) EPRINTF("%02x", ((uint8_t*)(__p))[n]);
#define EPRINTFBYTESTRING(__p, __l) for(uint32_t n = 0; n < (__l); n++) EPRINTF("%02x ", ((uint8_t*)(__p))[n]);
#else
#define EPRINTF(...)
#define EPRINTFHEXSTRING(__p, __l)
#define EPRINTFBYTESTRING(__p, __l)
#endif

typedef struct
{
    uint32_t noClear:1;
} DiceProperties_t, *PDiceProperties_t;

typedef struct
{
    uint32_t magic;                 // 4
    uint32_t size;                  // 4
    DiceProperties_t properties;    // 4
    uint32_t rollBackProtection;    // 4
    uint32_t bootCounter;           // 4
    ecc_publickey devicePub;        // 19 * 4 = 76
    ecc_publickey authorityPub;     // 19 * 4 = 76
    uint32_t dontTouchSize;         // 4
    uint32_t certBagLen;            // 4
    char certBag[1];
} DicePublicInfo_t, *PDicePublicInfo_t; // 180 + certBagLen

typedef struct
{
    uint32_t magic;                 // 4
    ecc_privatekey devicePrv;       // 9 * 4 = 36
    DicePublicInfo_t info;          // 180 + certBagLen
} DicePersistedData_t, *PDicePersistedData_t;

typedef union
{
    struct
    {
        struct
        {
            uint32_t magic;
            uint32_t codeSize;
            uint32_t version;
            uint32_t issueDate;
            char name[16];
            uint8_t appDigest[SHA256_DIGEST_LENGTH];
            uint8_t alternateDigest[SHA256_DIGEST_LENGTH];
        } sign;
        ecc_signature signature;
    } s;
    uint32_t raw32[DICEAPPSIGNATUREHDRSIZE / sizeof(uint32_t)];
    uint8_t raw8[DICEAPPSIGNATUREHDRSIZE];
} DiceEmbeddedSignature_t, *PDiceEmbeddedSignature_t;

typedef struct
{
    uint32_t magic;                              // 4
    ecc_privatekey compoundPrv;                  // 9 * 4 = 36
    ecc_privatekey alternatePrv;                 // 9 * 4 = 36
    ecc_publickey compoundPub;                   // 19 * 4 = 76
    ecc_publickey alternatePub;                  // 19 * 4 = 76
    DicePublicInfo_t info;                       // @0xE8: 168 + certBagLen
} DiceData_t, *PDiceData_t;

DICE_STATUS DiceLockDown(void);
void DiceGenerateDFUString(void);
DICE_STATUS DiceInitialize(void);
DICE_STATUS DiceSecure(void);
bool DiceNullCheck(void* dataPtr, uint32_t dataSize);
void DiceGetRandom(uint8_t* entropyPtr, uint32_t entropySize);
void DicePrintInfo(void);
void DicePrintInfoHex(char* varName, void* dataPtr, uint32_t dataSize);
void DiceBlink(uint32_t info);

#include "DiceX509Bldr.h"
