// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <mmintrin.h>
#endif

#include "build/build_config.h"
#include "media/base/simd/filter_yuv.h"

namespace media {

#if defined(COMPILER_MSVC)
// Warning 4799 is about calling emms before the function exits.
// We calls emms in a frame level so suppress this warning.
#pragma warning(push)
#pragma warning(disable: 4799)
#endif

void FilterYUVRows_MMX(uint8* dest,
                       const uint8* src0,
                       const uint8* src1,
                       int width,
                       int fraction) {
  int pixel = 0;

  // Process the unaligned bytes first.
  int unaligned_width =
      (8 - (reinterpret_cast<uintptr_t>(dest) & 7)) & 7;
  while (pixel < width && pixel < unaligned_width) {
    dest[pixel] = (src0[pixel] * (256 - fraction) +
                   src1[pixel] * fraction) >> 8;
    ++pixel;
  }

  __m64 zero = _mm_setzero_si64();
  __m64 src1_fraction = _mm_set1_pi16(fraction);
  __m64 src0_fraction = _mm_set1_pi16(256 - fraction);
  const __m64* src0_64 = reinterpret_cast<const __m64*>(src0 + pixel);
  const __m64* src1_64 = reinterpret_cast<const __m64*>(src1 + pixel);
  __m64* dest64 = reinterpret_cast<__m64*>(dest + pixel);
  __m64* end64 = reinterpret_cast<__m64*>(
      reinterpret_cast<uintptr_t>(dest + width) & ~7);

  while (dest64 < end64) {
    __m64 src0 = *src0_64++;
    __m64 src1 = *src1_64++;
    __m64 src2 = _mm_unpackhi_pi8(src0, zero);
    __m64 src3 = _mm_unpackhi_pi8(src1, zero);
    src0 = _mm_unpacklo_pi8(src0, zero);
    src1 = _mm_unpacklo_pi8(src1, zero);
    src0 = _mm_mullo_pi16(src0, src0_fraction);
    src1 = _mm_mullo_pi16(src1, src1_fraction);
    src2 = _mm_mullo_pi16(src2, src0_fraction);
    src3 = _mm_mullo_pi16(src3, src1_fraction);
    src0 = _mm_add_pi16(src0, src1);
    src2 = _mm_add_pi16(src2, src3);
    src0 = _mm_srli_pi16(src0, 8);
    src2 = _mm_srli_pi16(src2, 8);
    src0 = _mm_packs_pu16(src0, src2);
    *dest64++ = src0;
    pixel += 8;
  }

  while (pixel < width) {
    dest[pixel] = (src0[pixel] * (256 - fraction) +
                   src1[pixel] * fraction) >> 8;
    ++pixel;
  }
}

#if defined(COMPILER_MSVC)
#pragma warning(pop)
#endif

}  // namespace media
