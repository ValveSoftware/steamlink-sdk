// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi_tfp_picture.h"

#include <X11/Xlib.h>

#include "media/gpu/va_surface.h"
#include "media/gpu/vaapi_wrapper.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_image_glx.h"
#include "ui/gl/scoped_binders.h"

namespace media {

VaapiTFPPicture::VaapiTFPPicture(
    const scoped_refptr<VaapiWrapper>& vaapi_wrapper,
    const MakeGLContextCurrentCallback& make_context_current_cb,
    const BindGLImageCallback& bind_image_cb,
    int32_t picture_buffer_id,
    const gfx::Size& size,
    uint32_t texture_id,
    uint32_t client_texture_id)
    : VaapiPicture(vaapi_wrapper,
                   make_context_current_cb,
                   bind_image_cb,
                   picture_buffer_id,
                   size,
                   texture_id,
                   client_texture_id),
      x_display_(gfx::GetXDisplay()),
      x_pixmap_(0) {}

VaapiTFPPicture::~VaapiTFPPicture() {
  if (glx_image_.get() && make_context_current_cb_.Run()) {
    glx_image_->ReleaseTexImage(GL_TEXTURE_2D);
    DCHECK_EQ(glGetError(), static_cast<GLenum>(GL_NO_ERROR));
  }

  if (x_pixmap_)
    XFreePixmap(x_display_, x_pixmap_);
}

bool VaapiTFPPicture::Initialize() {
  DCHECK(x_pixmap_);

  if (texture_id_ != 0 && !make_context_current_cb_.is_null()) {
    if (!make_context_current_cb_.Run())
      return false;

    glx_image_ = new gl::GLImageGLX(size_, GL_RGB);
    if (!glx_image_->Initialize(x_pixmap_)) {
      // x_pixmap_ will be freed in the destructor.
      LOG(ERROR) << "Failed creating a GLX Pixmap for TFP";
      return false;
    }

    gl::ScopedTextureBinder texture_binder(GL_TEXTURE_2D, texture_id_);
    if (!glx_image_->BindTexImage(GL_TEXTURE_2D)) {
      LOG(ERROR) << "Failed to bind texture to glx image";
      return false;
    }
  }

  return true;
}

bool VaapiTFPPicture::Allocate(gfx::BufferFormat format) {
  if (format != gfx::BufferFormat::BGRX_8888 &&
      format != gfx::BufferFormat::BGRA_8888) {
    LOG(ERROR) << "Unsupported format";
    return false;
  }

  XWindowAttributes win_attr;
  int screen = DefaultScreen(x_display_);
  XGetWindowAttributes(x_display_, RootWindow(x_display_, screen), &win_attr);
  // TODO(posciak): pass the depth required by libva, not the RootWindow's
  // depth
  x_pixmap_ = XCreatePixmap(x_display_, RootWindow(x_display_, screen),
                            size_.width(), size_.height(), win_attr.depth);
  if (!x_pixmap_) {
    LOG(ERROR) << "Failed creating an X Pixmap for TFP";
    return false;
  }

  return Initialize();
}

bool VaapiTFPPicture::ImportGpuMemoryBufferHandle(
    gfx::BufferFormat format,
    const gfx::GpuMemoryBufferHandle& gpu_memory_buffer_handle) {
  NOTIMPLEMENTED() << "GpuMemoryBufferHandle import not implemented";
  return false;
}

bool VaapiTFPPicture::DownloadFromSurface(
    const scoped_refptr<VASurface>& va_surface) {
  return vaapi_wrapper_->PutSurfaceIntoPixmap(va_surface->id(), x_pixmap_,
                                              va_surface->size());
}

}  // namespace media
