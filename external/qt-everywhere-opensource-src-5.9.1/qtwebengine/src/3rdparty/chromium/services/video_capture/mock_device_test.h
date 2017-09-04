// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_MOCK_DEVICE_TEST_H_
#define SERVICES_VIDEO_CAPTURE_MOCK_DEVICE_TEST_H_

#include "services/service_manager/public/cpp/service_test.h"
#include "services/video_capture/mock_device_descriptor_receiver.h"
#include "services/video_capture/mock_media_device_impl.h"
#include "services/video_capture/mock_receiver.h"
#include "services/video_capture/public/cpp/capture_settings.h"
#include "services/video_capture/public/interfaces/service.mojom.h"

namespace video_capture {

// Reusable test setup for testing with a single mock device.
class MockDeviceTest : public service_manager::test::ServiceTest {
 public:
  MockDeviceTest();
  ~MockDeviceTest() override;

  void SetUp() override;

 protected:
  mojom::ServicePtr service_;
  mojom::DeviceFactoryPtr factory_;
  MockDeviceDescriptorReceiver descriptor_receiver_;

  std::unique_ptr<MockMediaDeviceImpl> mock_device_;
  std::unique_ptr<MockReceiver> mock_receiver_;
  mojom::MockMediaDevicePtr mock_device_proxy_;
  mojom::DevicePtr device_proxy_;
  mojom::ReceiverPtr mock_receiver_proxy_;
  CaptureSettings requested_settings_;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_MOCK_DEVICE_TEST_H_
