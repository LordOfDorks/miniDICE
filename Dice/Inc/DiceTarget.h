#ifndef _DICE_TARGET_H
#define _DICE_TARGET_H
/******************************************************************************
 * Copyright (c) 2012-2014, AllSeen Alliance. All rights reserved.
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
#ifndef WIN32
#include "stm32l0xx_hal.h"
#endif
#define assert(expr) ((void)0)

#ifdef WIN32
typedef signed char int8_t;           // 8-bit signed integer
typedef unsigned char uint8_t;        // 8-bit unsigned integer
typedef signed short int16_t;         // 16-bit signed integer
typedef unsigned short uint16_t;      // 16-bit unsigned integer
typedef signed int int32_t;           // 32-bit signed integer
typedef unsigned int uint32_t;        // 32-bit unsigned integer
typedef signed long long int64_t;     // 64-bit signed integer
typedef unsigned long long uint64_t;  // 64-bit unsigned integer
#endif

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

#ifndef MAX
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif

#ifndef SWAP_UINT16
#define SWAP_UINT16(x) (((x) >> 8) | ((x) << 8))
#endif

#ifndef SWAP_UINT32
#define SWAP_UINT32(x) (((x) >> 24) | (((x) & 0x00FF0000) >> 8) | (((x) & 0x0000FF00) << 8) | ((x) << 24))
#endif

#ifndef true
#define true (1)
#endif

#ifndef false
#define false (0)
#endif

#ifndef YES
#define YES (1)
#endif

#ifndef NO
#define NO  (0)
#endif

#ifndef WIN32
#ifndef bool
typedef int bool;
#endif
#else
//#define bool int
#endif

#define WORD_ALIGN(x) ((x & 0x3) ? ((x >> 2) + 1) << 2 : x)
#define HOST_IS_LITTLE_ENDIAN  true
#define HOST_IS_BIG_ENDIAN     false
#define ALIGNED_ACCESS_REQUIRED

#ifndef NDEBUG

extern uint8_t dbgCONFIGUREME;
extern uint8_t dbgINIT;
extern uint8_t dbgNET;
extern uint8_t dbgTARGET_CRYPTO;
extern uint8_t dbgTARGET_NVRAM;
extern uint8_t dbgTARGET_UTIL;

#endif

#define inline __inline

// Main method allows argc, argv
#define MAIN_ALLOWS_ARGS

#ifdef ALL_SYMBOLS
# define    static
#endif

#define MEMSET_BZERO(__ptr, __val, __len) for(unsigned int i = 0; i < (__len); i++) ((unsigned char*)(__ptr))[i] = 0x00;
#define MEMCPY_BCOPY(__dst, __src, __sze) for (unsigned int i = 0; i < (__sze); i++) ((unsigned char*)(__dst))[i] = ((unsigned char*)(__src))[i];

#if BYTE_ORDER == LITTLE_ENDIAN
#if !defined(ALIGNED_ACCESS_REQUIRED)
#define REVERSE32(w,x)  { \
    sha2_word32 tmp = (w); \
    tmp = (tmp >> 16) | (tmp << 16); \
    (x) = ((tmp & 0xff00ff00UL) >> 8) | ((tmp & 0x00ff00ffUL) << 8); \
}
#else
#define REVERSE32(w,x) { \
    sha2_uint8_t *b = (sha2_uint8_t*) &w; \
    sha2_word32 tmp = 0; \
    tmp = ((sha2_word32)*b++); \
    tmp = (tmp << 8) | ((sha2_word32)*b++); \
    tmp = (tmp << 8) | ((sha2_word32)*b++); \
    tmp = (tmp << 8) | ((sha2_word32)*b++); \
    (x) = tmp; \
}
#endif /* ALIGNED_ACCESS_REQUIRED */
#define REVERSE64(w,x)  { \
    sha2_word64 tmp = (w); \
    tmp = (tmp >> 32) | (tmp << 32); \
    tmp = ((tmp & 0xff00ff00ff00ff00ULL) >> 8) | \
          ((tmp & 0x00ff00ff00ff00ffULL) << 8); \
    (x) = ((tmp & 0xffff0000ffff0000ULL) >> 16) | \
          ((tmp & 0x0000ffff0000ffffULL) << 16); \
}
#endif /* BYTE_ORDER == LITTLE_ENDIAN */

#endif
