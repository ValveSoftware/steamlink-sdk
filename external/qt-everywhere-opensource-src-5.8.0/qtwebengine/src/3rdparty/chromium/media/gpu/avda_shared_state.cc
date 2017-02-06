// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/avda_shared_state.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "media/gpu/avda_codec_image.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/scoped_make_current.h"

namespace media {

AVDASharedState::AVDASharedState()
    : surface_texture_service_id_(0),
      frame_available_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED) {}

AVDASharedState::~AVDASharedState() {
  if (!surface_texture_service_id_)
    return;

  ui::ScopedMakeCurrent scoped_make_current(context_.get(), surface_.get());
  if (scoped_make_current.Succeeded()) {
    glDeleteTextures(1, &surface_texture_service_id_);
    DCHECK_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
  }
}

void AVDASharedState::SignalFrameAvailable() {
  frame_available_event_.Signal();
}

void AVDASharedState::WaitForFrameAvailable() {
  DCHECK(!release_time_.is_null());

  // 5msec covers >99.9% of cases, so just wait for up to that much before
  // giving up.  If an error occurs, we might not ever get a notification.
  const base::TimeDelta max_wait = base::TimeDelta::FromMilliseconds(5);
  const base::TimeTicks call_time = base::TimeTicks::Now();
  const base::TimeDelta elapsed = call_time - release_time_;
  const base::TimeDelta remaining = max_wait - elapsed;
  release_time_ = base::TimeTicks();

  if (remaining <= base::TimeDelta()) {
    if (!frame_available_event_.IsSignaled()) {
      DVLOG(1) << "Deferred WaitForFrameAvailable() timed out, elapsed: "
               << elapsed.InMillisecondsF() << "ms";
    }
    return;
  }

  DCHECK_LE(remaining, max_wait);
  SCOPED_UMA_HISTOGRAM_TIMER("Media.AvdaCodecImage.WaitTimeForFrame");
  if (!frame_available_event_.TimedWait(remaining)) {
    DVLOG(1) << "WaitForFrameAvailable() timed out, elapsed: "
             << elapsed.InMillisecondsF()
             << "ms, additionally waited: " << remaining.InMillisecondsF()
             << "ms, total: " << (elapsed + remaining).InMillisecondsF()
             << "ms";
  }
}

void AVDASharedState::SetSurfaceTexture(
    scoped_refptr<gl::SurfaceTexture> surface_texture,
    GLuint attached_service_id) {
  surface_texture_ = surface_texture;
  surface_texture_service_id_ = attached_service_id;
  context_ = gl::GLContext::GetCurrent();
  surface_ = gl::GLSurface::GetCurrent();
  DCHECK(context_);
  DCHECK(surface_);
}

void AVDASharedState::CodecChanged(MediaCodecBridge* codec) {
  for (auto& image_kv : codec_images_)
    image_kv.second->CodecChanged(codec);
  release_time_ = base::TimeTicks();
}

void AVDASharedState::SetImageForPicture(int picture_buffer_id,
                                         AVDACodecImage* image) {
  if (!image) {
    DCHECK(codec_images_.find(picture_buffer_id) != codec_images_.end());
    codec_images_.erase(picture_buffer_id);
    return;
  }

  DCHECK(codec_images_.find(picture_buffer_id) == codec_images_.end());
  codec_images_[picture_buffer_id] = image;
}

AVDACodecImage* AVDASharedState::GetImageForPicture(
    int picture_buffer_id) const {
  auto it = codec_images_.find(picture_buffer_id);
  return it == codec_images_.end() ? nullptr : it->second;
}

void AVDASharedState::RenderCodecBufferToSurfaceTexture(
    MediaCodecBridge* codec,
    int codec_buffer_index) {
  if (!release_time_.is_null())
    WaitForFrameAvailable();
  codec->ReleaseOutputBuffer(codec_buffer_index, true);
  release_time_ = base::TimeTicks::Now();
}

}  // namespace media
