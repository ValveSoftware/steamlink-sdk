// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/memory/ref_counted.h"
#include "device/usb/mock_usb_device.h"
#include "device/usb/usb_descriptors.h"
#include "device/usb/usb_device_filter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

using testing::Return;

class UsbFilterTest : public testing::Test {
 public:
  void SetUp() override {
    UsbConfigDescriptor config(1, false, false, 0);
    config.interfaces.emplace_back(1, 0, 0xff, 0x42, 0x01);

    android_phone_ = new MockUsbDevice(0x18d1, 0x4ee2, config);
  }

 protected:
  scoped_refptr<MockUsbDevice> android_phone_;
};

TEST_F(UsbFilterTest, MatchAny) {
  UsbDeviceFilter filter;
  ASSERT_TRUE(filter.Matches(android_phone_));
}

TEST_F(UsbFilterTest, MatchVendorId) {
  UsbDeviceFilter filter;
  filter.SetVendorId(0x18d1);
  ASSERT_TRUE(filter.Matches(android_phone_));
}

TEST_F(UsbFilterTest, MatchVendorIdNegative) {
  UsbDeviceFilter filter;
  filter.SetVendorId(0x1d6b);
  ASSERT_FALSE(filter.Matches(android_phone_));
}

TEST_F(UsbFilterTest, MatchProductId) {
  UsbDeviceFilter filter;
  filter.SetVendorId(0x18d1);
  filter.SetProductId(0x4ee2);
  ASSERT_TRUE(filter.Matches(android_phone_));
}

TEST_F(UsbFilterTest, MatchProductIdNegative) {
  UsbDeviceFilter filter;
  filter.SetVendorId(0x18d1);
  filter.SetProductId(0x4ee1);
  ASSERT_FALSE(filter.Matches(android_phone_));
}

TEST_F(UsbFilterTest, MatchInterfaceClass) {
  UsbDeviceFilter filter;
  filter.SetInterfaceClass(0xff);
  ASSERT_TRUE(filter.Matches(android_phone_));
}

TEST_F(UsbFilterTest, MatchInterfaceClassNegative) {
  UsbDeviceFilter filter;
  filter.SetInterfaceClass(0xe0);
  ASSERT_FALSE(filter.Matches(android_phone_));
}

TEST_F(UsbFilterTest, MatchInterfaceSubclass) {
  UsbDeviceFilter filter;
  filter.SetInterfaceClass(0xff);
  filter.SetInterfaceSubclass(0x42);
  ASSERT_TRUE(filter.Matches(android_phone_));
}

TEST_F(UsbFilterTest, MatchInterfaceSubclassNegative) {
  UsbDeviceFilter filter;
  filter.SetInterfaceClass(0xff);
  filter.SetInterfaceSubclass(0x01);
  ASSERT_FALSE(filter.Matches(android_phone_));
}

TEST_F(UsbFilterTest, MatchInterfaceProtocol) {
  UsbDeviceFilter filter;
  filter.SetInterfaceClass(0xff);
  filter.SetInterfaceSubclass(0x42);
  filter.SetInterfaceProtocol(0x01);
  ASSERT_TRUE(filter.Matches(android_phone_));
}

TEST_F(UsbFilterTest, MatchInterfaceProtocolNegative) {
  UsbDeviceFilter filter;
  filter.SetInterfaceClass(0xff);
  filter.SetInterfaceSubclass(0x42);
  filter.SetInterfaceProtocol(0x02);
  ASSERT_FALSE(filter.Matches(android_phone_));
}

TEST_F(UsbFilterTest, MatchAnyEmptyListNegative) {
  std::vector<UsbDeviceFilter> filters;
  ASSERT_FALSE(UsbDeviceFilter::MatchesAny(android_phone_, filters));
}

TEST_F(UsbFilterTest, MatchesAnyVendorId) {
  std::vector<UsbDeviceFilter> filters(1);
  filters.back().SetVendorId(0x18d1);
  ASSERT_TRUE(UsbDeviceFilter::MatchesAny(android_phone_, filters));
}

TEST_F(UsbFilterTest, MatchesAnyVendorIdNegative) {
  std::vector<UsbDeviceFilter> filters(1);
  filters.back().SetVendorId(0x1d6b);
  ASSERT_FALSE(UsbDeviceFilter::MatchesAny(android_phone_, filters));
}

}  // namespace

}  // namespace device
