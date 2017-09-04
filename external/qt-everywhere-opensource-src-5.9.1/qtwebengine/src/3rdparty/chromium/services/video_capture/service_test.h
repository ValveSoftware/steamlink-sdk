// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_SERVICE_TEST_H_
#define SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_SERVICE_TEST_H_

#include "services/service_manager/public/cpp/service_test.h"
#include "services/video_capture/mock_device_descriptor_receiver.h"
#include "services/video_capture/public/interfaces/service.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace video_capture {

// Basic test fixture that sets up a connection to the fake device factory.
class ServiceTest : public service_manager::test::ServiceTest {
 public:
  ServiceTest();
  ~ServiceTest() override;

  void SetUp() override;

 protected:
  mojom::ServicePtr service_;
  mojom::DeviceFactoryPtr factory_;
  MockDeviceDescriptorReceiver descriptor_receiver_;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_SERVICE_TEST_H_
