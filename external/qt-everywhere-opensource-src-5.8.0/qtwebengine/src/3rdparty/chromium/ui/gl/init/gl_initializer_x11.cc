// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/init/gl_initializer.h"

#include "base/logging.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_surface_glx.h"
#include "ui/gl/gl_surface_osmesa_x11.h"

namespace gl {
namespace init {

bool InitializeGLOneOffPlatform() {
  switch (GetGLImplementation()) {
    case kGLImplementationDesktopGL:
      if (!GLSurfaceGLX::InitializeOneOff()) {
        LOG(ERROR) << "GLSurfaceGLX::InitializeOneOff failed.";
        return false;
      }
      return true;
    case kGLImplementationOSMesaGL:
      if (!GLSurfaceOSMesaX11::InitializeOneOff()) {
        LOG(ERROR) << "GLSurfaceOSMesaX11::InitializeOneOff failed.";
        return false;
      }
      return true;
    case kGLImplementationEGLGLES2:
      if (!GLSurfaceEGL::InitializeOneOff()) {
        LOG(ERROR) << "GLSurfaceEGL::InitializeOneOff failed.";
        return false;
      }
      return true;
    default:
      return true;
  }
}

}  // namespace init
}  // namespace gl
