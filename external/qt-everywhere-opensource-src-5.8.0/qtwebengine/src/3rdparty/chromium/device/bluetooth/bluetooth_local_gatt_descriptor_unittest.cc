// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_local_gatt_descriptor.h"
#include "device/bluetooth/test/bluetooth_gatt_server_test.h"
#include "device/bluetooth/test/bluetooth_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class BluetoothLocalGattDescriptorTest : public BluetoothGattServerTest {
 public:
  void SetUp() override {
    BluetoothGattServerTest::SetUp();

    StartGattSetup();
    // We will need this device to use with simulating read/write attribute
    // value events.
    device_ = SimulateLowEnergyDevice(1);
    characteristic_ = BluetoothLocalGattCharacteristic::Create(
        BluetoothUUID(kTestUUIDGenericAttribute),
        device::BluetoothLocalGattCharacteristic::Properties(),
        device::BluetoothLocalGattCharacteristic::Permissions(),
        service_.get());
    read_descriptor_ = BluetoothLocalGattDescriptor::Create(
        BluetoothUUID(kTestUUIDGenericAttribute),
        device::BluetoothLocalGattCharacteristic::PERMISSION_READ,
        characteristic_.get());
    write_descriptor_ = BluetoothLocalGattDescriptor::Create(
        BluetoothUUID(kTestUUIDGenericAttribute),
        device::BluetoothLocalGattCharacteristic::
            PERMISSION_WRITE_ENCRYPTED_AUTHENTICATED,
        characteristic_.get());
    EXPECT_LT(0u, read_descriptor_->GetIdentifier().size());
    EXPECT_LT(0u, write_descriptor_->GetIdentifier().size());
    CompleteGattSetup();
  }

 protected:
  base::WeakPtr<BluetoothLocalGattCharacteristic> characteristic_;
  base::WeakPtr<BluetoothLocalGattDescriptor> read_descriptor_;
  base::WeakPtr<BluetoothLocalGattDescriptor> write_descriptor_;
  BluetoothDevice* device_;
};

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
TEST_F(BluetoothLocalGattDescriptorTest, ReadLocalDescriptorValue) {
  delegate_->value_to_write_ = 0x1337;
  SimulateLocalGattDescriptorValueReadRequest(
      device_, read_descriptor_.get(), GetReadValueCallback(Call::EXPECTED),
      GetCallback(Call::NOT_EXPECTED));

  EXPECT_EQ(delegate_->value_to_write_, GetInteger(last_read_value_));
  EXPECT_EQ(device_->GetIdentifier(), delegate_->last_seen_device_);
}
#endif  // defined(OS_CHROMEOS) || defined(OS_LINUX)

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
TEST_F(BluetoothLocalGattDescriptorTest, WriteLocalDescriptorValue) {
  const uint64_t kValueToWrite = 0x7331ul;
  SimulateLocalGattDescriptorValueWriteRequest(
      device_, write_descriptor_.get(), GetValue(kValueToWrite),
      GetCallback(Call::EXPECTED), GetCallback(Call::NOT_EXPECTED));

  EXPECT_EQ(kValueToWrite, delegate_->last_written_value_);
  EXPECT_EQ(device_->GetIdentifier(), delegate_->last_seen_device_);
}
#endif  // defined(OS_CHROMEOS) || defined(OS_LINUX)

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
TEST_F(BluetoothLocalGattDescriptorTest, ReadLocalDescriptorValueFail) {
  delegate_->value_to_write_ = 0x1337;
  delegate_->should_fail_ = true;
  SimulateLocalGattDescriptorValueReadRequest(
      device_, read_descriptor_.get(), GetReadValueCallback(Call::NOT_EXPECTED),
      GetCallback(Call::EXPECTED));

  EXPECT_NE(delegate_->value_to_write_, GetInteger(last_read_value_));
  EXPECT_NE(device_->GetIdentifier(), delegate_->last_seen_device_);
}
#endif  // defined(OS_CHROMEOS) || defined(OS_LINUX)

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
TEST_F(BluetoothLocalGattDescriptorTest, WriteLocalDescriptorValueFail) {
  const uint64_t kValueToWrite = 0x7331ul;
  delegate_->should_fail_ = true;
  SimulateLocalGattDescriptorValueWriteRequest(
      device_, write_descriptor_.get(), GetValue(kValueToWrite),
      GetCallback(Call::NOT_EXPECTED), GetCallback(Call::EXPECTED));

  EXPECT_NE(kValueToWrite, delegate_->last_written_value_);
  EXPECT_NE(device_->GetIdentifier(), delegate_->last_seen_device_);
}
#endif  // defined(OS_CHROMEOS) || defined(OS_LINUX)

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
TEST_F(BluetoothLocalGattDescriptorTest,
       ReadLocalDescriptorValueWrongPermissions) {
  delegate_->value_to_write_ = 0x1337;
  SimulateLocalGattDescriptorValueReadRequest(
      device_, write_descriptor_.get(),
      GetReadValueCallback(Call::NOT_EXPECTED), GetCallback(Call::EXPECTED));

  EXPECT_NE(delegate_->value_to_write_, GetInteger(last_read_value_));
  EXPECT_NE(device_->GetIdentifier(), delegate_->last_seen_device_);
}
#endif  // defined(OS_CHROMEOS) || defined(OS_LINUX)

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
TEST_F(BluetoothLocalGattDescriptorTest,
       WriteLocalDescriptorValueWrongPermissions) {
  const uint64_t kValueToWrite = 0x7331ul;
  SimulateLocalGattDescriptorValueWriteRequest(
      device_, read_descriptor_.get(), GetValue(kValueToWrite),
      GetCallback(Call::NOT_EXPECTED), GetCallback(Call::EXPECTED));

  EXPECT_NE(kValueToWrite, delegate_->last_written_value_);
  EXPECT_NE(device_->GetIdentifier(), delegate_->last_seen_device_);
}
#endif  // defined(OS_CHROMEOS) || defined(OS_LINUX)

}  // namespace device
