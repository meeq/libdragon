/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*------------------------------------------------------------------------------

    Table of contents

    1. Include headers
    2. Module defines
    3. Data types
    4. Function prototypes

------------------------------------------------------------------------------*/

#ifndef H264SWDEC_STREAM_H
#define H264SWDEC_STREAM_H

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/

#include "basetype.h"
#include "../profile.h"


/*------------------------------------------------------------------------------
    2. Module defines
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    3. Data types
------------------------------------------------------------------------------*/

#ifdef H264BSD_N64
// strmData_t structure optimized for speed on N64. By relying on the fact that
// on N64 pointers are 32-bits, we stuff both the byte pointer and the bit offset
// within the same entity pCurr (lowest 3 bits are the bit offset). This way
// it's possible to manipulate bit pointers very easily and still extract the
// actual memory pointer when required.
typedef struct
{
    u64  pCurr;             /* bit-pointer to the current position in the buffer */
    u64  pEnd;              /* bit-pointer to the end of the buffer */
    u8  *pStart;            /* byte pointer to the start of the buffer */
} strmData_t;

#define STRM_CURR_PTR(s)     ((uint8_t*)(u32)(pStrmData->pCurr>>3))
#define STRM_CURR_BITOFF(s)  (pStrmData->pCurr & 7)

#else
typedef struct
{
    u8  *pStrmBuffStart;    /* pointer to start of stream buffer */
    u8  *pStrmCurrPos;      /* current read address in stream buffer */
    u32  bitPosInWord;      /* bit position in stream buffer byte */
    u32  strmBuffSize;      /* size of stream buffer (bytes) */
    u32  strmBuffReadBits;  /* number of bits read from stream buffer */
} strmData_t;
#endif
/*------------------------------------------------------------------------------
    4. Function prototypes
------------------------------------------------------------------------------*/

// On N64, to improve performance, make all functions as inline.
#ifndef H264BSD_N64
u32 h264bsdIsByteAligned(strmData_t *);
u32 h264bsdFlushBits(strmData_t *pStrmData, u32 numBits);
u32 h264bsdGetBits(strmData_t *pStrmData, u32 numBits);
u32 h264bsdShowBits32(strmData_t *pStrmData);
#else

static inline u32 h264bsdIsByteAligned(strmData_t *pStrmData) {
    return (pStrmData->pCurr & 7) == 0;
}

__attribute__((noinline, cold))
u64 h264bsdLoadTail64(const strmData_t *pStrmData);

static inline u64 h264bsdLoad64(const strmData_t *pStrmData) {
    // Load the 8 bytes that start at the byte of the current bitpointer.
    // The word is not 8-byte aligned, so use byte accesses (the compiler
    // still generates the MIPS ldl/ldr opcodes).
    // Do not read past pEnd: the caller's buffer can end there, and a cached
    // read of the memory after it can load a cache line that belongs to a
    // DMA target. Near the end, h264bsdLoadTail64 reads only the bytes before
    // pEnd.
    u64 pos = pStrmData->pCurr & ~(u64)7;
    if (__builtin_expect(pos + 64 > pStrmData->pEnd, 0))
        return h264bsdLoadTail64(pStrmData);
    u8 *pStrm = STRM_CURR_PTR(pStrmData);
    return ((u64)pStrm[0] << 56) | ((u64)pStrm[1] << 48) |
           ((u64)pStrm[2] << 40) | ((u64)pStrm[3] << 32) |
           ((u64)pStrm[4] << 24) | ((u64)pStrm[5] << 16) |
           ((u64)pStrm[6] <<  8) | ((u64)pStrm[7] <<  0);
}

static inline u32 h264bsdShowBits32(strmData_t *pStrmData) {
    u64 val = h264bsdLoad64(pStrmData);
    val <<= STRM_CURR_BITOFF(pStrmData);
    val >>= 32;
    return (u32)val;
}

static inline u64 h264bsdShowBits64(strmData_t *pStrmData, u32 *bits) {
    u64 val = h264bsdLoad64(pStrmData);
    val <<= STRM_CURR_BITOFF(pStrmData);
    *bits = 64 - STRM_CURR_BITOFF(pStrmData);
    pStrmData->pCurr &= ~7;
    return val;
}

#ifndef HANTRO_OK
#define HANTRO_OK 0
#endif
#ifndef END_OF_STREAM 
#define END_OF_STREAM 0xFFFFFFFFU
#endif

static inline u32 h264bsdFlushBits(strmData_t *pStrmData, u32 numBits) {
    pStrmData->pCurr += numBits;
    if ( pStrmData->pCurr < pStrmData->pEnd )
        return(HANTRO_OK);
    else
        return(END_OF_STREAM);    
}

static inline u32 h264bsdGetBits(strmData_t *pStrmData, u32 numBits) {
    u32 out = h264bsdShowBits32(pStrmData) >> (32 - numBits);
    if (h264bsdFlushBits(pStrmData, numBits) == HANTRO_OK)
        return(out);
    else
        return(END_OF_STREAM);
}

#endif


#endif /* #ifdef H264SWDEC_STREAM_H */

