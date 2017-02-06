// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/usb_device_permission_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

TEST(USBDevicePermissionTest, PermissionDataOrder) {
  EXPECT_LT(UsbDevicePermissionData(0x02ad, 0x138c, -1),
            UsbDevicePermissionData(0x02ad, 0x138d, -1));
  ASSERT_LT(UsbDevicePermissionData(0x02ad, 0x138d, -1),
            UsbDevicePermissionData(0x02ae, 0x138c, -1));
  EXPECT_LT(UsbDevicePermissionData(0x02ad, 0x138c, -1),
            UsbDevicePermissionData(0x02ad, 0x138c, 0));
}

}  // namespace extensions
