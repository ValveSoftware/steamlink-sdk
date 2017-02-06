// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_GPU_GPU_VIDEO_DECODE_ACCELERATOR_FACTORY_H_
#define CONTENT_PUBLIC_GPU_GPU_VIDEO_DECODE_ACCELERATOR_FACTORY_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "gpu/config/gpu_info.h"
#include "media/video/video_decode_accelerator.h"

namespace gl {
class GLContext;
class GLImage;
}

namespace gpu {
namespace gles2 {
class GLES2Decoder;
}
}

namespace media {
class GpuVideoDecodeAcceleratorFactoryImpl;
}

namespace content {

// This factory allows creation of VideoDecodeAccelerator implementations,
// providing the most applicable VDA for current platform and given
// configuration. To be used in GPU process only.
class CONTENT_EXPORT GpuVideoDecodeAcceleratorFactory {
 public:
  virtual ~GpuVideoDecodeAcceleratorFactory();

  // Return current GLContext.
  using GetGLContextCallback = base::Callback<gl::GLContext*(void)>;

  // Make the applicable GL context current. To be called by VDAs before
  // executing any GL calls. Return true on success, false otherwise.
  using MakeGLContextCurrentCallback = base::Callback<bool(void)>;

  // Bind |image| to |client_texture_id| given |texture_target|. If
  // |can_bind_to_sampler| is true, then the image may be used as a sampler
  // directly, otherwise a copy to a staging buffer is required.
  // Return true on success, false otherwise.
  using BindGLImageCallback =
      base::Callback<bool(uint32_t client_texture_id,
                          uint32_t texture_target,
                          const scoped_refptr<gl::GLImage>& image,
                          bool can_bind_to_sampler)>;

  // Return a WeakPtr to a GLES2Decoder, if one is available.
  using GetGLES2DecoderCallback =
      base::Callback<base::WeakPtr<gpu::gles2::GLES2Decoder>(void)>;

  // Create a factory capable of producing VDA instances for current platform.
  static std::unique_ptr<GpuVideoDecodeAcceleratorFactory> Create(
      const GetGLContextCallback& get_gl_context_cb,
      const MakeGLContextCurrentCallback& make_context_current_cb,
      const BindGLImageCallback& bind_image_cb);

  static std::unique_ptr<GpuVideoDecodeAcceleratorFactory>
  CreateWithGLES2Decoder(
      const GetGLContextCallback& get_gl_context_cb,
      const MakeGLContextCurrentCallback& make_context_current_cb,
      const BindGLImageCallback& bind_image_cb,
      const GetGLES2DecoderCallback& get_gles2_decoder_cb);

  // Create a factory capable of producing VDA instances for current platform
  // with no GL support.
  // A factory created with this method will only be able to produce VDAs with
  // no ability to call GL functions/access GL state. This also implies no
  // ability to decode into textures provided by the client.
  static std::unique_ptr<GpuVideoDecodeAcceleratorFactory> CreateWithNoGL();

  // Return decoder capabilities supported on the current platform.
  static gpu::VideoDecodeAcceleratorCapabilities GetDecoderCapabilities();

  // Create a VDA for the current platform for |client| with the given |config|
  // and for given |gpu_preferences|. Return nullptr on failure.
  virtual std::unique_ptr<media::VideoDecodeAccelerator> CreateVDA(
      media::VideoDecodeAccelerator::Client* client,
      const media::VideoDecodeAccelerator::Config& config);

 private:
  // TODO(posciak): This is temporary and will not be needed once
  // media::GpuVideoDecodeAcceleratorFactoryImpl implements
  // GpuVideoDecodeAcceleratorFactory, see crbug.com/597150 and related.
  GpuVideoDecodeAcceleratorFactory(
      std::unique_ptr<media::GpuVideoDecodeAcceleratorFactoryImpl>
      gvdafactory_impl);

  std::unique_ptr<media::GpuVideoDecodeAcceleratorFactoryImpl>
      gvdafactory_impl_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_GPU_GPU_VIDEO_DECODE_ACCELERATOR_FACTORY_H_
