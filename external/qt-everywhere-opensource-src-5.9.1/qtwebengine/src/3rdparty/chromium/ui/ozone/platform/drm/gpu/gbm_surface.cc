// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/gbm_surface.h"

#include <utility>

#include "base/logging.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/ozone/gl/gl_image_ozone_native_pixmap.h"
#include "ui/ozone/platform/drm/gpu/drm_window_proxy.h"
#include "ui/ozone/platform/drm/gpu/gbm_surface_factory.h"
#include "ui/ozone/public/native_pixmap.h"

namespace ui {

GbmSurface::GbmSurface(GbmSurfaceFactory* surface_factory,
                       std::unique_ptr<DrmWindowProxy> window,
                       gfx::AcceleratedWidget widget)
    : GbmSurfaceless(surface_factory, std::move(window), widget) {
  for (auto& texture : textures_)
    texture = 0;
}

unsigned int GbmSurface::GetBackingFramebufferObject() {
  return fbo_;
}

bool GbmSurface::OnMakeCurrent(gl::GLContext* context) {
  DCHECK(!context_ || context == context_);
  context_ = context;
  if (!fbo_) {
    glGenFramebuffersEXT(1, &fbo_);
    if (!fbo_)
      return false;
    glGenTextures(arraysize(textures_), textures_);
    if (!CreatePixmaps())
      return false;
  }
  BindFramebuffer();
  glBindFramebufferEXT(GL_FRAMEBUFFER, fbo_);
  return SurfacelessEGL::OnMakeCurrent(context);
}

bool GbmSurface::Resize(const gfx::Size& size,
                        float scale_factor,
                        bool has_alpha) {
  if (size == GetSize())
    return true;
  // Alpha value isn't actually used in allocating buffers yet, so always use
  // true instead.
  return GbmSurfaceless::Resize(size, scale_factor, true) && CreatePixmaps();
}

bool GbmSurface::SupportsPostSubBuffer() {
  return false;
}

void GbmSurface::SwapBuffersAsync(const SwapCompletionCallback& callback) {
  if (!images_[current_surface_]->ScheduleOverlayPlane(
          widget(), 0, gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE,
          gfx::Rect(GetSize()), gfx::RectF(1, 1))) {
    callback.Run(gfx::SwapResult::SWAP_FAILED);
    return;
  }
  GbmSurfaceless::SwapBuffersAsync(callback);
  current_surface_ ^= 1;
  BindFramebuffer();
}

void GbmSurface::Destroy() {
  if (!context_)
    return;
  scoped_refptr<gl::GLContext> previous_context = gl::GLContext::GetCurrent();
  scoped_refptr<GLSurface> previous_surface;

  bool was_current = previous_context && previous_context->IsCurrent(nullptr) &&
                     GLSurface::GetCurrent() == this;
  if (!was_current) {
    // Only take a reference to previous surface if it's not |this|
    // because otherwise we can take a self reference from our own dtor.
    previous_surface = GLSurface::GetCurrent();
    context_->MakeCurrent(this);
  }

  glBindFramebufferEXT(GL_FRAMEBUFFER, 0);
  if (fbo_) {
    glDeleteTextures(arraysize(textures_), textures_);
    for (auto& texture : textures_)
      texture = 0;
    glDeleteFramebuffersEXT(1, &fbo_);
    fbo_ = 0;
  }
  for (auto& image : images_)
    image = nullptr;

  if (!was_current) {
    if (previous_context) {
      previous_context->MakeCurrent(previous_surface.get());
    } else {
      context_->ReleaseCurrent(this);
    }
  }
}

bool GbmSurface::IsSurfaceless() const {
  return false;
}

GbmSurface::~GbmSurface() {
  Destroy();
}

void GbmSurface::BindFramebuffer() {
  gl::ScopedFramebufferBinder fb(fbo_);
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                            textures_[current_surface_], 0);
}

bool GbmSurface::CreatePixmaps() {
  if (!fbo_)
    return true;
  for (size_t i = 0; i < arraysize(textures_); i++) {
    scoped_refptr<NativePixmap> pixmap = surface_factory()->CreateNativePixmap(
        widget(), GetSize(), gfx::BufferFormat::BGRA_8888,
        gfx::BufferUsage::SCANOUT);
    if (!pixmap)
      return false;
    scoped_refptr<GLImageOzoneNativePixmap> image =
        new GLImageOzoneNativePixmap(GetSize(), GL_BGRA_EXT);
    if (!image->Initialize(pixmap.get(), gfx::BufferFormat::BGRA_8888))
      return false;
    images_[i] = image;
    // Bind image to texture.
    gl::ScopedTextureBinder binder(GL_TEXTURE_2D, textures_[i]);
    if (!images_[i]->BindTexImage(GL_TEXTURE_2D))
      return false;
  }
  return true;
}

}  // namespace ui
