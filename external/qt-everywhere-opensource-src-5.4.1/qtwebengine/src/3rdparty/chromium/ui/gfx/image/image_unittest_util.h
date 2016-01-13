// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Because the unit tests for gfx::Image are spread across multiple
// implementation files, this header contains the reusable components.

#ifndef UI_GFX_IMAGE_IMAGE_UNITTEST_UTIL_H_
#define UI_GFX_IMAGE_IMAGE_UNITTEST_UTIL_H_

#include "ui/gfx/image/image.h"
#include "third_party/skia/include/core/SkColor.h"

namespace gfx {
namespace test {

#if defined(OS_IOS)
typedef UIImage* PlatformImage;
#elif defined(OS_MACOSX)
typedef NSImage* PlatformImage;
#else
typedef gfx::ImageSkia PlatformImage;
#endif

std::vector<float> Get1xAnd2xScales();

// Create a bitmap of |width|x|height|.
const SkBitmap CreateBitmap(int width, int height);

// Creates an ImageSkia of |width|x|height| DIP with bitmap data for an
// arbitrary scale factor.
gfx::ImageSkia CreateImageSkia(int width, int height);

// Returns PNG encoded bytes for a bitmap of |edge_size|x|edge_size|.
scoped_refptr<base::RefCountedMemory> CreatePNGBytes(int edge_size);

// TODO(rohitrao): Remove the no-argument version of CreateImage().
gfx::Image CreateImage();
gfx::Image CreateImage(int width, int height);

// Returns true if the images are equal. Converts the images to ImageSkia to
// compare them.
bool IsEqual(const gfx::Image& image1, const gfx::Image& image2);

bool IsEqual(const SkBitmap& bitmap1, const SkBitmap& bitmap2);

bool IsEqual(const scoped_refptr<base::RefCountedMemory>& bytes,
             const SkBitmap& bitmap);

// An image which was not successfully decoded to PNG should be a red bitmap.
// Fails if the bitmap is not red.
void CheckImageIndicatesPNGDecodeFailure(const gfx::Image& image);

// Returns true if the structure of |image_skia| matches the structure
// described by |width|, |height|, and |scale_factors|.
// The structure matches if:
// - |image_skia| is non null.
// - |image_skia| has ImageSkiaReps of |scale_factors|.
// - Each of the ImageSkiaReps has a pixel size of |image_skia|.size() *
//   scale_factor.
bool ImageSkiaStructureMatches(
    const gfx::ImageSkia& image_skia,
    int width,
    int height,
    const std::vector<float>& scale_factors);

bool IsEmpty(const gfx::Image& image);

PlatformImage CreatePlatformImage();

gfx::Image::RepresentationType GetPlatformRepresentationType();

PlatformImage ToPlatformType(const gfx::Image& image);
PlatformImage CopyPlatformType(const gfx::Image& image);

SkColor GetPlatformImageColor(PlatformImage image, int x, int y);
void CheckColors(SkColor color1, SkColor color2);
void CheckIsTransparent(SkColor color);

bool IsPlatformImageValid(PlatformImage image);

bool PlatformImagesEqual(PlatformImage image1, PlatformImage image2);

}  // namespace test
}  // namespace gfx

#endif  // UI_GFX_IMAGE_IMAGE_UNITTEST_UTIL_H_
