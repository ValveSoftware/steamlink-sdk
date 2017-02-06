// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "skia/ext/bitmap_platform_device_cairo.h"
#include "skia/ext/platform_canvas.h"

#if defined(OS_OPENBSD)
#include <cairo.h>
#else
#include <cairo/cairo.h>
#endif

namespace skia {

namespace {

void CairoSurfaceReleaseProc(void*, void* context) {
  SkASSERT(context);
  cairo_surface_destroy(static_cast<cairo_surface_t*>(context));
}

// Back the destination bitmap by a Cairo surface.  The bitmap's
// pixelRef takes ownership of the passed-in surface and will call
// cairo_surface_destroy() upon destruction.
//
// Note: it may immediately destroy the surface, if it fails to create a bitmap
// with pixels, thus the caller must either ref() the surface before hand, or
// it must not refer to the surface after this call.
bool InstallCairoSurfacePixels(SkBitmap* dst,
                               cairo_surface_t* surface,
                               bool is_opaque) {
  SkASSERT(dst);
  if (!surface) {
    return false;
  }
  SkImageInfo info
      = SkImageInfo::MakeN32(cairo_image_surface_get_width(surface),
                             cairo_image_surface_get_height(surface),
                             is_opaque ? kOpaque_SkAlphaType
                                       : kPremul_SkAlphaType);
  return dst->installPixels(info,
                            cairo_image_surface_get_data(surface),
                            cairo_image_surface_get_stride(surface),
                            NULL,
                            &CairoSurfaceReleaseProc,
                            static_cast<void*>(surface));
}

void LoadMatrixToContext(cairo_t* context, const SkMatrix& matrix) {
  cairo_matrix_t cairo_matrix;
  cairo_matrix_init(&cairo_matrix,
                    SkScalarToFloat(matrix.getScaleX()),
                    SkScalarToFloat(matrix.getSkewY()),
                    SkScalarToFloat(matrix.getSkewX()),
                    SkScalarToFloat(matrix.getScaleY()),
                    SkScalarToFloat(matrix.getTranslateX()),
                    SkScalarToFloat(matrix.getTranslateY()));
  cairo_set_matrix(context, &cairo_matrix);
}

void LoadClipToContext(cairo_t* context, const SkIRect& clip_bounds) {
  cairo_reset_clip(context);

  cairo_rectangle(context, clip_bounds.fLeft, clip_bounds.fTop,
                  clip_bounds.width(), clip_bounds.height());
  cairo_clip(context);
}

}  // namespace

void BitmapPlatformDevice::LoadConfig(const SkMatrix& transform,
                                      const SkIRect& clip_bounds) {
  if (!cairo_)
    return;  // Nothing to do.

  LoadClipToContext(cairo_, clip_bounds);
  LoadMatrixToContext(cairo_, transform);
}

// We use this static factory function instead of the regular constructor so
// that we can create the pixel data before calling the constructor. This is
// required so that we can call the base class' constructor with the pixel
// data.
BitmapPlatformDevice* BitmapPlatformDevice::Create(int width, int height,
                                                   bool is_opaque,
                                                   cairo_surface_t* surface) {
  if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
    cairo_surface_destroy(surface);
    return NULL;
  }

  // must call this before trying to install the surface, since that may result
  // in the surface being destroyed.
  cairo_t* cairo = cairo_create(surface);

  SkBitmap bitmap;
  if (!InstallCairoSurfacePixels(&bitmap, surface, is_opaque)) {
    cairo_destroy(cairo);
    return NULL;
  }

  // The device object will take ownership of the graphics context.
  return new BitmapPlatformDevice(bitmap, cairo);
}

BitmapPlatformDevice* BitmapPlatformDevice::Create(int width, int height,
                                                   bool is_opaque) {
  // This initializes the bitmap to all zeros.
  cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                                        width, height);

  BitmapPlatformDevice* device = Create(width, height, is_opaque, surface);

#ifndef NDEBUG
    if (device && is_opaque)  // Fill with bright bluish green
        SkCanvas(device).drawColor(0xFF00FF80);
#endif

  return device;
}

BitmapPlatformDevice* BitmapPlatformDevice::Create(int width, int height,
                                                   bool is_opaque,
                                                   uint8_t* data) {
  cairo_surface_t* surface = cairo_image_surface_create_for_data(
      data, CAIRO_FORMAT_ARGB32, width, height,
      cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width));

  return Create(width, height, is_opaque, surface);
}

// Ownership of the cairo object is transferred.
BitmapPlatformDevice::BitmapPlatformDevice(
    const SkBitmap& bitmap,
    cairo_t* cairo)
    : SkBitmapDevice(bitmap),
      cairo_(cairo) {
  SetPlatformDevice(this, this);
}

BitmapPlatformDevice::~BitmapPlatformDevice() {
  cairo_destroy(cairo_);
}

SkBaseDevice* BitmapPlatformDevice::onCreateDevice(const CreateInfo& info,
                                                   const SkPaint*) {
  SkASSERT(info.fInfo.colorType() == kN32_SkColorType);
  return BitmapPlatformDevice::Create(info.fInfo.width(), info.fInfo.height(),
                                      info.fInfo.isOpaque());
}

cairo_t* BitmapPlatformDevice::BeginPlatformPaint(
      const SkMatrix& transform,
      const SkIRect& clip_bounds) {
  LoadConfig(transform, clip_bounds);
  cairo_surface_t* surface = cairo_get_target(cairo_);
  // Tell cairo to flush anything it has pending.
  cairo_surface_flush(surface);
  // Tell Cairo that we (probably) modified (actually, will modify) its pixel
  // buffer directly.
  cairo_surface_mark_dirty(surface);
  return cairo_;
}

// PlatformCanvas impl

SkCanvas* CreatePlatformCanvas(int width, int height, bool is_opaque,
                               uint8_t* data, OnFailureType failureType) {
  sk_sp<SkBaseDevice> dev(
      BitmapPlatformDevice::Create(width, height, is_opaque, data));
  return CreateCanvas(dev, failureType);
}

}  // namespace skia
