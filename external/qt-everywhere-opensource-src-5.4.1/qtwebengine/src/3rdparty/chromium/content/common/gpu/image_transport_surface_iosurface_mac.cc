// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/image_transport_surface_iosurface_mac.h"

#include "content/common/gpu/gpu_messages.h"

namespace content {
namespace {

// IOSurface dimensions will be rounded up to a multiple of this value in order
// to reduce memory thrashing during resize. This must be a power of 2.
const uint32 kIOSurfaceDimensionRoundup = 64;

int RoundUpSurfaceDimension(int number) {
  DCHECK(number >= 0);
  // Cast into unsigned space for portable bitwise ops.
  uint32 unsigned_number = static_cast<uint32>(number);
  uint32 roundup_sub_1 = kIOSurfaceDimensionRoundup - 1;
  unsigned_number = (unsigned_number + roundup_sub_1) & ~roundup_sub_1;
  return static_cast<int>(unsigned_number);
}

void AddBooleanValue(CFMutableDictionaryRef dictionary,
                     const CFStringRef key,
                     bool value) {
  CFDictionaryAddValue(dictionary, key,
                       (value ? kCFBooleanTrue : kCFBooleanFalse));
}

void AddIntegerValue(CFMutableDictionaryRef dictionary,
                     const CFStringRef key,
                     int32 value) {
  base::ScopedCFTypeRef<CFNumberRef> number(
      CFNumberCreate(NULL, kCFNumberSInt32Type, &value));
  CFDictionaryAddValue(dictionary, key, number.get());
}

}  // namespace

IOSurfaceStorageProvider::IOSurfaceStorageProvider() {}

IOSurfaceStorageProvider::~IOSurfaceStorageProvider() {
  DCHECK(!io_surface_);
}

gfx::Size IOSurfaceStorageProvider::GetRoundedSize(gfx::Size size) {
  return gfx::Size(RoundUpSurfaceDimension(size.width()),
                   RoundUpSurfaceDimension(size.height()));
}

bool IOSurfaceStorageProvider::AllocateColorBufferStorage(
    CGLContextObj context,
    gfx::Size size) {
  // Allocate a new IOSurface, which is the GPU resource that can be
  // shared across processes.
  base::ScopedCFTypeRef<CFMutableDictionaryRef> properties;
  properties.reset(CFDictionaryCreateMutable(kCFAllocatorDefault,
                                             0,
                                             &kCFTypeDictionaryKeyCallBacks,
                                             &kCFTypeDictionaryValueCallBacks));
  AddIntegerValue(properties,
                  kIOSurfaceWidth,
                  size.width());
  AddIntegerValue(properties,
                  kIOSurfaceHeight,
                  size.height());
  AddIntegerValue(properties,
                  kIOSurfaceBytesPerElement, 4);
  AddBooleanValue(properties,
                  kIOSurfaceIsGlobal, true);
  // I believe we should be able to unreference the IOSurfaces without
  // synchronizing with the browser process because they are
  // ultimately reference counted by the operating system.
  io_surface_.reset(IOSurfaceCreate(properties));
  io_surface_handle_ = IOSurfaceGetID(io_surface_);

  // Don't think we need to identify a plane.
  GLuint plane = 0;
  CGLError cglerror = CGLTexImageIOSurface2D(
      context,
      GL_TEXTURE_RECTANGLE_ARB,
      GL_RGBA,
      size.width(),
      size.height(),
      GL_BGRA,
      GL_UNSIGNED_INT_8_8_8_8_REV,
      io_surface_.get(),
      plane);
  if (cglerror != kCGLNoError) {
    DLOG(ERROR) << "CGLTexImageIOSurface2D failed with CGL error: " << cglerror;
    return false;
  }

  glFlush();
  return true;
}

void IOSurfaceStorageProvider::FreeColorBufferStorage() {
  io_surface_.reset();
  io_surface_handle_ = 0;
}

uint64 IOSurfaceStorageProvider::GetSurfaceHandle() const {
  return io_surface_handle_;
}

}  //  namespace content
