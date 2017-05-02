/*(Copyright)

Microsoft Copyright 2015, 2016
Confidential Information

*/
#ifndef _DICE_DER_ENC_H
#define _DICE_DER_ENC_H

#include <stdbool.h>
#include "DiceSha256.h"
#include "DiceEcc.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SWAP_UINT16(x) (((x) >> 8) | ((x) << 8))
#define SWAP_UINT32(x) (((x) >> 24) | (((x) & 0x00FF0000) >> 8) | (((x) & 0x0000FF00) << 8) | ((x) << 24))

#define DER_MAX_PEM     0x500
#define DER_MAX_TBS     0x400
#define DER_MAX_NESTED  0x10

#define ASRT(_X) if(!(_X))      {goto Error;}
#define CHK(_X)  if(((_X)) < 0) {goto Error;}

//
// For P256 bigval_t types hold 288-bit 2's complement numbers (9
// 32-bit words).  For P192 they hold 224-bit 2's complement numbers
// (7 32-bit words).
//
// The representation is little endian by word and native endian
// within each word.
// The least significant word of the value is in the 0th word and there is an
// extra word of zero at the top.
//
#define DICE_ECC_PRIVATE_BYTES  (4 * (BIGLEN - 1))
#define DICE_ECC_COORD_BYTES    DICE_ECC_PRIVATE_BYTES

//
// Context structure for the DER-encoder. This structure contains a fixed-
// length array for nested SEQUENCES (which imposes a nesting limit).
// The buffer use for encoded data is caller-allocted.
//
typedef struct
{
    uint8_t     *Buffer;        // Encoded data
    uint32_t     Length;        // Size, in bytes, of Buffer
    uint32_t     Position;      // Current buffer position

    // SETS, SEQUENCES, etc. can be nested. This array contains the start of
    // the payload for collection types and is set by  DERStartSequenceOrSet().
    // Collections are "popped" using DEREndSequenceOrSet().
    int CollectionStart[DER_MAX_NESTED];
    int CollectionPos;
} DERBuilderContext;

// We only have a small subset of potential PEM encodings
enum CertType {
    CERT_TYPE = 0,
    PUBLICKEY_TYPE,
    ECC_PRIVATEKEY_TYPE,
    CERT_REQ_TYPE,
    LAST_CERT_TYPE
};

void
DERInitContext(
    DERBuilderContext   *Context,
    uint8_t             *Buffer,
    uint32_t             Length
);

int
DERGetEncodedLength(
    DERBuilderContext   *Context
);


int
DERAddOID(
    DERBuilderContext   *Context,
    int                 *Values
);

int
DERAddUTF8String(
    DERBuilderContext   *Context,
    const char          *Str
);

int 
DERAddPrintableString(
    DERBuilderContext   *Context,
    const char          *Str
);


int
DERAddUTCTime(
    DERBuilderContext   *Context,
    const uint32_t      TimeStamp
);

int
DERAddIntegerFromArray(
    DERBuilderContext   *Context,
    uint8_t             *Val,
    uint32_t            NumBytes
);

int
DERAddInteger(
    DERBuilderContext   *Context,
    int                 Val
);

int
DERAddShortExplicitInteger(
    DERBuilderContext   *Context,
    int                  Val
);

int
DERAddBoolean(
    DERBuilderContext   *Context,
    bool                 Val
);


int
DERAddBitString(
    DERBuilderContext   *Context,
    uint8_t             *BitString,
    uint32_t             BitStringNumBytes
);

int
DERAddOctetString(
    DERBuilderContext   *Context,
    uint8_t             *OctetString,
    uint32_t             OctetStringLen
);

int
DERAddSequenceOctets(
    DERBuilderContext   *Context,
    uint8_t             Num,
    uint8_t             *OctetString,
    uint32_t             OctetStringLen
);

int
DERStartSequenceOrSet(
    DERBuilderContext   *Context,
    bool                 Sequence
);

int
DERStartExplicit(
    DERBuilderContext   *Context,
    uint32_t             Num
);

int
DERStartEnvelopingOctetString(
    DERBuilderContext   *Context
);

int
DERStartEnvelopingBitString(
    DERBuilderContext   *Context
);

int
DERPopNesting(
    DERBuilderContext   *Context
);

int
DERGetNestingDepth(
    DERBuilderContext   *Context
);

void
DERExportEccPub(
    ecc_publickey       *a,
    uint8_t             *b,
    uint32_t            *s
);

int
DERGetEccPub(
    DERBuilderContext   *Context,
    ecc_publickey       *Pub
);

int
DERGetEccPrv(
    DERBuilderContext   *Context,
    ecc_publickey       *Pub,
    ecc_privatekey      *Prv
);

int
DERTbsToCert(
    DERBuilderContext   *Context
);

int
DERtoPEM(
    DERBuilderContext   *Context,
    uint32_t            Type,
    char                *PEM,
    uint32_t            Length
);

#ifdef __cplusplus
}
#endif
#endif
