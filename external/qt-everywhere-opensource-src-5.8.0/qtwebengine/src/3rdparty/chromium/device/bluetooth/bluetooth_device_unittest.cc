// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_device.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/run_loop.h"
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
  BluetoothDevice::UUIDList uuids = device->GetUUIDs();
  EXPECT_TRUE(ContainsValue(uuids, BluetoothUUID(kTestUUIDGenericAccess)));
  EXPECT_TRUE(ContainsValue(uuids, BluetoothUUID(kTestUUIDGenericAttribute)));
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
  BluetoothDevice::UUIDList uuids = device->GetUUIDs();
  EXPECT_EQ(0u, uuids.size());
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)

#if defined(OS_ANDROID)
// GetDeviceName for Device with no name.
TEST_F(BluetoothTest, GetDeviceName_NullName) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(5);
  EXPECT_EQ("", device->GetDeviceName());
}
#endif  // defined(OS_ANDROID)

// TODO(scheib): Test with a device with no name. http://crbug.com/506415
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
