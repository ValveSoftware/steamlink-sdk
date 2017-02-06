// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/init/gl_initializer.h"

#include <OpenGL/CGLRenderers.h>

#include <vector>

#include "base/logging.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gpu_switching_manager.h"

namespace gl {
namespace init {

namespace {

bool InitializeOneOffForSandbox() {
  static bool initialized = false;
  if (initialized)
    return true;

  // This is called from the sandbox warmup code on Mac OS X.
  // GPU-related stuff is very slow without this, probably because
  // the sandbox prevents loading graphics drivers or some such.
  std::vector<CGLPixelFormatAttribute> attribs;
  if (ui::GpuSwitchingManager::GetInstance()->SupportsDualGpus()) {
    // Avoid switching to the discrete GPU just for this pixel
    // format selection.
    attribs.push_back(kCGLPFAAllowOfflineRenderers);
  }
  if (GetGLImplementation() == kGLImplementationAppleGL) {
    attribs.push_back(kCGLPFARendererID);
    attribs.push_back(
        static_cast<CGLPixelFormatAttribute>(kCGLRendererGenericFloatID));
  }
  attribs.push_back(static_cast<CGLPixelFormatAttribute>(0));

  CGLPixelFormatObj format;
  GLint num_pixel_formats;
  if (CGLChoosePixelFormat(&attribs.front(), &format, &num_pixel_formats) !=
      kCGLNoError) {
    LOG(ERROR) << "Error choosing pixel format.";
    return false;
  }
  if (!format) {
    LOG(ERROR) << "format == 0.";
    return false;
  }
  CGLReleasePixelFormat(format);
  DCHECK_NE(num_pixel_formats, 0);
  initialized = true;
  return true;
}

}  // namespace

bool InitializeGLOneOffPlatform() {
  switch (GetGLImplementation()) {
    case kGLImplementationDesktopGL:
    case kGLImplementationDesktopGLCoreProfile:
    case kGLImplementationAppleGL:
      if (!InitializeOneOffForSandbox()) {
        LOG(ERROR) << "GLSurfaceCGL::InitializeOneOff failed.";
        return false;
      }
      return true;
    default:
      return true;
  }
}

}  // namespace init
}  // namespace gl
