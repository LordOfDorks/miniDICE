/******************************************************************************
 * Copyright (c) 2014, AllSeen Alliance. All rights reserved.
 *
 *    Permission to use, copy, modify, and/or distribute this software for any
 *    purpose with or without fee is hereby granted, provided that the above
 *    copyright notice and this permission notice appear in all copies.
 *
 *    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 ******************************************************************************/

#include <DiceKdf.h>

#if HOST_IS_LITTLE_ENDIAN
#define UINT32_TO_BIGENDIAN(i)          \
        ( ((i) & 0xff000000ULL >> 24)      \
        | ((i) & 0x00ff0000ULL >> 8)       \
        | ((i) & 0x0000ff00ULL << 8)       \
        | ((i) & 0x000000ffULL << 24))
#else
#define UINT32_TO_BIG_ENDIAN(i) (i)
#endif
#define UINT32_FROM_BIGENDIAN uint32_tO_BIGENDIAN

//
// Create the fixed content for a KDF
// @param fixed  buffer to receive the fixed content
// @param fixedSize  indicates the available space in fixed
// @param label the label parameter (optional)
// @param labelSize
// @param context the context value (optional)
// @param contextSize
// @param numberOfBits the number of bits to be produced
//
uint32_t Dice_KDF_Fixed(
    uint8_t         *fixed,
    uint32_t          fixedSize,
    const uint8_t   *label,
    uint32_t          labelSize,
    const uint8_t   *context,
    uint32_t          contextSize,
    uint32_t        numberOfBits
)
{
    uint32_t total = (((label) ? labelSize : 0) + ((context) ? contextSize : 0) + 5);

    if(!fixed) return total;

    if (label) {
        MEMCPY_BCOPY(fixed, label, labelSize);
        fixed += labelSize;
    }
    *fixed++ = 0;
    if (context) {
        MEMCPY_BCOPY(fixed, context, contextSize);
        fixed += contextSize;
    }
    uint32_t flipped;
    REVERSE32(numberOfBits, flipped);
    MEMCPY_BCOPY(fixed, &flipped, 4);
    return total;
}

//
// Do KDF from SP800-108 -- HMAC based counter mode. This function does a single
// iteration. The counter parameter is incremented before it is used so that
// a caller can set counter to 0 for the first iteration.
// @param out the output digest of a single iteration (a SHA256 digest)
// @param key the HMAC key
// @param keySize
// @param counter the running counter value (may be NULL)
// @param fixed the label parameter (optional)
// @param fixedSize
//
void Dice_KDF_SHA256(
    uint8_t         *out,
    const uint8_t   *key,
    uint32_t          keySize,
    uint32_t        *counter,
    const uint8_t   *fixed,
    uint32_t          fixedSize
)
{
    DICE_HMAC_SHA256_CTX    ctx;
    uint32_t                ctr = counter ? ++*counter : 1;

    assert(out && key && fixed);

    // Start the HMAC
    Dice_HMAC_SHA256_Init(&ctx, key, keySize);
    // Add the counter
    uint32_t flipped;
    REVERSE32(ctr, flipped);
    Dice_HMAC_SHA256_Update(&ctx, (uint8_t *)&flipped, 4);
    // Add fixed stuff
    Dice_HMAC_SHA256_Update(&ctx, fixed, fixedSize);
    Dice_HMAC_SHA256_Final(&ctx, out);
    // Wipe the secret from the stack
    MEMSET_BZERO(&ctx, 0x00, sizeof(ctx));
}

//
// Do KDF from SP800-108 -- HMAC based counter mode.
// @param out the output digest of a single iteration (a SHA256 digest)
// @param key the HMAC key
// @param keySize
// @param counter the running counter value (may be NULL)
// @param fixed the label parameter (optional)
// @param fixedSize
//
void Dice_KDF_SHA256_Seed(
    uint8_t         *out,
    uint32_t        outSize,
    const uint8_t   *key,
    uint32_t          keySize,
    uint32_t        *counter,
    const uint8_t   *fixed,
    uint32_t          fixedSize
)
{
    uint32_t ctr = counter ? *counter : 0;

    for(uint32_t n = 0; n < outSize; n += SHA256_DIGEST_LENGTH)
    {
        uint8_t digest[SHA256_DIGEST_LENGTH];
        ++ctr;
        Dice_KDF_SHA256(digest, key, keySize, &ctr, fixed, fixedSize);
        for(uint32_t m = 0; m < MIN(SHA256_DIGEST_LENGTH, outSize - n); m++)
        {
            out[n + m] = digest[m];
        }
    }
    if(counter) *counter = ctr;
}
