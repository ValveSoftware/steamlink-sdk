// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/simd/convert_rgb_to_yuv.h"

#include <stdint.h>

#include <memory>

#include "base/cpu.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Reference code that converts RGB pixels to YUV pixels.
int ConvertRGBToY(const uint8_t* rgb) {
  int y = 25 * rgb[0] + 129 * rgb[1] + 66 * rgb[2];
  y = ((y + 128) >> 8) + 16;
  return std::max(0, std::min(255, y));
}

int ConvertRGBToU(const uint8_t* rgb, int size) {
  int u = 112 * rgb[0] - 74 * rgb[1] - 38 * rgb[2];
  u = ((u + 128) >> 8) + 128;
  return std::max(0, std::min(255, u));
}

int ConvertRGBToV(const uint8_t* rgb, int size) {
  int v = -18 * rgb[0] - 94 * rgb[1] + 112 * rgb[2];
  v = ((v + 128) >> 8) + 128;
  return std::max(0, std::min(255, v));
}

}  // namespace

// Assembly code confuses MemorySanitizer. Do not run it in MSan builds.
#if defined(MEMORY_SANITIZER)
#define MAYBE_SideBySideRGB DISABLED_SideBySideRGB
#else
#define MAYBE_SideBySideRGB SideBySideRGB
#endif

// A side-by-side test that verifies our ASM functions that convert RGB pixels
// to YUV pixels can output the expected results. This test converts RGB pixels
// to YUV pixels with our ASM functions (which use SSE, SSE2, SSE3, and SSSE3)
// and compare the output YUV pixels with the ones calculated with out reference
// functions implemented in C++.
TEST(YUVConvertTest, MAYBE_SideBySideRGB) {
  // We skip this test on PCs which does not support SSE3 because this test
  // needs it.
  base::CPU cpu;
  if (!cpu.has_ssse3())
    return;

  // This test checks a subset of all RGB values so this test does not take so
  // long time.
  const int kStep = 8;
  const int kWidth = 256 / kStep;

  for (int size = 3; size <= 4; ++size) {
    // Create the output buffers.
    std::unique_ptr<uint8_t[]> rgb(new uint8_t[kWidth * size]);
    std::unique_ptr<uint8_t[]> y(new uint8_t[kWidth]);
    std::unique_ptr<uint8_t[]> u(new uint8_t[kWidth / 2]);
    std::unique_ptr<uint8_t[]> v(new uint8_t[kWidth / 2]);

    // Choose the function that converts from RGB pixels to YUV ones.
    void (*convert)(const uint8_t*, uint8_t*, uint8_t*, uint8_t*, int, int, int,
                    int, int) = NULL;
    if (size == 3)
      convert = media::ConvertRGB24ToYUV_SSSE3;
    else
      convert = media::ConvertRGB32ToYUV_SSSE3;

    int total_error = 0;
    for (int r = 0; r < kWidth; ++r) {
      for (int g = 0; g < kWidth; ++g) {

        // Fill the input pixels.
        for (int b = 0; b < kWidth; ++b) {
          rgb[b * size + 0] = b * kStep;
          rgb[b * size + 1] = g * kStep;
          rgb[b * size + 2] = r * kStep;
          if (size == 4)
            rgb[b * size + 3] = 255;
        }

        // Convert the input RGB pixels to YUV ones.
        convert(rgb.get(), y.get(), u.get(), v.get(), kWidth, 1, kWidth * size,
                kWidth, kWidth / 2);

        // Check the output Y pixels.
        for (int i = 0; i < kWidth; ++i) {
          const uint8_t* p = &rgb[i * size];
          int error = ConvertRGBToY(p) - y[i];
          total_error += error > 0 ? error : -error;
        }

        // Check the output U pixels.
        for (int i = 0; i < kWidth / 2; ++i) {
          const uint8_t* p = &rgb[i * 2 * size];
          int error = ConvertRGBToU(p, size) - u[i];
          total_error += error > 0 ? error : -error;
        }

        // Check the output V pixels.
        for (int i = 0; i < kWidth / 2; ++i) {
          const uint8_t* p = &rgb[i * 2 * size];
          int error = ConvertRGBToV(p, size) - v[i];
          total_error += error > 0 ? error : -error;
        }
      }
    }

    EXPECT_EQ(0, total_error);
  }
}
