// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_PLATFORM_DEVICE_H_
#define SKIA_EXT_PLATFORM_DEVICE_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#include <vector>
#endif

#include "skia/ext/native_drawing_context.h"
#include "third_party/skia/include/core/SkBitmapDevice.h"
#include "third_party/skia/include/core/SkTypes.h"

class SkMatrix;
class SkPath;

namespace skia {

class PlatformDevice;
class ScopedPlatformPaint;

// The following routines provide accessor points for the functionality
// exported by the various PlatformDevice ports.  
// All calls to PlatformDevice::* should be routed through these 
// helper functions.

// DEPRECATED
// Bind a PlatformDevice instance, |platform_device|, to |device|.
SK_API void SetPlatformDevice(SkBaseDevice* device, PlatformDevice* platform_device);

// DEPRECATED
// Retrieve the previous argument to SetPlatformDevice().
SK_API PlatformDevice* GetPlatformDevice(SkBaseDevice* device);

// A SkBitmapDevice is basically a wrapper around SkBitmap that provides a 
// surface for SkCanvas to draw into. PlatformDevice provides a surface 
// Windows can also write to. It also provides functionality to play well 
// with GDI drawing functions. This class is abstract and must be subclassed. 
// It provides the basic interface to implement it either with or without 
// a bitmap backend.
//
// PlatformDevice provides an interface which sub-classes of SkBaseDevice can 
// also provide to allow for drawing by the native platform into the device.
// TODO(robertphillips): Once the bitmap-specific entry points are removed
// from SkBaseDevice it might make sense for PlatformDevice to be derived
// from it.
class SK_API PlatformDevice {
 public:
  virtual ~PlatformDevice() {}

#if defined(OS_MACOSX)
  // The CGContext that corresponds to the bitmap, used for CoreGraphics
  // operations drawing into the bitmap. This is possibly heavyweight, so it
  // should exist only during one pass of rendering.
  virtual CGContextRef GetBitmapContext(const SkMatrix& transform,
                                        const SkIRect& clip_bounds) = 0;
#endif

 private:
  // The DC that corresponds to the bitmap, used for GDI operations drawing
  // into the bitmap. This is possibly heavyweight, so it should be existant
  // only during one pass of rendering.
  virtual NativeDrawingContext BeginPlatformPaint(const SkMatrix& transform,
                                                  const SkIRect& clip_bounds);

  friend class ScopedPlatformPaint;
};

}  // namespace skia

#endif  // SKIA_EXT_PLATFORM_DEVICE_H_
