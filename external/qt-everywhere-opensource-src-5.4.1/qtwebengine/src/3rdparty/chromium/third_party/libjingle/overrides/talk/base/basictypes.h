// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file overrides the inclusion of talk/base/basictypes.h to remove
// collisions with Chromium's base/basictypes.h.   We then add back a few
// items that Chromium's version doesn't provide, but libjingle expects.

#ifndef OVERRIDES_TALK_BASE_BASICTYPES_H__
#define OVERRIDES_TALK_BASE_BASICTYPES_H__

#include "base/basictypes.h"
#include "build/build_config.h"

#ifndef INT_TYPES_DEFINED
#define INT_TYPES_DEFINED

#ifdef COMPILER_MSVC
#if _MSC_VER >= 1600
#include <stdint.h>
#else
typedef unsigned __int64 uint64;
typedef __int64 int64;
#endif
#ifndef INT64_C
#define INT64_C(x) x ## I64
#endif
#ifndef UINT64_C
#define UINT64_C(x) x ## UI64
#endif
#define INT64_F "I64"
#else  // COMPILER_MSVC
#ifndef INT64_C
#define INT64_C(x) x ## LL
#endif
#ifndef UINT64_C
#define UINT64_C(x) x ## ULL
#endif
#ifndef INT64_F
#define INT64_F "ll"
#endif
#endif  // COMPILER_MSVC
#endif  // INT_TYPES_DEFINED

// Detect compiler is for x86 or x64.
#if defined(__x86_64__) || defined(_M_X64) || \
    defined(__i386__) || defined(_M_IX86)
#define CPU_X86 1
#endif
// Detect compiler is for arm.
#if defined(__arm__) || defined(_M_ARM)
#define CPU_ARM 1
#endif
#if defined(CPU_X86) && defined(CPU_ARM)
#error CPU_X86 and CPU_ARM both defined.
#endif
#if !defined(ARCH_CPU_BIG_ENDIAN) && !defined(ARCH_CPU_LITTLE_ENDIAN)
// x86, arm or GCC provided __BYTE_ORDER__ macros
#if CPU_X86 || CPU_ARM ||  \
  (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#define ARCH_CPU_LITTLE_ENDIAN
#elif defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define ARCH_CPU_BIG_ENDIAN
#else
#error ARCH_CPU_BIG_ENDIAN or ARCH_CPU_LITTLE_ENDIAN should be defined.
#endif
#endif
#if defined(ARCH_CPU_BIG_ENDIAN) && defined(ARCH_CPU_LITTLE_ENDIAN)
#error ARCH_CPU_BIG_ENDIAN and ARCH_CPU_LITTLE_ENDIAN both defined.
#endif

#ifdef WIN32
typedef int socklen_t;
#endif

namespace talk_base {
template<class T> inline T _min(T a, T b) { return (a > b) ? b : a; }
template<class T> inline T _max(T a, T b) { return (a < b) ? b : a; }

// For wait functions that take a number of milliseconds, kForever indicates
// unlimited time.
const int kForever = -1;
}

#ifdef WIN32
#if _MSC_VER < 1700
  #define alignof(t) __alignof(t)
#endif
#else  // !WIN32
#define alignof(t) __alignof__(t)
#endif  // !WIN32
#define IS_ALIGNED(p, a) (0==(reinterpret_cast<uintptr_t>(p) & ((a)-1)))
#define ALIGNP(p, t) \
  (reinterpret_cast<uint8*>(((reinterpret_cast<uintptr_t>(p) + \
  ((t)-1)) & ~((t)-1))))

// LIBJINGLE_DEFINE_STATIC_LOCAL() is a libjingle's copy
// of CR_DEFINE_STATIC_LOCAL().
#define LIBJINGLE_DEFINE_STATIC_LOCAL(type, name, arguments) \
  CR_DEFINE_STATIC_LOCAL(type, name, arguments)

#endif // OVERRIDES_TALK_BASE_BASICTYPES_H__
