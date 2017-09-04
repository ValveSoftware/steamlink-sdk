// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/capture_export.h"
#include "media/capture/video/video_capture_device.h"

namespace media {

// Callback interface for VideoCaptureDeviceClient to communicate with its
// clients.
class CAPTURE_EXPORT VideoFrameReceiver {
 public:
  virtual ~VideoFrameReceiver(){};

  virtual void OnIncomingCapturedVideoFrame(
      std::unique_ptr<media::VideoCaptureDevice::Client::Buffer> buffer,
      scoped_refptr<media::VideoFrame> frame) = 0;
  virtual void OnError() = 0;
  virtual void OnLog(const std::string& message) = 0;
  virtual void OnBufferDestroyed(int buffer_id_to_drop) = 0;
};

}  // namespace media
