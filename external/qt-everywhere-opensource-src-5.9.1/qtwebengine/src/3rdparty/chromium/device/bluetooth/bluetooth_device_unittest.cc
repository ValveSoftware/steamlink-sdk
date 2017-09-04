// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_device.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "device/bluetooth/test/test_bluetooth_adapter_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "device/bluetooth/test/bluetooth_test_android.h"
#elif defined(OS_MACOSX)
#include "device/bluetooth/test/bluetooth_test_mac.h"
#elif defined(OS_WIN)
#include "device/bluetooth/test/bluetooth_test_win.h"
#elif defined(OS_CHROMEOS) || defined(OS_LINUX)
#include "device/bluetooth/test/bluetooth_test_bluez.h"
#endif

namespace device {

namespace {

int8_t ToInt8(BluetoothTest::TestRSSI rssi) {
  return static_cast<int8_t>(rssi);
}

int8_t ToInt8(BluetoothTest::TestTxPower tx_power) {
  return static_cast<int8_t>(tx_power);
}

}  // namespace

using UUIDSet = BluetoothDevice::UUIDSet;
using ServiceDataMap = BluetoothDevice::ServiceDataMap;

TEST(BluetoothDeviceTest, CanonicalizeAddressFormat_AcceptsAllValidFormats) {
  // There are three valid separators (':', '-', and none).
  // Case shouldn't matter.
  const char* const kValidFormats[] = {
    "1A:2B:3C:4D:5E:6F",
    "1a:2B:3c:4D:5e:6F",
    "1a:2b:3c:4d:5e:6f",
    "1A-2B-3C-4D-5E-6F",
    "1a-2B-3c-4D-5e-6F",
    "1a-2b-3c-4d-5e-6f",
    "1A2B3C4D5E6F",
    "1a2B3c4D5e6F",
    "1a2b3c4d5e6f",
  };

  for (size_t i = 0; i < arraysize(kValidFormats); ++i) {
    SCOPED_TRACE(std::string("Input format: '") + kValidFormats[i] + "'");
    EXPECT_EQ("1A:2B:3C:4D:5E:6F",
              BluetoothDevice::CanonicalizeAddress(kValidFormats[i]));
  }
}

TEST(BluetoothDeviceTest, CanonicalizeAddressFormat_RejectsInvalidFormats) {
  const char* const kValidFormats[] = {
    // Empty string.
    "",
    // Too short.
    "1A:2B:3C:4D:5E",
    // Too long.
    "1A:2B:3C:4D:5E:6F:70",
    // Missing a separator.
    "1A:2B:3C:4D:5E6F",
    // Mixed separators.
    "1A:2B-3C:4D-5E:6F",
    // Invalid characters.
    "1A:2B-3C:4D-5E:6X",
    // Separators in the wrong place.
    "1:A2:B3:C4:D5:E6F",
  };

  for (size_t i = 0; i < arraysize(kValidFormats); ++i) {
    SCOPED_TRACE(std::string("Input format: '") + kValidFormats[i] + "'");
    EXPECT_EQ(std::string(),
              BluetoothDevice::CanonicalizeAddress(kValidFormats[i]));
  }
}

#if defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)
// Verifies basic device properties, e.g. GetAddress, GetName, ...
TEST_F(BluetoothTest, LowEnergyDeviceProperties) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(1);
  ASSERT_TRUE(device);
// Bluetooth class information for BLE device is not available on Windows.
#ifndef OS_WIN
  EXPECT_EQ(0x1F00u, device->GetBluetoothClass());
#endif
  EXPECT_EQ(kTestDeviceAddress1, device->GetAddress());
  EXPECT_EQ(BluetoothDevice::VENDOR_ID_UNKNOWN, device->GetVendorIDSource());
  EXPECT_EQ(0, device->GetVendorID());
  EXPECT_EQ(0, device->GetProductID());
  EXPECT_EQ(0, device->GetDeviceID());
  EXPECT_EQ(base::UTF8ToUTF16(kTestDeviceName), device->GetNameForDisplay());
  EXPECT_FALSE(device->IsPaired());
  UUIDSet uuids = device->GetUUIDs();
  EXPECT_TRUE(base::ContainsKey(uuids, BluetoothUUID(kTestUUIDGenericAccess)));
  EXPECT_TRUE(
      base::ContainsKey(uuids, BluetoothUUID(kTestUUIDGenericAttribute)));
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)

#if defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)
// Device with no advertised Service UUIDs.
TEST_F(BluetoothTest, LowEnergyDeviceNoUUIDs) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  ASSERT_TRUE(device);
  UUIDSet uuids = device->GetUUIDs();
  EXPECT_EQ(0u, uuids.size());
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)

#if defined(OS_MACOSX)
// TODO(ortuno): Enable on Android once it supports Service Data.
// http://crbug.com/639408
TEST_F(BluetoothTest, GetServiceDataUUIDs_GetServiceDataForUUID) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();

  // Receive Advertisement with service data.
  BluetoothDevice* device = SimulateLowEnergyDevice(1);

  EXPECT_EQ(ServiceDataMap({{BluetoothUUID(kTestUUIDHeartRate), {1}}}),
            device->GetServiceData());
  EXPECT_EQ(UUIDSet({BluetoothUUID(kTestUUIDHeartRate)}),
            device->GetServiceDataUUIDs());
  EXPECT_EQ(std::vector<uint8_t>({1}),
            *device->GetServiceDataForUUID(BluetoothUUID(kTestUUIDHeartRate)));

  // Receive Advertisement with no service data.
  SimulateLowEnergyDevice(3);

  EXPECT_TRUE(device->GetServiceData().empty());
  EXPECT_TRUE(device->GetServiceDataUUIDs().empty());
  EXPECT_EQ(nullptr,
            device->GetServiceDataForUUID(BluetoothUUID(kTestUUIDHeartRate)));

  // Receive Advertisement with new service data.
  SimulateLowEnergyDevice(2);

  EXPECT_EQ(ServiceDataMap({{BluetoothUUID(kTestUUIDHeartRate), {2}},
                            {BluetoothUUID(kTestUUIDImmediateAlert), {0}}}),
            device->GetServiceData());
  EXPECT_EQ(UUIDSet({BluetoothUUID(kTestUUIDHeartRate),
                     BluetoothUUID(kTestUUIDImmediateAlert)}),
            device->GetServiceDataUUIDs());
  EXPECT_EQ(std::vector<uint8_t>({2}),
            *device->GetServiceDataForUUID(BluetoothUUID(kTestUUIDHeartRate)));
  EXPECT_EQ(
      std::vector<uint8_t>({0}),
      *device->GetServiceDataForUUID(BluetoothUUID(kTestUUIDImmediateAlert)));

  // Stop discovery.
  discovery_sessions_[0]->Stop(GetCallback(Call::EXPECTED),
                               GetErrorCallback(Call::NOT_EXPECTED));
  ASSERT_FALSE(adapter_->IsDiscovering());
  ASSERT_FALSE(discovery_sessions_[0]->IsActive());

  EXPECT_TRUE(device->GetServiceData().empty());
  EXPECT_TRUE(device->GetServiceDataUUIDs().empty());
  EXPECT_EQ(nullptr,
            device->GetServiceDataForUUID(BluetoothUUID(kTestUUIDHeartRate)));
  EXPECT_EQ(nullptr, device->GetServiceDataForUUID(
                         BluetoothUUID(kTestUUIDImmediateAlert)));
}
#endif  // defined(OS_MACOSX)

#if defined(OS_ANDROID) || defined(OS_MACOSX)
// Tests that the Advertisement Data fields are correctly updated during
// discovery.
TEST_F(BluetoothTest, AdvertisementData_Discovery) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();
  TestBluetoothAdapterObserver observer(adapter_);

  // Start Discovery Session and receive Advertisement, should
  // not notify of device changed because the device is new.
  //  - GetInquiryRSSI: Should return the packet's rssi.
  //  - GetUUIDs: Should return Advertised UUIDs.
  //  - GetServiceData: Should return advertised Service Data.
  //  - GetInquiryTxPower: Should return the packet's advertised Tx Power.
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(1);

  EXPECT_EQ(0, observer.device_changed_count());

  EXPECT_EQ(ToInt8(TestRSSI::LOWEST), device->GetInquiryRSSI().value());
  EXPECT_EQ(UUIDSet({BluetoothUUID(kTestUUIDGenericAccess),
                     BluetoothUUID(kTestUUIDGenericAttribute)}),
            device->GetUUIDs());
#if defined(OS_MACOSX)
  // TODO(ortuno): Enable on Android once it supports Service Data.
  // http://crbug.com/639408
  EXPECT_EQ(ServiceDataMap({{BluetoothUUID(kTestUUIDHeartRate), {1}}}),
            device->GetServiceData());
#endif  // defined(OS_MACOSX)
  EXPECT_EQ(ToInt8(TestTxPower::LOWEST), device->GetInquiryTxPower().value());

  // Receive Advertisement with no UUIDs, Service Data, or Tx Power, should
  // notify device changed.
  //  - GetInquiryRSSI: Should return packet's rssi.
  //  - GetUUIDs: Should return no UUIDs.
  //  - GetServiceData: Should return empty map.
  //  - GetInquiryTxPower: Should return nullopt because of no Tx Power.
  SimulateLowEnergyDevice(3);
  EXPECT_EQ(1, observer.device_changed_count());

  EXPECT_EQ(ToInt8(TestRSSI::LOW), device->GetInquiryRSSI().value());
  EXPECT_TRUE(device->GetUUIDs().empty());
#if defined(OS_MACOSX)
  // TODO(ortuno): Enable on Android once it supports Service Data.
  // http://crbug.com/639408
  EXPECT_TRUE(device->GetServiceData().empty());
#endif  // defined(OS_MACOSX)
  EXPECT_FALSE(device->GetInquiryTxPower());

  // Receive Advertisement with different UUIDs, Service Data, and Tx Power,
  // should notify device changed.
  //  - GetInquiryRSSI: Should return last packet's rssi.
  //  - GetUUIDs: Should return latest Advertised UUIDs.
  //  - GetServiceData: Should return last advertised Service Data.
  //  - GetInquiryTxPower: Should return last advertised Tx Power.
  SimulateLowEnergyDevice(2);
  EXPECT_EQ(2, observer.device_changed_count());

  EXPECT_EQ(ToInt8(TestRSSI::LOWER), device->GetInquiryRSSI().value());
  EXPECT_EQ(UUIDSet({BluetoothUUID(kTestUUIDImmediateAlert),
                     BluetoothUUID(kTestUUIDLinkLoss)}),
            device->GetUUIDs());
#if defined(OS_MACOSX)
  // TODO(ortuno): Enable on Android once it supports Service Data.
  // http://crbug.com/639408
  EXPECT_EQ(ServiceDataMap({{BluetoothUUID(kTestUUIDHeartRate), {2}},
                            {BluetoothUUID(kTestUUIDImmediateAlert), {0}}}),
            device->GetServiceData());
#endif  // defined(OS_MACOSX)
  EXPECT_EQ(ToInt8(TestTxPower::LOWER), device->GetInquiryTxPower().value());

  // Stop discovery session, should notify of device changed.
  //  - GetInquiryRSSI: Should return nullopt because we are no longer
  //    discovering.
  //  - GetUUIDs: Should not return any UUIDs.
  //  - GetServiceData: Should return empty map.
  //  - GetInquiryTxPower: Should return nullopt because we are no longer
  //    discovering.
  discovery_sessions_[0]->Stop(GetCallback(Call::EXPECTED),
                               GetErrorCallback(Call::NOT_EXPECTED));
  ASSERT_FALSE(adapter_->IsDiscovering());
  ASSERT_FALSE(discovery_sessions_[0]->IsActive());

  EXPECT_EQ(3, observer.device_changed_count());

  EXPECT_FALSE(device->GetInquiryRSSI());
  EXPECT_TRUE(device->GetUUIDs().empty());
#if defined(OS_MACOSX)
  // TODO(ortuno): Enable on Android once it supports Service Data.
  // http://crbug.com/639408
  EXPECT_TRUE(device->GetServiceData().empty());
#endif  // defined(OS_MACOSX)
  EXPECT_FALSE(device->GetInquiryTxPower());

  // Discover the device again with different UUIDs, should notify of device
  // changed.
  //  - GetInquiryRSSI: Should return last packet's rssi.
  //  - GetUUIDs: Should return only the latest Advertised UUIDs.
  //  - GetServiceData: Should return last advertise Service Data.
  //  - GetInquiryTxPower: Should return last advertised Tx Power.
  StartLowEnergyDiscoverySession();
  device = SimulateLowEnergyDevice(1);

  EXPECT_EQ(4, observer.device_changed_count());

  EXPECT_EQ(ToInt8(TestRSSI::LOWEST), device->GetInquiryRSSI().value());
  EXPECT_EQ(UUIDSet({BluetoothUUID(kTestUUIDGenericAccess),
                     BluetoothUUID(kTestUUIDGenericAttribute)}),
            device->GetUUIDs());
#if defined(OS_MACOSX)
  // TODO(ortuno): Enable on Android once it supports Service Data.
  // http://crbug.com/639408
  EXPECT_EQ(ServiceDataMap({{BluetoothUUID(kTestUUIDHeartRate), {1}}}),
            device->GetServiceData());
#endif  // defined(OS_MACOSX)
  EXPECT_EQ(ToInt8(TestTxPower::LOWEST), device->GetInquiryTxPower().value());
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX)

#if defined(OS_ANDROID) || defined(OS_MACOSX)
// Tests Advertisement Data is updated correctly during a connection.
TEST_F(BluetoothTest, GetUUIDs_Connection) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }

  InitWithFakeAdapter();
  TestBluetoothAdapterObserver observer(adapter_);

  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(1);
  discovery_sessions_[0]->Stop(GetCallback(Call::EXPECTED),
                               GetErrorCallback(Call::NOT_EXPECTED));

  // Connect to the device.
  //  - GetUUIDs: Should return no UUIDs because Services have not been
  //    discovered.
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
  SimulateGattConnection(device);
  ASSERT_TRUE(device->IsConnected());

  EXPECT_TRUE(device->GetUUIDs().empty());

  observer.Reset();

  // Discover services, should notify of device changed.
  //  - GetUUIDs: Should return the device's services' UUIDs.
  std::vector<std::string> services;
  services.push_back(BluetoothUUID(kTestUUIDGenericAccess).canonical_value());
  SimulateGattServicesDiscovered(device, services);

  EXPECT_EQ(1, observer.device_changed_count());

  EXPECT_EQ(UUIDSet({BluetoothUUID(kTestUUIDGenericAccess)}),
            device->GetUUIDs());

#if defined(OS_MACOSX)
  // TODO(ortuno): Enable in Android and Windows.
  // Android and Windows don't yet support service changed events.
  // http://crbug.com/548280
  // http://crbug.com/579202

  observer.Reset();

  // Notify of services changed, should notify of device changed.
  //  - GetUUIDs: Should return no UUIDs because we no longer know what services
  //    the device has.
  SimulateGattServicesChanged(device);

  ASSERT_FALSE(device->IsGattServicesDiscoveryComplete());
  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_TRUE(device->GetUUIDs().empty());

  // Services discovered again, should notify of device changed.
  //  - GetUUIDs: Should return Service UUIDs.
  SimulateGattServicesDiscovered(device, {} /* services */);

  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_EQ(UUIDSet({BluetoothUUID(kTestUUIDGenericAccess)}),
            device->GetUUIDs());

#endif  // defined(OS_MACOSX)

  observer.Reset();

  // Disconnect, should notify device changed.
  //  - GetUUIDs: Should return no UUIDs since we no longer know what services
  //    the device holds and notify of device changed.
  gatt_connections_[0]->Disconnect();
  SimulateGattDisconnection(device);
  ASSERT_FALSE(device->IsGattConnected());

  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_TRUE(device->GetUUIDs().empty());
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX)

#if defined(OS_ANDROID) || defined(OS_MACOSX)
// Tests Advertisement Data is updated correctly when we start discovery
// during a connection.
TEST_F(BluetoothTest, AdvertisementData_DiscoveryDuringConnection) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }

  InitWithFakeAdapter();
  TestBluetoothAdapterObserver observer(adapter_);

  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(1);

  EXPECT_EQ(UUIDSet({BluetoothUUID(kTestUUIDGenericAccess),
                     BluetoothUUID(kTestUUIDGenericAttribute)}),
            device->GetUUIDs());
  discovery_sessions_[0]->Stop(GetCallback(Call::EXPECTED),
                               GetErrorCallback(Call::NOT_EXPECTED));
  ASSERT_FALSE(adapter_->IsDiscovering());
  ASSERT_FALSE(discovery_sessions_[0]->IsActive());
  ASSERT_EQ(0u, device->GetUUIDs().size());
  discovery_sessions_.clear();

  // Connect.
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
  SimulateGattConnection(device);
  ASSERT_TRUE(device->IsConnected());

  observer.Reset();

  // Start Discovery and receive advertisement during connection,
  // should notify of device changed.
  //  - GetInquiryRSSI: Should return the packet's rssi.
  //  - GetUUIDs: Should return only Advertised UUIDs since services haven't
  //    been discovered yet.
  //  - GetServiceData: Should return last advertised Service Data.
  //  - GetInquiryTxPower: Should return the packet's advertised Tx Power.
  StartLowEnergyDiscoverySession();
  ASSERT_TRUE(adapter_->IsDiscovering());
  ASSERT_TRUE(discovery_sessions_[0]->IsActive());
  device = SimulateLowEnergyDevice(1);

  EXPECT_EQ(1, observer.device_changed_count());

  EXPECT_EQ(ToInt8(TestRSSI::LOWEST), device->GetInquiryRSSI().value());
  EXPECT_EQ(UUIDSet({BluetoothUUID(kTestUUIDGenericAccess),
                     BluetoothUUID(kTestUUIDGenericAttribute)}),
            device->GetUUIDs());
#if defined(OS_MACOSX)
  // TODO(ortuno): Enable on Android once it supports Service Data.
  // http://crbug.com/639408
  EXPECT_EQ(ServiceDataMap({{BluetoothUUID(kTestUUIDHeartRate), {1}}}),
            device->GetServiceData());
#endif  // defined(OS_MACOSX)
  EXPECT_EQ(ToInt8(TestTxPower::LOWEST), device->GetInquiryTxPower().value());

  // Discover services, should notify of device changed.
  //  - GetUUIDs: Should return both Advertised UUIDs and Service UUIDs.
  std::vector<std::string> services;
  services.push_back(BluetoothUUID(kTestUUIDHeartRate).canonical_value());
  SimulateGattServicesDiscovered(device, services);

  EXPECT_EQ(2, observer.device_changed_count());

  EXPECT_EQ(UUIDSet({BluetoothUUID(kTestUUIDGenericAccess),
                     BluetoothUUID(kTestUUIDGenericAttribute),
                     BluetoothUUID(kTestUUIDHeartRate)}),
            device->GetUUIDs());

  // Receive advertisement again, notify of device changed.
  //  - GetInquiryRSSI: Should return last packet's rssi.
  //  - GetUUIDs: Should return only new Advertised UUIDs and Service UUIDs.
  //  - GetServiceData: Should return last advertised Service Data.
  //  - GetInquiryTxPower: Should return the last packet's advertised Tx Power.
  device = SimulateLowEnergyDevice(2);

  EXPECT_EQ(3, observer.device_changed_count());
  EXPECT_EQ(ToInt8(TestRSSI::LOWER), device->GetInquiryRSSI().value());
  EXPECT_EQ(UUIDSet({BluetoothUUID(kTestUUIDLinkLoss),
                     BluetoothUUID(kTestUUIDImmediateAlert),
                     BluetoothUUID(kTestUUIDHeartRate)}),
            device->GetUUIDs());
#if defined(OS_MACOSX)
  // TODO(ortuno): Enable on Android once it supports Service Data.
  // http://crbug.com/639408
  EXPECT_EQ(ServiceDataMap({{BluetoothUUID(kTestUUIDHeartRate), {2}},
                            {BluetoothUUID(kTestUUIDImmediateAlert), {0}}}),
            device->GetServiceData());
#endif  // defined(OS_MACOSX)
  EXPECT_EQ(ToInt8(TestTxPower::LOWER), device->GetInquiryTxPower().value());

  // Stop discovery session, should notify of device changed.
  //  - GetInquiryRSSI: Should return nullopt because we are no longer
  //    discovering.
  //  - GetUUIDs: Should only return Service UUIDs.
  //  - GetServiceData: Should return an empty map since we are no longer
  //    discovering.
  //  - GetInquiryTxPower: Should return nullopt because we are no longer
  //    discovering.
  discovery_sessions_[0]->Stop(GetCallback(Call::EXPECTED),
                               GetErrorCallback(Call::NOT_EXPECTED));
  ASSERT_FALSE(adapter_->IsDiscovering());
  ASSERT_FALSE(discovery_sessions_[0]->IsActive());

  EXPECT_EQ(4, observer.device_changed_count());
  EXPECT_FALSE(device->GetInquiryRSSI());
  EXPECT_EQ(UUIDSet({BluetoothUUID(kTestUUIDHeartRate)}), device->GetUUIDs());
#if defined(OS_MACOSX)
  // TODO(ortuno): Enable on Android once it supports Service Data.
  // http://crbug.com/639408
  EXPECT_EQ(ServiceDataMap(), device->GetServiceData());
#endif  // defined(OS_MACOSX)
  EXPECT_FALSE(device->GetInquiryTxPower());

  // Disconnect device, should notify of device changed.
  //  - GetUUIDs: Should return no UUIDs.
  gatt_connections_[0]->Disconnect();
  SimulateGattDisconnection(device);
  ASSERT_FALSE(device->IsGattConnected());

  EXPECT_EQ(5, observer.device_changed_count());

  EXPECT_TRUE(device->GetUUIDs().empty());
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX)

#if defined(OS_ANDROID) || defined(OS_MACOSX)
TEST_F(BluetoothTest, AdvertisementData_ConnectionDuringDiscovery) {
  // Tests that the Advertisement Data is correctly updated when
  // the device connects during discovery.
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }

  InitWithFakeAdapter();
  TestBluetoothAdapterObserver observer(adapter_);

  // Start discovery session and receive and advertisement. No device changed
  // notification because it's a new device.
  //  - GetInquiryRSSI: Should return the packet's rssi.
  //  - GetUUIDs: Should return Advertised UUIDs.
  //  - GetServiceData: Should return advertised Service Data.
  //  - GetInquiryTxPower: Should return the packet's advertised Tx Power.
  StartLowEnergyDiscoverySession();
  ASSERT_TRUE(adapter_->IsDiscovering());
  ASSERT_TRUE(discovery_sessions_[0]->IsActive());
  BluetoothDevice* device = SimulateLowEnergyDevice(1);

  EXPECT_EQ(0, observer.device_changed_count());
  EXPECT_EQ(ToInt8(TestRSSI::LOWEST), device->GetInquiryRSSI().value());
  EXPECT_EQ(UUIDSet({BluetoothUUID(kTestUUIDGenericAccess),
                     BluetoothUUID(kTestUUIDGenericAttribute)}),
            device->GetUUIDs());
#if defined(OS_MACOSX)
  // TODO(ortuno): Enable on Android once it supports Service Data.
  // http://crbug.com/639408
  EXPECT_EQ(ServiceDataMap({{BluetoothUUID(kTestUUIDHeartRate), {1}}}),
            device->GetServiceData());
#endif  // defined(OS_MACOSX)
  EXPECT_EQ(ToInt8(TestTxPower::LOWEST), device->GetInquiryTxPower().value());

  // Connect, should notify of device changed.
  //  - GetUUIDs: Should return Advertised UUIDs even before GATT Discovery.
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
  SimulateGattConnection(device);
  ASSERT_TRUE(device->IsConnected());

  observer.Reset();
  EXPECT_EQ(UUIDSet({BluetoothUUID(kTestUUIDGenericAccess),
                     BluetoothUUID(kTestUUIDGenericAttribute)}),
            device->GetUUIDs());

  // Receive Advertisement with new UUIDs, should notify of device changed.
  //  - GetInquiryRSSI: Should return the packet's rssi.
  //  - GetUUIDs: Should return new Advertised UUIDs.
  //  - GetServiceData: Should return new advertised Service Data.
  //  - GetInquiryTxPower: Should return the packet's advertised Tx Power.
  device = SimulateLowEnergyDevice(2);

  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_EQ(ToInt8(TestRSSI::LOWER), device->GetInquiryRSSI().value());
  EXPECT_EQ(UUIDSet({BluetoothUUID(kTestUUIDLinkLoss),
                     BluetoothUUID(kTestUUIDImmediateAlert)}),
            device->GetUUIDs());
#if defined(OS_MACOSX)
  // TODO(ortuno): Enable on Android once it supports Service Data.
  // http://crbug.com/639408
  EXPECT_EQ(ServiceDataMap({{BluetoothUUID(kTestUUIDHeartRate), {2}},
                            {BluetoothUUID(kTestUUIDImmediateAlert), {0}}}),
            device->GetServiceData());
#endif  // defined(OS_MACOSX)
  EXPECT_EQ(ToInt8(TestTxPower::LOWER), device->GetInquiryTxPower().value());

  // Discover Services, should notify of device changed.
  //  - GetUUIDs: Should return Advertised UUIDs and Service UUIDs.
  std::vector<std::string> services;
  services.push_back(BluetoothUUID(kTestUUIDHeartRate).canonical_value());
  SimulateGattServicesDiscovered(device, services);

  EXPECT_EQ(2, observer.device_changed_count());

  EXPECT_EQ(UUIDSet({BluetoothUUID(kTestUUIDLinkLoss),
                     BluetoothUUID(kTestUUIDImmediateAlert),
                     BluetoothUUID(kTestUUIDHeartRate)}),
            device->GetUUIDs());

  // Disconnect, should notify of device changed.
  //  - GetInquiryRSSI: Should return last packet's rssi.
  //  - GetUUIDs: Should return only Advertised UUIDs.
  //  - GetServiceData: Should still return same advertised Service Data.
  //  - GetInquiryTxPower: Should return the last packet's advertised Tx Power.
  gatt_connections_[0]->Disconnect();
  SimulateGattDisconnection(device);
  ASSERT_FALSE(device->IsGattConnected());

  EXPECT_EQ(3, observer.device_changed_count());
  EXPECT_EQ(ToInt8(TestRSSI::LOWER), device->GetInquiryRSSI().value());
  EXPECT_EQ(UUIDSet({BluetoothUUID(kTestUUIDLinkLoss),
                     BluetoothUUID(kTestUUIDImmediateAlert)}),
            device->GetUUIDs());

#if defined(OS_MACOSX)
  // TODO(ortuno): Enable on Android once it supports Service Data.
  // http://crbug.com/639408
  EXPECT_EQ(ServiceDataMap({{BluetoothUUID(kTestUUIDHeartRate), {2}},
                            {BluetoothUUID(kTestUUIDImmediateAlert), {0}}}),
            device->GetServiceData());
#endif  // defined(OS_MACOSX)
  EXPECT_EQ(ToInt8(TestTxPower::LOWER), device->GetInquiryTxPower().value());

  // Receive Advertisement with new UUIDs, should notify of device changed.
  //  - GetInquiryRSSI: Should return last packet's rssi.
  //  - GetUUIDs: Should return only new Advertised UUIDs.
  //  - GetServiceData: Should return only new advertised Service Data.
  //  - GetInquiryTxPower: Should return the last packet's advertised Tx Power.
  device = SimulateLowEnergyDevice(1);

  EXPECT_EQ(4, observer.device_changed_count());
  EXPECT_EQ(ToInt8(TestRSSI::LOWEST), device->GetInquiryRSSI().value());
  EXPECT_EQ(UUIDSet({BluetoothUUID(kTestUUIDGenericAccess),
                     BluetoothUUID(kTestUUIDGenericAttribute)}),
            device->GetUUIDs());
#if defined(OS_MACOSX)
  // TODO(ortuno): Enable on Android once it supports Service Data.
  // http://crbug.com/639408
  EXPECT_EQ(ServiceDataMap({{BluetoothUUID(kTestUUIDHeartRate), {1}}}),
            device->GetServiceData());
#endif  // defined(OS_MACOSX)
  EXPECT_EQ(ToInt8(TestTxPower::LOWEST), device->GetInquiryTxPower().value());

  // Stop discovery session, should notify of device changed.
  //  - GetInquiryRSSI: Should return nullopt because we are no longer
  //    discovering.
  //  - GetUUIDs: Should return no UUIDs.
  //  - GetServiceData: Should return no UUIDs since we are no longer
  //    discovering.
  //  - GetInquiryTxPower: Should return nullopt because we are no longer
  //    discovering.
  discovery_sessions_[0]->Stop(GetCallback(Call::EXPECTED),
                               GetErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(5, observer.device_changed_count());

  EXPECT_FALSE(device->GetInquiryRSSI());
  EXPECT_TRUE(device->GetUUIDs().empty());
#if defined(OS_MACOSX)
  // TODO(ortuno): Enable on Android once it supports Service Data.
  // http://crbug.com/639408
  EXPECT_TRUE(device->GetServiceData().empty());
#endif  // defined(OS_MACOSX)
  EXPECT_FALSE(device->GetInquiryTxPower());
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX)

#if defined(OS_ANDROID) || defined(OS_CHROMEOS) || defined(OS_MACOSX)
// GetName for Device with no name.
TEST_F(BluetoothTest, GetName_NullName) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();

// StartLowEnergyDiscoverySession is not yet implemented on ChromeOS|bluez,
// and is non trivial to implement. On ChromeOS, it is not essential for
// this test to operate, and so it is simply skipped. Android at least
// does require this step.
#if !defined(OS_CHROMEOS)
  StartLowEnergyDiscoverySession();
#endif

  BluetoothDevice* device = SimulateLowEnergyDevice(5);
  EXPECT_FALSE(device->GetName());
}
#endif  // defined(OS_ANDROID) || defined(OS_CHROMEOS)  || defined(OS_MACOSX)

// TODO(506415): Test GetNameForDisplay with a device with no name.
// BluetoothDevice::GetAddressWithLocalizedDeviceTypeName() will run, which
// requires string resources to be loaded. For that, something like
// InitSharedInstance must be run. See unittest files that call that. It will
// also require build configuration to generate string resources into a .pak
// file.

#if defined(OS_ANDROID) || defined(OS_MACOSX)
// Basic CreateGattConnection test.
TEST_F(BluetoothTest, CreateGattConnection) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);

  ResetEventCounts();
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
  SimulateGattConnection(device);
  ASSERT_EQ(1u, gatt_connections_.size());
  EXPECT_TRUE(device->IsGattConnected());
  EXPECT_TRUE(gatt_connections_[0]->IsConnected());
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX)

#if defined(OS_ANDROID) || defined(OS_MACOSX)
TEST_F(BluetoothTest, DisconnectionNotifiesDeviceChanged) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();
  TestBluetoothAdapterObserver observer(adapter_);
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
  SimulateGattConnection(device);
  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_TRUE(device->IsConnected());
  EXPECT_TRUE(device->IsGattConnected());

  SimulateGattDisconnection(device);
  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsGattConnected());
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX)

#if defined(OS_ANDROID) || defined(OS_MACOSX)
// Creates BluetoothGattConnection instances and tests that the interface
// functions even when some Disconnect and the BluetoothDevice is destroyed.
TEST_F(BluetoothTest, BluetoothGattConnection) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  std::string device_address = device->GetAddress();

  // CreateGattConnection
  ResetEventCounts();
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(1, gatt_connection_attempts_);
  SimulateGattConnection(device);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  ASSERT_EQ(1u, gatt_connections_.size());
  EXPECT_TRUE(device->IsGattConnected());
  EXPECT_TRUE(gatt_connections_[0]->IsConnected());

  // Connect again once already connected.
  ResetEventCounts();
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(0, gatt_connection_attempts_);
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  ASSERT_EQ(3u, gatt_connections_.size());

  // Test GetDeviceAddress
  EXPECT_EQ(device_address, gatt_connections_[0]->GetDeviceAddress());

  // Test IsConnected
  EXPECT_TRUE(gatt_connections_[0]->IsConnected());
  EXPECT_TRUE(gatt_connections_[1]->IsConnected());
  EXPECT_TRUE(gatt_connections_[2]->IsConnected());

  // Disconnect & Delete connection objects. Device stays connected.
  gatt_connections_[0]->Disconnect();  // Disconnect first.
  gatt_connections_.pop_back();        // Delete last.
  EXPECT_FALSE(gatt_connections_[0]->IsConnected());
  EXPECT_TRUE(gatt_connections_[1]->IsConnected());
  EXPECT_TRUE(device->IsGattConnected());
  EXPECT_EQ(0, gatt_disconnection_attempts_);

  // Delete device, connection objects should all be disconnected.
  gatt_disconnection_attempts_ = 0;
  DeleteDevice(device);
  EXPECT_EQ(1, gatt_disconnection_attempts_);
  EXPECT_FALSE(gatt_connections_[0]->IsConnected());
  EXPECT_FALSE(gatt_connections_[1]->IsConnected());

  // Test GetDeviceAddress after device deleted.
  EXPECT_EQ(device_address, gatt_connections_[0]->GetDeviceAddress());
  EXPECT_EQ(device_address, gatt_connections_[1]->GetDeviceAddress());
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX)

#if defined(OS_ANDROID) || defined(OS_MACOSX)
// Calls CreateGattConnection then simulates multiple connections from platform.
TEST_F(BluetoothTest,
       BluetoothGattConnection_ConnectWithMultipleOSConnections) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);

  // CreateGattConnection, & multiple connections from platform only invoke
  // callbacks once:
  ResetEventCounts();
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
  SimulateGattConnection(device);
  SimulateGattConnection(device);
  EXPECT_EQ(1, gatt_connection_attempts_);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(gatt_connections_[0]->IsConnected());

  // Become disconnected:
  SimulateGattDisconnection(device);
  EXPECT_FALSE(gatt_connections_[0]->IsConnected());
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX)

#if defined(OS_ANDROID) || defined(OS_MACOSX)
// Calls CreateGattConnection after already connected.
TEST_F(BluetoothTest, BluetoothGattConnection_AlreadyConnected) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);

  // Be already connected:
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
  SimulateGattConnection(device);
  EXPECT_TRUE(gatt_connections_[0]->IsConnected());

  // Then CreateGattConnection:
  ResetEventCounts();
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(0, gatt_connection_attempts_);
  EXPECT_TRUE(gatt_connections_[1]->IsConnected());
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX)

#if defined(OS_ANDROID) || defined(OS_MACOSX)
// Creates BluetoothGattConnection after one exists that has disconnected.
TEST_F(BluetoothTest,
       BluetoothGattConnection_NewConnectionLeavesPreviousDisconnected) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);

  // Create connection:
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
  SimulateGattConnection(device);

  // Disconnect connection:
  gatt_connections_[0]->Disconnect();
  SimulateGattDisconnection(device);

  // Create 2nd connection:
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
  SimulateGattConnection(device);

  EXPECT_FALSE(gatt_connections_[0]->IsConnected())
      << "The disconnected connection shouldn't become connected when another "
         "connection is created.";
  EXPECT_TRUE(gatt_connections_[1]->IsConnected());
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX)

#if defined(OS_ANDROID) || defined(OS_MACOSX)
// Deletes BluetoothGattConnection causing disconnection.
TEST_F(BluetoothTest, BluetoothGattConnection_DisconnectWhenObjectsDestroyed) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);

  // Create multiple connections and simulate connection complete:
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
  SimulateGattConnection(device);

  // Delete all CreateGattConnection objects, observe disconnection:
  ResetEventCounts();
  gatt_connections_.clear();
  EXPECT_EQ(1, gatt_disconnection_attempts_);
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX)

#if defined(OS_ANDROID) || defined(OS_MACOSX)
// Starts process of disconnecting and then calls BluetoothGattConnection.
TEST_F(BluetoothTest, BluetoothGattConnection_DisconnectInProgress) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);

  // Create multiple connections and simulate connection complete:
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
  SimulateGattConnection(device);

  // Disconnect all CreateGattConnection objects & create a new connection.
  // But, don't yet simulate the device disconnecting:
  ResetEventCounts();
  for (BluetoothGattConnection* connection : gatt_connections_)
    connection->Disconnect();
  EXPECT_EQ(1, gatt_disconnection_attempts_);

  // Create a connection.
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(0, gatt_connection_attempts_);  // No connection attempt.
  EXPECT_EQ(1, callback_count_);  // Device is assumed still connected.
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_FALSE(gatt_connections_.front()->IsConnected());
  EXPECT_TRUE(gatt_connections_.back()->IsConnected());

  // Actually disconnect:
  SimulateGattDisconnection(device);
  for (BluetoothGattConnection* connection : gatt_connections_)
    EXPECT_FALSE(connection->IsConnected());
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX)

#if defined(OS_ANDROID) || defined(OS_MACOSX)
// Calls CreateGattConnection but receives notice that the device disconnected
// before it ever connects.
TEST_F(BluetoothTest, BluetoothGattConnection_SimulateDisconnect) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);

  ResetEventCounts();
  device->CreateGattConnection(GetGattConnectionCallback(Call::NOT_EXPECTED),
                               GetConnectErrorCallback(Call::EXPECTED));
  EXPECT_EQ(1, gatt_connection_attempts_);
  SimulateGattDisconnection(device);
  EXPECT_EQ(BluetoothDevice::ERROR_FAILED, last_connect_error_code_);
  for (BluetoothGattConnection* connection : gatt_connections_)
    EXPECT_FALSE(connection->IsConnected());
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX)

#if defined(OS_ANDROID) || defined(OS_MACOSX)
// Calls CreateGattConnection & DisconnectGatt, then simulates connection.
TEST_F(BluetoothTest, BluetoothGattConnection_DisconnectGatt_SimulateConnect) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);

  ResetEventCounts();
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
  device->DisconnectGatt();
  EXPECT_EQ(1, gatt_connection_attempts_);
  EXPECT_EQ(1, gatt_disconnection_attempts_);
  SimulateGattConnection(device);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(gatt_connections_.back()->IsConnected());
  ResetEventCounts();
  SimulateGattDisconnection(device);
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX)

#if defined(OS_ANDROID) || defined(OS_MACOSX)
// Calls CreateGattConnection & DisconnectGatt, then simulates disconnection.
TEST_F(BluetoothTest,
       BluetoothGattConnection_DisconnectGatt_SimulateDisconnect) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);

  ResetEventCounts();
  device->CreateGattConnection(GetGattConnectionCallback(Call::NOT_EXPECTED),
                               GetConnectErrorCallback(Call::EXPECTED));
  device->DisconnectGatt();
  EXPECT_EQ(1, gatt_connection_attempts_);
  EXPECT_EQ(1, gatt_disconnection_attempts_);
  SimulateGattDisconnection(device);
  EXPECT_EQ(BluetoothDevice::ERROR_FAILED, last_connect_error_code_);
  for (BluetoothGattConnection* connection : gatt_connections_)
    EXPECT_FALSE(connection->IsConnected());
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX)

#if defined(OS_ANDROID) || defined(OS_MACOSX)
// Calls CreateGattConnection & DisconnectGatt, then checks that gatt services
// have been cleaned up.
TEST_F(BluetoothTest, BluetoothGattConnection_DisconnectGatt_Cleanup) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  EXPECT_FALSE(device->IsConnected());

  // Connect to the device
  ResetEventCounts();
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
  TestBluetoothAdapterObserver observer(adapter_);
  SimulateGattConnection(device);
  EXPECT_TRUE(device->IsConnected());

  // Discover services
  std::vector<std::string> services;
  services.push_back("00000000-0000-1000-8000-00805f9b34fb");
  services.push_back("00000001-0000-1000-8000-00805f9b34fb");
  SimulateGattServicesDiscovered(device, services);
  EXPECT_TRUE(device->IsGattServicesDiscoveryComplete());
  EXPECT_EQ(2u, device->GetGattServices().size());
  EXPECT_EQ(1, observer.gatt_services_discovered_count());

  // Disconnect from the device
  device->DisconnectGatt();
  SimulateGattDisconnection(device);
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsGattServicesDiscoveryComplete());
  EXPECT_EQ(0u, device->GetGattServices().size());

  // Verify that the device can be connected to again
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
  SimulateGattConnection(device);
  EXPECT_TRUE(device->IsConnected());

  // Verify that service discovery can be done again
  std::vector<std::string> services2;
  services2.push_back("00000002-0000-1000-8000-00805f9b34fb");
  SimulateGattServicesDiscovered(device, services2);
  EXPECT_TRUE(device->IsGattServicesDiscoveryComplete());
  EXPECT_EQ(1u, device->GetGattServices().size());
  EXPECT_EQ(2, observer.gatt_services_discovered_count());
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX)

#if defined(OS_ANDROID) || defined(OS_MACOSX)
// Calls CreateGattConnection, but simulate errors connecting. Also, verifies
// multiple errors should only invoke callbacks once.
TEST_F(BluetoothTest, BluetoothGattConnection_ErrorAfterConnection) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);

  ResetEventCounts();
  device->CreateGattConnection(GetGattConnectionCallback(Call::NOT_EXPECTED),
                               GetConnectErrorCallback(Call::EXPECTED));
  EXPECT_EQ(1, gatt_connection_attempts_);
  SimulateGattConnectionError(device, BluetoothDevice::ERROR_AUTH_FAILED);
  SimulateGattConnectionError(device, BluetoothDevice::ERROR_FAILED);
#if defined(OS_ANDROID)
  // TODO: Change to ERROR_AUTH_FAILED. We should be getting a callback
  // only with the first error, but our android framework doesn't yet
  // support sending different errors.
  // http://crbug.com/578191
  EXPECT_EQ(BluetoothDevice::ERROR_FAILED, last_connect_error_code_);
#else
  EXPECT_EQ(BluetoothDevice::ERROR_AUTH_FAILED, last_connect_error_code_);
#endif
  for (BluetoothGattConnection* connection : gatt_connections_)
    EXPECT_FALSE(connection->IsConnected());
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX)

#if defined(OS_ANDROID) || defined(OS_WIN) || defined(OS_MACOSX)
TEST_F(BluetoothTest, GattServices_ObserversCalls) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
  TestBluetoothAdapterObserver observer(adapter_);
  ResetEventCounts();
  SimulateGattConnection(device);
  EXPECT_EQ(1, gatt_discovery_attempts_);

  std::vector<std::string> services;
  services.push_back("00000000-0000-1000-8000-00805f9b34fb");
  services.push_back("00000001-0000-1000-8000-00805f9b34fb");
  SimulateGattServicesDiscovered(device, services);

  EXPECT_EQ(1, observer.gatt_services_discovered_count());
  EXPECT_EQ(2, observer.gatt_service_added_count());
}
#endif  // defined(OS_ANDROID) || defined(OS_WIN) || defined(OS_MACOSX)

#if defined(OS_ANDROID)
// macOS: Not applicable: This can never happen because when
// the device gets destroyed the CBPeripheralDelegate is also destroyed
// and no more events are dispatched.
TEST_F(BluetoothTest, GattServicesDiscovered_AfterDeleted) {
  // Tests that we don't crash if services are discovered after
  // the device object is deleted.
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
  ResetEventCounts();
  SimulateGattConnection(device);
  EXPECT_EQ(1, gatt_discovery_attempts_);

  RememberDeviceForSubsequentAction(device);
  DeleteDevice(device);

  std::vector<std::string> services;
  services.push_back("00000000-0000-1000-8000-00805f9b34fb");
  services.push_back("00000001-0000-1000-8000-00805f9b34fb");
  SimulateGattServicesDiscovered(nullptr /* use remembered device */, services);
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
// macOS: Not applicable: This can never happen because when
// the device gets destroyed the CBPeripheralDelegate is also destroyed
// and no more events are dispatched.
TEST_F(BluetoothTest, GattServicesDiscoveredError_AfterDeleted) {
  // Tests that we don't crash if there was an error discoverying services
  // after the device object is deleted.
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
  ResetEventCounts();
  SimulateGattConnection(device);
  EXPECT_EQ(1, gatt_discovery_attempts_);

  RememberDeviceForSubsequentAction(device);
  DeleteDevice(device);

  SimulateGattServicesDiscoveryError(nullptr /* use remembered device */);
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID) || defined(OS_MACOSX)
TEST_F(BluetoothTest, GattServicesDiscovered_AfterDisconnection) {
  // Tests that we don't crash there was an error discovering services after
  // the device disconnects.
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
  ResetEventCounts();
  SimulateGattConnection(device);
  EXPECT_EQ(1, gatt_discovery_attempts_);

  SimulateGattDisconnection(device);

  std::vector<std::string> services;
  services.push_back("00000000-0000-1000-8000-00805f9b34fb");
  services.push_back("00000001-0000-1000-8000-00805f9b34fb");
  SimulateGattServicesDiscovered(device, services);

  EXPECT_FALSE(device->IsGattServicesDiscoveryComplete());
  EXPECT_EQ(0u, device->GetGattServices().size());
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX)

#if defined(OS_ANDROID) || defined(OS_MACOSX)
TEST_F(BluetoothTest, GattServicesDiscoveredError_AfterDisconnection) {
  // Tests that we don't crash if services are discovered after
  // the device disconnects.
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
  ResetEventCounts();
  SimulateGattConnection(device);
  EXPECT_EQ(1, gatt_discovery_attempts_);

  SimulateGattDisconnection(device);

  SimulateGattServicesDiscoveryError(device);
  EXPECT_FALSE(device->IsGattServicesDiscoveryComplete());
  EXPECT_EQ(0u, device->GetGattServices().size());
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX)

#if defined(OS_ANDROID) || defined(OS_WIN) || defined(OS_MACOSX)
TEST_F(BluetoothTest, GetGattServices_and_GetGattService) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
  ResetEventCounts();
  SimulateGattConnection(device);
  EXPECT_EQ(1, gatt_discovery_attempts_);

  std::vector<std::string> services;
  services.push_back("00000000-0000-1000-8000-00805f9b34fb");
  // 2 duplicate UUIDs creating 2 instances.
  services.push_back("00000001-0000-1000-8000-00805f9b34fb");
  services.push_back("00000001-0000-1000-8000-00805f9b34fb");
  SimulateGattServicesDiscovered(device, services);
  EXPECT_EQ(3u, device->GetGattServices().size());

  // Test GetGattService:
  std::string service_id1 = device->GetGattServices()[0]->GetIdentifier();
  std::string service_id2 = device->GetGattServices()[1]->GetIdentifier();
  std::string service_id3 = device->GetGattServices()[2]->GetIdentifier();
  EXPECT_TRUE(device->GetGattService(service_id1));
  EXPECT_TRUE(device->GetGattService(service_id2));
  EXPECT_TRUE(device->GetGattService(service_id3));
}
#endif  // defined(OS_ANDROID) || defined(OS_WIN) || defined(OS_MACOSX)

#if defined(OS_ANDROID) || defined(OS_MACOSX)
TEST_F(BluetoothTest, GetGattServices_DiscoveryError) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
  ResetEventCounts();
  SimulateGattConnection(device);
  EXPECT_EQ(1, gatt_discovery_attempts_);

  SimulateGattServicesDiscoveryError(device);
  EXPECT_EQ(0u, device->GetGattServices().size());
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX)

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
TEST_F(BluetoothTest, GetDeviceTransportType) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();
  BluetoothDevice* device = SimulateLowEnergyDevice(1);
  EXPECT_EQ(BLUETOOTH_TRANSPORT_LE, device->GetType());

  BluetoothDevice* device2 = SimulateLowEnergyDevice(6);
  EXPECT_EQ(BLUETOOTH_TRANSPORT_DUAL, device2->GetType());

  BluetoothDevice* device3 = SimulateClassicDevice();
  EXPECT_EQ(BLUETOOTH_TRANSPORT_CLASSIC, device3->GetType());
}
#endif  // defined(OS_CHROMEOS) || defined(OS_LINUX)

}  // namespace device
