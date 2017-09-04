// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/cast/gl_surface_cast.h"

#include "ui/ozone/common/egl_util.h"
#include "ui/ozone/platform/cast/surface_factory_cast.h"

namespace ui {

GLSurfaceCast::GLSurfaceCast(gfx::AcceleratedWidget widget,
                             SurfaceFactoryCast* parent)
    : NativeViewGLSurfaceEGL(parent->GetNativeWindow()),
      widget_(widget),
      parent_(parent) {
  DCHECK(parent_);
}

gfx::SwapResult GLSurfaceCast::SwapBuffers() {
  gfx::SwapResult result = NativeViewGLSurfaceEGL::SwapBuffers();
  if (result == gfx::SwapResult::SWAP_ACK)
    parent_->OnSwapBuffers();

  return result;
}

gfx::SwapResult GLSurfaceCast::SwapBuffersWithDamage(int x,
                                                     int y,
                                                     int width,
                                                     int height) {
  gfx::SwapResult result =
      NativeViewGLSurfaceEGL::SwapBuffersWithDamage(x, y, width, height);
  if (result == gfx::SwapResult::SWAP_ACK)
    parent_->OnSwapBuffers();

  return result;
}

bool GLSurfaceCast::Resize(const gfx::Size& size,
                           float scale_factor,
                           bool has_alpha) {
  return parent_->ResizeDisplay(size) &&
         NativeViewGLSurfaceEGL::Resize(size, scale_factor, has_alpha);
}

bool GLSurfaceCast::ScheduleOverlayPlane(int z_order,
                                         gfx::OverlayTransform transform,
                                         gl::GLImage* image,
                                         const gfx::Rect& bounds_rect,
                                         const gfx::RectF& crop_rect) {
  return image->ScheduleOverlayPlane(widget_, z_order, transform, bounds_rect,
                                     crop_rect);
}

EGLConfig GLSurfaceCast::GetConfig() {
  if (!config_) {
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
    config_ = ChooseEGLConfig(GetDisplay(), config_attribs);
  }
  return config_;
}

GLSurfaceCast::~GLSurfaceCast() {
  Destroy();
  parent_->ChildDestroyed();
}

}  // namespace ui
