// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Because the unit tests for gfx::Image are spread across multiple
// implementation files, this header contains the reusable components.

#include "ui/gfx/image/image_unittest_util.h"

#include <cmath>

#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"

#if defined(OS_IOS)
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "skia/ext/skia_utils_ios.h"
#elif defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "skia/ext/skia_utils_mac.h"
#endif

namespace gfx {
namespace test {

namespace {

bool ColorComponentsClose(SkColor component1, SkColor component2) {
  int c1 = static_cast<int>(component1);
  int c2 = static_cast<int>(component2);
  return std::abs(c1 - c2) <= 40;
}

bool ColorsClose(SkColor color1, SkColor color2) {
  // Be tolerant of floating point rounding and lossy color space conversions.
  return ColorComponentsClose(SkColorGetR(color1), SkColorGetR(color2)) &&
         ColorComponentsClose(SkColorGetG(color1), SkColorGetG(color2)) &&
         ColorComponentsClose(SkColorGetB(color1), SkColorGetB(color2)) &&
         ColorComponentsClose(SkColorGetA(color1), SkColorGetA(color2));
}

}  // namespace

std::vector<float> Get1xAnd2xScales() {
  std::vector<float> scales;
  scales.push_back(1.0f);
  scales.push_back(2.0f);
  return scales;
}

const SkBitmap CreateBitmap(int width, int height) {
  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, width, height);
  bitmap.allocPixels();
  bitmap.eraseARGB(255, 0, 255, 0);
  return bitmap;
}

gfx::ImageSkia CreateImageSkia(int width, int height) {
  return gfx::ImageSkia::CreateFrom1xBitmap(CreateBitmap(width, height));
}

scoped_refptr<base::RefCountedMemory> CreatePNGBytes(int edge_size) {
  SkBitmap bitmap = CreateBitmap(edge_size, edge_size);
  scoped_refptr<base::RefCountedBytes> bytes(new base::RefCountedBytes());
  PNGCodec::EncodeBGRASkBitmap(bitmap, false, &bytes->data());
  return bytes;
}

gfx::Image CreateImage() {
  return CreateImage(100, 50);
}

gfx::Image CreateImage(int width, int height) {
  return gfx::Image::CreateFrom1xBitmap(CreateBitmap(width, height));
}

bool IsEqual(const gfx::Image& img1, const gfx::Image& img2) {
  img1.AsImageSkia().EnsureRepsForSupportedScales();
  img2.AsImageSkia().EnsureRepsForSupportedScales();
  std::vector<gfx::ImageSkiaRep> img1_reps = img1.AsImageSkia().image_reps();
  gfx::ImageSkia image_skia2 = img2.AsImageSkia();
  if (image_skia2.image_reps().size() != img1_reps.size())
    return false;

  for (size_t i = 0; i < img1_reps.size(); ++i) {
    float scale = img1_reps[i].scale();
    const gfx::ImageSkiaRep& image_rep2 = image_skia2.GetRepresentation(scale);
    if (image_rep2.scale() != scale ||
        !IsEqual(img1_reps[i].sk_bitmap(), image_rep2.sk_bitmap())) {
      return false;
    }
  }
  return true;
}

bool IsEqual(const SkBitmap& bmp1, const SkBitmap& bmp2) {
  if (bmp1.isNull() && bmp2.isNull())
    return true;

  if (bmp1.width() != bmp2.width() ||
      bmp1.height() != bmp2.height() ||
      bmp1.config() != SkBitmap::kARGB_8888_Config ||
      bmp2.config() != SkBitmap::kARGB_8888_Config) {
    return false;
  }

  SkAutoLockPixels lock1(bmp1);
  SkAutoLockPixels lock2(bmp2);
  if (!bmp1.getPixels() || !bmp2.getPixels())
    return false;

  for (int y = 0; y < bmp1.height(); ++y) {
    for (int x = 0; x < bmp1.width(); ++x) {
      if (!ColorsClose(bmp1.getColor(x,y), bmp2.getColor(x,y)))
        return false;
    }
  }

  return true;
}

bool IsEqual(const scoped_refptr<base::RefCountedMemory>& bytes,
             const SkBitmap& bitmap) {
  SkBitmap decoded;
  if (!bytes.get() ||
      !PNGCodec::Decode(bytes->front(), bytes->size(), &decoded)) {
    return bitmap.isNull();
  }

  return IsEqual(bitmap, decoded);
}

void CheckImageIndicatesPNGDecodeFailure(const gfx::Image& image) {
  SkBitmap bitmap = image.AsBitmap();
  EXPECT_FALSE(bitmap.isNull());
  EXPECT_LE(16, bitmap.width());
  EXPECT_LE(16, bitmap.height());
  SkAutoLockPixels auto_lock(bitmap);
  CheckColors(bitmap.getColor(10, 10), SK_ColorRED);
}

bool ImageSkiaStructureMatches(
    const gfx::ImageSkia& image_skia,
    int width,
    int height,
    const std::vector<float>& scales) {
  if (image_skia.isNull() ||
      image_skia.width() != width ||
      image_skia.height() != height ||
      image_skia.image_reps().size() != scales.size()) {
    return false;
  }

  for (size_t i = 0; i < scales.size(); ++i) {
    gfx::ImageSkiaRep image_rep =
        image_skia.GetRepresentation(scales[i]);
    if (image_rep.is_null() || image_rep.scale() != scales[i])
      return false;

    if (image_rep.pixel_width() != static_cast<int>(width * scales[i]) ||
        image_rep.pixel_height() != static_cast<int>(height * scales[i])) {
      return false;
    }
  }
  return true;
}

bool IsEmpty(const gfx::Image& image) {
  const SkBitmap& bmp = *image.ToSkBitmap();
  return bmp.isNull() ||
         (bmp.width() == 0 && bmp.height() == 0);
}

PlatformImage CreatePlatformImage() {
  const SkBitmap bitmap(CreateBitmap(25, 25));
#if defined(OS_IOS)
  float scale = ImageSkia::GetMaxSupportedScale();

  base::ScopedCFTypeRef<CGColorSpaceRef> color_space(
      CGColorSpaceCreateDeviceRGB());
  UIImage* image =
      gfx::SkBitmapToUIImageWithColorSpace(bitmap, scale, color_space);
  base::mac::NSObjectRetain(image);
  return image;
#elif defined(OS_MACOSX)
  NSImage* image = gfx::SkBitmapToNSImage(bitmap);
  base::mac::NSObjectRetain(image);
  return image;
#else
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
#endif
}

gfx::Image::RepresentationType GetPlatformRepresentationType() {
#if defined(OS_IOS)
  return gfx::Image::kImageRepCocoaTouch;
#elif defined(OS_MACOSX)
  return gfx::Image::kImageRepCocoa;
#else
  return gfx::Image::kImageRepSkia;
#endif
}

PlatformImage ToPlatformType(const gfx::Image& image) {
#if defined(OS_IOS)
  return image.ToUIImage();
#elif defined(OS_MACOSX)
  return image.ToNSImage();
#else
  return image.AsImageSkia();
#endif
}

PlatformImage CopyPlatformType(const gfx::Image& image) {
#if defined(OS_IOS)
  return image.CopyUIImage();
#elif defined(OS_MACOSX)
  return image.CopyNSImage();
#else
  return image.AsImageSkia();
#endif
}

#if defined(OS_MACOSX)
// Defined in image_unittest_util_mac.mm.
#else
SkColor GetPlatformImageColor(PlatformImage image, int x, int y) {
  SkBitmap bitmap = *image.bitmap();
  SkAutoLockPixels auto_lock(bitmap);
  return bitmap.getColor(x, y);
}
#endif

void CheckColors(SkColor color1, SkColor color2) {
  EXPECT_TRUE(ColorsClose(color1, color2));
}

void CheckIsTransparent(SkColor color) {
  EXPECT_LT(SkColorGetA(color) / 255.0, 0.05);
}

bool IsPlatformImageValid(PlatformImage image) {
#if defined(OS_MACOSX)
  return image != NULL;
#else
  return !image.isNull();
#endif
}

bool PlatformImagesEqual(PlatformImage image1, PlatformImage image2) {
#if defined(OS_MACOSX)
  return image1 == image2;
#else
  return image1.BackedBySameObjectAs(image2);
#endif
}

}  // namespace test
}  // namespace gfx
