// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/mock_media_stream_video_sink.h"

#include "media/base/bind_to_current_loop.h"

namespace content {

MockMediaStreamVideoSink::MockMediaStreamVideoSink()
    : number_of_frames_(0),
      enabled_(true),
      format_(media::VideoFrame::UNKNOWN),
      state_(blink::WebMediaStreamSource::ReadyStateLive),
      weak_factory_(this) {
}

MockMediaStreamVideoSink::~MockMediaStreamVideoSink() {
}

VideoCaptureDeliverFrameCB
MockMediaStreamVideoSink::GetDeliverFrameCB() {
  return media::BindToCurrentLoop(
      base::Bind(
          &MockMediaStreamVideoSink::DeliverVideoFrame,
          weak_factory_.GetWeakPtr()));
}

void MockMediaStreamVideoSink::DeliverVideoFrame(
    const scoped_refptr<media::VideoFrame>& frame,
    const media::VideoCaptureFormat& format,
    const base::TimeTicks& estimated_capture_time) {
  last_frame_ = frame;
  ++number_of_frames_;
  format_ = frame->format();
  frame_size_ = frame->natural_size();
  OnVideoFrame();
}

void MockMediaStreamVideoSink::OnReadyStateChanged(
    blink::WebMediaStreamSource::ReadyState state) {
  state_ = state;
}

void MockMediaStreamVideoSink::OnEnabledChanged(bool enabled) {
  enabled_ = enabled;
}

}  // namespace content
