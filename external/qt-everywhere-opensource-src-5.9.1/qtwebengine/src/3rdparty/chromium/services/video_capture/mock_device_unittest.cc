// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "services/video_capture/mock_device_test.h"

using testing::_;
using testing::Invoke;

namespace video_capture {

// Tests that the service stops the capture device when the client closes the
// connection to the device proxy.
TEST_F(MockDeviceTest, DeviceIsStoppedWhenDiscardingDeviceProxy) {
  base::RunLoop wait_loop;

  EXPECT_CALL(*mock_device_, AllocateAndStartPtr(_));
  EXPECT_CALL(*mock_device_, StopAndDeAllocate())
      .WillOnce(Invoke([&wait_loop]() { wait_loop.Quit(); }));

  device_proxy_->Start(requested_settings_,
                       std::move(mock_receiver_proxy_));
  device_proxy_.reset();

  wait_loop.Run();
}

// Tests that the service stops the capture device when the client closes the
// connection to the client proxy it provided to the service.
TEST_F(MockDeviceTest, DeviceIsStoppedWhenDiscardingDeviceClient) {
  base::RunLoop wait_loop;

  EXPECT_CALL(*mock_device_, AllocateAndStartPtr(_));
  EXPECT_CALL(*mock_device_, StopAndDeAllocate())
      .WillOnce(Invoke([&wait_loop]() { wait_loop.Quit(); }));

  device_proxy_->Start(requested_settings_,
                       std::move(mock_receiver_proxy_));
  mock_receiver_.reset();

  wait_loop.Run();
}

}  // namespace video_capture
