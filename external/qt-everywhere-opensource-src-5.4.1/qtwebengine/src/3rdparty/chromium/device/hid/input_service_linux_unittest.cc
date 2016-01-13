// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/message_loop/message_loop.h"
#include "device/hid/input_service_linux.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

TEST(InputServiceLinux, Simple) {
  base::MessageLoopForIO message_loop;
  InputServiceLinux* service = InputServiceLinux::GetInstance();

  ASSERT_TRUE(service);
  std::vector<InputServiceLinux::InputDeviceInfo> devices;
  service->GetDevices(&devices);
  for (size_t i = 0; i < devices.size(); ++i)
    ASSERT_TRUE(!devices[i].id.empty());
}

}  // namespace device
