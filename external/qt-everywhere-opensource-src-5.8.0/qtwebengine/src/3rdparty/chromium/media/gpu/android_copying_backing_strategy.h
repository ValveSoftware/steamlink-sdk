// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_COPYING_BACKING_STRATEGY_H_
#define MEDIA_GPU_ANDROID_COPYING_BACKING_STRATEGY_H_

#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "media/gpu/android_video_decode_accelerator.h"
#include "media/gpu/media_gpu_export.h"

namespace gpu {
class CopyTextureCHROMIUMResourceManager;
}

namespace media {
class PictureBuffer;
}

namespace media {

class AVDAStateProvider;

// A BackingStrategy implementation that copies images to PictureBuffer
// textures via gpu texture copy.
class MEDIA_GPU_EXPORT AndroidCopyingBackingStrategy
    : public AndroidVideoDecodeAccelerator::BackingStrategy {
 public:
  explicit AndroidCopyingBackingStrategy(AVDAStateProvider* state_provider);
  ~AndroidCopyingBackingStrategy() override;

  // AndroidVideoDecodeAccelerator::BackingStrategy
  gl::ScopedJavaSurface Initialize(int surface_view_id) override;
  void BeginCleanup(
      bool have_context,
      const AndroidVideoDecodeAccelerator::OutputBufferMap& buffers) override;
  void EndCleanup() override;
  scoped_refptr<gl::SurfaceTexture> GetSurfaceTexture() const override;
  uint32_t GetTextureTarget() const override;
  gfx::Size GetPictureBufferSize() const override;
  void UseCodecBufferForPictureBuffer(
      int32_t codec_buffer_index,
      const PictureBuffer& picture_buffer) override;
  void CodecChanged(VideoCodecBridge* codec) override;
  void OnFrameAvailable() override;
  bool ArePicturesOverlayable() override;
  void UpdatePictureBufferSize(PictureBuffer* picture_buffer,
                               const gfx::Size& new_size) override;

 private:
  // Used for copy the texture from surface texture to picture buffers.
  std::unique_ptr<gpu::CopyTextureCHROMIUMResourceManager> copier_;

  AVDAStateProvider* state_provider_;

  // A container of texture. Used to set a texture to |media_codec_|.
  scoped_refptr<gl::SurfaceTexture> surface_texture_;

  // The texture id which is set to |surface_texture_|.
  uint32_t surface_texture_id_;

  VideoCodecBridge* media_codec_;
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_COPYING_BACKING_STRATEGY_H_
