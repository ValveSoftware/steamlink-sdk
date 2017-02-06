// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/init/gl_initializer.h"

#include "base/logging.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_surface_wgl.h"
#include "ui/gl/vsync_provider_win.h"

namespace gl {
namespace init {

bool InitializeGLOneOffPlatform() {
  VSyncProviderWin::InitializeOneOff();

  switch (GetGLImplementation()) {
    case kGLImplementationDesktopGL:
      if (!GLSurfaceWGL::InitializeOneOff()) {
        LOG(ERROR) << "GLSurfaceWGL::InitializeOneOff failed.";
        return false;
      }
      break;
    case kGLImplementationEGLGLES2:
      if (!GLSurfaceEGL::InitializeOneOff()) {
        LOG(ERROR) << "GLSurfaceEGL::InitializeOneOff failed.";
        return false;
      }
      break;
    case kGLImplementationOSMesaGL:
    case kGLImplementationMockGL:
      break;
    default:
      NOTREACHED();
  }
  return true;
}

}  // namespace init
}  // namespace gl
