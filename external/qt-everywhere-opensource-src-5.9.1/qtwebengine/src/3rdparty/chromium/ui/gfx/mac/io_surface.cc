// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mac/io_surface.h"

#include <stddef.h>
#include <stdint.h>

#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/mach_logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/buffer_format_util.h"

namespace gfx {

namespace {

void AddIntegerValue(CFMutableDictionaryRef dictionary,
                     const CFStringRef key,
                     int32_t value) {
  base::ScopedCFTypeRef<CFNumberRef> number(
      CFNumberCreate(NULL, kCFNumberSInt32Type, &value));
  CFDictionaryAddValue(dictionary, key, number.get());
}

int32_t BytesPerElement(gfx::BufferFormat format, int plane) {
  switch (format) {
    case gfx::BufferFormat::R_8:
      DCHECK_EQ(plane, 0);
      return 1;
    case gfx::BufferFormat::BGRA_8888:
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::RGBA_8888:
      DCHECK_EQ(plane, 0);
      return 4;
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      static int32_t bytes_per_element[] = {1, 2};
      DCHECK_LT(static_cast<size_t>(plane), arraysize(bytes_per_element));
      return bytes_per_element[plane];
    case gfx::BufferFormat::RG_88:
    case gfx::BufferFormat::UYVY_422:
      DCHECK_EQ(plane, 0);
      return 2;
    case gfx::BufferFormat::ATC:
    case gfx::BufferFormat::ATCIA:
    case gfx::BufferFormat::DXT1:
    case gfx::BufferFormat::DXT5:
    case gfx::BufferFormat::ETC1:
    case gfx::BufferFormat::BGR_565:
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::YVU_420:
      NOTREACHED();
      return 0;
  }

  NOTREACHED();
  return 0;
}

int32_t PixelFormat(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::R_8:
      return 'L008';
    case gfx::BufferFormat::BGRA_8888:
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::RGBA_8888:
      return 'BGRA';
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return '420v';
    case gfx::BufferFormat::UYVY_422:
      return '2vuy';
    case gfx::BufferFormat::RG_88:
    case gfx::BufferFormat::ATC:
    case gfx::BufferFormat::ATCIA:
    case gfx::BufferFormat::DXT1:
    case gfx::BufferFormat::DXT5:
    case gfx::BufferFormat::ETC1:
    case gfx::BufferFormat::BGR_565:
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::YVU_420:
      NOTREACHED();
      return 0;
  }

  NOTREACHED();
  return 0;
}

}  // namespace

namespace internal {

// static
mach_port_t IOSurfaceMachPortTraits::Retain(mach_port_t port) {
  kern_return_t kr =
      mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_SEND, 1);
  MACH_LOG_IF(ERROR, kr != KERN_SUCCESS, kr)
      << "IOSurfaceMachPortTraits::Retain mach_port_mod_refs";
  return port;
}

// static
void IOSurfaceMachPortTraits::Release(mach_port_t port) {
  kern_return_t kr =
      mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_SEND, -1);
  MACH_LOG_IF(ERROR, kr != KERN_SUCCESS, kr)
      << "IOSurfaceMachPortTraits::Release mach_port_mod_refs";
}

}  // namespace internal

IOSurfaceRef CreateIOSurface(const gfx::Size& size, gfx::BufferFormat format) {
  TRACE_EVENT0("ui", "CreateIOSurface");
  base::TimeTicks start_time = base::TimeTicks::Now();

  size_t num_planes = gfx::NumberOfPlanesForBufferFormat(format);
  base::ScopedCFTypeRef<CFMutableArrayRef> planes(CFArrayCreateMutable(
      kCFAllocatorDefault, num_planes, &kCFTypeArrayCallBacks));

  // Don't specify plane information unless there are indeed multiple planes
  // because DisplayLink drivers do not support this.
  // http://crbug.com/527556
  if (num_planes > 1) {
    for (size_t plane = 0; plane < num_planes; ++plane) {
      size_t factor = gfx::SubsamplingFactorForBufferFormat(format, plane);

      base::ScopedCFTypeRef<CFMutableDictionaryRef> plane_info(
          CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                    &kCFTypeDictionaryKeyCallBacks,
                                    &kCFTypeDictionaryValueCallBacks));
      AddIntegerValue(plane_info, kIOSurfacePlaneWidth, size.width() / factor);
      AddIntegerValue(plane_info, kIOSurfacePlaneHeight,
                      size.height() / factor);
      AddIntegerValue(plane_info, kIOSurfacePlaneBytesPerElement,
                      BytesPerElement(format, plane));

      CFArrayAppendValue(planes, plane_info);
    }
  }

  base::ScopedCFTypeRef<CFMutableDictionaryRef> properties(
      CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));
  AddIntegerValue(properties, kIOSurfaceWidth, size.width());
  AddIntegerValue(properties, kIOSurfaceHeight, size.height());
  AddIntegerValue(properties, kIOSurfacePixelFormat, PixelFormat(format));
  if (num_planes > 1) {
    CFDictionaryAddValue(properties, kIOSurfacePlaneInfo, planes);
  } else {
    AddIntegerValue(properties, kIOSurfaceBytesPerElement,
                    BytesPerElement(format, 0));
  }

  IOSurfaceRef surface = IOSurfaceCreate(properties);

  // For unknown reasons, triggering this lock on OS X 10.9, on certain GPUs,
  // causes PDFs to render incorrectly. Hopefully this check can be removed once
  // pdfium switches to a Skia backend on Mac.
  // https://crbug.com/594343.
  // IOSurface clearing causes significant performance regression on about half
  // of all devices running Yosemite. https://crbug.com/606850#c22.
  bool should_clear = !base::mac::IsOS10_9() && !base::mac::IsOS10_10();

  if (should_clear) {
    // Zero-initialize the IOSurface. Calling IOSurfaceLock/IOSurfaceUnlock
    // appears to be sufficient. https://crbug.com/584760#c17
    IOReturn r = IOSurfaceLock(surface, 0, nullptr);
    DCHECK_EQ(kIOReturnSuccess, r);
    r = IOSurfaceUnlock(surface, 0, nullptr);
    DCHECK_EQ(kIOReturnSuccess, r);
  }

  bool force_system_color_space = false;

  // Displaying an IOSurface that does not have a color space using an
  // AVSampleBufferDisplayLayer can result in a black screen. Specify the
  // main display's color profile by default, which will result in no color
  // correction being done for the main monitor (which is the behavior of not
  // specifying a color space).
  // https://crbug.com/608879
  if (format == gfx::BufferFormat::YUV_420_BIPLANAR)
    force_system_color_space = true;

  // On Sierra, all IOSurfaces are color corrected as though they are in sRGB
  // color space by default. Prior to Sierra, IOSurfaces were not color
  // corrected (they were treated as though they were in the display color
  // space). Override this by defaulting IOSurfaces to be in the main display
  // color space.
  // https://crbug.com/654488
  if (base::mac::IsAtLeastOS10_12())
    force_system_color_space = true;

  if (force_system_color_space) {
    CGColorSpaceRef color_space = base::mac::GetSystemColorSpace();
    base::ScopedCFTypeRef<CFDataRef> color_space_icc(
        CGColorSpaceCopyICCProfile(color_space));
    // Note that nullptr is an acceptable input to IOSurfaceSetValue.
    IOSurfaceSetValue(surface, CFSTR("IOSurfaceColorSpace"), color_space_icc);
  }

  UMA_HISTOGRAM_TIMES("GPU.IOSurface.CreateTime",
                      base::TimeTicks::Now() - start_time);
  return surface;
}

}  // namespace gfx
