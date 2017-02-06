// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_AVDA_STATE_PROVIDER_H_
#define MEDIA_GPU_AVDA_STATE_PROVIDER_H_

#include "base/compiler_specific.h"
#include "base/threading/thread_checker.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "media/video/video_decode_accelerator.h"

namespace gl {
class SurfaceTexture;
}

namespace gpu {
namespace gles2 {
class GLES2Decoder;
}
}

namespace media {

// Helper class that provides the BackingStrategy with enough state
// to do useful work.
class AVDAStateProvider {
 public:
  // Various handy getters.
  virtual const gfx::Size& GetSize() const = 0;
  virtual const base::ThreadChecker& ThreadChecker() const = 0;
  virtual base::WeakPtr<gpu::gles2::GLES2Decoder> GetGlDecoder() const = 0;
  virtual gpu::gles2::TextureRef* GetTextureForPicture(
      const PictureBuffer& picture_buffer) = 0;

  // Create a SurfaceTexture and attach a new gl texture to it. |*service_id|
  // is set to the created texture id.
  virtual scoped_refptr<gl::SurfaceTexture> CreateAttachedSurfaceTexture(
      GLuint* service_id) = 0;

  // Helper function to report an error condition and stop decoding.
  // This will post NotifyError(), and transition to the error state.
  // It is meant to be called from the RETURN_ON_FAILURE macro.
  virtual void PostError(const ::tracked_objects::Location& from_here,
                         VideoDecodeAccelerator::Error error) = 0;

 protected:
  ~AVDAStateProvider() = default;
};

}  // namespace media

#endif  // MEDIA_GPU_AVDA_STATE_PROVIDER_H_
