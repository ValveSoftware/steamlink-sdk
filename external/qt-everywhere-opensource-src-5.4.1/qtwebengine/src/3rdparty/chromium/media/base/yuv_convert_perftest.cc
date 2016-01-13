// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/cpu.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/time/time.h"
#include "media/base/simd/convert_yuv_to_rgb.h"
#include "media/base/yuv_convert.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"

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
      : yuv_bytes_(new uint8[kYUV12Size]),
        rgb_bytes_converted_(new uint8[kRGBSize]) {
    base::FilePath path;
    CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &path));
    path = path.Append(FILE_PATH_LITERAL("media"))
               .Append(FILE_PATH_LITERAL("test"))
               .Append(FILE_PATH_LITERAL("data"))
               .Append(FILE_PATH_LITERAL("bali_640x360_P420.yuv"));

    // Verify file size is correct.
    int64 actual_size = 0;
    base::GetFileSize(path, &actual_size);
    CHECK_EQ(actual_size, kYUV12Size);

    // Verify bytes read are correct.
    int bytes_read = base::ReadFile(
        path, reinterpret_cast<char*>(yuv_bytes_.get()), kYUV12Size);

    CHECK_EQ(bytes_read, kYUV12Size);
  }

  scoped_ptr<uint8[]> yuv_bytes_;
  scoped_ptr<uint8[]> rgb_bytes_converted_;

 private:
  DISALLOW_COPY_AND_ASSIGN(YUVConvertPerfTest);
};

TEST_F(YUVConvertPerfTest, ConvertYUVToRGB32Row_MMX) {
  ASSERT_TRUE(base::CPU().has_mmx());

  base::TimeTicks start = base::TimeTicks::HighResNow();
  for (int i = 0; i < kPerfTestIterations; ++i) {
    for (int row = 0; row < kSourceHeight; ++row) {
      int chroma_row = row / 2;
      ConvertYUVToRGB32Row_MMX(
          yuv_bytes_.get() + row * kSourceWidth,
          yuv_bytes_.get() + kSourceUOffset + (chroma_row * kSourceWidth / 2),
          yuv_bytes_.get() + kSourceVOffset + (chroma_row * kSourceWidth / 2),
          rgb_bytes_converted_.get(),
          kWidth,
          GetLookupTable(YV12));
    }
  }
  double total_time_seconds =
      (base::TimeTicks::HighResNow() - start).InSecondsF();
  perf_test::PrintResult(
      "yuv_convert_perftest", "", "ConvertYUVToRGB32Row_MMX",
      kPerfTestIterations / total_time_seconds, "runs/s", true);

  media::EmptyRegisterState();
}

TEST_F(YUVConvertPerfTest, ConvertYUVToRGB32Row_SSE) {
  ASSERT_TRUE(base::CPU().has_sse());

  base::TimeTicks start = base::TimeTicks::HighResNow();
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
  double total_time_seconds =
      (base::TimeTicks::HighResNow() - start).InSecondsF();
  perf_test::PrintResult(
      "yuv_convert_perftest", "", "ConvertYUVToRGB32Row_SSE",
      kPerfTestIterations / total_time_seconds, "runs/s", true);
  media::EmptyRegisterState();
}

TEST_F(YUVConvertPerfTest, ScaleYUVToRGB32Row_MMX) {
  ASSERT_TRUE(base::CPU().has_mmx());

  const int kSourceDx = 80000;  // This value means a scale down.

  base::TimeTicks start = base::TimeTicks::HighResNow();
  for (int i = 0; i < kPerfTestIterations; ++i) {
    for (int row = 0; row < kSourceHeight; ++row) {
      int chroma_row = row / 2;
      ScaleYUVToRGB32Row_MMX(
          yuv_bytes_.get() + row * kSourceWidth,
          yuv_bytes_.get() + kSourceUOffset + (chroma_row * kSourceWidth / 2),
          yuv_bytes_.get() + kSourceVOffset + (chroma_row * kSourceWidth / 2),
          rgb_bytes_converted_.get(),
          kWidth,
          kSourceDx,
          GetLookupTable(YV12));
    }
  }
  double total_time_seconds =
      (base::TimeTicks::HighResNow() - start).InSecondsF();
  perf_test::PrintResult(
      "yuv_convert_perftest", "", "ScaleYUVToRGB32Row_MMX",
      kPerfTestIterations / total_time_seconds, "runs/s", true);
  media::EmptyRegisterState();
}

TEST_F(YUVConvertPerfTest, ScaleYUVToRGB32Row_SSE) {
  ASSERT_TRUE(base::CPU().has_sse());

  const int kSourceDx = 80000;  // This value means a scale down.

  base::TimeTicks start = base::TimeTicks::HighResNow();
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
  double total_time_seconds =
      (base::TimeTicks::HighResNow() - start).InSecondsF();
  perf_test::PrintResult(
      "yuv_convert_perftest", "", "ScaleYUVToRGB32Row_SSE",
      kPerfTestIterations / total_time_seconds, "runs/s", true);
  media::EmptyRegisterState();
}

TEST_F(YUVConvertPerfTest, LinearScaleYUVToRGB32Row_MMX) {
  ASSERT_TRUE(base::CPU().has_mmx());

  const int kSourceDx = 80000;  // This value means a scale down.

  base::TimeTicks start = base::TimeTicks::HighResNow();
  for (int i = 0; i < kPerfTestIterations; ++i) {
    for (int row = 0; row < kSourceHeight; ++row) {
      int chroma_row = row / 2;
      LinearScaleYUVToRGB32Row_MMX(
          yuv_bytes_.get() + row * kSourceWidth,
          yuv_bytes_.get() + kSourceUOffset + (chroma_row * kSourceWidth / 2),
          yuv_bytes_.get() + kSourceVOffset + (chroma_row * kSourceWidth / 2),
          rgb_bytes_converted_.get(),
          kWidth,
          kSourceDx,
          GetLookupTable(YV12));
    }
  }
  double total_time_seconds =
      (base::TimeTicks::HighResNow() - start).InSecondsF();
  perf_test::PrintResult(
      "yuv_convert_perftest", "", "LinearScaleYUVToRGB32Row_MMX",
      kPerfTestIterations / total_time_seconds, "runs/s", true);
  media::EmptyRegisterState();
}

TEST_F(YUVConvertPerfTest, LinearScaleYUVToRGB32Row_SSE) {
  ASSERT_TRUE(base::CPU().has_sse());

  const int kSourceDx = 80000;  // This value means a scale down.

  base::TimeTicks start = base::TimeTicks::HighResNow();
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
  double total_time_seconds =
      (base::TimeTicks::HighResNow() - start).InSecondsF();
  perf_test::PrintResult(
      "yuv_convert_perftest", "", "LinearScaleYUVToRGB32Row_SSE",
      kPerfTestIterations / total_time_seconds, "runs/s", true);
  media::EmptyRegisterState();
}

#endif  // !defined(ARCH_CPU_ARM_FAMILY) && !defined(ARCH_CPU_MIPS_FAMILY)

}  // namespace media
