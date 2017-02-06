// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_egl_surface.h"

#include <wayland-egl.h>

#include "third_party/khronos/EGL/egl.h"
#include "ui/ozone/common/egl_util.h"
#include "ui/ozone/platform/wayland/wayland_window.h"

namespace ui {

void EGLWindowDeleter::operator()(wl_egl_window* egl_window) {
  wl_egl_window_destroy(egl_window);
}

WaylandEGLSurface::WaylandEGLSurface(WaylandWindow* window,
                                     const gfx::Size& size)
    : window_(window), size_(size) {}

WaylandEGLSurface::~WaylandEGLSurface() {}

bool WaylandEGLSurface::Initialize() {
  egl_window_.reset(
      wl_egl_window_create(window_->surface(), size_.width(), size_.height()));
  return egl_window_ != nullptr;
}

intptr_t WaylandEGLSurface::GetNativeWindow() {
  DCHECK(egl_window_);
  return reinterpret_cast<intptr_t>(egl_window_.get());
}

bool WaylandEGLSurface::ResizeNativeWindow(const gfx::Size& viewport_size) {
  if (size_ == viewport_size)
    return true;
  DCHECK(egl_window_);
  wl_egl_window_resize(egl_window_.get(), size_.width(), size_.height(), 0, 0);
  size_ = viewport_size;
  return true;
}

bool WaylandEGLSurface::OnSwapBuffers() {
  return true;
}

void WaylandEGLSurface::OnSwapBuffersAsync(
    const SwapCompletionCallback& callback) {
  NOTREACHED();
}

std::unique_ptr<gfx::VSyncProvider> WaylandEGLSurface::CreateVSyncProvider() {
  NOTIMPLEMENTED();
  return nullptr;
}

void* /* EGLConfig */ WaylandEGLSurface::GetEGLSurfaceConfig(
    const EglConfigCallbacks& egl) {
  EGLint config_attribs[] = {EGL_BUFFER_SIZE,
                             32,
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
  return ChooseEGLConfig(egl, config_attribs);
}

}  // namespace ui
