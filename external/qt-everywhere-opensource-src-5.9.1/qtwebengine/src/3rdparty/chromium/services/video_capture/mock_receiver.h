// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_MOCK_RECEIVER_H_
#define SERVICES_VIDEO_CAPTURE_MOCK_RECEIVER_H_

#include "media/mojo/interfaces/media_types.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/video_capture/public/interfaces/receiver.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace video_capture {

class MockReceiver : public mojom::Receiver {
 public:
  MockReceiver(mojom::ReceiverRequest request);
  ~MockReceiver() override;

  // Use forwarding method to work around gmock not supporting move-only types.
  void OnIncomingCapturedVideoFrame(media::mojom::VideoFramePtr frame) override;

  MOCK_METHOD1(OnIncomingCapturedVideoFramePtr,
               void(const media::mojom::VideoFramePtr* frame));
  MOCK_METHOD0(OnError, void());
  MOCK_METHOD1(OnLog, void(const std::string&));
  MOCK_METHOD1(OnBufferDestroyed, void(int32_t));

 private:
  const mojo::Binding<mojom::Receiver> binding_;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_MOCK_RECEIVER_H_
