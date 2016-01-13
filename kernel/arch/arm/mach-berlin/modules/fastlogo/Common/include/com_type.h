/*******************************************************************************
* Copyright (C) Marvell International Ltd. and its affiliates
*
* Marvell GPL License Option
*
* If you received this File from Marvell, you may opt to use, redistribute and/or
* modify this File in accordance with the terms and conditions of the General
* Public License Version 2, June 1991 (the "GPL License"), a copy of which is
* available along with the File in the license.txt file or by writing to the Free
* Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 or
* on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
*
* THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED
* WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY
* DISCLAIMED.  The GPL License provides additional details about this warranty
* disclaimer.
********************************************************************************/



#ifndef __COMMON_TYPE_H__
#define __COMMON_TYPE_H__

#include <linux/kernel.h>
#include <linux/string.h>


////////////////////////////////////////////////////////////////////////////////
//! \brief Basic data types and macros
////////////////////////////////////////////////////////////////////////////////
typedef unsigned char       UCHAR;
typedef char                CHAR;
#ifndef BOOL
typedef UCHAR               BOOL;
#endif

typedef UCHAR               BOOLEAN;
typedef short               SHORT;
typedef unsigned short      USHORT;
typedef int                 INT;
typedef unsigned int        UINT;
typedef long                LONG;
typedef unsigned long       ULONG;
typedef long long           LONGLONG;
typedef unsigned long long  ULONGLONG;
typedef void                VOID;
typedef void*               PTR;
typedef void**              PHANDLE;
typedef void*               HANDLE;
typedef void*               PVOID;

typedef UCHAR               BYTE;

typedef CHAR                INT8;
typedef UCHAR               UINT8;
typedef short               INT16;
typedef unsigned short      UINT16;
typedef int                 INT32;
typedef unsigned int        UINT32;
typedef long long           INT64;
typedef unsigned long long  UINT64;
typedef unsigned int        SIZE_T;

typedef signed int          HRESULT;

typedef struct
{
    UINT    Data1;
    UINT16  Data2;
    UINT16  Data3;
    UCHAR   Data4[8];
} GUID;

#ifndef _MAX_PATH
#define _MAX_PATH                       260
#endif

#ifndef TRUE
#define TRUE                            (1)
#endif
#ifndef FALSE
#define FALSE                           (0)
#endif

#ifndef true
#define true                            (1)
#endif
#ifndef false
#define false                           (0)
#endif

#ifndef True
#define True                            (1)
#endif
#ifndef False
#define False                           (0)
#endif

#ifndef NULL
#define NULL            ((void *)0)
#endif

#ifndef IN
#define IN
#endif
#ifndef OUT
#define OUT
#endif
#ifndef INOUT
#define INOUT
#endif

#define MV_CAST(type)   type



////////////////////////////////////////////////////////////////////////////////
//! \brief Memory allocation and related...
////////////////////////////////////////////////////////////////////////////////

#define GaloisCalloc        calloc

#define GALOIS_CACHE_LINE           32


#ifdef BERLIN_SINGLE_CPU
#define SINGLE_CPU_FLICK_HACK
#endif

#define GaloisMalloc        malloc
#define GaloisCalloc        calloc
#define GaloisFree(p)               \
    do {                            \
        if ((p) != NULL) {          \
            free((void *)(p));      \
            (p) = NULL;             \
        }                           \
    } while (0)

#if defined(__LINUX__) || !defined(SMALL_MEM_POOL_SUPPORT)
#define CommonMemPoolMalloc GaloisMalloc
#define CommonMemPoolFree   GaloisFree
#endif

// BD IG/PG and GFX Memory malloc/free functions for small buffer.
// By Wanyong, 20090605.
#define GFXMalloc   CommonMemPoolMalloc
#define GFXFree     CommonMemPoolFree
#define BDRESUBMalloc       CommonMemPoolMalloc
#define BDRESUBFree     CommonMemPoolFree

#define GaloisMemmove       memmove
#define GaloisMemcpy        memcpy
#define GaloisMemClear(buf, n)  memset((buf), 0, (n))
#define GaloisMemSet(buf, c, n) memset((buf), (c), (n))
#define GaloisMemComp(buf1, buf2, n)    memcmp((buf1), (buf2), (n))

#define UINT_ADDR(a, b)     ((UINT *)(a) + ((b) >> 2))
#define USHORT_ADDR(a,b)    ((USHORT *)(a) + ((b) >> 1))
#define UCHAR_ADDR(a, b)    ((UCHAR *)(a) + (b))
#define INT_ADDR(a, b)      ((INT *)(a) + ((b) >> 2))
#define SHORT_ADDR(a,b)     ((SHORT *)(a) + ((b) >> 1))
#define CHAR_ADDR(a, b)     ((CHAR *)(a) + (b))



////////////////////////////////////////////////////////////////////////////////
//! \brief Some useful macros and defs
////////////////////////////////////////////////////////////////////////////////
#ifndef MIN
#define MIN(A, B)       ((A) < (B) ? (A) : (B))
#endif
#ifndef MAX
#define MAX(A, B)       ((A) > (B) ? (A) : (B))
#endif

#ifndef SWAP
#define SWAP(a, b) \
    do \
    { \
        (a) ^= (b); \
        (b) ^= (a); \
        (a) ^= (b); \
    } while (0)
#endif

#ifdef ENABLE_DEBUG

#define DBGPRINTF(x)        printk x
#define ERRPRINTF(x)        printk x
#define LOGPRINTF(x)        printk x
#define ASSERT(x)           printk (x)

#else

#define ASSERT(x)           do {;} while (0)
#define DBGPRINTF(x)        do {;} while (0)
#define ERRPRINTF(x)        do {;} while (0)
#define LOGPRINTF(x)        do {;} while (0)

#endif  // ENABLE_DEBUG

#ifndef ASSERT
#define ASSERT(x)           MV_ASSERT(x)
#endif  // ASSERT

/*common definition*/
#define KB                  (0x400)
#define MB                  (0x100000)

#ifdef CPU_BIG_ENDIAN
#define FCC_GEN(a, b, c, d)     (((a)<<24) |((b)<<16) | ((c)<<8) | (d))
#else
#define FCC_GEN(a, b, c, d)     (((d)<<24) |((c)<<16) | ((b)<<8) | (a))
#endif

// Definitions for array operations
#define GaloisMemberIndexInArray(array, member) \
    (((unsigned long)(member) - (unsigned long)(array)) / sizeof(*(member)))

#ifdef __cplusplus
extern "C" {
#endif

// Get number of items in an array;
// The input parameter 'array' has to be a 'c' array, not a pointer or 'c++' array
#define GaloisSizeOfArray(array) (sizeof(array)/sizeof(array[0]))

#ifdef __cplusplus
}
#endif

#endif  // #ifndef __COMMON_TYPE__
