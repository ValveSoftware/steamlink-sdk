// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi_picture.h"
#include "media/gpu/vaapi_wrapper.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_implementation.h"

#if defined(USE_X11)
#include "media/gpu/vaapi_tfp_picture.h"
#elif defined(USE_OZONE)
#include "media/gpu/vaapi_drm_picture.h"
#endif

namespace media {

VaapiPicture::VaapiPicture(
    const scoped_refptr<VaapiWrapper>& vaapi_wrapper,
    const MakeGLContextCurrentCallback& make_context_current_cb,
    const BindGLImageCallback& bind_image_cb,
    int32_t picture_buffer_id,
    const gfx::Size& size,
    uint32_t texture_id,
    uint32_t client_texture_id)
    : vaapi_wrapper_(vaapi_wrapper),
      make_context_current_cb_(make_context_current_cb),
      bind_image_cb_(bind_image_cb),
      size_(size),
      texture_id_(texture_id),
      client_texture_id_(client_texture_id),
      picture_buffer_id_(picture_buffer_id) {}

VaapiPicture::~VaapiPicture() {}

// static
linked_ptr<VaapiPicture> VaapiPicture::CreatePicture(
    const scoped_refptr<VaapiWrapper>& vaapi_wrapper,
    const MakeGLContextCurrentCallback& make_context_current_cb,
    const BindGLImageCallback& bind_image_cb,
    int32_t picture_buffer_id,
    const gfx::Size& size,
    uint32_t texture_id,
    uint32_t client_texture_id) {
  linked_ptr<VaapiPicture> picture;
#if defined(USE_X11)
  picture.reset(new VaapiTFPPicture(vaapi_wrapper, make_context_current_cb,
                                    bind_image_cb, picture_buffer_id, size,
                                    texture_id, client_texture_id));
#elif defined(USE_OZONE)
  picture.reset(new VaapiDrmPicture(vaapi_wrapper, make_context_current_cb,
                                    bind_image_cb, picture_buffer_id, size,
                                    texture_id, client_texture_id));
#endif  // USE_X11

  return picture;
}

bool VaapiPicture::AllowOverlay() const {
  return false;
}

// static
uint32_t VaapiPicture::GetGLTextureTarget() {
#if defined(USE_OZONE)
  return GL_TEXTURE_EXTERNAL_OES;
#else
  return GL_TEXTURE_2D;
#endif
}

}  // namespace media
