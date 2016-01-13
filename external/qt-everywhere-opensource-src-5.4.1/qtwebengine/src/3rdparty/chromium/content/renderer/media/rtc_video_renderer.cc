// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/rtc_video_renderer.h"

#include "base/debug/trace_event.h"
#include "base/message_loop/message_loop_proxy.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"

const int kMinFrameSize = 2;

namespace content {

RTCVideoRenderer::RTCVideoRenderer(
    const blink::WebMediaStreamTrack& video_track,
    const base::Closure& error_cb,
    const RepaintCB& repaint_cb)
    : error_cb_(error_cb),
      repaint_cb_(repaint_cb),
      message_loop_proxy_(base::MessageLoopProxy::current()),
      state_(STOPPED),
      frame_size_(kMinFrameSize, kMinFrameSize),
      video_track_(video_track),
      weak_factory_(this) {
}

RTCVideoRenderer::~RTCVideoRenderer() {
}

void RTCVideoRenderer::Start() {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STOPPED);

  AddToVideoTrack(
      this,
      media::BindToCurrentLoop(
          base::Bind(
              &RTCVideoRenderer::OnVideoFrame,
              weak_factory_.GetWeakPtr())),
      video_track_);
  state_ = STARTED;

  if (video_track_.source().readyState() ==
          blink::WebMediaStreamSource::ReadyStateEnded ||
      !video_track_.isEnabled()) {
    RenderSignalingFrame();
  }
}

void RTCVideoRenderer::Stop() {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  DCHECK(state_ == STARTED || state_ == PAUSED);
  RemoveFromVideoTrack(this, video_track_);
  weak_factory_.InvalidateWeakPtrs();
  state_ = STOPPED;
  frame_size_.set_width(kMinFrameSize);
  frame_size_.set_height(kMinFrameSize);
}

void RTCVideoRenderer::Play() {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  if (state_ == PAUSED) {
    state_ = STARTED;
  }
}

void RTCVideoRenderer::Pause() {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  if (state_ == STARTED) {
    state_ = PAUSED;
  }
}

void RTCVideoRenderer::OnReadyStateChanged(
    blink::WebMediaStreamSource::ReadyState state) {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  if (state == blink::WebMediaStreamSource::ReadyStateEnded)
    RenderSignalingFrame();
}

void RTCVideoRenderer::OnVideoFrame(
    const scoped_refptr<media::VideoFrame>& frame,
    const media::VideoCaptureFormat& format,
    const base::TimeTicks& estimated_capture_time) {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  if (state_ != STARTED) {
    return;
  }

  frame_size_ = frame->natural_size();

  TRACE_EVENT_INSTANT1("rtc_video_renderer",
                       "OnVideoFrame",
                       TRACE_EVENT_SCOPE_THREAD,
                       "timestamp",
                       frame->timestamp().InMilliseconds());
  repaint_cb_.Run(frame);
}

void RTCVideoRenderer::RenderSignalingFrame() {
  // This is necessary to make sure audio can play if the video tag src is
  // a MediaStream video track that has been rejected or ended.
  // It also ensure that the renderer don't hold a reference to a real video
  // frame if no more frames are provided. This is since there might be a
  // finite number of available buffers. E.g, video that
  // originates from a video camera.
  scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::CreateBlackFrame(frame_size_);
  OnVideoFrame(video_frame, media::VideoCaptureFormat(), base::TimeTicks());
}

}  // namespace content
