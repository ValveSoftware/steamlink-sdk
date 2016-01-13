// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/message_loop/message_loop.h"
#include "device/hid/hid_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

TEST(HidServiceTest, Create) {
  base::MessageLoopForIO message_loop;
  HidService* service = HidService::GetInstance();
  ASSERT_TRUE(service);

  std::vector<HidDeviceInfo> devices;
  service->GetDevices(&devices);
  for (std::vector<HidDeviceInfo>::iterator it = devices.begin();
      it != devices.end();
      ++it) {
    ASSERT_TRUE(it->device_id != kInvalidHidDeviceId);
  }
}

}  // namespace device
