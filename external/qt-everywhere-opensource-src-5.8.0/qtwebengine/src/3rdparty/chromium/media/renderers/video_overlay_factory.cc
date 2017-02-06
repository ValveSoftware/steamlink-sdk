// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/video_overlay_factory.h"

#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "media/base/video_frame.h"
#include "media/renderers/gpu_video_accelerator_factories.h"

namespace media {

class VideoOverlayFactory::Texture {
 public:
  explicit Texture(GpuVideoAcceleratorFactories* gpu_factories)
      : gpu_factories_(gpu_factories), texture_id_(0), image_id_(0) {
    DCHECK(gpu_factories_);
    DCHECK(gpu_factories_->GetTaskRunner()->BelongsToCurrentThread());

    std::unique_ptr<GpuVideoAcceleratorFactories::ScopedGLContextLock> lock(
        gpu_factories_->GetGLContextLock());
    CHECK(lock);
    gpu::gles2::GLES2Interface* gl = lock->ContextGL();

    gl->GenTextures(1, &texture_id_);
    gl->BindTexture(GL_TEXTURE_2D, texture_id_);
    image_id_ = gl->CreateGpuMemoryBufferImageCHROMIUM(1, 1, GL_RGBA,
                                                       GL_READ_WRITE_CHROMIUM);
    CHECK(image_id_);
    gl->BindTexImage2DCHROMIUM(GL_TEXTURE_2D, image_id_);

    gl->GenMailboxCHROMIUM(mailbox_.name);
    gl->ProduceTextureDirectCHROMIUM(texture_id_, GL_TEXTURE_2D, mailbox_.name);

    const GLuint64 fence_sync = gl->InsertFenceSyncCHROMIUM();
    gl->ShallowFlushCHROMIUM();
    gl->GenSyncTokenCHROMIUM(fence_sync, sync_token_.GetData());
  }

  ~Texture() {
    DCHECK(gpu_factories_->GetTaskRunner()->BelongsToCurrentThread());

    std::unique_ptr<GpuVideoAcceleratorFactories::ScopedGLContextLock> lock(
        gpu_factories_->GetGLContextLock());
    CHECK(lock);
    gpu::gles2::GLES2Interface* gl = lock->ContextGL();
    gl->BindTexture(GL_TEXTURE_2D, texture_id_);
    gl->ReleaseTexImage2DCHROMIUM(GL_TEXTURE_2D, image_id_);
    gl->DeleteTextures(1, &texture_id_);
    gl->DestroyImageCHROMIUM(image_id_);
  }

 private:
  friend class VideoOverlayFactory;
  GpuVideoAcceleratorFactories* gpu_factories_;

  GLuint texture_id_;
  GLuint image_id_;
  gpu::Mailbox mailbox_;
  gpu::SyncToken sync_token_;
};

VideoOverlayFactory::VideoOverlayFactory(
    GpuVideoAcceleratorFactories* gpu_factories)
    : gpu_factories_(gpu_factories) {}

VideoOverlayFactory::~VideoOverlayFactory() {}

scoped_refptr<VideoFrame> VideoOverlayFactory::CreateFrame(
    const gfx::Size& size) {
  // Frame size empty => video has one dimension = 0.
  // Dimension 0 case triggers a DCHECK later on in TextureMailbox if we push
  // through the overlay path.
  if (size.IsEmpty() || !gpu_factories_) {
    DVLOG(1) << "Create black frame " << size.width() << "x" << size.height();
    return VideoFrame::CreateBlackFrame(gfx::Size(1, 1));
  }

  DCHECK(gpu_factories_);
  DCHECK(gpu_factories_->GetTaskRunner()->BelongsToCurrentThread());

  // Lazily create overlay texture.
  if (!texture_)
    texture_.reset(new Texture(gpu_factories_));

  DVLOG(1) << "Create video overlay frame: " << size.ToString();
  gpu::MailboxHolder holders[VideoFrame::kMaxPlanes] = {gpu::MailboxHolder(
      texture_->mailbox_, texture_->sync_token_, GL_TEXTURE_2D)};
  scoped_refptr<VideoFrame> frame = VideoFrame::WrapNativeTextures(
      PIXEL_FORMAT_XRGB, holders, VideoFrame::ReleaseMailboxCB(),
      size,                // coded_size
      gfx::Rect(size),     // visible rect
      size,                // natural size
      base::TimeDelta());  // timestamp
  CHECK(frame);
  frame->metadata()->SetBoolean(VideoFrameMetadata::ALLOW_OVERLAY, true);
  // TODO(halliwell): this is to block idle suspend behaviour on Chromecast.
  // Find a better way to do this.
  frame->metadata()->SetBoolean(VideoFrameMetadata::DECODER_OWNS_FRAME, true);
  return frame;
}

}  // namespace media
