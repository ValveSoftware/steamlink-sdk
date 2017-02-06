// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/cast/surface_ozone_egl_cast.h"

#include "third_party/khronos/EGL/egl.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/ozone/common/egl_util.h"
#include "ui/ozone/platform/cast/surface_factory_cast.h"

namespace ui {

SurfaceOzoneEglCast::~SurfaceOzoneEglCast() {
  parent_->ChildDestroyed();
}

intptr_t SurfaceOzoneEglCast::GetNativeWindow() {
  return reinterpret_cast<intptr_t>(parent_->GetNativeWindow());
}

bool SurfaceOzoneEglCast::OnSwapBuffers() {
  parent_->OnSwapBuffers();
  return true;
}

void SurfaceOzoneEglCast::OnSwapBuffersAsync(
    const SwapCompletionCallback& callback) {
  parent_->OnSwapBuffers();
  callback.Run(gfx::SwapResult::SWAP_ACK);
}

bool SurfaceOzoneEglCast::ResizeNativeWindow(const gfx::Size& viewport_size) {
  return parent_->ResizeDisplay(viewport_size);
}

std::unique_ptr<gfx::VSyncProvider> SurfaceOzoneEglCast::CreateVSyncProvider() {
  return nullptr;
}

void* /* EGLConfig */ SurfaceOzoneEglCast::GetEGLSurfaceConfig(
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
