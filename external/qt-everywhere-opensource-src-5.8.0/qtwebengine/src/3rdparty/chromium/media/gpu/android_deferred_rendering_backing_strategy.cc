// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android_deferred_rendering_backing_strategy.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "base/android/build_info.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gl_stream_texture_image.h"
#include "gpu/command_buffer/service/gles2_cmd_copy_texture_chromium.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/ipc/common/gpu_surface_lookup.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "media/gpu/avda_codec_image.h"
#include "media/gpu/avda_return_on_failure.h"
#include "media/gpu/avda_shared_state.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/egl_util.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/scoped_make_current.h"

namespace media {

AndroidDeferredRenderingBackingStrategy::
    AndroidDeferredRenderingBackingStrategy(AVDAStateProvider* state_provider)
    : state_provider_(state_provider), media_codec_(nullptr) {}

AndroidDeferredRenderingBackingStrategy::
    ~AndroidDeferredRenderingBackingStrategy() {}

gl::ScopedJavaSurface AndroidDeferredRenderingBackingStrategy::Initialize(
    int surface_view_id) {
  shared_state_ = new AVDASharedState();

  bool using_virtual_context = false;
  if (gl::GLContext* context = gl::GLContext::GetCurrent()) {
    if (gl::GLShareGroup* share_group = context->share_group())
      using_virtual_context = !!share_group->GetSharedContext();
  }
  UMA_HISTOGRAM_BOOLEAN("Media.AVDA.VirtualContext", using_virtual_context);

  // Acquire the SurfaceView surface if given a valid id.
  if (surface_view_id != VideoDecodeAccelerator::Config::kNoSurfaceID) {
    return gpu::GpuSurfaceLookup::GetInstance()->AcquireJavaSurface(
        surface_view_id);
  }

  // Create a SurfaceTexture.
  GLuint service_id = 0;
  surface_texture_ = state_provider_->CreateAttachedSurfaceTexture(&service_id);
  shared_state_->SetSurfaceTexture(surface_texture_, service_id);
  return gl::ScopedJavaSurface(surface_texture_.get());
}

void AndroidDeferredRenderingBackingStrategy::BeginCleanup(
    bool have_context,
    const AndroidVideoDecodeAccelerator::OutputBufferMap& buffers) {
  // If we failed before Initialize, then do nothing.
  if (!shared_state_)
    return;

  // TODO(liberato): we should release all codec buffers here without rendering.
  // CodecChanged() will drop them, but is expected to be called after the codec
  // is no longer accessible.  It's unclear that VP8 flush in AVDA can't hang
  // waiting for our buffers.

  CodecChanged(nullptr);
}

void AndroidDeferredRenderingBackingStrategy::EndCleanup() {
  // Release the surface texture and any back buffers.  This will preserve the
  // front buffer, if any.
  if (surface_texture_)
    surface_texture_->ReleaseSurfaceTexture();
}

scoped_refptr<gl::SurfaceTexture>
AndroidDeferredRenderingBackingStrategy::GetSurfaceTexture() const {
  return surface_texture_;
}

uint32_t AndroidDeferredRenderingBackingStrategy::GetTextureTarget() const {
  // If we're using a surface texture, then we need an external texture target
  // to sample from it.  If not, then we'll use 2D transparent textures to draw
  // a transparent hole through which to see the SurfaceView.  This is normally
  // needed only for the devtools inspector, since the overlay mechanism handles
  // it otherwise.
  return surface_texture_ ? GL_TEXTURE_EXTERNAL_OES : GL_TEXTURE_2D;
}

gfx::Size AndroidDeferredRenderingBackingStrategy::GetPictureBufferSize()
    const {
  // For SurfaceView, request a 1x1 2D texture to reduce memory during
  // initialization.  For SurfaceTexture, allocate a picture buffer that is the
  // actual frame size.  Note that it will be an external texture anyway, so it
  // doesn't allocate an image of that size.  However, it's still important to
  // get the coded size right, so that VideoLayerImpl doesn't try to scale the
  // texture when building the quad for it.
  return surface_texture_ ? state_provider_->GetSize() : gfx::Size(1, 1);
}

void AndroidDeferredRenderingBackingStrategy::SetImageForPicture(
    const PictureBuffer& picture_buffer,
    const scoped_refptr<gpu::gles2::GLStreamTextureImage>& image) {
  gpu::gles2::TextureRef* texture_ref =
      state_provider_->GetTextureForPicture(picture_buffer);
  RETURN_IF_NULL(texture_ref);

  gpu::gles2::TextureManager* texture_manager =
      state_provider_->GetGlDecoder()->GetContextGroup()->texture_manager();
  RETURN_IF_NULL(texture_manager);

  // Default to zero which will clear the stream texture service id if one was
  // previously set.
  GLuint stream_texture_service_id = 0;
  if (image) {
    if (shared_state_->surface_texture_service_id() != 0) {
      // Override the Texture's service id, so that it will use the one that is
      // attached to the SurfaceTexture.
      stream_texture_service_id = shared_state_->surface_texture_service_id();
    }

    // Also set the parameters for the level if we're not clearing the image.
    const gfx::Size size = state_provider_->GetSize();
    texture_manager->SetLevelInfo(texture_ref, GetTextureTarget(), 0, GL_RGBA,
                                  size.width(), size.height(), 1, 0, GL_RGBA,
                                  GL_UNSIGNED_BYTE, gfx::Rect());

    static_cast<AVDACodecImage*>(image.get())
        ->set_texture(texture_ref->texture());
  }

  // If we're clearing the image, or setting a SurfaceTexture backed image, we
  // set the state to UNBOUND. For SurfaceTexture images, this ensures that the
  // implementation will call CopyTexImage, which is where AVDACodecImage
  // updates the SurfaceTexture to the right frame.
  auto image_state = gpu::gles2::Texture::UNBOUND;
  // For SurfaceView we set the state to BOUND because ScheduleOverlayPlane
  // requires it. If something tries to sample from this texture it won't work,
  // but there's no way to sample from a SurfaceView anyway, so it doesn't
  // matter.
  if (image && !surface_texture_)
    image_state = gpu::gles2::Texture::BOUND;
  texture_manager->SetLevelStreamTextureImage(texture_ref, GetTextureTarget(),
                                              0, image.get(), image_state,
                                              stream_texture_service_id);
}

void AndroidDeferredRenderingBackingStrategy::UseCodecBufferForPictureBuffer(
    int32_t codec_buf_index,
    const PictureBuffer& picture_buffer) {
  // Make sure that the decoder is available.
  RETURN_IF_NULL(state_provider_->GetGlDecoder());

  // Notify the AVDACodecImage for picture_buffer that it should use the
  // decoded buffer codec_buf_index to render this frame.
  AVDACodecImage* avda_image =
      shared_state_->GetImageForPicture(picture_buffer.id());
  RETURN_IF_NULL(avda_image);

  // Note that this is not a race, since we do not re-use a PictureBuffer
  // until after the CC is done drawing it.
  pictures_out_for_display_.push_back(picture_buffer.id());
  avda_image->set_media_codec_buffer_index(codec_buf_index);
  avda_image->set_size(state_provider_->GetSize());

  MaybeRenderEarly();
}

void AndroidDeferredRenderingBackingStrategy::AssignOnePictureBuffer(
    const PictureBuffer& picture_buffer,
    bool have_context) {
  // Attach a GLImage to each texture that will use the surface texture.
  // We use a refptr here in case SetImageForPicture fails.
  scoped_refptr<gpu::gles2::GLStreamTextureImage> gl_image =
      new AVDACodecImage(picture_buffer.id(), shared_state_, media_codec_,
                         state_provider_->GetGlDecoder(), surface_texture_);
  SetImageForPicture(picture_buffer, gl_image);

  if (!surface_texture_ && have_context) {
    // To make devtools work, we're using a 2D texture.  Make it transparent,
    // so that it draws a hole for the SV to show through.  This is only
    // because devtools draws and reads back, which skips overlay processing.
    // It's unclear why devtools renders twice -- once normally, and once
    // including a readback layer.  The result is that the device screen
    // flashes as we alternately draw the overlay hole and this texture,
    // unless we make the texture transparent.
    static const uint8_t rgba[] = {0, 0, 0, 0};
    const gfx::Size size(1, 1);
    DCHECK_LE(1u, picture_buffer.texture_ids().size());
    glBindTexture(GL_TEXTURE_2D, picture_buffer.texture_ids()[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.width(), size.height(), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, rgba);
  }
}

void AndroidDeferredRenderingBackingStrategy::ReleaseCodecBufferForPicture(
    const PictureBuffer& picture_buffer) {
  AVDACodecImage* avda_image =
      shared_state_->GetImageForPicture(picture_buffer.id());
  RETURN_IF_NULL(avda_image);
  avda_image->UpdateSurface(AVDACodecImage::UpdateMode::DISCARD_CODEC_BUFFER);
}

void AndroidDeferredRenderingBackingStrategy::ReuseOnePictureBuffer(
    const PictureBuffer& picture_buffer) {
  pictures_out_for_display_.erase(
      std::remove(pictures_out_for_display_.begin(),
                  pictures_out_for_display_.end(), picture_buffer.id()),
      pictures_out_for_display_.end());

  // At this point, the CC must be done with the picture.  We can't really
  // check for that here directly.  it's guaranteed in gpu_video_decoder.cc,
  // when it waits on the sync point before releasing the mailbox.  That sync
  // point is inserted by destroying the resource in VideoLayerImpl::DidDraw.
  ReleaseCodecBufferForPicture(picture_buffer);
  MaybeRenderEarly();
}

void AndroidDeferredRenderingBackingStrategy::ReleaseCodecBuffers(
    const AndroidVideoDecodeAccelerator::OutputBufferMap& buffers) {
  for (const std::pair<int, PictureBuffer>& entry : buffers)
    ReleaseCodecBufferForPicture(entry.second);
}

void AndroidDeferredRenderingBackingStrategy::MaybeRenderEarly() {
  if (pictures_out_for_display_.empty())
    return;

  // See if we can consume the front buffer / render to the SurfaceView. Iterate
  // in reverse to find the most recent front buffer. If none is found, the
  // |front_index| will point to the beginning of the array.
  size_t front_index = pictures_out_for_display_.size() - 1;
  AVDACodecImage* first_renderable_image = nullptr;
  for (int i = front_index; i >= 0; --i) {
    const int id = pictures_out_for_display_[i];
    AVDACodecImage* avda_image = shared_state_->GetImageForPicture(id);
    if (!avda_image)
      continue;

    // Update the front buffer index as we move along to shorten the number of
    // candidate images we look at for back buffer rendering.
    front_index = i;
    first_renderable_image = avda_image;

    // If we find a front buffer, stop and indicate that front buffer rendering
    // is not possible since another image is already in the front buffer.
    if (avda_image->was_rendered_to_front_buffer()) {
      first_renderable_image = nullptr;
      break;
    }
  }

  if (first_renderable_image) {
    first_renderable_image->UpdateSurface(
        AVDACodecImage::UpdateMode::RENDER_TO_FRONT_BUFFER);
  }

  // Back buffer rendering is only available for surface textures. We'll always
  // have at least one front buffer, so the next buffer must be the backbuffer.
  size_t backbuffer_index = front_index + 1;
  if (!surface_texture_ || backbuffer_index >= pictures_out_for_display_.size())
    return;

  // See if the back buffer is free. If so, then render the frame adjacent to
  // the front buffer.  The listing is in render order, so we can just use the
  // first unrendered frame if there is back buffer space.
  first_renderable_image = shared_state_->GetImageForPicture(
      pictures_out_for_display_[backbuffer_index]);
  if (!first_renderable_image ||
      first_renderable_image->was_rendered_to_back_buffer()) {
    return;
  }

  // Due to the loop in the beginning this should never be true.
  DCHECK(!first_renderable_image->was_rendered_to_front_buffer());
  first_renderable_image->UpdateSurface(
      AVDACodecImage::UpdateMode::RENDER_TO_BACK_BUFFER);
}

void AndroidDeferredRenderingBackingStrategy::CodecChanged(
    VideoCodecBridge* codec) {
  media_codec_ = codec;
  shared_state_->CodecChanged(codec);
}

void AndroidDeferredRenderingBackingStrategy::OnFrameAvailable() {
  shared_state_->SignalFrameAvailable();
}

bool AndroidDeferredRenderingBackingStrategy::ArePicturesOverlayable() {
  // SurfaceView frames are always overlayable because that's the only way to
  // display them.
  return !surface_texture_;
}

void AndroidDeferredRenderingBackingStrategy::UpdatePictureBufferSize(
    PictureBuffer* picture_buffer,
    const gfx::Size& new_size) {
  // This strategy uses EGL images which manage the texture size for us.  We
  // simply update the PictureBuffer meta-data and leave the texture as-is.
  picture_buffer->set_size(new_size);
}

}  // namespace media
