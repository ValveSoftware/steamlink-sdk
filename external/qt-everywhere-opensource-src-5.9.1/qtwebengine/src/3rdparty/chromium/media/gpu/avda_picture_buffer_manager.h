// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_AVDA_PICTURE_BUFFER_MANAGER_H_
#define MEDIA_GPU_AVDA_PICTURE_BUFFER_MANAGER_H_

#include <stdint.h>
#include <vector>

#include "base/macros.h"
#include "media/gpu/avda_state_provider.h"
#include "media/gpu/media_gpu_export.h"

namespace gpu {
namespace gles2 {
class GLStreamTextureImage;
}
}

namespace gl {
class ScopedJavaSurface;
class SurfaceTexture;
}

namespace media {
class AVDACodecImage;
class AVDASharedState;
class VideoCodecBridge;

// AVDAPictureBufferManager is used by AVDA to associate its PictureBuffers with
// MediaCodec output buffers. It attaches AVDACodecImages to the PictureBuffer
// textures so that when they're used to draw the AVDACodecImage can release the
// MediaCodec buffer to the backing Surface. If the Surface is a SurfaceTexture,
// the front buffer can then be used to draw without needing to copy the pixels.
// If the Surface is a SurfaceView, the release causes the frame to be displayed
// immediately.
class MEDIA_GPU_EXPORT AVDAPictureBufferManager {
 public:
  using PictureBufferMap = std::map<int32_t, PictureBuffer>;

  explicit AVDAPictureBufferManager(AVDAStateProvider* state_provider);
  virtual ~AVDAPictureBufferManager();

  // Must be called before anything else. If |surface_id| is |kNoSurfaceID|
  // then a new SurfaceTexture will be returned. Otherwise, the corresponding
  // SurfaceView will be returned.
  //
  // May be called multiple times to switch to a new |surface_id|. Picture
  // buffers will be updated to use the new surface during the call to
  // UseCodecBufferForPictureBuffer().
  gl::ScopedJavaSurface Initialize(int surface_id);

  void Destroy(const PictureBufferMap& buffers);

  // Sets up |picture_buffer| so that its texture will refer to the image that
  // is represented by the decoded output buffer at codec_buffer_index.
  void UseCodecBufferForPictureBuffer(int32_t codec_buffer_index,
                                      const PictureBuffer& picture_buffer);

  // Assigns a picture buffer and attaches an image to its texture.
  void AssignOnePictureBuffer(const PictureBuffer& picture_buffer,
                              bool have_context);

  // Reuses a picture buffer to hold a new frame.
  void ReuseOnePictureBuffer(const PictureBuffer& picture_buffer);

  // Release MediaCodec buffers.
  void ReleaseCodecBuffers(const PictureBufferMap& buffers);

  // Attempts to free up codec output buffers by rendering early.
  void MaybeRenderEarly();

  // Called when the MediaCodec instance changes. If |codec| is nullptr the
  // MediaCodec is being destroyed. Previously provided codecs should no longer
  // be referenced.
  void CodecChanged(VideoCodecBridge* codec);

  // Whether the pictures buffers are overlayable.
  bool ArePicturesOverlayable();

  // Are there any unrendered picture buffers oustanding?
  bool HasUnrenderedPictures() const;

  // Returns the GL texture target that the PictureBuffer textures use.
  // Always use OES textures even though this will cause flickering in dev tools
  // when inspecting a fullscreen video.  See http://crbug.com/592798
  static constexpr GLenum kTextureTarget = GL_TEXTURE_EXTERNAL_OES;

 private:
  // Release any codec buffer that is associated with the given picture buffer
  // back to the codec.  It is okay if there is no such buffer.
  void ReleaseCodecBufferForPicture(const PictureBuffer& picture_buffer);

  // Sets up the texture references (as found by |picture_buffer|), for the
  // specified |image|. If |image| is null, clears any ref on the texture
  // associated with |picture_buffer|.
  void SetImageForPicture(const PictureBuffer& picture_buffer,
                          gpu::gles2::GLStreamTextureImage* image);

  AVDACodecImage* GetImageForPicture(int picture_buffer_id) const;

  scoped_refptr<AVDASharedState> shared_state_;

  AVDAStateProvider* const state_provider_;

  // The SurfaceTexture to render to. Non-null after Initialize() if
  // we're not rendering to a SurfaceView.
  scoped_refptr<gl::SurfaceTexture> surface_texture_;

  VideoCodecBridge* media_codec_;

  // Picture buffer IDs that are out for display. Stored in order of frames as
  // they are returned from the decoder.
  std::vector<int32_t> pictures_out_for_display_;

  // Maps a picture buffer id to a AVDACodecImage.
  std::map<int, scoped_refptr<AVDACodecImage>> codec_images_;

  DISALLOW_COPY_AND_ASSIGN(AVDAPictureBufferManager);
};

}  // namespace media

#endif  // MEDIA_GPU_AVDA_PICTURE_BUFFER_MANAGER_H_
