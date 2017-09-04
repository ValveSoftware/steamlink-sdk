// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_MOCK_DEVICE_DESCRIPTOR_RECEIVER_H_
#define SERVICES_VIDEO_CAPTURE_MOCK_DEVICE_DESCRIPTOR_RECEIVER_H_

#include "media/capture/video/video_capture_device_descriptor.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace video_capture {

// Mock for receiving device descriptors from a call to
// mojom::VideoCaptureDeviceFactory::EnumerateDeviceDescriptors().
class MockDeviceDescriptorReceiver {
 public:
  MockDeviceDescriptorReceiver();
  ~MockDeviceDescriptorReceiver();

  MOCK_METHOD1(
      OnEnumerateDeviceDescriptorsCallback,
      void(const std::vector<media::VideoCaptureDeviceDescriptor>&));
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_MOCK_DEVICE_DESCRIPTOR_RECEIVER_H_
