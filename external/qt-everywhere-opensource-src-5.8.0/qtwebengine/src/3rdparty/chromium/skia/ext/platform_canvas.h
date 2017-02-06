// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_PLATFORM_CANVAS_H_
#define SKIA_EXT_PLATFORM_CANVAS_H_

#include <stddef.h>
#include <stdint.h>

// The platform-specific device will include the necessary platform headers
// to get the surface type.
#include "build/build_config.h"
#include "skia/ext/platform_surface.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/skia/include/core/SkPixmap.h"

class SkBaseDevice;

// A PlatformCanvas is a software-rasterized SkCanvas which is *also*
// addressable by the platform-specific drawing API (GDI, Core Graphics,
// Cairo...).

namespace skia {

//
//  Note about error handling.
//
//  Creating a canvas can fail at times, most often because we fail to allocate
//  the backing-store (pixels). This can be from out-of-memory, or something
//  more opaque, like GDI or cairo reported a failure.
//
//  To allow the caller to handle the failure, every Create... factory takes an
//  enum as its last parameter. The default value is kCrashOnFailure. If the
//  caller passes kReturnNullOnFailure, then the caller is responsible to check
//  the return result.
//
enum OnFailureType {
  CRASH_ON_FAILURE,
  RETURN_NULL_ON_FAILURE
};

#if defined(WIN32)
  // The shared_section parameter is passed to gfx::PlatformDevice::create.
  // See it for details.
  SK_API SkCanvas* CreatePlatformCanvas(int width,
                                        int height,
                                        bool is_opaque,
                                        HANDLE shared_section,
                                        OnFailureType failure_type);

  // Draws the top layer of the canvas into the specified HDC. Only works
  // with a SkCanvas with a BitmapPlatformDevice.
  SK_API void DrawToNativeContext(SkCanvas* canvas,
                                  HDC hdc,
                                  int x,
                                  int y,
                                  const RECT* src_rect);
#elif defined(__APPLE__)
  SK_API SkCanvas* CreatePlatformCanvas(CGContextRef context,
                                        int width,
                                        int height,
                                        bool is_opaque,
                                        OnFailureType failure_type);

  SK_API SkCanvas* CreatePlatformCanvas(int width,
                                        int height,
                                        bool is_opaque,
                                        uint8_t* context,
                                        OnFailureType failure_type);
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) || \
      defined(__sun) || defined(ANDROID)
  // Linux ---------------------------------------------------------------------

  // Construct a canvas from the given memory region. The memory is not cleared
  // first. @data must be, at least, @height * StrideForWidth(@width) bytes.
  SK_API SkCanvas* CreatePlatformCanvas(int width,
                                        int height,
                                        bool is_opaque,
                                        uint8_t* data,
                                        OnFailureType failure_type);
#endif

static inline SkCanvas* CreatePlatformCanvas(int width,
                                             int height,
                                             bool is_opaque) {
  return CreatePlatformCanvas(width, height, is_opaque, 0, CRASH_ON_FAILURE);
}

SK_API SkCanvas* CreateCanvas(const sk_sp<SkBaseDevice>& device,
                              OnFailureType failure_type);

static inline SkCanvas* CreateBitmapCanvas(int width,
                                           int height,
                                           bool is_opaque) {
  return CreatePlatformCanvas(width, height, is_opaque, 0, CRASH_ON_FAILURE);
}

static inline SkCanvas* TryCreateBitmapCanvas(int width,
                                              int height,
                                              bool is_opaque) {
  return CreatePlatformCanvas(width, height, is_opaque, 0,
                              RETURN_NULL_ON_FAILURE);
}

// Return the stride (length of a line in bytes) for the given width. Because
// we use 32-bits per pixel, this will be roughly 4*width. However, for
// alignment reasons we may wish to increase that.
SK_API size_t PlatformCanvasStrideForWidth(unsigned width);

// Returns the SkBaseDevice pointer of the topmost rect with a non-empty
// clip. In practice, this is usually either the top layer or nothing, since
// we usually set the clip to new layers when we make them.
//
// This may return NULL, so callers need to check.
//
// This is different than SkCanvas' getDevice, because that returns the
// bottommost device.
//
// Danger: the resulting device should not be saved. It will be invalidated
// by the next call to save() or restore().
SK_API SkBaseDevice* GetTopDevice(const SkCanvas& canvas);

// Copies pixels from the SkCanvas into an SkBitmap, fetching pixels from
// GPU memory if necessary.
//
// The bitmap will remain empty if we can't allocate enough memory for a copy
// of the pixels.
SK_API SkBitmap ReadPixels(SkCanvas* canvas);

// Gives the pixmap passed in *writable* access to the pixels backing this
// canvas. All writes to the pixmap should be visible if the canvas is
// raster-backed.
//
// Returns false on failure: if either argument is nullptr, or if the
// pixels can not be retrieved from the canvas. In the latter case resets
// the pixmap to empty.
SK_API bool GetWritablePixels(SkCanvas* canvas, SkPixmap* pixmap);

// Returns true if native platform routines can be used to draw on the
// given canvas. If this function returns false,
// ScopedPlatformPaint::GetPlatformSurface() should return NULL.
SK_API bool SupportsPlatformPaint(const SkCanvas* canvas);

// This object guards calls to platform drawing routines. The surface
// returned from GetPlatformSurface() can be used with the native platform
// routines.
class SK_API ScopedPlatformPaint {
 public:
  explicit ScopedPlatformPaint(SkCanvas* canvas);

  // Returns the PlatformSurface to use for native platform drawing calls.
  PlatformSurface GetPlatformSurface() { return platform_surface_; }

 private:
  SkCanvas* canvas_;
  PlatformSurface platform_surface_;

  // Disallow copy and assign
  ScopedPlatformPaint(const ScopedPlatformPaint&);
  ScopedPlatformPaint& operator=(const ScopedPlatformPaint&);
};

// Following routines are used in print preview workflow to mark the
// preview metafile.
SK_API SkMetaData& GetMetaData(const SkCanvas& canvas);

#if defined(OS_MACOSX)
SK_API void SetIsPreviewMetafile(const SkCanvas& canvas, bool is_preview);
SK_API bool IsPreviewMetafile(const SkCanvas& canvas);

// Returns the CGContext that backing the SkCanvas.
// Returns NULL if none is bound.
SK_API CGContextRef GetBitmapContext(const SkCanvas& canvas);
#endif

}  // namespace skia

#endif  // SKIA_EXT_PLATFORM_CANVAS_H_
