// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/yuv_convert.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/base_paths.h"
#include "base/cpu.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "media/base/djb2.h"
#include "media/base/simd/convert_rgb_to_yuv.h"
#include "media/base/simd/convert_yuv_to_rgb.h"
#include "media/base/simd/filter_yuv.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

// Size of raw image.
static const int kSourceWidth = 640;
static const int kSourceHeight = 360;
static const int kSourceYSize = kSourceWidth * kSourceHeight;
static const int kSourceUOffset = kSourceYSize;
static const int kSourceVOffset = kSourceYSize * 5 / 4;
static const int kScaledWidth = 1024;
static const int kScaledHeight = 768;
static const int kDownScaledWidth = 512;
static const int kDownScaledHeight = 320;
static const int kBpp = 4;

// Surface sizes for various test files.
static const int kYUV12Size = kSourceYSize * 12 / 8;
static const int kYUV16Size = kSourceYSize * 16 / 8;
static const int kRGBSize = kSourceYSize * kBpp;
static const int kRGBSizeScaled = kScaledWidth * kScaledHeight * kBpp;
static const int kRGB24Size = kSourceYSize * 3;
static const int kRGBSizeConverted = kSourceYSize * kBpp;

#if !defined(ARCH_CPU_ARM_FAMILY) && !defined(ARCH_CPU_MIPS_FAMILY) && \
    !defined(OS_ANDROID)
static const int kSourceAOffset = kSourceYSize * 12 / 8;
static const int kYUVA12Size = kSourceYSize * 20 / 8;
#endif

// Helper for reading test data into a std::unique_ptr<uint8_t[]>.
static void ReadData(const base::FilePath::CharType* filename,
                     int expected_size,
                     std::unique_ptr<uint8_t[]>* data) {
  data->reset(new uint8_t[expected_size]);

  base::FilePath path;
  CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &path));
  path = path.Append(FILE_PATH_LITERAL("media"))
             .Append(FILE_PATH_LITERAL("test"))
             .Append(FILE_PATH_LITERAL("data"))
             .Append(filename);

  // Verify file size is correct.
  int64_t actual_size = 0;
  base::GetFileSize(path, &actual_size);
  CHECK_EQ(actual_size, expected_size);

  // Verify bytes read are correct.
  int bytes_read = base::ReadFile(
      path, reinterpret_cast<char*>(data->get()), expected_size);
  CHECK_EQ(bytes_read, expected_size);
}

static void ReadYV12Data(std::unique_ptr<uint8_t[]>* data) {
  ReadData(FILE_PATH_LITERAL("bali_640x360_P420.yuv"), kYUV12Size, data);
}

static void ReadYV16Data(std::unique_ptr<uint8_t[]>* data) {
  ReadData(FILE_PATH_LITERAL("bali_640x360_P422.yuv"), kYUV16Size, data);
}

#if !defined(ARCH_CPU_ARM_FAMILY) && !defined(ARCH_CPU_MIPS_FAMILY) && \
    !defined(OS_ANDROID)
static void ReadYV12AData(std::unique_ptr<uint8_t[]>* data) {
  ReadData(FILE_PATH_LITERAL("bali_640x360_P420_alpha.yuv"), kYUVA12Size, data);
}
#endif

static void ReadRGB24Data(std::unique_ptr<uint8_t[]>* data) {
  ReadData(FILE_PATH_LITERAL("bali_640x360_RGB24.rgb"), kRGB24Size, data);
}

#if defined(OS_ANDROID)
// Helper for swapping red and blue channels of RGBA or BGRA.
static void SwapRedAndBlueChannels(unsigned char* pixels, size_t buffer_size) {
  for (size_t i = 0; i < buffer_size; i += 4) {
    std::swap(pixels[i], pixels[i + 2]);
  }
}
#endif

namespace media {

TEST(YUVConvertTest, YV12) {
  // Allocate all surfaces.
  std::unique_ptr<uint8_t[]> yuv_bytes;
  std::unique_ptr<uint8_t[]> rgb_bytes(new uint8_t[kRGBSize]);
  std::unique_ptr<uint8_t[]> rgb_converted_bytes(
      new uint8_t[kRGBSizeConverted]);

  // Read YUV reference data from file.
  ReadYV12Data(&yuv_bytes);

  // Convert a frame of YUV to 32 bit ARGB.
  media::ConvertYUVToRGB32(yuv_bytes.get(),
                           yuv_bytes.get() + kSourceUOffset,
                           yuv_bytes.get() + kSourceVOffset,
                           rgb_converted_bytes.get(),            // RGB output
                           kSourceWidth, kSourceHeight,          // Dimensions
                           kSourceWidth,                         // YStride
                           kSourceWidth / 2,                     // UVStride
                           kSourceWidth * kBpp,                  // RGBStride
                           media::YV12);

#if defined(OS_ANDROID)
  SwapRedAndBlueChannels(rgb_converted_bytes.get(), kRGBSizeConverted);
#endif

  uint32_t rgb_hash =
      DJB2Hash(rgb_converted_bytes.get(), kRGBSizeConverted, kDJB2HashSeed);
  EXPECT_EQ(2413171226u, rgb_hash);
}

TEST(YUVConvertTest, YV16) {
  // Allocate all surfaces.
  std::unique_ptr<uint8_t[]> yuv_bytes;
  std::unique_ptr<uint8_t[]> rgb_bytes(new uint8_t[kRGBSize]);
  std::unique_ptr<uint8_t[]> rgb_converted_bytes(
      new uint8_t[kRGBSizeConverted]);

  // Read YUV reference data from file.
  ReadYV16Data(&yuv_bytes);

  // Convert a frame of YUV to 32 bit ARGB.
  media::ConvertYUVToRGB32(yuv_bytes.get(),                        // Y
                           yuv_bytes.get() + kSourceUOffset,       // U
                           yuv_bytes.get() + kSourceYSize * 3 / 2, // V
                           rgb_converted_bytes.get(),              // RGB output
                           kSourceWidth, kSourceHeight,            // Dimensions
                           kSourceWidth,                           // YStride
                           kSourceWidth / 2,                       // UVStride
                           kSourceWidth * kBpp,                    // RGBStride
                           media::YV16);

#if defined(OS_ANDROID)
  SwapRedAndBlueChannels(rgb_converted_bytes.get(), kRGBSizeConverted);
#endif

  uint32_t rgb_hash =
      DJB2Hash(rgb_converted_bytes.get(), kRGBSizeConverted, kDJB2HashSeed);
  EXPECT_EQ(4222342047u, rgb_hash);
}

struct YUVScaleTestData {
  YUVScaleTestData(media::YUVType y, media::ScaleFilter s, uint32_t r)
      : yuv_type(y), scale_filter(s), rgb_hash(r) {}

  media::YUVType yuv_type;
  media::ScaleFilter scale_filter;
  uint32_t rgb_hash;
};

class YUVScaleTest : public ::testing::TestWithParam<YUVScaleTestData> {
 public:
  YUVScaleTest() {
    switch (GetParam().yuv_type) {
      case media::YV12:
      case media::YV12J:
      case media::YV12HD:
        ReadYV12Data(&yuv_bytes_);
        break;
      case media::YV16:
        ReadYV16Data(&yuv_bytes_);
        break;
    }

    rgb_bytes_.reset(new uint8_t[kRGBSizeScaled]);
  }

  // Helpers for getting the proper Y, U and V plane offsets.
  uint8_t* y_plane() { return yuv_bytes_.get(); }
  uint8_t* u_plane() { return yuv_bytes_.get() + kSourceYSize; }
  uint8_t* v_plane() {
    switch (GetParam().yuv_type) {
      case media::YV12:
      case media::YV12J:
      case media::YV12HD:
        return yuv_bytes_.get() + kSourceVOffset;
      case media::YV16:
        return yuv_bytes_.get() + kSourceYSize * 3 / 2;
    }
    return NULL;
  }

  std::unique_ptr<uint8_t[]> yuv_bytes_;
  std::unique_ptr<uint8_t[]> rgb_bytes_;
};

TEST_P(YUVScaleTest, NoScale) {
  media::ScaleYUVToRGB32(y_plane(),                    // Y
                         u_plane(),                    // U
                         v_plane(),                    // V
                         rgb_bytes_.get(),             // RGB output
                         kSourceWidth, kSourceHeight,  // Dimensions
                         kSourceWidth, kSourceHeight,  // Dimensions
                         kSourceWidth,                 // YStride
                         kSourceWidth / 2,             // UvStride
                         kSourceWidth * kBpp,          // RgbStride
                         GetParam().yuv_type,
                         media::ROTATE_0,
                         GetParam().scale_filter);

  uint32_t yuv_hash = DJB2Hash(rgb_bytes_.get(), kRGBSize, kDJB2HashSeed);

  media::ConvertYUVToRGB32(y_plane(),                    // Y
                           u_plane(),                    // U
                           v_plane(),                    // V
                           rgb_bytes_.get(),             // RGB output
                           kSourceWidth, kSourceHeight,  // Dimensions
                           kSourceWidth,                 // YStride
                           kSourceWidth / 2,             // UVStride
                           kSourceWidth * kBpp,          // RGBStride
                           GetParam().yuv_type);

  uint32_t rgb_hash = DJB2Hash(rgb_bytes_.get(), kRGBSize, kDJB2HashSeed);

  EXPECT_EQ(yuv_hash, rgb_hash);
}

TEST_P(YUVScaleTest, Normal) {
  media::ScaleYUVToRGB32(y_plane(),                    // Y
                         u_plane(),                    // U
                         v_plane(),                    // V
                         rgb_bytes_.get(),             // RGB output
                         kSourceWidth, kSourceHeight,  // Dimensions
                         kScaledWidth, kScaledHeight,  // Dimensions
                         kSourceWidth,                 // YStride
                         kSourceWidth / 2,             // UvStride
                         kScaledWidth * kBpp,          // RgbStride
                         GetParam().yuv_type,
                         media::ROTATE_0,
                         GetParam().scale_filter);

#if defined(OS_ANDROID)
  SwapRedAndBlueChannels(rgb_bytes_.get(), kRGBSizeScaled);
#endif

  uint32_t rgb_hash = DJB2Hash(rgb_bytes_.get(), kRGBSizeScaled, kDJB2HashSeed);
  EXPECT_EQ(GetParam().rgb_hash, rgb_hash);
}

TEST_P(YUVScaleTest, ZeroSourceSize) {
  media::ScaleYUVToRGB32(y_plane(),                    // Y
                         u_plane(),                    // U
                         v_plane(),                    // V
                         rgb_bytes_.get(),             // RGB output
                         0, 0,                         // Dimensions
                         kScaledWidth, kScaledHeight,  // Dimensions
                         kSourceWidth,                 // YStride
                         kSourceWidth / 2,             // UvStride
                         kScaledWidth * kBpp,          // RgbStride
                         GetParam().yuv_type,
                         media::ROTATE_0,
                         GetParam().scale_filter);

  // Testing for out-of-bound read/writes with AddressSanitizer.
}

TEST_P(YUVScaleTest, ZeroDestinationSize) {
  media::ScaleYUVToRGB32(y_plane(),                    // Y
                         u_plane(),                    // U
                         v_plane(),                    // V
                         rgb_bytes_.get(),             // RGB output
                         kSourceWidth, kSourceHeight,  // Dimensions
                         0, 0,                         // Dimensions
                         kSourceWidth,                 // YStride
                         kSourceWidth / 2,             // UvStride
                         kScaledWidth * kBpp,          // RgbStride
                         GetParam().yuv_type,
                         media::ROTATE_0,
                         GetParam().scale_filter);

  // Testing for out-of-bound read/writes with AddressSanitizer.
}

TEST_P(YUVScaleTest, OddWidthAndHeightNotCrash) {
  media::ScaleYUVToRGB32(y_plane(),                    // Y
                         u_plane(),                    // U
                         v_plane(),                    // V
                         rgb_bytes_.get(),             // RGB output
                         kSourceWidth, kSourceHeight,  // Dimensions
                         3, 3,                         // Dimensions
                         kSourceWidth,                 // YStride
                         kSourceWidth / 2,             // UvStride
                         kScaledWidth * kBpp,          // RgbStride
                         GetParam().yuv_type,
                         media::ROTATE_0,
                         GetParam().scale_filter);
}

INSTANTIATE_TEST_CASE_P(
    YUVScaleFormats, YUVScaleTest,
    ::testing::Values(
        YUVScaleTestData(media::YV12, media::FILTER_NONE, 4136904952u),
        YUVScaleTestData(media::YV16, media::FILTER_NONE, 1501777547u),
        YUVScaleTestData(media::YV12, media::FILTER_BILINEAR, 3164274689u),
        YUVScaleTestData(media::YV16, media::FILTER_BILINEAR, 3095878046u)));

// This tests a known worst case YUV value, and for overflow.
TEST(YUVConvertTest, Clamp) {
  // Allocate all surfaces.
  std::unique_ptr<uint8_t[]> yuv_bytes(new uint8_t[1]);
  std::unique_ptr<uint8_t[]> rgb_bytes(new uint8_t[1]);
  std::unique_ptr<uint8_t[]> rgb_converted_bytes(new uint8_t[1]);

  // Values that failed previously in bug report.
  unsigned char y = 255u;
  unsigned char u = 255u;
  unsigned char v = 19u;

  // Prefill extra large destination buffer to test for overflow.
  unsigned char rgb[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
  unsigned char expected[8] = { 255, 255, 104, 255, 4, 5, 6, 7 };
  // Convert a frame of YUV to 32 bit ARGB.
  media::ConvertYUVToRGB32(&y,       // Y
                           &u,       // U
                           &v,       // V
                           &rgb[0],  // RGB output
                           1, 1,     // Dimensions
                           0,        // YStride
                           0,        // UVStride
                           0,        // RGBStride
                           media::YV12);

#if defined(OS_ANDROID)
  SwapRedAndBlueChannels(rgb, kBpp);
#endif

  int expected_test = memcmp(rgb, expected, sizeof(expected));
  EXPECT_EQ(0, expected_test);
}

TEST(YUVConvertTest, RGB24ToYUV) {
  // Allocate all surfaces.
  std::unique_ptr<uint8_t[]> rgb_bytes;
  std::unique_ptr<uint8_t[]> yuv_converted_bytes(new uint8_t[kYUV12Size]);

  // Read RGB24 reference data from file.
  ReadRGB24Data(&rgb_bytes);

  // Convert to I420.
  media::ConvertRGB24ToYUV(rgb_bytes.get(),
                           yuv_converted_bytes.get(),
                           yuv_converted_bytes.get() + kSourceUOffset,
                           yuv_converted_bytes.get() + kSourceVOffset,
                           kSourceWidth, kSourceHeight,        // Dimensions
                           kSourceWidth * 3,                   // RGBStride
                           kSourceWidth,                       // YStride
                           kSourceWidth / 2);                  // UVStride

  uint32_t rgb_hash =
      DJB2Hash(yuv_converted_bytes.get(), kYUV12Size, kDJB2HashSeed);
  EXPECT_EQ(320824432u, rgb_hash);
}

TEST(YUVConvertTest, RGB32ToYUV) {
  // Allocate all surfaces.
  std::unique_ptr<uint8_t[]> yuv_bytes(new uint8_t[kYUV12Size]);
  std::unique_ptr<uint8_t[]> rgb_bytes(new uint8_t[kRGBSize]);
  std::unique_ptr<uint8_t[]> yuv_converted_bytes(new uint8_t[kYUV12Size]);
  std::unique_ptr<uint8_t[]> rgb_converted_bytes(new uint8_t[kRGBSize]);

  // Read YUV reference data from file.
  base::FilePath yuv_url;
  EXPECT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &yuv_url));
  yuv_url = yuv_url.Append(FILE_PATH_LITERAL("media"))
                   .Append(FILE_PATH_LITERAL("test"))
                   .Append(FILE_PATH_LITERAL("data"))
                   .Append(FILE_PATH_LITERAL("bali_640x360_P420.yuv"));
  EXPECT_EQ(static_cast<int>(kYUV12Size),
            base::ReadFile(yuv_url,
                           reinterpret_cast<char*>(yuv_bytes.get()),
                           static_cast<int>(kYUV12Size)));

  // Convert a frame of YUV to 32 bit ARGB.
  media::ConvertYUVToRGB32(yuv_bytes.get(),
                           yuv_bytes.get() + kSourceUOffset,
                           yuv_bytes.get() + kSourceVOffset,
                           rgb_bytes.get(),            // RGB output
                           kSourceWidth, kSourceHeight,          // Dimensions
                           kSourceWidth,                         // YStride
                           kSourceWidth / 2,                     // UVStride
                           kSourceWidth * kBpp,                  // RGBStride
                           media::YV12);

  // Convert RGB32 to YV12.
  media::ConvertRGB32ToYUV(rgb_bytes.get(),
                           yuv_converted_bytes.get(),
                           yuv_converted_bytes.get() + kSourceUOffset,
                           yuv_converted_bytes.get() + kSourceVOffset,
                           kSourceWidth, kSourceHeight,        // Dimensions
                           kSourceWidth * 4,                   // RGBStride
                           kSourceWidth,                       // YStride
                           kSourceWidth / 2);                  // UVStride

  // Convert YV12 back to RGB32.
  media::ConvertYUVToRGB32(yuv_converted_bytes.get(),
                           yuv_converted_bytes.get() + kSourceUOffset,
                           yuv_converted_bytes.get() + kSourceVOffset,
                           rgb_converted_bytes.get(),            // RGB output
                           kSourceWidth, kSourceHeight,          // Dimensions
                           kSourceWidth,                         // YStride
                           kSourceWidth / 2,                     // UVStride
                           kSourceWidth * kBpp,                  // RGBStride
                           media::YV12);

  int error = 0;
  for (int i = 0; i < kRGBSize; ++i) {
    int diff = rgb_converted_bytes[i] - rgb_bytes[i];
    if (diff < 0)
      diff = -diff;
    error += diff;
  }

  // Make sure error is within bound.
  DVLOG(1) << "Average error per channel: " << error / kRGBSize;
  EXPECT_GT(5, error / kRGBSize);
}

TEST(YUVConvertTest, DownScaleYUVToRGB32WithRect) {
  // Read YUV reference data from file.
  base::FilePath yuv_url;
  EXPECT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &yuv_url));
  yuv_url = yuv_url.Append(FILE_PATH_LITERAL("media"))
                   .Append(FILE_PATH_LITERAL("test"))
                   .Append(FILE_PATH_LITERAL("data"))
                   .Append(FILE_PATH_LITERAL("bali_640x360_P420.yuv"));
  const size_t size_of_yuv = kSourceYSize * 12 / 8;  // 12 bpp.
  std::unique_ptr<uint8_t[]> yuv_bytes(new uint8_t[size_of_yuv]);
  EXPECT_EQ(static_cast<int>(size_of_yuv),
            base::ReadFile(yuv_url,
                           reinterpret_cast<char*>(yuv_bytes.get()),
                           static_cast<int>(size_of_yuv)));

  // Scale the full frame of YUV to 32 bit ARGB.
  // The API currently only supports down-scaling, so we don't test up-scaling.
  const size_t size_of_rgb_scaled = kDownScaledWidth * kDownScaledHeight * kBpp;
  std::unique_ptr<uint8_t[]> rgb_scaled_bytes(new uint8_t[size_of_rgb_scaled]);
  gfx::Rect sub_rect(0, 0, kDownScaledWidth, kDownScaledHeight);

  // We can't compare with the full-frame scaler because it uses slightly
  // different sampling coordinates.
  media::ScaleYUVToRGB32WithRect(
      yuv_bytes.get(),                          // Y
      yuv_bytes.get() + kSourceUOffset,         // U
      yuv_bytes.get() + kSourceVOffset,         // V
      rgb_scaled_bytes.get(),                   // Rgb output
      kSourceWidth, kSourceHeight,              // Dimensions
      kDownScaledWidth, kDownScaledHeight,      // Dimensions
      sub_rect.x(), sub_rect.y(),               // Dest rect
      sub_rect.right(), sub_rect.bottom(),      // Dest rect
      kSourceWidth,                             // YStride
      kSourceWidth / 2,                         // UvStride
      kDownScaledWidth * kBpp);                 // RgbStride

  uint32_t rgb_hash_full_rect =
      DJB2Hash(rgb_scaled_bytes.get(), size_of_rgb_scaled, kDJB2HashSeed);

  // Re-scale sub-rectangles and verify the results are the same.
  int next_sub_rect = 0;
  while (!sub_rect.IsEmpty()) {
    // Scale a partial rectangle.
    media::ScaleYUVToRGB32WithRect(
        yuv_bytes.get(),                          // Y
        yuv_bytes.get() + kSourceUOffset,         // U
        yuv_bytes.get() + kSourceVOffset,         // V
        rgb_scaled_bytes.get(),                   // Rgb output
        kSourceWidth, kSourceHeight,              // Dimensions
        kDownScaledWidth, kDownScaledHeight,      // Dimensions
        sub_rect.x(), sub_rect.y(),               // Dest rect
        sub_rect.right(), sub_rect.bottom(),      // Dest rect
        kSourceWidth,                             // YStride
        kSourceWidth / 2,                         // UvStride
        kDownScaledWidth * kBpp);                 // RgbStride
    uint32_t rgb_hash_sub_rect =
        DJB2Hash(rgb_scaled_bytes.get(), size_of_rgb_scaled, kDJB2HashSeed);

    EXPECT_EQ(rgb_hash_full_rect, rgb_hash_sub_rect);

    // Now pick choose a quarter rect of this sub-rect.
    if (next_sub_rect & 1)
      sub_rect.set_x(sub_rect.x() + sub_rect.width() / 2);
    if (next_sub_rect & 2)
      sub_rect.set_y(sub_rect.y() + sub_rect.height() / 2);
    sub_rect.set_width(sub_rect.width() / 2);
    sub_rect.set_height(sub_rect.height() / 2);
    next_sub_rect++;
  }
}

#if !defined(ARCH_CPU_ARM_FAMILY) && !defined(ARCH_CPU_MIPS_FAMILY)
#if !defined(OS_ANDROID)
TEST(YUVConvertTest, YUVAtoARGB_MMX_MatchReference) {
  // Allocate all surfaces.
  std::unique_ptr<uint8_t[]> yuv_bytes;
  std::unique_ptr<uint8_t[]> rgb_bytes(new uint8_t[kRGBSize]);
  std::unique_ptr<uint8_t[]> rgb_converted_bytes(
      new uint8_t[kRGBSizeConverted]);
  std::unique_ptr<uint8_t[]> rgb_converted_bytes_ref(
      new uint8_t[kRGBSizeConverted]);

  // Read YUV reference data from file.
  ReadYV12AData(&yuv_bytes);

  // Convert a frame of YUV to 32 bit ARGB using both C and MMX versions.
  media::ConvertYUVAToARGB_C(yuv_bytes.get(),
                             yuv_bytes.get() + kSourceUOffset,
                             yuv_bytes.get() + kSourceVOffset,
                             yuv_bytes.get() + kSourceAOffset,
                             rgb_converted_bytes_ref.get(),
                             kSourceWidth,
                             kSourceHeight,
                             kSourceWidth,
                             kSourceWidth / 2,
                             kSourceWidth,
                             kSourceWidth * kBpp,
                             media::YV12);
  media::ConvertYUVAToARGB_MMX(yuv_bytes.get(),
                               yuv_bytes.get() + kSourceUOffset,
                               yuv_bytes.get() + kSourceVOffset,
                               yuv_bytes.get() + kSourceAOffset,
                               rgb_converted_bytes.get(),
                               kSourceWidth,
                               kSourceHeight,
                               kSourceWidth,
                               kSourceWidth / 2,
                               kSourceWidth,
                               kSourceWidth * kBpp,
                               media::YV12);

  EXPECT_EQ(0,
            memcmp(rgb_converted_bytes.get(),
                   rgb_converted_bytes_ref.get(),
                   kRGBSizeConverted));
}
#endif  // !defined(OS_ANDROID)

TEST(YUVConvertTest, RGB32ToYUV_SSE2_MatchReference) {
  base::CPU cpu;
  if (!cpu.has_sse2()) {
    LOG(WARNING) << "System doesn't support SSE2, test not executed.";
    return;
  }

  // Allocate all surfaces.
  std::unique_ptr<uint8_t[]> yuv_bytes(new uint8_t[kYUV12Size]);
  std::unique_ptr<uint8_t[]> rgb_bytes(new uint8_t[kRGBSize]);
  std::unique_ptr<uint8_t[]> yuv_converted_bytes(new uint8_t[kYUV12Size]);
  std::unique_ptr<uint8_t[]> yuv_reference_bytes(new uint8_t[kYUV12Size]);

  ReadYV12Data(&yuv_bytes);

  // Convert a frame of YUV to 32 bit ARGB.
  media::ConvertYUVToRGB32(
      yuv_bytes.get(),
      yuv_bytes.get() + kSourceUOffset,
      yuv_bytes.get() + kSourceVOffset,
      rgb_bytes.get(),            // RGB output
      kSourceWidth, kSourceHeight,          // Dimensions
      kSourceWidth,                         // YStride
      kSourceWidth / 2,                     // UVStride
      kSourceWidth * kBpp,                  // RGBStride
      media::YV12);

  // Convert RGB32 to YV12 with SSE2 version.
  media::ConvertRGB32ToYUV_SSE2(
      rgb_bytes.get(),
      yuv_converted_bytes.get(),
      yuv_converted_bytes.get() + kSourceUOffset,
      yuv_converted_bytes.get() + kSourceVOffset,
      kSourceWidth, kSourceHeight,        // Dimensions
      kSourceWidth * 4,                   // RGBStride
      kSourceWidth,                       // YStride
      kSourceWidth / 2);                  // UVStride

  // Convert RGB32 to YV12 with reference version.
  media::ConvertRGB32ToYUV_SSE2_Reference(
      rgb_bytes.get(),
      yuv_reference_bytes.get(),
      yuv_reference_bytes.get() + kSourceUOffset,
      yuv_reference_bytes.get() + kSourceVOffset,
      kSourceWidth, kSourceHeight,        // Dimensions
      kSourceWidth * 4,                   // RGBStride
      kSourceWidth,                       // YStride
      kSourceWidth / 2);                  // UVStride

  // Now convert a odd width and height, this overrides part of the buffer
  // generated above but that is fine because the point of this test is to
  // match the result with the reference code.

  // Convert RGB32 to YV12 with SSE2 version.
  media::ConvertRGB32ToYUV_SSE2(
      rgb_bytes.get(),
      yuv_converted_bytes.get(),
      yuv_converted_bytes.get() + kSourceUOffset,
      yuv_converted_bytes.get() + kSourceVOffset,
      7, 7,                               // Dimensions
      kSourceWidth * 4,                   // RGBStride
      kSourceWidth,                       // YStride
      kSourceWidth / 2);                  // UVStride

  // Convert RGB32 to YV12 with reference version.
  media::ConvertRGB32ToYUV_SSE2_Reference(
      rgb_bytes.get(),
      yuv_reference_bytes.get(),
      yuv_reference_bytes.get() + kSourceUOffset,
      yuv_reference_bytes.get() + kSourceVOffset,
      7, 7,                               // Dimensions
      kSourceWidth * 4,                   // RGBStride
      kSourceWidth,                       // YStride
      kSourceWidth / 2);                  // UVStride

  int error = 0;
  for (int i = 0; i < kYUV12Size; ++i) {
    int diff = yuv_reference_bytes[i] - yuv_converted_bytes[i];
    if (diff < 0)
      diff = -diff;
    error += diff;
  }

  // Make sure there's no difference from the reference.
  EXPECT_EQ(0, error);
}

TEST(YUVConvertTest, ConvertYUVToRGB32Row_SSE) {
  base::CPU cpu;
  if (!cpu.has_sse()) {
    LOG(WARNING) << "System not supported. Test skipped.";
    return;
  }

  std::unique_ptr<uint8_t[]> yuv_bytes(new uint8_t[kYUV12Size]);
  std::unique_ptr<uint8_t[]> rgb_bytes_reference(new uint8_t[kRGBSize]);
  std::unique_ptr<uint8_t[]> rgb_bytes_converted(new uint8_t[kRGBSize]);
  ReadYV12Data(&yuv_bytes);

  const int kWidth = 167;
  ConvertYUVToRGB32Row_C(yuv_bytes.get(),
                         yuv_bytes.get() + kSourceUOffset,
                         yuv_bytes.get() + kSourceVOffset,
                         rgb_bytes_reference.get(),
                         kWidth,
                         GetLookupTable(YV12));
  ConvertYUVToRGB32Row_SSE(yuv_bytes.get(),
                           yuv_bytes.get() + kSourceUOffset,
                           yuv_bytes.get() + kSourceVOffset,
                           rgb_bytes_converted.get(),
                           kWidth,
                           GetLookupTable(YV12));
  media::EmptyRegisterState();
  EXPECT_EQ(0, memcmp(rgb_bytes_reference.get(),
                      rgb_bytes_converted.get(),
                      kWidth * kBpp));
}

// 64-bit release + component builds on Windows are too smart and optimizes
// away the function being tested.
#if defined(OS_WIN) && (defined(ARCH_CPU_X86) || !defined(COMPONENT_BUILD))
TEST(YUVConvertTest, ScaleYUVToRGB32Row_SSE) {
  base::CPU cpu;
  if (!cpu.has_sse()) {
    LOG(WARNING) << "System not supported. Test skipped.";
    return;
  }

  std::unique_ptr<uint8_t[]> yuv_bytes(new uint8_t[kYUV12Size]);
  std::unique_ptr<uint8_t[]> rgb_bytes_reference(new uint8_t[kRGBSize]);
  std::unique_ptr<uint8_t[]> rgb_bytes_converted(new uint8_t[kRGBSize]);
  ReadYV12Data(&yuv_bytes);

  const int kWidth = 167;
  const int kSourceDx = 80000;  // This value means a scale down.
  ScaleYUVToRGB32Row_C(yuv_bytes.get(),
                       yuv_bytes.get() + kSourceUOffset,
                       yuv_bytes.get() + kSourceVOffset,
                       rgb_bytes_reference.get(),
                       kWidth,
                       kSourceDx,
                       GetLookupTable(YV12));
  ScaleYUVToRGB32Row_SSE(yuv_bytes.get(),
                         yuv_bytes.get() + kSourceUOffset,
                         yuv_bytes.get() + kSourceVOffset,
                         rgb_bytes_converted.get(),
                         kWidth,
                         kSourceDx,
                         GetLookupTable(YV12));
  media::EmptyRegisterState();
  EXPECT_EQ(0, memcmp(rgb_bytes_reference.get(),
                      rgb_bytes_converted.get(),
                      kWidth * kBpp));
}

TEST(YUVConvertTest, LinearScaleYUVToRGB32Row_SSE) {
  base::CPU cpu;
  if (!cpu.has_sse()) {
    LOG(WARNING) << "System not supported. Test skipped.";
    return;
  }

  std::unique_ptr<uint8_t[]> yuv_bytes(new uint8_t[kYUV12Size]);
  std::unique_ptr<uint8_t[]> rgb_bytes_reference(new uint8_t[kRGBSize]);
  std::unique_ptr<uint8_t[]> rgb_bytes_converted(new uint8_t[kRGBSize]);
  ReadYV12Data(&yuv_bytes);

  const int kWidth = 167;
  const int kSourceDx = 80000;  // This value means a scale down.
  LinearScaleYUVToRGB32Row_C(yuv_bytes.get(),
                             yuv_bytes.get() + kSourceUOffset,
                             yuv_bytes.get() + kSourceVOffset,
                             rgb_bytes_reference.get(),
                             kWidth,
                             kSourceDx,
                             GetLookupTable(YV12));
  LinearScaleYUVToRGB32Row_SSE(yuv_bytes.get(),
                               yuv_bytes.get() + kSourceUOffset,
                               yuv_bytes.get() + kSourceVOffset,
                               rgb_bytes_converted.get(),
                               kWidth,
                               kSourceDx,
                               GetLookupTable(YV12));
  media::EmptyRegisterState();
  EXPECT_EQ(0, memcmp(rgb_bytes_reference.get(),
                      rgb_bytes_converted.get(),
                      kWidth * kBpp));
}
#endif  // defined(OS_WIN) && (ARCH_CPU_X86 || COMPONENT_BUILD)

TEST(YUVConvertTest, FilterYUVRows_C_OutOfBounds) {
  std::unique_ptr<uint8_t[]> src(new uint8_t[16]);
  std::unique_ptr<uint8_t[]> dst(new uint8_t[16]);

  memset(src.get(), 0xff, 16);
  memset(dst.get(), 0, 16);

  media::FilterYUVRows_C(dst.get(), src.get(), src.get(), 1, 255);

  EXPECT_EQ(255u, dst[0]);
  for (int i = 1; i < 16; ++i) {
    EXPECT_EQ(0u, dst[i]) << " not equal at " << i;
  }
}

TEST(YUVConvertTest, FilterYUVRows_SSE2_OutOfBounds) {
  base::CPU cpu;
  if (!cpu.has_sse2()) {
    LOG(WARNING) << "System not supported. Test skipped.";
    return;
  }

  std::unique_ptr<uint8_t[]> src(new uint8_t[16]);
  std::unique_ptr<uint8_t[]> dst(new uint8_t[16]);

  memset(src.get(), 0xff, 16);
  memset(dst.get(), 0, 16);

  media::FilterYUVRows_SSE2(dst.get(), src.get(), src.get(), 1, 255);

  EXPECT_EQ(255u, dst[0]);
  for (int i = 1; i < 16; ++i) {
    EXPECT_EQ(0u, dst[i]);
  }
}

TEST(YUVConvertTest, FilterYUVRows_SSE2_UnalignedDestination) {
  base::CPU cpu;
  if (!cpu.has_sse2()) {
    LOG(WARNING) << "System not supported. Test skipped.";
    return;
  }

  const int kSize = 64;
  std::unique_ptr<uint8_t[]> src(new uint8_t[kSize]);
  std::unique_ptr<uint8_t[]> dst_sample(new uint8_t[kSize]);
  std::unique_ptr<uint8_t[]> dst(new uint8_t[kSize]);

  memset(dst_sample.get(), 0, kSize);
  memset(dst.get(), 0, kSize);
  for (int i = 0; i < kSize; ++i)
    src[i] = 100 + i;

  media::FilterYUVRows_C(dst_sample.get(),
                         src.get(), src.get(), 37, 128);

  // Generate an unaligned output address.
  uint8_t* dst_ptr = reinterpret_cast<uint8_t*>(
      (reinterpret_cast<uintptr_t>(dst.get() + 16) & ~15) + 1);
  media::FilterYUVRows_SSE2(dst_ptr, src.get(), src.get(), 37, 128);
  media::EmptyRegisterState();

  EXPECT_EQ(0, memcmp(dst_sample.get(), dst_ptr, 37));
}

#if defined(ARCH_CPU_X86_64)

TEST(YUVConvertTest, ScaleYUVToRGB32Row_SSE2_X64) {
  std::unique_ptr<uint8_t[]> yuv_bytes(new uint8_t[kYUV12Size]);
  std::unique_ptr<uint8_t[]> rgb_bytes_reference(new uint8_t[kRGBSize]);
  std::unique_ptr<uint8_t[]> rgb_bytes_converted(new uint8_t[kRGBSize]);
  ReadYV12Data(&yuv_bytes);

  const int kWidth = 167;
  const int kSourceDx = 80000;  // This value means a scale down.
  ScaleYUVToRGB32Row_C(yuv_bytes.get(),
                       yuv_bytes.get() + kSourceUOffset,
                       yuv_bytes.get() + kSourceVOffset,
                       rgb_bytes_reference.get(),
                       kWidth,
                       kSourceDx,
                       GetLookupTable(YV12));
  ScaleYUVToRGB32Row_SSE2_X64(yuv_bytes.get(),
                              yuv_bytes.get() + kSourceUOffset,
                              yuv_bytes.get() + kSourceVOffset,
                              rgb_bytes_converted.get(),
                              kWidth,
                              kSourceDx,
                              GetLookupTable(YV12));
  media::EmptyRegisterState();
  EXPECT_EQ(0, memcmp(rgb_bytes_reference.get(),
                      rgb_bytes_converted.get(),
                      kWidth * kBpp));
}

TEST(YUVConvertTest, LinearScaleYUVToRGB32Row_MMX_X64) {
  std::unique_ptr<uint8_t[]> yuv_bytes(new uint8_t[kYUV12Size]);
  std::unique_ptr<uint8_t[]> rgb_bytes_reference(new uint8_t[kRGBSize]);
  std::unique_ptr<uint8_t[]> rgb_bytes_converted(new uint8_t[kRGBSize]);
  ReadYV12Data(&yuv_bytes);

  const int kWidth = 167;
  const int kSourceDx = 80000;  // This value means a scale down.
  LinearScaleYUVToRGB32Row_C(yuv_bytes.get(),
                             yuv_bytes.get() + kSourceUOffset,
                             yuv_bytes.get() + kSourceVOffset,
                             rgb_bytes_reference.get(),
                             kWidth,
                             kSourceDx,
                             GetLookupTable(YV12));
  LinearScaleYUVToRGB32Row_MMX_X64(yuv_bytes.get(),
                                   yuv_bytes.get() + kSourceUOffset,
                                   yuv_bytes.get() + kSourceVOffset,
                                   rgb_bytes_converted.get(),
                                   kWidth,
                                   kSourceDx,
                                   GetLookupTable(YV12));
  media::EmptyRegisterState();
  EXPECT_EQ(0, memcmp(rgb_bytes_reference.get(),
                      rgb_bytes_converted.get(),
                      kWidth * kBpp));
}

#endif  // defined(ARCH_CPU_X86_64)

#endif  // defined(ARCH_CPU_X86_FAMILY)

}  // namespace media
