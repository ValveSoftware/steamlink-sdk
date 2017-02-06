// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains an implementation of picture allocation for the
// X11 window system used by VaapiVideoDecodeAccelerator to produce
// output pictures.

#ifndef MEDIA_GPU_VAAPI_TFP_PICTURE_H_
#define MEDIA_GPU_VAAPI_TFP_PICTURE_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "media/gpu/vaapi_picture.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_bindings.h"

namespace gfx {
class GLContextGLX;
}

namespace gl {
class GLImageGLX;
}

namespace media {

class VaapiWrapper;

// Implementation of VaapiPicture for the X11 backed chromium.
class VaapiTFPPicture : public VaapiPicture {
 public:
  VaapiTFPPicture(const scoped_refptr<VaapiWrapper>& vaapi_wrapper,
                  const MakeGLContextCurrentCallback& make_context_current_cb,
                  const BindGLImageCallback& bind_image_cb,
                  int32_t picture_buffer_id,
                  const gfx::Size& size,
                  uint32_t texture_id,
                  uint32_t client_texture_id);

  ~VaapiTFPPicture() override;

  bool Allocate(gfx::BufferFormat format) override;
  bool ImportGpuMemoryBufferHandle(
      gfx::BufferFormat format,
      const gfx::GpuMemoryBufferHandle& gpu_memory_buffer_handle) override;

  bool DownloadFromSurface(const scoped_refptr<VASurface>& va_surface) override;

 private:
  bool Initialize();

  Display* x_display_;

  Pixmap x_pixmap_;
  scoped_refptr<gl::GLImageGLX> glx_image_;

  DISALLOW_COPY_AND_ASSIGN(VaapiTFPPicture);
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_TFP_PICTURE_H_
