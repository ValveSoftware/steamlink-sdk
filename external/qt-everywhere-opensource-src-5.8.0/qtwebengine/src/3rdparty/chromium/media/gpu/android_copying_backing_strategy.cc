// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android_copying_backing_strategy.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gles2_cmd_copy_texture_chromium.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "media/base/limits.h"
#include "media/gpu/avda_return_on_failure.h"
#include "media/video/picture.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_bindings.h"

namespace media {

AndroidCopyingBackingStrategy::AndroidCopyingBackingStrategy(
    AVDAStateProvider* state_provider)
    : state_provider_(state_provider),
      surface_texture_id_(0),
      media_codec_(nullptr) {}

AndroidCopyingBackingStrategy::~AndroidCopyingBackingStrategy() {}

gl::ScopedJavaSurface AndroidCopyingBackingStrategy::Initialize(
    int surface_view_id) {
  if (surface_view_id != VideoDecodeAccelerator::Config::kNoSurfaceID) {
    LOG(ERROR) << "The copying strategy should not be initialized with a "
                  "surface id.";
    return gl::ScopedJavaSurface();
  }

  surface_texture_ =
      state_provider_->CreateAttachedSurfaceTexture(&surface_texture_id_);
  return gl::ScopedJavaSurface(surface_texture_.get());
}

void AndroidCopyingBackingStrategy::BeginCleanup(
    bool have_context,
    const AndroidVideoDecodeAccelerator::OutputBufferMap& buffers) {
  DCHECK(state_provider_->ThreadChecker().CalledOnValidThread());

  if (copier_)
    copier_->Destroy();

  if (surface_texture_id_ && have_context)
    glDeleteTextures(1, &surface_texture_id_);
}

void AndroidCopyingBackingStrategy::EndCleanup() {}

scoped_refptr<gl::SurfaceTexture>
AndroidCopyingBackingStrategy::GetSurfaceTexture() const {
  return surface_texture_;
}

uint32_t AndroidCopyingBackingStrategy::GetTextureTarget() const {
  return GL_TEXTURE_2D;
}

gfx::Size AndroidCopyingBackingStrategy::GetPictureBufferSize() const {
  return state_provider_->GetSize();
}

void AndroidCopyingBackingStrategy::UseCodecBufferForPictureBuffer(
    int32_t codec_buf_index,
    const PictureBuffer& picture_buffer) {
  // Make sure that the decoder is available.
  RETURN_ON_FAILURE(state_provider_, state_provider_->GetGlDecoder().get(),
                    "Failed to get gles2 decoder instance.", ILLEGAL_STATE);

  // Render the codec buffer into |surface_texture_|, and switch it to be
  // the front buffer.
  // This ignores the emitted ByteBuffer and instead relies on rendering to
  // the codec's SurfaceTexture and then copying from that texture to the
  // client's PictureBuffer's texture.  This means that each picture's data
  // is written three times: once to the ByteBuffer, once to the
  // SurfaceTexture, and once to the client's texture.  It would be nicer to
  // either:
  // 1) Render directly to the client's texture from MediaCodec (one write);
  //    or
  // 2) Upload the ByteBuffer to the client's texture (two writes).
  // Unfortunately neither is possible:
  // 1) MediaCodec's use of SurfaceTexture is a singleton, and the texture
  //    written to can't change during the codec's lifetime.  b/11990461
  // 2) The ByteBuffer is likely to contain the pixels in a vendor-specific,
  //    opaque/non-standard format.  It's not possible to negotiate the
  //    decoder to emit a specific colorspace, even using HW CSC.  b/10706245
  // So, we live with these two extra copies per picture :(
  {
    TRACE_EVENT0("media", "AVDA::ReleaseOutputBuffer");
    media_codec_->ReleaseOutputBuffer(codec_buf_index, true);
  }

  {
    TRACE_EVENT0("media", "AVDA::UpdateTexImage");
    surface_texture_->UpdateTexImage();
  }

  float transform_matrix[16];
  surface_texture_->GetTransformMatrix(transform_matrix);

  DCHECK_LE(1u, picture_buffer.texture_ids().size());
  uint32_t picture_buffer_texture_id = picture_buffer.texture_ids()[0];

  // Defer initializing the CopyTextureCHROMIUMResourceManager until it is
  // needed because it takes 10s of milliseconds to initialize.
  if (!copier_) {
    copier_.reset(new gpu::CopyTextureCHROMIUMResourceManager());
    copier_->Initialize(state_provider_->GetGlDecoder().get(),
                        state_provider_->GetGlDecoder()
                            ->GetContextGroup()
                            ->feature_info()
                            ->feature_flags());
  }

  // Here, we copy |surface_texture_id_| to the picture buffer instead of
  // setting new texture to |surface_texture_| by calling attachToGLContext()
  // because:
  // 1. Once we call detachFrameGLContext(), it deletes the texture previously
  //    attached.
  // 2. SurfaceTexture requires us to apply a transform matrix when we show
  //    the texture.
  copier_->DoCopyTextureWithTransform(
      state_provider_->GetGlDecoder().get(), GL_TEXTURE_EXTERNAL_OES,
      surface_texture_id_, GL_TEXTURE_2D, picture_buffer_texture_id,
      state_provider_->GetSize().width(), state_provider_->GetSize().height(),
      true, false, false, transform_matrix);
}

void AndroidCopyingBackingStrategy::CodecChanged(VideoCodecBridge* codec) {
  media_codec_ = codec;
}

void AndroidCopyingBackingStrategy::OnFrameAvailable() {
  // TODO(liberato): crbug.com/574948 .  The OnFrameAvailable logic can be
  // moved into AVDA, and we should wait for it before doing the copy.
  // Because there were some test failures, we don't do this now but
  // instead preserve the old behavior.
}

bool AndroidCopyingBackingStrategy::ArePicturesOverlayable() {
  return false;
}

void AndroidCopyingBackingStrategy::UpdatePictureBufferSize(
    PictureBuffer* picture_buffer,
    const gfx::Size& new_size) {
  // This strategy uses 2D textures who's allocated memory is dependent on the
  // size. To update size in all places, we must:
  // 1) Update the PictureBuffer meta-data
  picture_buffer->set_size(new_size);

  // 2) Update the GL texture via glTexImage2D. This step assumes the caller
  // has made our GL context current.
  DCHECK_LE(1u, picture_buffer->texture_ids().size());
  glBindTexture(GL_TEXTURE_2D, picture_buffer->texture_ids()[0]);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, new_size.width(), new_size.height(),
               0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  state_provider_->GetGlDecoder()->RestoreActiveTextureUnitBinding(
      GL_TEXTURE_2D);

  // 3) Update the CHROMIUM Texture's size.
  gpu::gles2::TextureRef* texture_ref =
      state_provider_->GetTextureForPicture(*picture_buffer);
  RETURN_IF_NULL(texture_ref);
  gpu::gles2::TextureManager* texture_manager =
      state_provider_->GetGlDecoder()->GetContextGroup()->texture_manager();
  RETURN_IF_NULL(texture_manager);
  texture_manager->SetLevelInfo(texture_ref, GetTextureTarget(), 0, GL_RGBA,
                                new_size.width(), new_size.height(), 1, 0,
                                GL_RGBA, GL_UNSIGNED_BYTE, gfx::Rect(new_size));
}

}  // namespace media
