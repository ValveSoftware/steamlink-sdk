// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_RECEIVER_MOJO_TO_MEDIA_ADAPTER_H_
#define SERVICES_VIDEO_CAPTURE_RECEIVER_MOJO_TO_MEDIA_ADAPTER_H_

#include "media/capture/video/video_frame_receiver.h"
#include "services/video_capture/public/interfaces/receiver.mojom.h"

namespace video_capture {

// Adapter that allows a mojom::VideoFrameReceiver to be used in place of
// a media::VideoFrameReceiver.
class ReceiverMojoToMediaAdapter : public media::VideoFrameReceiver {
 public:
  ReceiverMojoToMediaAdapter(mojom::ReceiverPtr receiver);
  ~ReceiverMojoToMediaAdapter() override;

  // media::VideoFrameReceiver:
  void OnIncomingCapturedVideoFrame(
      std::unique_ptr<media::VideoCaptureDevice::Client::Buffer> buffer,
      scoped_refptr<media::VideoFrame> frame) override;
  void OnError() override;
  void OnLog(const std::string& message) override;
  void OnBufferDestroyed(int buffer_id_to_drop) override;

 private:
  mojom::ReceiverPtr receiver_;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_RECEIVER_MOJO_TO_MEDIA_ADAPTER_H_
