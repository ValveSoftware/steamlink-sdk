// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_WAYLAND_EGL_SURFACE_H_
#define UI_OZONE_PLATFORM_WAYLAND_WAYLAND_EGL_SURFACE_H_

#include <memory>

#include "ui/gfx/geometry/size.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/ozone/public/surface_ozone_egl.h"

struct wl_egl_window;

namespace ui {

class WaylandWindow;

struct EGLWindowDeleter {
  void operator()(wl_egl_window* egl_window);
};

class WaylandEGLSurface : public SurfaceOzoneEGL {
 public:
  WaylandEGLSurface(WaylandWindow* window, const gfx::Size& size);
  ~WaylandEGLSurface() override;

  bool Initialize();

  // SurfaceOzoneEGL
  intptr_t GetNativeWindow() override;
  bool ResizeNativeWindow(const gfx::Size& viewport_size) override;
  bool OnSwapBuffers() override;
  void OnSwapBuffersAsync(const SwapCompletionCallback& callback) override;
  std::unique_ptr<gfx::VSyncProvider> CreateVSyncProvider() override;
  void* /* EGLConfig */ GetEGLSurfaceConfig(
      const EglConfigCallbacks& egl) override;

 private:
  WaylandWindow* window_;

  gfx::Size size_;
  std::unique_ptr<wl_egl_window, EGLWindowDeleter> egl_window_;

  DISALLOW_COPY_AND_ASSIGN(WaylandEGLSurface);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_WAYLAND_EGL_SURFACE_H_
