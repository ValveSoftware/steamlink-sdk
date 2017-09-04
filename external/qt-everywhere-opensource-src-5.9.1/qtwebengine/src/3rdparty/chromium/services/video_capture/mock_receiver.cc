// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/mock_receiver.h"

namespace video_capture {

MockReceiver::MockReceiver(mojom::ReceiverRequest request)
    : binding_(this, std::move(request)) {}

MockReceiver::~MockReceiver() = default;

void MockReceiver::OnIncomingCapturedVideoFrame(
    media::mojom::VideoFramePtr frame) {
  OnIncomingCapturedVideoFramePtr(&frame);
}

}  // namespace video_capture
