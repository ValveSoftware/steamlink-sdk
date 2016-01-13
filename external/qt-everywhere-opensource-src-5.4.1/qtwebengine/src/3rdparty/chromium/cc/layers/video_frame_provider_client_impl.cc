// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/video_frame_provider_client_impl.h"

#include "base/debug/trace_event.h"
#include "cc/base/math_util.h"
#include "cc/layers/video_layer_impl.h"
#include "media/base/video_frame.h"

namespace cc {

// static
scoped_refptr<VideoFrameProviderClientImpl>
    VideoFrameProviderClientImpl::Create(
        VideoFrameProvider* provider) {
  return make_scoped_refptr(
      new VideoFrameProviderClientImpl(provider));
}

VideoFrameProviderClientImpl::~VideoFrameProviderClientImpl() {}

VideoFrameProviderClientImpl::VideoFrameProviderClientImpl(
    VideoFrameProvider* provider)
    : active_video_layer_(NULL), provider_(provider) {
  // This only happens during a commit on the compositor thread while the main
  // thread is blocked. That makes this a thread-safe call to set the video
  // frame provider client that does not require a lock. The same is true of
  // the call to Stop().
  provider_->SetVideoFrameProviderClient(this);

  // This matrix is the default transformation for stream textures, and flips
  // on the Y axis.
  stream_texture_matrix_ = gfx::Transform(
      1.0, 0.0, 0.0, 0.0,
      0.0, -1.0, 0.0, 1.0,
      0.0, 0.0, 1.0, 0.0,
      0.0, 0.0, 0.0, 1.0);
}

void VideoFrameProviderClientImpl::Stop() {
  if (!provider_)
    return;
  provider_->SetVideoFrameProviderClient(NULL);
  provider_ = NULL;
}

scoped_refptr<media::VideoFrame>
VideoFrameProviderClientImpl::AcquireLockAndCurrentFrame() {
  provider_lock_.Acquire();  // Balanced by call to ReleaseLock().
  if (!provider_)
    return NULL;

  return provider_->GetCurrentFrame();
}

void VideoFrameProviderClientImpl::PutCurrentFrame(
    const scoped_refptr<media::VideoFrame>& frame) {
  provider_lock_.AssertAcquired();
  provider_->PutCurrentFrame(frame);
}

void VideoFrameProviderClientImpl::ReleaseLock() {
  provider_lock_.AssertAcquired();
  provider_lock_.Release();
}

void VideoFrameProviderClientImpl::StopUsingProvider() {
  // Block the provider from shutting down until this client is done
  // using the frame.
  base::AutoLock locker(provider_lock_);
  provider_ = NULL;
}

void VideoFrameProviderClientImpl::DidReceiveFrame() {
  TRACE_EVENT1("cc",
               "VideoFrameProviderClientImpl::DidReceiveFrame",
               "active_video_layer",
               !!active_video_layer_);
  if (active_video_layer_)
    active_video_layer_->SetNeedsRedraw();
}

void VideoFrameProviderClientImpl::DidUpdateMatrix(const float* matrix) {
  stream_texture_matrix_ = gfx::Transform(
      matrix[0], matrix[4], matrix[8], matrix[12],
      matrix[1], matrix[5], matrix[9], matrix[13],
      matrix[2], matrix[6], matrix[10], matrix[14],
      matrix[3], matrix[7], matrix[11], matrix[15]);
  if (active_video_layer_)
    active_video_layer_->SetNeedsRedraw();
}

}  // namespace cc
