// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_surface_factory.h"

#include <X11/Xlib.h>

#include "third_party/khronos/EGL/egl.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/gl/egl_util.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/ozone/common/egl_util.h"
#include "ui/ozone/common/gl_ozone_egl.h"
#include "ui/ozone/platform/x11/gl_ozone_glx.h"

namespace ui {

namespace {

// GLSurface implementation for Ozone X11 EGL.
class GLSurfaceEGLOzoneX11 : public gl::NativeViewGLSurfaceEGL {
 public:
  explicit GLSurfaceEGLOzoneX11(EGLNativeWindowType window);

  // gl::NativeViewGLSurfaceEGL:
  EGLConfig GetConfig() override;
  bool Resize(const gfx::Size& size,
              float scale_factor,
              bool has_alpha) override;

 private:
  ~GLSurfaceEGLOzoneX11() override;

  DISALLOW_COPY_AND_ASSIGN(GLSurfaceEGLOzoneX11);
};

GLSurfaceEGLOzoneX11::GLSurfaceEGLOzoneX11(EGLNativeWindowType window)
    : NativeViewGLSurfaceEGL(window) {}

EGLConfig GLSurfaceEGLOzoneX11::GetConfig() {
  // Try matching the window depth with an alpha channel, because we're worried
  // the destination alpha width could constrain blending precision.
  const int kBufferSizeOffset = 1;
  const int kAlphaSizeOffset = 3;
  EGLint config_attribs[] = {EGL_BUFFER_SIZE,
                             ~0,  // To be replaced.
                             EGL_ALPHA_SIZE,
                             8,
                             EGL_BLUE_SIZE,
                             8,
                             EGL_GREEN_SIZE,
                             8,
                             EGL_RED_SIZE,
                             8,
                             EGL_RENDERABLE_TYPE,
                             EGL_OPENGL_ES2_BIT,
                             EGL_SURFACE_TYPE,
                             EGL_WINDOW_BIT,
                             EGL_NONE};

  // Get the depth of XWindow for surface.
  XWindowAttributes win_attribs;
  if (XGetWindowAttributes(gfx::GetXDisplay(), window_, &win_attribs)) {
    config_attribs[kBufferSizeOffset] = win_attribs.depth;
  }

  EGLDisplay display = GetDisplay();

  EGLConfig config;
  EGLint num_configs;
  if (!eglChooseConfig(display, config_attribs, &config, 1, &num_configs)) {
    LOG(ERROR) << "eglChooseConfig failed with error "
               << GetLastEGLErrorString();
    return nullptr;
  }

  if (num_configs > 0) {
    EGLint config_depth;
    if (!eglGetConfigAttrib(display, config, EGL_BUFFER_SIZE, &config_depth)) {
      LOG(ERROR) << "eglGetConfigAttrib failed with error "
                 << GetLastEGLErrorString();
      return nullptr;
    }
    if (config_depth == config_attribs[kBufferSizeOffset]) {
      return config;
    }
  }

  // Try without an alpha channel.
  config_attribs[kAlphaSizeOffset] = 0;
  if (!eglChooseConfig(display, config_attribs, &config, 1, &num_configs)) {
    LOG(ERROR) << "eglChooseConfig failed with error "
               << GetLastEGLErrorString();
    return nullptr;
  }

  if (num_configs == 0) {
    LOG(ERROR) << "No suitable EGL configs found.";
    return nullptr;
  }
  return config;
}

bool GLSurfaceEGLOzoneX11::Resize(const gfx::Size& size,
                                  float scale_factor,
                                  bool has_alpha) {
  if (size == GetSize())
    return true;

  size_ = size;

  eglWaitGL();
  XResizeWindow(gfx::GetXDisplay(), window_, size.width(), size.height());
  eglWaitNative(EGL_CORE_NATIVE_ENGINE);

  return true;
}

GLSurfaceEGLOzoneX11::~GLSurfaceEGLOzoneX11() {
  Destroy();
}

class GLOzoneEGLX11 : public GLOzoneEGL {
 public:
  GLOzoneEGLX11() {}
  ~GLOzoneEGLX11() override {}

  scoped_refptr<gl::GLSurface> CreateViewGLSurface(
      gfx::AcceleratedWidget window) override {
    return gl::InitializeGLSurface(new GLSurfaceEGLOzoneX11(window));
  }

  scoped_refptr<gl::GLSurface> CreateOffscreenGLSurface(
      const gfx::Size& size) override {
    return gl::InitializeGLSurface(new gl::PbufferGLSurfaceEGL(size));
  }

 protected:
  intptr_t GetNativeDisplay() override {
    return reinterpret_cast<intptr_t>(gfx::GetXDisplay());
  }

  bool LoadGLES2Bindings() override { return LoadDefaultEGLGLES2Bindings(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(GLOzoneEGLX11);
};

}  // namespace

X11SurfaceFactory::X11SurfaceFactory() {
  glx_implementation_.reset(new GLOzoneGLX());
  egl_implementation_.reset(new GLOzoneEGLX11());
}

X11SurfaceFactory::~X11SurfaceFactory() {}

std::vector<gl::GLImplementation>
X11SurfaceFactory::GetAllowedGLImplementations() {
  std::vector<gl::GLImplementation> impls;
  impls.push_back(gl::kGLImplementationEGLGLES2);
  // DesktopGL (GLX) should be the first option when crbug.com/646982 is fixed.
  impls.push_back(gl::kGLImplementationDesktopGL);
  impls.push_back(gl::kGLImplementationOSMesaGL);
  return impls;
}

GLOzone* X11SurfaceFactory::GetGLOzone(gl::GLImplementation implementation) {
  switch (implementation) {
    case gl::kGLImplementationDesktopGL:
      return glx_implementation_.get();
    case gl::kGLImplementationEGLGLES2:
      return egl_implementation_.get();
    default:
      return nullptr;
  }
}

}  // namespace ui
