// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains an interface of output pictures for the Vaapi
// video decoder. This is implemented by different window system
// (X11/Ozone) and used by VaapiVideoDecodeAccelerator to produce
// output pictures.

#ifndef MEDIA_GPU_VAAPI_PICTURE_H_
#define MEDIA_GPU_VAAPI_PICTURE_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/threading/non_thread_safe.h"
#include "media/gpu/gpu_video_decode_accelerator_helpers.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gl {
class GLImage;
}

namespace media {

class VASurface;
class VaapiWrapper;

// Picture is native pixmap abstraction (X11/Ozone).
class VaapiPicture : public base::NonThreadSafe {
 public:
  // Create a VaapiPicture of |size| to be associated with |picture_buffer_id|.
  // If provided, bind it to |texture_id|, as well as to |client_texture_id|
  // using |bind_image_cb|.
  static linked_ptr<VaapiPicture> CreatePicture(
      const scoped_refptr<VaapiWrapper>& vaapi_wrapper,
      const MakeGLContextCurrentCallback& make_context_current_cb,
      const BindGLImageCallback& bind_image_cb,
      int32_t picture_buffer_id,
      const gfx::Size& size,
      uint32_t texture_id,
      uint32_t client_texture_id);

  virtual ~VaapiPicture();

  // Use the buffer of |format|, pointed to by |gpu_memory_buffer_handle| as the
  // backing storage for this picture. This takes ownership of the handle and
  // will close it even on failure. Return true on success, false otherwise.
  virtual bool ImportGpuMemoryBufferHandle(
      gfx::BufferFormat format,
      const gfx::GpuMemoryBufferHandle& gpu_memory_buffer_handle) = 0;

  // Allocate a buffer of |format| to use as backing storage for this picture.
  // Return true on success.
  virtual bool Allocate(gfx::BufferFormat format) = 0;

  int32_t picture_buffer_id() const { return picture_buffer_id_; }

  virtual bool AllowOverlay() const;

  // Downloads the |va_surface| into the picture, potentially scaling
  // it if needed.
  virtual bool DownloadFromSurface(
      const scoped_refptr<VASurface>& va_surface) = 0;

  // Get the texture target used to bind EGLImages (either
  // GL_TEXTURE_2D on X11 or GL_TEXTURE_EXTERNAL_OES on DRM).
  static uint32_t GetGLTextureTarget();

 protected:
  VaapiPicture(const scoped_refptr<VaapiWrapper>& vaapi_wrapper,
               const MakeGLContextCurrentCallback& make_context_current_cb,
               const BindGLImageCallback& bind_image_cb,
               int32_t picture_buffer_id,
               const gfx::Size& size,
               uint32_t texture_id,
               uint32_t client_texture_id);

  scoped_refptr<VaapiWrapper> vaapi_wrapper_;

  const MakeGLContextCurrentCallback make_context_current_cb_;
  const BindGLImageCallback bind_image_cb_;

  const gfx::Size size_;
  const uint32_t texture_id_;
  const uint32_t client_texture_id_;

 private:
  int32_t picture_buffer_id_;

  DISALLOW_COPY_AND_ASSIGN(VaapiPicture);
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_PICTURE_H_
