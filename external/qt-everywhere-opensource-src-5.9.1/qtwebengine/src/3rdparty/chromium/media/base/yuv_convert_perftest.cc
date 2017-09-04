// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>

#include "base/base_paths.h"
#include "base/cpu.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/base/simd/convert_yuv_to_rgb.h"
#include "media/base/yuv_convert.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"
#include "third_party/libyuv/include/libyuv/row.h"

namespace media {
#if !defined(ARCH_CPU_ARM_FAMILY) && !defined(ARCH_CPU_MIPS_FAMILY)
// Size of raw image.
static const int kSourceWidth = 640;
static const int kSourceHeight = 360;
static const int kSourceYSize = kSourceWidth * kSourceHeight;
static const int kSourceUOffset = kSourceYSize;
static const int kSourceVOffset = kSourceYSize * 5 / 4;
static const int kBpp = 4;

// Width of the row to convert. Odd so that we exercise the ending
// one-pixel-leftover case.
static const int kWidth = 639;

// Surface sizes for various test files.
static const int kYUV12Size = kSourceYSize * 12 / 8;
static const int kRGBSize = kSourceYSize * kBpp;

static const int kPerfTestIterations = 2000;

class YUVConvertPerfTest : public testing::Test {
 public:
  YUVConvertPerfTest()
      : yuv_bytes_(new uint8_t[kYUV12Size]),
        rgb_bytes_converted_(new uint8_t[kRGBSize]) {
    base::FilePath path;
    CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &path));
    path = path.Append(FILE_PATH_LITERAL("media"))
               .Append(FILE_PATH_LITERAL("test"))
               .Append(FILE_PATH_LITERAL("data"))
               .Append(FILE_PATH_LITERAL("bali_640x360_P420.yuv"));

    // Verify file size is correct.
    int64_t actual_size = 0;
    base::GetFileSize(path, &actual_size);
    CHECK_EQ(actual_size, kYUV12Size);

    // Verify bytes read are correct.
    int bytes_read = base::ReadFile(
        path, reinterpret_cast<char*>(yuv_bytes_.get()), kYUV12Size);

    CHECK_EQ(bytes_read, kYUV12Size);
  }

  std::unique_ptr<uint8_t[]> yuv_bytes_;
  std::unique_ptr<uint8_t[]> rgb_bytes_converted_;

 private:
  DISALLOW_COPY_AND_ASSIGN(YUVConvertPerfTest);
};

TEST_F(YUVConvertPerfTest, ConvertYUVToRGB32Row_SSE) {
  ASSERT_TRUE(base::CPU().has_sse());

  base::TimeTicks start = base::TimeTicks::Now();
  for (int i = 0; i < kPerfTestIterations; ++i) {
    for (int row = 0; row < kSourceHeight; ++row) {
      int chroma_row = row / 2;
      ConvertYUVToRGB32Row_SSE(
          yuv_bytes_.get() + row * kSourceWidth,
          yuv_bytes_.get() + kSourceUOffset + (chroma_row * kSourceWidth / 2),
          yuv_bytes_.get() + kSourceVOffset + (chroma_row * kSourceWidth / 2),
          rgb_bytes_converted_.get(),
          kWidth,
          GetLookupTable(YV12));
    }
  }
  media::EmptyRegisterState();
  double total_time_seconds = (base::TimeTicks::Now() - start).InSecondsF();
  perf_test::PrintResult(
      "yuv_convert_perftest", "", "ConvertYUVToRGB32Row_SSE",
      kPerfTestIterations / total_time_seconds, "runs/s", true);
}

#ifdef HAS_I422TOARGBROW_SSSE3
TEST_F(YUVConvertPerfTest, I422ToARGBRow_SSSE3) {
  ASSERT_TRUE(base::CPU().has_ssse3());

  base::TimeTicks start = base::TimeTicks::Now();
  for (int i = 0; i < kPerfTestIterations; ++i) {
    for (int row = 0; row < kSourceHeight; ++row) {
      int chroma_row = row / 2;
      libyuv::I422ToARGBRow_SSSE3(
          yuv_bytes_.get() + row * kSourceWidth,
          yuv_bytes_.get() + kSourceUOffset + (chroma_row * kSourceWidth / 2),
          yuv_bytes_.get() + kSourceVOffset + (chroma_row * kSourceWidth / 2),
          rgb_bytes_converted_.get(), &libyuv::kYuvI601Constants, kWidth);
    }
  }
  double total_time_seconds = (base::TimeTicks::Now() - start).InSecondsF();
  perf_test::PrintResult("yuv_convert_perftest", "", "I422ToARGBRow_SSSE3",
                         kPerfTestIterations / total_time_seconds, "runs/s",
                         true);
}
#endif

TEST_F(YUVConvertPerfTest, ConvertYUVAToARGBRow_MMX) {
  ASSERT_TRUE(base::CPU().has_sse());

  base::TimeTicks start = base::TimeTicks::Now();
  for (int i = 0; i < kPerfTestIterations; ++i) {
    for (int row = 0; row < kSourceHeight; ++row) {
      int chroma_row = row / 2;
      ConvertYUVAToARGBRow_MMX(
          yuv_bytes_.get() + row * kSourceWidth,
          yuv_bytes_.get() + kSourceUOffset + (chroma_row * kSourceWidth / 2),
          yuv_bytes_.get() + kSourceVOffset + (chroma_row * kSourceWidth / 2),
          yuv_bytes_.get() + row * kSourceWidth,  // hack: use luma for alpha
          rgb_bytes_converted_.get(), kWidth, GetLookupTable(YV12));
    }
  }
  media::EmptyRegisterState();
  double total_time_seconds = (base::TimeTicks::Now() - start).InSecondsF();
  perf_test::PrintResult("yuv_convert_perftest", "", "ConvertYUVAToARGBRow_MMX",
                         kPerfTestIterations / total_time_seconds, "runs/s",
                         true);
}

#ifdef HAS_I422ALPHATOARGBROW_SSSE3
TEST_F(YUVConvertPerfTest, I422AlphaToARGBRow_SSSE3) {
  ASSERT_TRUE(base::CPU().has_ssse3());

  base::TimeTicks start = base::TimeTicks::Now();
  for (int i = 0; i < kPerfTestIterations; ++i) {
    for (int row = 0; row < kSourceHeight; ++row) {
      int chroma_row = row / 2;
      libyuv::I422AlphaToARGBRow_SSSE3(
          yuv_bytes_.get() + row * kSourceWidth,
          yuv_bytes_.get() + kSourceUOffset + (chroma_row * kSourceWidth / 2),
          yuv_bytes_.get() + kSourceVOffset + (chroma_row * kSourceWidth / 2),
          yuv_bytes_.get() + row * kSourceWidth,  // hack: use luma for alpha
          rgb_bytes_converted_.get(), &libyuv::kYuvI601Constants, kWidth);
    }
  }
  double total_time_seconds = (base::TimeTicks::Now() - start).InSecondsF();
  perf_test::PrintResult("yuv_convert_perftest", "", "I422AlphaToARGBRow_SSSE3",
                         kPerfTestIterations / total_time_seconds, "runs/s",
                         true);
}
#endif

// 64-bit release + component builds on Windows are too smart and optimizes
// away the function being tested.
#if defined(OS_WIN) && (defined(ARCH_CPU_X86) || !defined(COMPONENT_BUILD))
TEST_F(YUVConvertPerfTest, ScaleYUVToRGB32Row_SSE) {
  ASSERT_TRUE(base::CPU().has_sse());

  const int kSourceDx = 80000;  // This value means a scale down.

  base::TimeTicks start = base::TimeTicks::Now();
  for (int i = 0; i < kPerfTestIterations; ++i) {
    for (int row = 0; row < kSourceHeight; ++row) {
      int chroma_row = row / 2;
      ScaleYUVToRGB32Row_SSE(
          yuv_bytes_.get() + row * kSourceWidth,
          yuv_bytes_.get() + kSourceUOffset + (chroma_row * kSourceWidth / 2),
          yuv_bytes_.get() + kSourceVOffset + (chroma_row * kSourceWidth / 2),
          rgb_bytes_converted_.get(),
          kWidth,
          kSourceDx,
          GetLookupTable(YV12));
    }
  }
  media::EmptyRegisterState();
  double total_time_seconds = (base::TimeTicks::Now() - start).InSecondsF();
  perf_test::PrintResult(
      "yuv_convert_perftest", "", "ScaleYUVToRGB32Row_SSE",
      kPerfTestIterations / total_time_seconds, "runs/s", true);
}

TEST_F(YUVConvertPerfTest, LinearScaleYUVToRGB32Row_SSE) {
  ASSERT_TRUE(base::CPU().has_sse());

  const int kSourceDx = 80000;  // This value means a scale down.

  base::TimeTicks start = base::TimeTicks::Now();
  for (int i = 0; i < kPerfTestIterations; ++i) {
    for (int row = 0; row < kSourceHeight; ++row) {
      int chroma_row = row / 2;
      LinearScaleYUVToRGB32Row_SSE(
          yuv_bytes_.get() + row * kSourceWidth,
          yuv_bytes_.get() + kSourceUOffset + (chroma_row * kSourceWidth / 2),
          yuv_bytes_.get() + kSourceVOffset + (chroma_row * kSourceWidth / 2),
          rgb_bytes_converted_.get(),
          kWidth,
          kSourceDx,
          GetLookupTable(YV12));
    }
  }
  media::EmptyRegisterState();
  double total_time_seconds = (base::TimeTicks::Now() - start).InSecondsF();
  perf_test::PrintResult(
      "yuv_convert_perftest", "", "LinearScaleYUVToRGB32Row_SSE",
      kPerfTestIterations / total_time_seconds, "runs/s", true);
}
#endif  // defined(OS_WIN) && (ARCH_CPU_X86 || COMPONENT_BUILD)

#endif  // !defined(ARCH_CPU_ARM_FAMILY) && !defined(ARCH_CPU_MIPS_FAMILY)

}  // namespace media
