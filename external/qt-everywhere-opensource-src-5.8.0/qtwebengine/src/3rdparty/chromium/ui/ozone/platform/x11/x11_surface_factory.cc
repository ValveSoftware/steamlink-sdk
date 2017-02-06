// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_surface_factory.h"

#include <X11/Xlib.h>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "third_party/khronos/EGL/egl.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/ozone/common/egl_util.h"
#include "ui/ozone/public/surface_ozone_egl.h"

namespace ui {

namespace {

class X11SurfaceEGL : public SurfaceOzoneEGL {
 public:
  explicit X11SurfaceEGL(gfx::AcceleratedWidget widget) : widget_(widget) {}
  ~X11SurfaceEGL() override {}

  intptr_t GetNativeWindow() override { return widget_; }

  bool OnSwapBuffers() override { return true; }

  void OnSwapBuffersAsync(const SwapCompletionCallback& callback) override {
    NOTREACHED();
  }

  bool ResizeNativeWindow(const gfx::Size& viewport_size) override {
    return true;
  }

  std::unique_ptr<gfx::VSyncProvider> CreateVSyncProvider() override {
    return nullptr;
  }

  void* /* EGLConfig */ GetEGLSurfaceConfig(
      const EglConfigCallbacks& egl) override;

 private:
  gfx::AcceleratedWidget widget_;

  DISALLOW_COPY_AND_ASSIGN(X11SurfaceEGL);
};

void* /* EGLConfig */ X11SurfaceEGL::GetEGLSurfaceConfig(
    const EglConfigCallbacks& egl) {
  // Try matching the window depth with an alpha channel,
  // because we're worried the destination alpha width could
  // constrain blending precision.
  EGLConfig config;
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

  // Get the depth of XWindow for surface
  XWindowAttributes win_attribs;
  if (XGetWindowAttributes(gfx::GetXDisplay(), widget_, &win_attribs)) {
    config_attribs[kBufferSizeOffset] = win_attribs.depth;
  }

  EGLint num_configs;
  if (!egl.choose_config.Run(config_attribs, &config, 1, &num_configs)) {
    LOG(ERROR) << "eglChooseConfig failed with error "
               << egl.get_last_error_string.Run();
    return nullptr;
  }
  if (num_configs > 0) {
    EGLint config_depth;
    if (!egl.get_config_attribute.Run(config, EGL_BUFFER_SIZE, &config_depth)) {
      LOG(ERROR) << "eglGetConfigAttrib failed with error "
                 << egl.get_last_error_string.Run();
      return nullptr;
    }
    if (config_depth == config_attribs[kBufferSizeOffset]) {
      return config;
    }
  }

  // Try without an alpha channel.
  config_attribs[kAlphaSizeOffset] = 0;
  if (!egl.choose_config.Run(config_attribs, &config, 1, &num_configs)) {
    LOG(ERROR) << "eglChooseConfig failed with error "
               << egl.get_last_error_string.Run();
    return nullptr;
  }
  if (num_configs == 0) {
    LOG(ERROR) << "No suitable EGL configs found.";
    return nullptr;
  }
  return config;
}

}  // namespace

X11SurfaceFactory::X11SurfaceFactory() {}

X11SurfaceFactory::~X11SurfaceFactory() {}

std::unique_ptr<SurfaceOzoneEGL> X11SurfaceFactory::CreateEGLSurfaceForWidget(
    gfx::AcceleratedWidget widget) {
  return base::WrapUnique(new X11SurfaceEGL(widget));
}

bool X11SurfaceFactory::LoadEGLGLES2Bindings(
    AddGLLibraryCallback add_gl_library,
    SetGLGetProcAddressProcCallback set_gl_get_proc_address) {
  return LoadDefaultEGLGLES2Bindings(add_gl_library, set_gl_get_proc_address);
}

intptr_t X11SurfaceFactory::GetNativeDisplay() {
  return reinterpret_cast<intptr_t>(gfx::GetXDisplay());
}

}  // namespace ui
