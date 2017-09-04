// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/fake_device_test.h"

#include "base/run_loop.h"

using testing::_;
using testing::Invoke;

namespace video_capture {

FakeDeviceTest::MockSupportedFormatsReceiver::MockSupportedFormatsReceiver() =
    default;

FakeDeviceTest::MockSupportedFormatsReceiver::~MockSupportedFormatsReceiver() =
    default;

FakeDeviceTest::FakeDeviceTest() : FakeDeviceDescriptorTest() {}

FakeDeviceTest::~FakeDeviceTest() = default;

void FakeDeviceTest::SetUp() {
  FakeDeviceDescriptorTest::SetUp();

  // Query factory for supported formats of fake device
  base::RunLoop wait_loop;
  EXPECT_CALL(supported_formats_receiver_, OnGetSupportedFormatsCallback(_))
      .WillOnce(Invoke(
          [this, &wait_loop](const std::vector<I420CaptureFormat>& formats) {
            fake_device_first_supported_format_ = formats[0];
            wait_loop.Quit();
          }));
  factory_->GetSupportedFormats(
      fake_device_descriptor_.device_id,
      base::Bind(&MockSupportedFormatsReceiver::OnGetSupportedFormatsCallback,
      base::Unretained(&supported_formats_receiver_)));
  wait_loop.Run();

  requestable_settings_.format = fake_device_first_supported_format_;
  requestable_settings_.resolution_change_policy =
      media::RESOLUTION_POLICY_FIXED_RESOLUTION;
  requestable_settings_.power_line_frequency =
      media::PowerLineFrequency::FREQUENCY_DEFAULT;

  factory_->CreateDevice(
      std::move(fake_device_descriptor_.device_id),
      mojo::GetProxy(&fake_device_proxy_),
      base::Bind([](mojom::DeviceAccessResultCode result_code) {
        ASSERT_EQ(mojom::DeviceAccessResultCode::SUCCESS, result_code);
      }));
}

}  // namespace video_capture
