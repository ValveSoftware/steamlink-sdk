// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_DEVICE_CLIENT_MEDIA_TO_MOJO_MOCK_ADAPTER_H_
#define SERVICES_VIDEO_CAPTURE_DEVICE_CLIENT_MEDIA_TO_MOJO_MOCK_ADAPTER_H_

#include "media/capture/video/video_capture_device.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/video_capture/public/interfaces/mock_device.mojom.h"

namespace video_capture {

// Adapter that allows a media::VideoCaptureDevice::Client to be used in place
// of a mojom::MockDeviceClient. Since the implemented interface
// MockDeviceClient is empty, its purpose is mostly to own the wrapped
// instance of media::VideoCaptureDevice::Client and keep it alive until the
// connection is closed.
class DeviceClientMediaToMojoMockAdapter : public mojom::MockDeviceClient {
 public:
  ~DeviceClientMediaToMojoMockAdapter() override;

  DeviceClientMediaToMojoMockAdapter(
      std::unique_ptr<media::VideoCaptureDevice::Client> client);

 private:
  std::unique_ptr<media::VideoCaptureDevice::Client> client_;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_DEVICE_CLIENT_MEDIA_TO_MOJO_MOCK_ADAPTER_H_
