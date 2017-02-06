// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/demo/surfaceless_gl_renderer.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/macros.h"
#include "base/trace_event/trace_event.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_image_ozone_native_pixmap.h"
#include "ui/gl/gl_surface.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace ui {

SurfacelessGlRenderer::BufferWrapper::BufferWrapper() {
}

SurfacelessGlRenderer::BufferWrapper::~BufferWrapper() {
  if (gl_fb_)
    glDeleteFramebuffersEXT(1, &gl_fb_);

  if (gl_tex_) {
    image_->ReleaseTexImage(GL_TEXTURE_2D);
    glDeleteTextures(1, &gl_tex_);
    image_->Destroy(true);
  }
}

bool SurfacelessGlRenderer::BufferWrapper::Initialize(
    gfx::AcceleratedWidget widget,
    const gfx::Size& size) {
  glGenFramebuffersEXT(1, &gl_fb_);
  glGenTextures(1, &gl_tex_);

  scoped_refptr<NativePixmap> pixmap =
      OzonePlatform::GetInstance()
          ->GetSurfaceFactoryOzone()
          ->CreateNativePixmap(widget, size, gfx::BufferFormat::BGRX_8888,
                               gfx::BufferUsage::SCANOUT);
  scoped_refptr<gl::GLImageOzoneNativePixmap> image(
      new gl::GLImageOzoneNativePixmap(size, GL_RGB));
  if (!image->Initialize(pixmap.get(), gfx::BufferFormat::BGRX_8888)) {
    LOG(ERROR) << "Failed to create GLImage";
    return false;
  }
  image_ = image;

  glBindFramebufferEXT(GL_FRAMEBUFFER, gl_fb_);
  glBindTexture(GL_TEXTURE_2D, gl_tex_);
  image_->BindTexImage(GL_TEXTURE_2D);

  glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                            gl_tex_, 0);
  if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    LOG(ERROR) << "Failed to create framebuffer "
               << glCheckFramebufferStatusEXT(GL_FRAMEBUFFER);
    return false;
  }

  widget_ = widget;
  size_ = size;

  return true;
}

void SurfacelessGlRenderer::BufferWrapper::BindFramebuffer() {
  glBindFramebufferEXT(GL_FRAMEBUFFER, gl_fb_);
}

SurfacelessGlRenderer::SurfacelessGlRenderer(
    gfx::AcceleratedWidget widget,
    const scoped_refptr<gl::GLSurface>& surface,
    const gfx::Size& size)
    : GlRenderer(widget, surface, size), weak_ptr_factory_(this) {}

SurfacelessGlRenderer::~SurfacelessGlRenderer() {
  // Need to make current when deleting the framebuffer resources allocated in
  // the buffers.
  context_->MakeCurrent(surface_.get());
}

bool SurfacelessGlRenderer::Initialize() {
  if (!GlRenderer::Initialize())
    return false;

  for (size_t i = 0; i < arraysize(buffers_); ++i) {
    buffers_[i].reset(new BufferWrapper());
    if (!buffers_[i]->Initialize(widget_, size_))
      return false;
  }

  PostRenderFrameTask(gfx::SwapResult::SWAP_ACK);
  return true;
}

void SurfacelessGlRenderer::RenderFrame() {
  TRACE_EVENT0("ozone", "SurfacelessGlRenderer::RenderFrame");

  float fraction = NextFraction();

  context_->MakeCurrent(surface_.get());
  buffers_[back_buffer_]->BindFramebuffer();

  glViewport(0, 0, size_.width(), size_.height());
  glClearColor(1 - fraction, 0.0, fraction, 1.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  surface_->ScheduleOverlayPlane(0, gfx::OVERLAY_TRANSFORM_NONE,
                                 buffers_[back_buffer_]->image(),
                                 gfx::Rect(size_), gfx::RectF(0, 0, 1, 1));
  back_buffer_ ^= 1;
  surface_->SwapBuffersAsync(
      base::Bind(&SurfacelessGlRenderer::PostRenderFrameTask,
                 weak_ptr_factory_.GetWeakPtr()));
}

void SurfacelessGlRenderer::PostRenderFrameTask(gfx::SwapResult result) {
  switch (result) {
    case gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS:
      for (size_t i = 0; i < arraysize(buffers_); ++i) {
        buffers_[i].reset(new BufferWrapper());
        if (!buffers_[i]->Initialize(widget_, size_))
          LOG(FATAL) << "Failed to recreate buffer";
      }
    // Fall through since we want to render a new frame anyways.
    case gfx::SwapResult::SWAP_ACK:
      GlRenderer::PostRenderFrameTask(result);
      break;
    case gfx::SwapResult::SWAP_FAILED:
      LOG(FATAL) << "Failed to swap buffers";
      break;
  }
}

}  // namespace ui
