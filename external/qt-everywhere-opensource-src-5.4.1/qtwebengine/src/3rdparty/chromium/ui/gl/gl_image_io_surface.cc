// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_io_surface.h"

#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"

// Note that this must be included after gl_bindings.h to avoid conflicts.
#include <OpenGL/CGLIOSurface.h>

namespace gfx {

GLImageIOSurface::GLImageIOSurface(gfx::Size size)
    : size_(size) {}

GLImageIOSurface::~GLImageIOSurface() { Destroy(); }

bool GLImageIOSurface::Initialize(gfx::GpuMemoryBufferHandle buffer) {
  io_surface_.reset(IOSurfaceLookup(buffer.io_surface_id));
  if (!io_surface_) {
    LOG(ERROR) << "IOSurface lookup failed";
    return false;
  }

  return true;
}

gfx::Size GLImageIOSurface::GetSize() { return size_; }

bool GLImageIOSurface::BindTexImage(unsigned target) {
  if (target != GL_TEXTURE_RECTANGLE_ARB) {
    // This might be supported in the future. For now, perform strict
    // validation so we know what's going on.
    LOG(ERROR) << "IOSurface requires TEXTURE_RECTANGLE_ARB target";
    return false;
  }

  CGLContextObj cgl_context =
      static_cast<CGLContextObj>(GLContext::GetCurrent()->GetHandle());

  DCHECK(io_surface_);
  CGLError cgl_error = CGLTexImageIOSurface2D(cgl_context,
                                              target,
                                              GL_RGBA,
                                              size_.width(),
                                              size_.height(),
                                              GL_BGRA,
                                              GL_UNSIGNED_INT_8_8_8_8_REV,
                                              io_surface_.get(),
                                              0);
  if (cgl_error != kCGLNoError) {
    LOG(ERROR) << "Error in CGLTexImageIOSurface2D";
    return false;
  }

  return true;
}

}  // namespace gfx
