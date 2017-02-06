// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/platform_canvas.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "skia/ext/platform_device.h"
#include "third_party/skia/include/core/SkMetaData.h"
#include "third_party/skia/include/core/SkTypes.h"

namespace {

#if defined(OS_MACOSX)
const char kIsPreviewMetafileKey[] = "CrIsPreviewMetafile";

void SetBoolMetaData(const SkCanvas& canvas, const char* key,  bool value) {
  SkMetaData& meta = skia::GetMetaData(canvas);
  meta.setBool(key, value);
}

bool GetBoolMetaData(const SkCanvas& canvas, const char* key) {
  bool value;
  SkMetaData& meta = skia::GetMetaData(canvas);
  if (!meta.findBool(key, &value))
    value = false;
  return value;
}
#endif

}  // namespace

namespace skia {

SkBaseDevice* GetTopDevice(const SkCanvas& canvas) {
  return canvas.getTopDevice(true);
}

SkBitmap ReadPixels(SkCanvas* canvas) {
  SkBitmap bitmap;
  bitmap.setInfo(canvas->imageInfo());
  canvas->readPixels(&bitmap, 0, 0);
  return bitmap;
}

bool GetWritablePixels(SkCanvas* canvas, SkPixmap* result) {
  if (!canvas || !result) {
    return false;
  }

  SkImageInfo info;
  size_t row_bytes;
  void* pixels = canvas->accessTopLayerPixels(&info, &row_bytes);
  if (!pixels) {
    result->reset();
    return false;
  }

  result->reset(info, pixels, row_bytes);
  return true;
}

bool SupportsPlatformPaint(const SkCanvas* canvas) {
  return GetPlatformDevice(GetTopDevice(*canvas)) != nullptr;
}

size_t PlatformCanvasStrideForWidth(unsigned width) {
  return 4 * width;
}

SkCanvas* CreateCanvas(const sk_sp<SkBaseDevice>& device,
                       OnFailureType failureType) {
  if (!device) {
    if (CRASH_ON_FAILURE == failureType)
      SK_CRASH();
    return nullptr;
  }
  return new SkCanvas(device.get());
}

SkMetaData& GetMetaData(const SkCanvas& canvas) {
  SkBaseDevice* device = canvas.getDevice();
  DCHECK(device != nullptr);
  return device->getMetaData();
}

#if defined(OS_MACOSX)
void SetIsPreviewMetafile(const SkCanvas& canvas, bool is_preview) {
  SetBoolMetaData(canvas, kIsPreviewMetafileKey, is_preview);
}

bool IsPreviewMetafile(const SkCanvas& canvas) {
  return GetBoolMetaData(canvas, kIsPreviewMetafileKey);
}

CGContextRef GetBitmapContext(const SkCanvas& canvas) {
  SkBaseDevice* device = GetTopDevice(canvas);
  PlatformDevice* platform_device = GetPlatformDevice(device);
  SkIRect clip_bounds;
  canvas.getClipDeviceBounds(&clip_bounds);
  return platform_device ?
      platform_device->GetBitmapContext(
          canvas.getTotalMatrix(), clip_bounds) :
      nullptr;
}

#endif

ScopedPlatformPaint::ScopedPlatformPaint(SkCanvas* canvas) :
    canvas_(canvas),
    platform_surface_(nullptr) {
  // TODO(tomhudson) we're assuming non-null canvas?
  PlatformDevice* platform_device = GetPlatformDevice(GetTopDevice(*canvas));
  if (platform_device) {
    // Compensate for drawing to a layer rather than the entire canvas
    SkMatrix ctm;
    SkIRect clip_bounds;
    canvas->temporary_internal_describeTopLayer(&ctm, &clip_bounds);
    platform_surface_ = platform_device->BeginPlatformPaint(ctm, clip_bounds);
  }
}

}  // namespace skia
