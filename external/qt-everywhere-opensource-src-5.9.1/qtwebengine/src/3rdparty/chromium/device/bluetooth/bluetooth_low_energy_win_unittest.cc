// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_low_energy_win.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/strings/sys_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kValidDeviceInstanceId[] =
    "BTHLE\\DEV_BC6A29AB5FB0\\8&31038925&0&BC6A29AB5FB0";

const char kInvalidDeviceInstanceId[] =
    "BTHLE\\BC6A29AB5FB0_DEV_\\8&31038925&0&BC6A29AB5FB0";

}  // namespace

namespace device {

class BluetoothLowEnergyWinTest : public testing::Test {};

TEST_F(BluetoothLowEnergyWinTest, ExtractValidBluetoothAddress) {
  BLUETOOTH_ADDRESS btha;
  std::string error;
  bool success =
      device::win::ExtractBluetoothAddressFromDeviceInstanceIdForTesting(
          kValidDeviceInstanceId, &btha, &error);

  EXPECT_TRUE(success);
  EXPECT_TRUE(error.empty());
  EXPECT_EQ(0xbc, btha.rgBytes[5]);
  EXPECT_EQ(0x6a, btha.rgBytes[4]);
  EXPECT_EQ(0x29, btha.rgBytes[3]);
  EXPECT_EQ(0xab, btha.rgBytes[2]);
  EXPECT_EQ(0x5f, btha.rgBytes[1]);
  EXPECT_EQ(0xb0, btha.rgBytes[0]);
}

TEST_F(BluetoothLowEnergyWinTest, ExtractInvalidBluetoothAddress) {
  BLUETOOTH_ADDRESS btha;
  std::string error;
  bool success =
      device::win::ExtractBluetoothAddressFromDeviceInstanceIdForTesting(
          kInvalidDeviceInstanceId, &btha, &error);

  EXPECT_FALSE(success);
  EXPECT_FALSE(error.empty());
}

TEST_F(BluetoothLowEnergyWinTest, DeviceRegistryPropertyValueAsString) {
  std::string test_value = "String used for round trip test.";
  std::wstring wide_value = base::SysUTF8ToWide(test_value);
  size_t buffer_size = (wide_value.size() + 1) * sizeof(wchar_t);
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[buffer_size]);
  memcpy(buffer.get(), wide_value.c_str(), buffer_size);
  std::unique_ptr<device::win::DeviceRegistryPropertyValue> value =
      device::win::DeviceRegistryPropertyValue::Create(
          REG_SZ, std::move(buffer), buffer_size);
  EXPECT_EQ(test_value, value->AsString());
}

TEST_F(BluetoothLowEnergyWinTest, DeviceRegistryPropertyValueAsDWORD) {
  DWORD test_value = 5u;
  size_t buffer_size = sizeof(DWORD);
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[buffer_size]);
  memcpy(buffer.get(), &test_value, buffer_size);
  std::unique_ptr<device::win::DeviceRegistryPropertyValue> value =
      device::win::DeviceRegistryPropertyValue::Create(
          REG_DWORD, std::move(buffer), buffer_size);
  EXPECT_EQ(test_value, value->AsDWORD());
}

TEST_F(BluetoothLowEnergyWinTest, DevicePropertyValueAsUint32) {
  uint32_t test_value = 5u;
  size_t buffer_size = sizeof(uint32_t);
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[buffer_size]);
  memcpy(buffer.get(), &test_value, buffer_size);
  std::unique_ptr<device::win::DevicePropertyValue> value(
      new device::win::DevicePropertyValue(DEVPROP_TYPE_UINT32,
                                           std::move(buffer), buffer_size));
  EXPECT_EQ(test_value, value->AsUint32());
}

}  // namespace device
