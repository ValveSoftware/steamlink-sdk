// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_AVDA_CODEC_IMAGE_H_
#define MEDIA_GPU_AVDA_CODEC_IMAGE_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "gpu/command_buffer/service/gl_stream_texture_image.h"
#include "media/gpu/avda_shared_state.h"

namespace ui {
class ScopedMakeCurrent;
}

namespace media {

class VideoCodecBridge;

// GLImage that renders MediaCodec buffers to a SurfaceTexture or SurfaceView as
// needed in order to draw them.
class AVDACodecImage : public gpu::gles2::GLStreamTextureImage {
 public:
  AVDACodecImage(int picture_buffer_id,
                 const scoped_refptr<AVDASharedState>& shared_state,
                 VideoCodecBridge* codec,
                 const base::WeakPtr<gpu::gles2::GLES2Decoder>& decoder,
                 const scoped_refptr<gl::SurfaceTexture>& surface_texture);

  // gl::GLImage implementation
  void Destroy(bool have_context) override;
  gfx::Size GetSize() override;
  unsigned GetInternalFormat() override;
  bool BindTexImage(unsigned target) override;
  void ReleaseTexImage(unsigned target) override;
  bool CopyTexImage(unsigned target) override;
  bool CopyTexSubImage(unsigned target,
                       const gfx::Point& offset,
                       const gfx::Rect& rect) override;
  bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                            int z_order,
                            gfx::OverlayTransform transform,
                            const gfx::Rect& bounds_rect,
                            const gfx::RectF& crop_rect) override;
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t process_tracing_id,
                    const std::string& dump_name) override;
  // gpu::gles2::GLStreamTextureMatrix implementation
  void GetTextureMatrix(float xform[16]) override;

  enum class UpdateMode {
    // Discards the codec buffer, no UpdateTexImage().
    DISCARD_CODEC_BUFFER,

    // Renders to back buffer, no UpdateTexImage(); can only be used with a
    // valid |surface_texture_|.
    RENDER_TO_BACK_BUFFER,

    // Renders to the back buffer. When used with a SurfaceView, promotion to
    // the front buffer is automatic. When using a |surface_texture_|,
    // UpdateTexImage() is called to promote the back buffer into the front.
    RENDER_TO_FRONT_BUFFER
  };

  // Releases the attached codec buffer (if not already released) indicated by
  // |codec_buffer_index_| and updates the surface if specified by the given
  // |update_mode|.  See UpdateMode documentation for details.
  void UpdateSurface(UpdateMode update_mode);

  // Updates the MediaCodec for this image; clears |codec_buffer_index_|.
  void CodecChanged(MediaCodecBridge* codec);

  void set_texture(gpu::gles2::Texture* texture) { texture_ = texture; }

  // Decoded buffer index that has the image for us to display.
  void set_media_codec_buffer_index(int buffer_index) {
    codec_buffer_index_ = buffer_index;
  }

  // Set the size of the current image.
  void set_size(const gfx::Size& size) { size_ = size; }

  // Indicates if the codec buffer has been released to the back buffer.
  bool was_rendered_to_back_buffer() const {
    return codec_buffer_index_ == kUpdateOnly;
  }

  // Indicates if the codec buffer has been released to the front buffer.
  bool was_rendered_to_front_buffer() const {
    return codec_buffer_index_ == kRendered;
  }

 protected:
  ~AVDACodecImage() override;

 private:
  // Make sure that the surface texture's front buffer is current.  This will
  // save / restore the current context.  It will optionally restore the texture
  // bindings in the surface texture's context, based on |mode|.  This is
  // intended as a hint if we don't need to change contexts.  If we do need to
  // change contexts, then we'll always preserve the texture bindings in the
  // both contexts.  In other words, the caller is telling us whether it's
  // okay to change the binding in the current context.
  enum RestoreBindingsMode { kDontRestoreBindings, kDoRestoreBindings };
  void UpdateSurfaceTexture(RestoreBindingsMode mode);

  // Internal helper for UpdateSurface() that allows callers to specify the
  // RestoreBindingsMode when a SurfaceTexture is already attached prior to
  // calling this method.
  void UpdateSurfaceInternal(UpdateMode update_mode,
                             RestoreBindingsMode attached_bindings_mode);

  // Releases the attached codec buffer (if not already released) indicated by
  // |codec_buffer_index_|. Never updates the actual surface. See UpdateMode
  // documentation for details. For the purposes of this function the values
  // RENDER_TO_FRONT_BUFFER and RENDER_TO_BACK_BUFFER do the same thing.
  void ReleaseOutputBuffer(UpdateMode update_mode);

  // Make shared_state_->context() current if it isn't already.
  std::unique_ptr<ui::ScopedMakeCurrent> MakeCurrentIfNeeded();

  // Return whether there is a codec buffer that we haven't rendered yet.  Will
  // return false also if there's no codec or we otherwise can't update.
  bool IsCodecBufferOutstanding() const;

  // Shared state between the AVDA and all AVDACodecImages.
  scoped_refptr<AVDASharedState> shared_state_;

  // The MediaCodec buffer index that we should render. Must be >= 0 or one of
  // the enum values below.
  enum { kUpdateOnly = -1, kRendered = -2, kInvalidCodecBufferIndex = -3 };
  int codec_buffer_index_;

  // Our image size.
  gfx::Size size_;

  // May be null.
  MediaCodecBridge* media_codec_;

  const base::WeakPtr<gpu::gles2::GLES2Decoder> decoder_;

  // The SurfaceTexture to render to. This is null when rendering to a
  // SurfaceView.
  const scoped_refptr<gl::SurfaceTexture> surface_texture_;

  // The texture that we're attached to.
  gpu::gles2::Texture* texture_;

  // Texture matrix of the front buffer of the surface texture.
  float gl_matrix_[16];

  // The picture buffer id attached to this image.
  int picture_buffer_id_;

  DISALLOW_COPY_AND_ASSIGN(AVDACodecImage);
};

}  // namespace media

#endif  // MEDIA_GPU_AVDA_CODEC_IMAGE_H_
