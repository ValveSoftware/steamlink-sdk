// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/video_frame_compositor.h"

#include "media/base/video_frame.h"

namespace content {

static bool IsOpaque(const scoped_refptr<media::VideoFrame>& frame) {
  switch (frame->format()) {
    case media::VideoFrame::UNKNOWN:
    case media::VideoFrame::YV12:
    case media::VideoFrame::YV12J:
    case media::VideoFrame::YV16:
    case media::VideoFrame::I420:
    case media::VideoFrame::YV24:
    case media::VideoFrame::NV12:
      return true;

    case media::VideoFrame::YV12A:
#if defined(VIDEO_HOLE)
    case media::VideoFrame::HOLE:
#endif  // defined(VIDEO_HOLE)
    case media::VideoFrame::NATIVE_TEXTURE:
      break;
  }
  return false;
}

VideoFrameCompositor::VideoFrameCompositor(
    const base::Callback<void(gfx::Size)>& natural_size_changed_cb,
    const base::Callback<void(bool)>& opacity_changed_cb)
    : natural_size_changed_cb_(natural_size_changed_cb),
      opacity_changed_cb_(opacity_changed_cb),
      client_(NULL) {
}

VideoFrameCompositor::~VideoFrameCompositor() {
  if (client_)
    client_->StopUsingProvider();
}

void VideoFrameCompositor::SetVideoFrameProviderClient(
    cc::VideoFrameProvider::Client* client) {
  if (client_)
    client_->StopUsingProvider();
  client_ = client;
}

scoped_refptr<media::VideoFrame> VideoFrameCompositor::GetCurrentFrame() {
  return current_frame_;
}

void VideoFrameCompositor::PutCurrentFrame(
    const scoped_refptr<media::VideoFrame>& frame) {
}

void VideoFrameCompositor::UpdateCurrentFrame(
    const scoped_refptr<media::VideoFrame>& frame) {
  if (current_frame_ &&
      current_frame_->natural_size() != frame->natural_size()) {
    natural_size_changed_cb_.Run(frame->natural_size());
  }

  if (!current_frame_ || IsOpaque(current_frame_) != IsOpaque(frame)) {
    opacity_changed_cb_.Run(IsOpaque(frame));
  }

  current_frame_ = frame;

  if (client_)
    client_->DidReceiveFrame();
}

}  // namespace content
