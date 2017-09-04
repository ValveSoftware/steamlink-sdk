// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_gatt_service.h"

#include "build/build_config.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/test/test_bluetooth_adapter_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "device/bluetooth/test/bluetooth_test_android.h"
#elif defined(OS_MACOSX)
#include "device/bluetooth/test/bluetooth_test_mac.h"
#elif defined(OS_WIN)
#include "device/bluetooth/test/bluetooth_test_win.h"
#endif

namespace device {

#if defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)
class BluetoothRemoteGattServiceTest : public BluetoothTest {};
#endif

#if defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)
TEST_F(BluetoothRemoteGattServiceTest, GetIdentifier) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  // 2 devices to verify unique IDs across them.
  BluetoothDevice* device1 = SimulateLowEnergyDevice(3);
  BluetoothDevice* device2 = SimulateLowEnergyDevice(4);
  device1->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                                GetConnectErrorCallback(Call::NOT_EXPECTED));
  device2->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                                GetConnectErrorCallback(Call::NOT_EXPECTED));
  SimulateGattConnection(device1);
  SimulateGattConnection(device2);

  // 2 duplicate UUIDs creating 2 service instances on each device.
  std::vector<std::string> services;
  std::string uuid = "00000000-0000-1000-8000-00805f9b34fb";
  services.push_back(uuid);
  services.push_back(uuid);
  SimulateGattServicesDiscovered(device1, services);
  SimulateGattServicesDiscovered(device2, services);
  BluetoothRemoteGattService* service1 = device1->GetGattServices()[0];
  BluetoothRemoteGattService* service2 = device1->GetGattServices()[1];
  BluetoothRemoteGattService* service3 = device2->GetGattServices()[0];
  BluetoothRemoteGattService* service4 = device2->GetGattServices()[1];

  // All IDs are unique, even though they have the same UUID.
  EXPECT_NE(service1->GetIdentifier(), service2->GetIdentifier());
  EXPECT_NE(service1->GetIdentifier(), service3->GetIdentifier());
  EXPECT_NE(service1->GetIdentifier(), service4->GetIdentifier());

  EXPECT_NE(service2->GetIdentifier(), service3->GetIdentifier());
  EXPECT_NE(service2->GetIdentifier(), service4->GetIdentifier());

  EXPECT_NE(service3->GetIdentifier(), service4->GetIdentifier());
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)

#if defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)
TEST_F(BluetoothRemoteGattServiceTest, GetUUID) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
  SimulateGattConnection(device);

  // Create multiple instances with the same UUID.
  BluetoothUUID uuid("00000000-0000-1000-8000-00805f9b34fb");
  std::vector<std::string> services;
  services.push_back(uuid.canonical_value());
  services.push_back(uuid.canonical_value());
  SimulateGattServicesDiscovered(device, services);

  // Each has the same UUID.
  EXPECT_EQ(uuid, device->GetGattServices()[0]->GetUUID());
  EXPECT_EQ(uuid, device->GetGattServices()[1]->GetUUID());
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)

#if defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)
TEST_F(BluetoothRemoteGattServiceTest, GetCharacteristics_FindNone) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
  SimulateGattConnection(device);

  // Simulate a service, with no Characteristics:
  std::vector<std::string> services;
  services.push_back("00000000-0000-1000-8000-00805f9b34fb");
  SimulateGattServicesDiscovered(device, services);
  BluetoothRemoteGattService* service = device->GetGattServices()[0];

  EXPECT_EQ(0u, service->GetCharacteristics().size());
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)

#if defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)
TEST_F(BluetoothRemoteGattServiceTest,
       GetCharacteristics_and_GetCharacteristic) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
  SimulateGattConnection(device);

  // Simulate a service, with several Characteristics:
  std::vector<std::string> services;
  services.push_back("00000000-0000-1000-8000-00805f9b34fb");
  SimulateGattServicesDiscovered(device, services);
  BluetoothRemoteGattService* service = device->GetGattServices()[0];
  std::string characteristic_uuid1 = "11111111-0000-1000-8000-00805f9b34fb";
  std::string characteristic_uuid2 = "22222222-0000-1000-8000-00805f9b34fb";
  std::string characteristic_uuid3 = characteristic_uuid2;  // Duplicate UUID.
  std::string characteristic_uuid4 = "33333333-0000-1000-8000-00805f9b34fb";
  SimulateGattCharacteristic(service, characteristic_uuid1, /* properties */ 0);
  SimulateGattCharacteristic(service, characteristic_uuid2, /* properties */ 0);
  SimulateGattCharacteristic(service, characteristic_uuid3, /* properties */ 0);
  SimulateGattCharacteristic(service, characteristic_uuid4, /* properties */ 0);

  // Verify that GetCharacteristic can retrieve characteristics again by ID,
  // and that the same Characteristics come back.
  EXPECT_EQ(4u, service->GetCharacteristics().size());
  std::string char_id1 = service->GetCharacteristics()[0]->GetIdentifier();
  std::string char_id2 = service->GetCharacteristics()[1]->GetIdentifier();
  std::string char_id3 = service->GetCharacteristics()[2]->GetIdentifier();
  std::string char_id4 = service->GetCharacteristics()[3]->GetIdentifier();
  BluetoothUUID char_uuid1 = service->GetCharacteristics()[0]->GetUUID();
  BluetoothUUID char_uuid2 = service->GetCharacteristics()[1]->GetUUID();
  BluetoothUUID char_uuid3 = service->GetCharacteristics()[2]->GetUUID();
  BluetoothUUID char_uuid4 = service->GetCharacteristics()[3]->GetUUID();
  EXPECT_EQ(char_uuid1, service->GetCharacteristic(char_id1)->GetUUID());
  EXPECT_EQ(char_uuid2, service->GetCharacteristic(char_id2)->GetUUID());
  EXPECT_EQ(char_uuid3, service->GetCharacteristic(char_id3)->GetUUID());
  EXPECT_EQ(char_uuid4, service->GetCharacteristic(char_id4)->GetUUID());

  // GetCharacteristics & GetCharacteristic return the same object for the same
  // ID:
  EXPECT_EQ(service->GetCharacteristics()[0],
            service->GetCharacteristic(char_id1));
  EXPECT_EQ(service->GetCharacteristic(char_id1),
            service->GetCharacteristic(char_id1));
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)

#if defined(OS_MACOSX) || defined(OS_WIN)
TEST_F(BluetoothRemoteGattServiceTest, GattCharacteristics_ObserversCalls) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
  SimulateGattConnection(device);

  TestBluetoothAdapterObserver observer(adapter_);

  // Simulate a service, with several Characteristics:
  std::vector<std::string> services;
  services.push_back("00000000-0000-1000-8000-00805f9b34fb");
  SimulateGattServicesDiscovered(device, services);
  BluetoothRemoteGattService* service = device->GetGattServices()[0];
  std::string characteristic_uuid1 = "11111111-0000-1000-8000-00805f9b34fb";
  std::string characteristic_uuid2 = "22222222-0000-1000-8000-00805f9b34fb";
  std::string characteristic_uuid3 = characteristic_uuid2;  // Duplicate UUID.
  std::string characteristic_uuid4 = "33333333-0000-1000-8000-00805f9b34fb";
  SimulateGattCharacteristic(service, characteristic_uuid1, /* properties */ 0);
  SimulateGattCharacteristic(service, characteristic_uuid2, /* properties */ 0);
  SimulateGattCharacteristic(service, characteristic_uuid3, /* properties */ 0);
  SimulateGattCharacteristic(service, characteristic_uuid4, /* properties */ 0);
#if !defined(OS_WIN)
  // TODO(620895) GattCharacteristicAdded has to be implemented for Windows.
  EXPECT_EQ(4, observer.gatt_characteristic_added_count());
#endif  // !defined(OS_WIN)

  // Simulate remove of characteristics one by one.
  EXPECT_EQ(4u, service->GetCharacteristics().size());
  std::string removed_char = service->GetCharacteristics()[0]->GetIdentifier();
  SimulateGattCharacteristicRemoved(service,
                                    service->GetCharacteristic(removed_char));
  EXPECT_EQ(1, observer.gatt_characteristic_removed_count());
  EXPECT_FALSE(service->GetCharacteristic(removed_char));
  EXPECT_EQ(3u, service->GetCharacteristics().size());
  removed_char = service->GetCharacteristics()[0]->GetIdentifier();
  SimulateGattCharacteristicRemoved(service,
                                    service->GetCharacteristic(removed_char));
  EXPECT_EQ(2, observer.gatt_characteristic_removed_count());
  EXPECT_FALSE(service->GetCharacteristic(removed_char));
  EXPECT_EQ(2u, service->GetCharacteristics().size());
  removed_char = service->GetCharacteristics()[0]->GetIdentifier();
  SimulateGattCharacteristicRemoved(service,
                                    service->GetCharacteristic(removed_char));
  EXPECT_EQ(3, observer.gatt_characteristic_removed_count());
  EXPECT_FALSE(service->GetCharacteristic(removed_char));
  EXPECT_EQ(1u, service->GetCharacteristics().size());
  removed_char = service->GetCharacteristics()[0]->GetIdentifier();
  SimulateGattCharacteristicRemoved(service,
                                    service->GetCharacteristic(removed_char));
  EXPECT_EQ(4, observer.gatt_characteristic_removed_count());
  EXPECT_FALSE(service->GetCharacteristic(removed_char));
  EXPECT_EQ(0u, service->GetCharacteristics().size());

#if defined(OS_MACOSX)
  // SimulateGattServicesDiscovered
  // 4 * SimulateGattCharacteristic
  // 4 * SimulateGattCharacteristicRemoved
  EXPECT_EQ(9, observer.gatt_service_changed_count());
#else  // defined(OS_MACOSX)
  EXPECT_EQ(4, observer.gatt_service_changed_count());
#endif  // defined(OS_MACOSX)
}
#endif  //  defined(OS_MACOSX) || defined(OS_WIN)

#if defined(OS_WIN) || defined(OS_MACOSX)
TEST_F(BluetoothRemoteGattServiceTest, SimulateGattServiceRemove) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
  SimulateGattConnection(device);

  TestBluetoothAdapterObserver observer(adapter_);

  // Simulate two primary GATT services.
  std::vector<std::string> services;
  services.push_back("00000000-0000-1000-8000-00805f9b34fb");
  services.push_back("01010101-0101-1000-8000-00805f9b34fb");
  SimulateGattServicesDiscovered(device, services);
  EXPECT_EQ(2u, device->GetGattServices().size());

  // Simulate remove of a primary service.
  BluetoothRemoteGattService* service1 = device->GetGattServices()[0];
  BluetoothRemoteGattService* service2 = device->GetGattServices()[1];
  std::string removed_service = service1->GetIdentifier();
  SimulateGattServiceRemoved(device->GetGattService(removed_service));
  EXPECT_EQ(1, observer.gatt_service_removed_count());
  EXPECT_EQ(1u, device->GetGattServices().size());
  EXPECT_FALSE(device->GetGattService(removed_service));
  EXPECT_EQ(device->GetGattServices()[0], service2);
}
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

}  // namespace device
