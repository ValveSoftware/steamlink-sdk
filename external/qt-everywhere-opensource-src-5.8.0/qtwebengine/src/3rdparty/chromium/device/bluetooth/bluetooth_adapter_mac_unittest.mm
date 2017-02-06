// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter_mac.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/test/test_simple_task_runner.h"
#include "build/build_config.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_discovery_session_outcome.h"
#include "device/bluetooth/bluetooth_low_energy_device_mac.h"
#include "device/bluetooth/test/mock_bluetooth_cbperipheral_mac.h"
#include "device/bluetooth/test/mock_bluetooth_central_manager_mac.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/ocmock/OCMock/OCMock.h"

#if defined(OS_IOS)
#import <CoreBluetooth/CoreBluetooth.h>
#else  // !defined(OS_IOS)
#import <IOBluetooth/IOBluetooth.h>
#endif  // defined(OS_IOS)

#import <Foundation/Foundation.h>

namespace {
// |kTestHashAddress| is the hash corresponding to identifier |kTestNSUUID|.
const char* const kTestNSUUID = "00000000-1111-2222-3333-444444444444";
const std::string kTestHashAddress = "D1:6F:E3:22:FD:5B";
const int kTestRssi = 0;
}  // namespace

namespace device {

class BluetoothAdapterMacTest : public testing::Test {
 public:
  BluetoothAdapterMacTest()
      : ui_task_runner_(new base::TestSimpleTaskRunner()),
        adapter_(new BluetoothAdapterMac()),
        adapter_mac_(static_cast<BluetoothAdapterMac*>(adapter_.get())),
        callback_count_(0),
        error_callback_count_(0) {
    adapter_mac_->InitForTest(ui_task_runner_);
  }

  // Helper methods for setup and access to BluetoothAdapterMacTest's members.
  void PollAdapter() { adapter_mac_->PollAdapter(); }

  void LowEnergyDeviceUpdated(CBPeripheral* peripheral,
                              NSDictionary* advertisement_data,
                              int rssi) {
    adapter_mac_->LowEnergyDeviceUpdated(peripheral, advertisement_data, rssi);
  }

  BluetoothDevice* GetDevice(const std::string& address) {
    return adapter_->GetDevice(address);
  }

  CBPeripheral* CreateMockPeripheral(const char* identifier) {
    if (!BluetoothAdapterMac::IsLowEnergyAvailable()) {
      LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
      return nil;
    }
    base::scoped_nsobject<MockCBPeripheral> mock_peripheral(
        [[MockCBPeripheral alloc] initWithUTF8StringIdentifier:identifier]);
    return [mock_peripheral.get().peripheral retain];
  }

  NSDictionary* AdvertisementData() {
    NSDictionary* advertisement_data = @{
      CBAdvertisementDataIsConnectable : @(YES),
      CBAdvertisementDataServiceDataKey : [NSDictionary dictionary],
    };
    return [advertisement_data retain];
  }

  std::string GetHashAddress(CBPeripheral* peripheral) {
    return BluetoothLowEnergyDeviceMac::GetPeripheralHashAddress(peripheral);
  }

  void AddLowEnergyDevice(BluetoothLowEnergyDeviceMac* device) {
    adapter_mac_->devices_.set(device->GetAddress(),
                               std::unique_ptr<BluetoothDevice>(device));
  }

  int NumDevices() { return adapter_mac_->devices_.size(); }

  bool DevicePresent(CBPeripheral* peripheral) {
    BluetoothDevice* device = adapter_mac_->GetDevice(
        BluetoothLowEnergyDeviceMac::GetPeripheralHashAddress(peripheral));
    return (device != NULL);
  }

  bool SetMockCentralManager(CBCentralManagerState desired_state) {
    if (!BluetoothAdapterMac::IsLowEnergyAvailable()) {
      LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
      return false;
    }
    mock_central_manager_.reset([[MockCentralManager alloc] init]);
    [mock_central_manager_ setState:desired_state];
    CBCentralManager* centralManager =
        (CBCentralManager*)mock_central_manager_.get();
    adapter_mac_->SetCentralManagerForTesting(centralManager);
    return true;
  }

  void AddDiscoverySession(BluetoothDiscoveryFilter* discovery_filter) {
    adapter_mac_->AddDiscoverySession(
        discovery_filter,
        base::Bind(&BluetoothAdapterMacTest::Callback, base::Unretained(this)),
        base::Bind(&BluetoothAdapterMacTest::DiscoveryErrorCallback,
                   base::Unretained(this)));
  }

  void RemoveDiscoverySession(BluetoothDiscoveryFilter* discovery_filter) {
    adapter_mac_->RemoveDiscoverySession(
        discovery_filter,
        base::Bind(&BluetoothAdapterMacTest::Callback, base::Unretained(this)),
        base::Bind(&BluetoothAdapterMacTest::DiscoveryErrorCallback,
                   base::Unretained(this)));
  }

  int NumDiscoverySessions() { return adapter_mac_->num_discovery_sessions_; }

  // Generic callbacks.
  void Callback() { ++callback_count_; }
  void ErrorCallback() { ++error_callback_count_; }
  void DiscoveryErrorCallback(UMABluetoothDiscoverySessionOutcome) {
    ++error_callback_count_;
  }

 protected:
  scoped_refptr<base::TestSimpleTaskRunner> ui_task_runner_;
  scoped_refptr<BluetoothAdapter> adapter_;
  BluetoothAdapterMac* adapter_mac_;

  // Owned by |adapter_mac_|.
  base::scoped_nsobject<MockCentralManager> mock_central_manager_;

  int callback_count_;
  int error_callback_count_;
};

TEST_F(BluetoothAdapterMacTest, Poll) {
  PollAdapter();
  EXPECT_FALSE(ui_task_runner_->GetPendingTasks().empty());
}

TEST_F(BluetoothAdapterMacTest, AddDiscoverySessionWithLowEnergyFilter) {
  if (!SetMockCentralManager(CBCentralManagerStatePoweredOn))
    return;
  EXPECT_EQ(0, [mock_central_manager_ scanForPeripheralsCallCount]);
  EXPECT_EQ(0, NumDiscoverySessions());

  std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter(
      new BluetoothDiscoveryFilter(BLUETOOTH_TRANSPORT_LE));
  AddDiscoverySession(discovery_filter.get());
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(1, NumDiscoverySessions());

  // Check that adding a discovery session resulted in
  // scanForPeripheralsWithServices being called on the Central Manager.
  EXPECT_EQ(1, [mock_central_manager_ scanForPeripheralsCallCount]);
}

// TODO(krstnmnlsn): Test changing the filter when adding the second discovery
// session (once we have that ability).
TEST_F(BluetoothAdapterMacTest, AddSecondDiscoverySessionWithLowEnergyFilter) {
  if (!SetMockCentralManager(CBCentralManagerStatePoweredOn))
    return;
  std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter(
      new BluetoothDiscoveryFilter(BLUETOOTH_TRANSPORT_LE));
  AddDiscoverySession(discovery_filter.get());
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(1, NumDiscoverySessions());

  // We replaced the success callback handed to AddDiscoverySession, so
  // |adapter_mac_| should remain in a discovering state indefinitely.
  EXPECT_TRUE(adapter_mac_->IsDiscovering());

  AddDiscoverySession(discovery_filter.get());
  EXPECT_EQ(2, [mock_central_manager_ scanForPeripheralsCallCount]);
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(2, NumDiscoverySessions());
}

TEST_F(BluetoothAdapterMacTest, RemoveDiscoverySessionWithLowEnergyFilter) {
  if (!SetMockCentralManager(CBCentralManagerStatePoweredOn))
    return;
  EXPECT_EQ(0, [mock_central_manager_ scanForPeripheralsCallCount]);

  std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter(
      new BluetoothDiscoveryFilter(BLUETOOTH_TRANSPORT_LE));
  AddDiscoverySession(discovery_filter.get());
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(1, NumDiscoverySessions());

  EXPECT_EQ(0, [mock_central_manager_ stopScanCallCount]);
  RemoveDiscoverySession(discovery_filter.get());
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(0, NumDiscoverySessions());

  // Check that removing the discovery session resulted in stopScan being called
  // on the Central Manager.
  EXPECT_EQ(1, [mock_central_manager_ stopScanCallCount]);
}

TEST_F(BluetoothAdapterMacTest, RemoveDiscoverySessionWithLowEnergyFilterFail) {
  if (!SetMockCentralManager(CBCentralManagerStatePoweredOn))
    return;
  EXPECT_EQ(0, [mock_central_manager_ scanForPeripheralsCallCount]);
  EXPECT_EQ(0, [mock_central_manager_ stopScanCallCount]);
  EXPECT_EQ(0, NumDiscoverySessions());

  std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter(
      new BluetoothDiscoveryFilter(BLUETOOTH_TRANSPORT_LE));
  RemoveDiscoverySession(discovery_filter.get());
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(0, NumDiscoverySessions());

  // Check that stopScan was not called.
  EXPECT_EQ(0, [mock_central_manager_ stopScanCallCount]);
}

TEST_F(BluetoothAdapterMacTest, CheckGetPeripheralHashAddress) {
  if (!SetMockCentralManager(CBCentralManagerStatePoweredOn))
    return;
  base::scoped_nsobject<CBPeripheral> mock_peripheral(
      CreateMockPeripheral(kTestNSUUID));
  if (mock_peripheral.get() == nil)
    return;
  EXPECT_EQ(kTestHashAddress, GetHashAddress(mock_peripheral));
}

TEST_F(BluetoothAdapterMacTest, LowEnergyDeviceUpdatedNewDevice) {
  if (!SetMockCentralManager(CBCentralManagerStatePoweredOn))
    return;
  base::scoped_nsobject<CBPeripheral> mock_peripheral(
      CreateMockPeripheral(kTestNSUUID));
  if (mock_peripheral.get() == nil)
    return;
  base::scoped_nsobject<NSDictionary> advertisement_data(AdvertisementData());

  EXPECT_EQ(0, NumDevices());
  EXPECT_FALSE(DevicePresent(mock_peripheral));
  LowEnergyDeviceUpdated(mock_peripheral, advertisement_data, kTestRssi);
  EXPECT_EQ(1, NumDevices());
  EXPECT_TRUE(DevicePresent(mock_peripheral));
}

}  // namespace device
