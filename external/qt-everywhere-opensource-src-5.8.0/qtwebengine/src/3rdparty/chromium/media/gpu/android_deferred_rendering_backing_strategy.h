// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_DEFERRED_RENDERING_BACKING_STRATEGY_H_
#define MEDIA_GPU_ANDROID_DEFERRED_RENDERING_BACKING_STRATEGY_H_

#include <stdint.h>
#include <vector>

#include "base/macros.h"
#include "media/gpu/android_video_decode_accelerator.h"
#include "media/gpu/media_gpu_export.h"

namespace gl {
class GLImage;
}

namespace gpu {
namespace gles2 {
class GLStreamTextureImage;
class TextureRef;
}
}

namespace media {

class AVDACodecImage;
class AVDASharedState;

// A BackingStrategy implementation that defers releasing codec buffers until
// a PictureBuffer's texture is used to draw, then draws using the surface
// texture's front buffer rather than a copy.  To do this, it uses a GLImage
// implementation to talk to MediaCodec.
class MEDIA_GPU_EXPORT AndroidDeferredRenderingBackingStrategy
    : public AndroidVideoDecodeAccelerator::BackingStrategy {
 public:
  explicit AndroidDeferredRenderingBackingStrategy(
      AVDAStateProvider* state_provider);
  ~AndroidDeferredRenderingBackingStrategy() override;

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
  void AssignOnePictureBuffer(const PictureBuffer&, bool) override;
  void ReuseOnePictureBuffer(const PictureBuffer& picture_buffer) override;
  void MaybeRenderEarly() override;
  void CodecChanged(VideoCodecBridge* codec) override;
  void ReleaseCodecBuffers(
      const AndroidVideoDecodeAccelerator::OutputBufferMap& buffers) override;
  void OnFrameAvailable() override;
  bool ArePicturesOverlayable() override;
  void UpdatePictureBufferSize(PictureBuffer* picture_buffer,
                               const gfx::Size& new_size) override;

 private:
  // Release any codec buffer that is associated with the given picture buffer
  // back to the codec.  It is okay if there is no such buffer.
  void ReleaseCodecBufferForPicture(const PictureBuffer& picture_buffer);

  // Sets up the texture references (as found by |picture_buffer|), for the
  // specified |image|. If |image| is null, clears any ref on the texture
  // associated with |picture_buffer|.
  void SetImageForPicture(
      const PictureBuffer& picture_buffer,
      const scoped_refptr<gpu::gles2::GLStreamTextureImage>& image);

  // Make a copy of the SurfaceTexture's front buffer and associate all given
  // picture buffer textures with it. The picture buffer textures will not
  // dependend on |this|, the SurfaceTexture, the MediaCodec or the VDA, so it's
  // used to back the picture buffers when the VDA is being destroyed.
  void CopySurfaceTextureToPictures(
      const AndroidVideoDecodeAccelerator::OutputBufferMap& buffers);

  // Return true if and only if CopySurfaceTextureToPictures is expected to work
  // on this device.
  bool ShouldCopyPictures() const;

  scoped_refptr<AVDASharedState> shared_state_;

  AVDAStateProvider* state_provider_;

  // The SurfaceTexture to render to. Non-null after Initialize() if
  // we're not rendering to a SurfaceView.
  scoped_refptr<gl::SurfaceTexture> surface_texture_;

  VideoCodecBridge* media_codec_;

  // Picture buffer IDs that are out for display. Stored in order of frames as
  // they are returned from the decoder.
  std::vector<int32_t> pictures_out_for_display_;

  DISALLOW_COPY_AND_ASSIGN(AndroidDeferredRenderingBackingStrategy);
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_DEFERRED_RENDERING_BACKING_STRATEGY_H_
