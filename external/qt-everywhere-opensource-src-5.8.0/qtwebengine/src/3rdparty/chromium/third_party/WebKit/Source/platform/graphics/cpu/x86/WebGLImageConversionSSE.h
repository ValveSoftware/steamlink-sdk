// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebGLImageConversionSSE_h
#define WebGLImageConversionSSE_h

#if CPU(X86) || CPU(X86_64)

#include <emmintrin.h>

namespace blink {

namespace SIMD {

ALWAYS_INLINE void unpackOneRowOfRGBA4444LittleToRGBA8(const uint16_t*& source, uint8_t*& destination, unsigned& pixelsPerRow)
{
    __m128i immediate0x0f = _mm_set1_epi16(0x0F);
    unsigned pixelsPerRowTrunc = (pixelsPerRow / 8) * 8;
    for (unsigned i = 0; i < pixelsPerRowTrunc; i += 8) {

        __m128i packedValue = _mm_loadu_si128((__m128i*)source);
        __m128i r = _mm_srli_epi16(packedValue, 12);
        __m128i g = _mm_and_si128(_mm_srli_epi16(packedValue, 8), immediate0x0f);
        __m128i b = _mm_and_si128(_mm_srli_epi16(packedValue, 4), immediate0x0f);
        __m128i a = _mm_and_si128(packedValue, immediate0x0f);
        __m128i componentR = _mm_or_si128(_mm_slli_epi16(r, 4), r);
        __m128i componentG = _mm_or_si128(_mm_slli_epi16(g, 4), g);
        __m128i componentB = _mm_or_si128(_mm_slli_epi16(b, 4), b);
        __m128i componentA = _mm_or_si128(_mm_slli_epi16(a, 4), a);

        __m128i componentRG = _mm_or_si128(_mm_slli_epi16(componentG, 8), componentR);
        __m128i componentBA = _mm_or_si128(_mm_slli_epi16(componentA, 8), componentB);
        __m128i componentRGBA1 = _mm_unpackhi_epi16(componentRG, componentBA);
        __m128i componentRGBA2 = _mm_unpacklo_epi16(componentRG, componentBA);

        _mm_storeu_si128((__m128i*)destination, componentRGBA2);
        _mm_storeu_si128((__m128i*)(destination + 16), componentRGBA1);

        source += 8;
        destination += 32;
    }
    pixelsPerRow -= pixelsPerRowTrunc;
}

ALWAYS_INLINE void unpackOneRowOfRGBA5551LittleToRGBA8(const uint16_t*& source, uint8_t*& destination, unsigned& pixelsPerRow)
{
    __m128i immediate0x1f = _mm_set1_epi16(0x1F);
    __m128i immediate0x7 = _mm_set1_epi16(0x7);
    __m128i immediate0x1 = _mm_set1_epi16(0x1);
    unsigned pixelsPerRowTrunc = (pixelsPerRow / 8) * 8;
    for (unsigned i = 0; i < pixelsPerRowTrunc; i += 8) {

        __m128i packedValue = _mm_loadu_si128((__m128i*)source);
        __m128i r = _mm_srli_epi16(packedValue, 11);
        __m128i g = _mm_and_si128(_mm_srli_epi16(packedValue, 6), immediate0x1f);
        __m128i b = _mm_and_si128(_mm_srli_epi16(packedValue, 1), immediate0x1f);
        __m128i componentR = _mm_or_si128(_mm_slli_epi16(r, 3), _mm_and_si128(r, immediate0x7));
        __m128i componentG = _mm_or_si128(_mm_slli_epi16(g, 3), _mm_and_si128(g, immediate0x7));
        __m128i componentB = _mm_or_si128(_mm_slli_epi16(b, 3), _mm_and_si128(b, immediate0x7));
        __m128i componentA = _mm_cmpeq_epi16(_mm_and_si128(packedValue, immediate0x1), immediate0x1);

        __m128i componentRG = _mm_or_si128(_mm_slli_epi16(componentG, 8), componentR);
        __m128i componentBA = _mm_or_si128(_mm_slli_epi16(componentA, 8), componentB);
        __m128i componentRGBA1 = _mm_unpackhi_epi16(componentRG, componentBA);
        __m128i componentRGBA2 = _mm_unpacklo_epi16(componentRG, componentBA);

        _mm_storeu_si128((__m128i*)destination, componentRGBA2);
        _mm_storeu_si128((__m128i*)(destination + 16), componentRGBA1);

        source += 8;
        destination += 32;
    }
    pixelsPerRow -= pixelsPerRowTrunc;
}

ALWAYS_INLINE void packOneRowOfRGBA8LittleToRA8(const uint8_t*& source, uint8_t*& destination, unsigned& pixelsPerRow)
{

    float tmp[4];
    unsigned pixelsPerRowTrunc = (pixelsPerRow / 4) * 4;
    const __m128 maxPixelValue = _mm_set1_ps(255.0);
    for (unsigned i = 0; i < pixelsPerRowTrunc; i += 4) {
        __m128 scaleFactor = _mm_set_ps(source[15] ? source[15] : 255.0,
            source[11] ? source[11] : 255.0,
            source[7] ? source[7] : 255.0,
            source[3] ? source[3] : 255.0);
        __m128 sourceR = _mm_set_ps(source[12], source[8], source[4], source[0]);

        sourceR = _mm_mul_ps(sourceR, _mm_div_ps(maxPixelValue, scaleFactor));

        _mm_storeu_ps(tmp, sourceR);

        destination[0] = static_cast<uint8_t>(tmp[0]);
        destination[1] = static_cast<uint8_t>(source[3]);
        destination[2] = static_cast<uint8_t>(tmp[1]);
        destination[3] = static_cast<uint8_t>(source[7]);
        destination[4] = static_cast<uint8_t>(tmp[2]);
        destination[5] = static_cast<uint8_t>(source[11]);
        destination[6] = static_cast<uint8_t>(tmp[3]);
        destination[7] = static_cast<uint8_t>(source[15]);

        source += 16;
        destination += 8;
    }
    pixelsPerRow -= pixelsPerRowTrunc;
}

ALWAYS_INLINE void unpackOneRowOfBGRA8LittleToRGBA8(const uint32_t*& source, uint32_t*& destination, unsigned& pixelsPerRow)
{
    __m128i bgra, rgba;
    __m128i brMask = _mm_set1_epi32(0x00ff00ff);
    __m128i gaMask = _mm_set1_epi32(0xff00ff00);
    unsigned pixelsPerRowTrunc = (pixelsPerRow / 4) * 4;

    for (unsigned i = 0; i < pixelsPerRowTrunc; i += 4) {
        bgra = _mm_loadu_si128((const __m128i*)(source));
        rgba = _mm_shufflehi_epi16(_mm_shufflelo_epi16(bgra, 0xB1), 0xB1);

        rgba = _mm_or_si128(_mm_and_si128(rgba, brMask), _mm_and_si128(bgra, gaMask));
        _mm_storeu_si128((__m128i*)(destination), rgba);

        source += 4;
        destination += 4;
    }

    pixelsPerRow -= pixelsPerRowTrunc;
}

ALWAYS_INLINE void packOneRowOfRGBA8LittleToR8(const uint8_t*& source, uint8_t*& destination, unsigned& pixelsPerRow)
{
    float tmp[4];
    unsigned pixelsPerRowTrunc = (pixelsPerRow / 4) * 4;

    for (unsigned i = 0; i < pixelsPerRowTrunc; i += 4) {
        __m128 scale = _mm_set_ps(source[15] ? source[15] : 255,
            source[11] ? source[11] : 255,
            source[7] ? source[7] : 255,
            source[3] ? source[3] : 255);

        __m128 sourceR = _mm_set_ps(source[12], source[8], source[4], source[0]);
        sourceR = _mm_mul_ps(sourceR, _mm_div_ps(_mm_set1_ps(255), scale));

        _mm_storeu_ps(tmp, sourceR);
        destination[0] = static_cast<uint8_t>(tmp[0]);
        destination[1] = static_cast<uint8_t>(tmp[1]);
        destination[2] = static_cast<uint8_t>(tmp[2]);
        destination[3] = static_cast<uint8_t>(tmp[3]);

        source += 16;
        destination += 4;
    }

    pixelsPerRow -= pixelsPerRowTrunc;
}

// This function deliberately doesn't mutate the incoming source, destination,
// or pixelsPerRow arguments, since it always handles the full row.
ALWAYS_INLINE void packOneRowOfRGBA8LittleToRGBA8(const uint8_t* source, uint8_t* destination, unsigned pixelsPerRow)
{
    float tmp[4];
    float scale;

    for (unsigned i = 0; i < pixelsPerRow; i++) {
        scale = source[3] ? source[3] : 255;
        __m128 sourceR = _mm_set_ps(0,
            source[2],
            source[1],
            source[0]);

        sourceR = _mm_mul_ps(sourceR, _mm_div_ps(_mm_set1_ps(255), _mm_set1_ps(scale)));

        _mm_storeu_ps(tmp, sourceR);
        destination[0] = static_cast<uint8_t>(tmp[0]);
        destination[1] = static_cast<uint8_t>(tmp[1]);
        destination[2] = static_cast<uint8_t>(tmp[2]);
        destination[3] = source[3];

        source += 4;
        destination += 4;
    }
}

} // namespace SIMD
} // namespace blink

#endif // CPU(X86) || CPU(X86_64)

#endif // WebGLImageConversionSSE_h
