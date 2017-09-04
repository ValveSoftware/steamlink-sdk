// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/fake_device_descriptor_test.h"

#include "base/run_loop.h"

using testing::_;
using testing::Invoke;

namespace video_capture {

FakeDeviceDescriptorTest::FakeDeviceDescriptorTest()
    : video_capture::ServiceTest() {}

FakeDeviceDescriptorTest::~FakeDeviceDescriptorTest() = default;

void FakeDeviceDescriptorTest::SetUp() {
  video_capture::ServiceTest::SetUp();

  base::RunLoop wait_loop;
  EXPECT_CALL(descriptor_receiver_, OnEnumerateDeviceDescriptorsCallback(_))
      .WillOnce(Invoke([this, &wait_loop](
          const std::vector<media::VideoCaptureDeviceDescriptor>& descriptors) {
        fake_device_descriptor_ = descriptors[0];
        wait_loop.Quit();
      }));
  factory_->EnumerateDeviceDescriptors(base::Bind(
      &MockDeviceDescriptorReceiver::OnEnumerateDeviceDescriptorsCallback,
      base::Unretained(&descriptor_receiver_)));
  wait_loop.Run();
}

}  // namespace video_capture
