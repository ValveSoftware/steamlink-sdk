// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/snapshot/snapshot_async.h"

#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/numerics/safe_conversions.h"
#include "base/task_runner_util.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/skbitmap_operations.h"

namespace ui {

namespace {

void OnFrameScalingFinished(const GrabWindowSnapshotAsyncCallback& callback,
                            const SkBitmap& scaled_bitmap) {
  callback.Run(gfx::Image(gfx::ImageSkia::CreateFrom1xBitmap(scaled_bitmap)));
}

SkBitmap ScaleBitmap(const SkBitmap& input_bitmap,
                     const gfx::Size& target_size) {
  return skia::ImageOperations::Resize(input_bitmap,
                                       skia::ImageOperations::RESIZE_GOOD,
                                       target_size.width(),
                                       target_size.height(),
                                       static_cast<SkBitmap::Allocator*>(NULL));
}

scoped_refptr<base::RefCountedBytes> EncodeBitmap(const SkBitmap& bitmap) {
  scoped_refptr<base::RefCountedBytes> png_data(new base::RefCountedBytes);
  SkAutoLockPixels lock(bitmap);
  unsigned char* pixels = reinterpret_cast<unsigned char*>(bitmap.getPixels());
#if SK_A32_SHIFT == 24 && SK_R32_SHIFT == 16 && SK_G32_SHIFT == 8
  gfx::PNGCodec::ColorFormat kColorFormat = gfx::PNGCodec::FORMAT_BGRA;
#elif SK_A32_SHIFT == 24 && SK_B32_SHIFT == 16 && SK_G32_SHIFT == 8
  gfx::PNGCodec::ColorFormat kColorFormat = gfx::PNGCodec::FORMAT_RGBA;
#else
#error Unknown color format
#endif
  if (!gfx::PNGCodec::Encode(pixels,
                             kColorFormat,
                             gfx::Size(bitmap.width(), bitmap.height()),
                             base::checked_cast<int>(bitmap.rowBytes()),
                             true,
                             std::vector<gfx::PNGCodec::Comment>(),
                             &png_data->data())) {
    return scoped_refptr<base::RefCountedBytes>();
  }
  return png_data;
}

}  // namespace

void SnapshotAsync::ScaleCopyOutputResult(
    const GrabWindowSnapshotAsyncCallback& callback,
    const gfx::Size& target_size,
    scoped_refptr<base::TaskRunner> background_task_runner,
    std::unique_ptr<cc::CopyOutputResult> result) {
  if (result->IsEmpty()) {
    callback.Run(gfx::Image());
    return;
  }

  // TODO(sergeyu): Potentially images can be scaled on GPU before reading it
  // from GPU. Image scaling is implemented in content::GlHelper, but it's can't
  // be used here because it's not in content/public. Move the scaling code
  // somewhere so that it can be reused here.
  base::PostTaskAndReplyWithResult(
      background_task_runner.get(),
      FROM_HERE,
      base::Bind(ScaleBitmap, *result->TakeBitmap(), target_size),
      base::Bind(&OnFrameScalingFinished, callback));
}

void SnapshotAsync::EncodeCopyOutputResult(
    const GrabWindowSnapshotAsyncPNGCallback& callback,
    scoped_refptr<base::TaskRunner> background_task_runner,
    std::unique_ptr<cc::CopyOutputResult> result) {
  if (result->IsEmpty()) {
    callback.Run(scoped_refptr<base::RefCountedBytes>());
    return;
  }

  // TODO(sergeyu): Potentially images can be scaled on GPU before reading it
  // from GPU. Image scaling is implemented in content::GlHelper, but it's can't
  // be used here because it's not in content/public. Move the scaling code
  // somewhere so that it can be reused here.
  base::PostTaskAndReplyWithResult(
      background_task_runner.get(),
      FROM_HERE,
      base::Bind(EncodeBitmap, *result->TakeBitmap()),
      callback);
}

}  // namespace ui
