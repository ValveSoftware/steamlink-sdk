// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/bitmap_platform_device_mac.h"

#include <memory>

#include "skia/ext/platform_canvas.h"
#include "skia/ext/skia_utils_mac.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkClipStack.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkRect.h"

namespace skia {

const int kWidth = 400;
const int kHeight = 300;

class BitmapPlatformDeviceMacTest : public testing::Test {
 public:
  BitmapPlatformDeviceMacTest() {
    bitmap_.reset(BitmapPlatformDevice::Create(
        NULL, kWidth, kHeight, /*is_opaque=*/true));
  }

  sk_sp<BitmapPlatformDevice> bitmap_;
};

TEST_F(BitmapPlatformDeviceMacTest, ClipRectTransformWithTranslate) {
  SkMatrix transform;
  transform.setTranslate(50, 140);

  sk_sp<SkCanvas> canvas(skia::CreateCanvas(bitmap_, CRASH_ON_FAILURE));
  canvas->setMatrix(transform);

  ScopedPlatformPaint p(canvas.get());
  CGContextRef context = p.GetPlatformSurface();

  SkRect clip_rect = skia::CGRectToSkRect(CGContextGetClipBoundingBox(context));
  transform.mapRect(&clip_rect);
  EXPECT_EQ(0, clip_rect.fLeft);
  EXPECT_EQ(0, clip_rect.fTop);
  EXPECT_EQ(kWidth, clip_rect.width());
  EXPECT_EQ(kHeight, clip_rect.height());
}

TEST_F(BitmapPlatformDeviceMacTest, ClipRectTransformWithScale) {
  SkMatrix transform;
  transform.setScale(0.5, 0.5);

  sk_sp<SkCanvas> canvas(skia::CreateCanvas(bitmap_, CRASH_ON_FAILURE));
  canvas->setMatrix(transform);

  ScopedPlatformPaint p(canvas.get());
  CGContextRef context = p.GetPlatformSurface();

  SkRect clip_rect = skia::CGRectToSkRect(CGContextGetClipBoundingBox(context));
  transform.mapRect(&clip_rect);
  EXPECT_EQ(0, clip_rect.fLeft);
  EXPECT_EQ(0, clip_rect.fTop);
  EXPECT_EQ(kWidth, clip_rect.width());
  EXPECT_EQ(kHeight, clip_rect.height());
}

}  // namespace skia
