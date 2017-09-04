// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/receiver_mojo_to_media_adapter.h"

#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/common/mojo_shared_buffer_video_frame.h"

namespace video_capture {

ReceiverMojoToMediaAdapter::ReceiverMojoToMediaAdapter(
    mojom::ReceiverPtr receiver)
    : receiver_(std::move(receiver)) {}

ReceiverMojoToMediaAdapter::~ReceiverMojoToMediaAdapter() = default;

void ReceiverMojoToMediaAdapter::OnIncomingCapturedVideoFrame(
    std::unique_ptr<media::VideoCaptureDevice::Client::Buffer> buffer,
    scoped_refptr<media::VideoFrame> frame) {
  // O: |frame| should already be backed by a MojoSharedBufferVideoFrame
  //    assuming we have used the correct buffer factory with the pool.
  auto video_frame_ptr = media::mojom::VideoFrame::From(std::move(frame));
  receiver_->OnIncomingCapturedVideoFrame(std::move(video_frame_ptr));
}

void ReceiverMojoToMediaAdapter::OnError() {
  receiver_->OnError();
}

void ReceiverMojoToMediaAdapter::OnLog(const std::string& message) {
  receiver_->OnLog(message);
}

void ReceiverMojoToMediaAdapter::OnBufferDestroyed(int buffer_id_to_drop) {
  // Nothing to do here.
  // This call is only needed for clients who need to explicitly share buffers
  // with other processes and keep track of which buffers the buffer pool is
  // no longer going to reuse/resurrect.
  // In the world of Mojo, we do not need to share buffers explicitly.
}

}  // namespace video_capture
