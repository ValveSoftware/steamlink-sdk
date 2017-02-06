// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(USE_EGL)
#include <EGL/egl.h>
#endif

#include "ui/gl/gl_bindings.h"

#if defined(USE_GLX)
#include "ui/gfx/x/x11_types.h"  // nogncheck
#endif

#if defined(OS_WIN)
#include "ui/gl/gl_surface_wgl.h"
#endif

#if defined(USE_EGL)
#include "ui/gl/gl_surface_egl.h"
#endif

namespace gl {

std::string DriverOSMESA::GetPlatformExtensions() {
  return "";
}

#if defined(OS_WIN)
std::string DriverWGL::GetPlatformExtensions() {
  const char* str = nullptr;
  str = wglGetExtensionsStringARB(GLSurfaceWGL::GetDisplayDC());
  if (str)
    return str;
  return wglGetExtensionsStringEXT();
}
#endif

#if defined(USE_EGL)
#if !defined(TOOLKIT_QT)
std::string DriverEGL::GetPlatformExtensions() {
  EGLDisplay display = GLSurfaceEGL::InitializeDisplay();
  if (display == EGL_NO_DISPLAY)
    return "";
  const char* str = eglQueryString(display, EGL_EXTENSIONS);
  return str ? std::string(str) : "";
}
#endif
// static
std::string DriverEGL::GetClientExtensions() {
  const char* str = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
  return str ? std::string(str) : "";
}
#endif

#if defined(USE_GLX)
std::string DriverGLX::GetPlatformExtensions() {
  const char* str = glXQueryExtensionsString(gfx::GetXDisplay(), 0);
  return str ? std::string(str) : "";
}
#endif

}  // namespace gl
