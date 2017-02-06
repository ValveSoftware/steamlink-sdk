// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"
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
class BluetoothRemoteGattCharacteristicTest : public BluetoothTest {
 public:
  // Creates adapter_, device_, service_, characteristic1_, & characteristic2_.
  // |properties| will be used for each characteristic.
  void FakeCharacteristicBoilerplate(int properties = 0) {
    InitWithFakeAdapter();
    StartLowEnergyDiscoverySession();
    device_ = SimulateLowEnergyDevice(3);
    device_->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                                  GetConnectErrorCallback(Call::NOT_EXPECTED));
    SimulateGattConnection(device_);
    std::vector<std::string> services;
    std::string uuid("00000000-0000-1000-8000-00805f9b34fb");
    services.push_back(uuid);
    SimulateGattServicesDiscovered(device_, services);
    ASSERT_EQ(1u, device_->GetGattServices().size());
    service_ = device_->GetGattServices()[0];
    SimulateGattCharacteristic(service_, uuid, properties);
    SimulateGattCharacteristic(service_, uuid, properties);
    ASSERT_EQ(2u, service_->GetCharacteristics().size());
    characteristic1_ = service_->GetCharacteristics()[0];
    characteristic2_ = service_->GetCharacteristics()[1];
    ResetEventCounts();
  }

  enum class StartNotifySetupError {
    CHARACTERISTIC_PROPERTIES,
    CONFIG_DESCRIPTOR_MISSING,
    CONFIG_DESCRIPTOR_DUPLICATE,
    SET_NOTIFY,
    WRITE_DESCRIPTOR,
    NONE
  };
  // Constructs characteristics with |properties|, calls StartNotifySession,
  // and verifies the appropriate |expected_config_descriptor_value| is written.
  // Error scenarios in this boilerplate are tested by setting |error| to the
  // setup stage to test.
  void StartNotifyBoilerplate(
      int properties,
      uint16_t expected_config_descriptor_value,
      StartNotifySetupError error = StartNotifySetupError::NONE) {
    if (error == StartNotifySetupError::CHARACTERISTIC_PROPERTIES) {
      properties = 0;
    }
    ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(properties));

#if !defined(OS_MACOSX)
    // macOS: Not applicable. CoreBluetooth exposes -[CBPeripheral
    // setNotifyValue:forCharacteristic:] which handles all interactions with
    // the CCC descriptor.
    size_t expected_descriptors_count = 0;
    if (error != StartNotifySetupError::CONFIG_DESCRIPTOR_MISSING) {
      SimulateGattDescriptor(
          characteristic1_,
          BluetoothRemoteGattDescriptor::ClientCharacteristicConfigurationUuid()
              .canonical_value());
      expected_descriptors_count++;
    }
    if (error == StartNotifySetupError::CONFIG_DESCRIPTOR_DUPLICATE) {
      SimulateGattDescriptor(
          characteristic1_,
          BluetoothRemoteGattDescriptor::ClientCharacteristicConfigurationUuid()
              .canonical_value());
      expected_descriptors_count++;
    }
    ASSERT_EQ(expected_descriptors_count,
              characteristic1_->GetDescriptors().size());
#endif  // !defined(OS_MACOSX)

    if (error == StartNotifySetupError::SET_NOTIFY) {
      SimulateGattCharacteristicSetNotifyWillFailSynchronouslyOnce(
          characteristic1_);
    }

    if (error == StartNotifySetupError::WRITE_DESCRIPTOR) {
      SimulateGattDescriptorWriteWillFailSynchronouslyOnce(
          characteristic1_->GetDescriptors()[0]);
    }

    if (error != StartNotifySetupError::NONE) {
      characteristic1_->StartNotifySession(
          GetNotifyCallback(Call::NOT_EXPECTED),
          GetGattErrorCallback(Call::EXPECTED));
      return;
    }

    characteristic1_->StartNotifySession(
        GetNotifyCallback(Call::EXPECTED),
        GetGattErrorCallback(Call::NOT_EXPECTED));

    EXPECT_EQ(0, callback_count_);
    SimulateGattNotifySessionStarted(characteristic1_);
    EXPECT_EQ(1, gatt_notify_characteristic_attempts_);
    EXPECT_EQ(1, callback_count_);
    EXPECT_EQ(0, error_callback_count_);
    ASSERT_EQ(1u, notify_sessions_.size());
    ASSERT_TRUE(notify_sessions_[0]);
    EXPECT_EQ(characteristic1_->GetIdentifier(),
              notify_sessions_[0]->GetCharacteristicIdentifier());
    EXPECT_TRUE(notify_sessions_[0]->IsActive());

    // Verify the Client Characteristic Configuration descriptor was written to.
#if !defined(OS_MACOSX)
    // macOS: Not applicable. CoreBluetooth exposes -[CBPeripheral
    // setNotifyValue:forCharacteristic:] which handles all interactions with
    // the CCC descriptor.
    EXPECT_EQ(1, gatt_write_descriptor_attempts_);
    EXPECT_EQ(2u, last_write_value_.size());
    uint8_t expected_byte0 = expected_config_descriptor_value & 0xFF;
    uint8_t expected_byte1 = (expected_config_descriptor_value >> 8) & 0xFF;
    EXPECT_EQ(expected_byte0, last_write_value_[0]);
    EXPECT_EQ(expected_byte1, last_write_value_[1]);
#endif  // !defined(OS_MACOSX)
  }

  BluetoothDevice* device_ = nullptr;
  BluetoothRemoteGattService* service_ = nullptr;
  BluetoothRemoteGattCharacteristic* characteristic1_ = nullptr;
  BluetoothRemoteGattCharacteristic* characteristic2_ = nullptr;
};
#endif

#if defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)
TEST_F(BluetoothRemoteGattCharacteristicTest, GetIdentifier) {
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

  // 3 services (all with same UUID).
  //   1 on the first device (to test characteristic instances across devices).
  //   2 on the second device (to test same device, multiple service instances).
  std::vector<std::string> services;
  std::string uuid = "00000000-0000-1000-8000-00805f9b34fb";
  services.push_back(uuid);
  SimulateGattServicesDiscovered(device1, services);
  services.push_back(uuid);
  SimulateGattServicesDiscovered(device2, services);
  BluetoothRemoteGattService* service1 = device1->GetGattServices()[0];
  BluetoothRemoteGattService* service2 = device2->GetGattServices()[0];
  BluetoothRemoteGattService* service3 = device2->GetGattServices()[1];
  // 6 characteristics (same UUID), 2 on each service.
  SimulateGattCharacteristic(service1, uuid, /* properties */ 0);
  SimulateGattCharacteristic(service1, uuid, /* properties */ 0);
  SimulateGattCharacteristic(service2, uuid, /* properties */ 0);
  SimulateGattCharacteristic(service2, uuid, /* properties */ 0);
  SimulateGattCharacteristic(service3, uuid, /* properties */ 0);
  SimulateGattCharacteristic(service3, uuid, /* properties */ 0);
  BluetoothRemoteGattCharacteristic* char1 = service1->GetCharacteristics()[0];
  BluetoothRemoteGattCharacteristic* char2 = service1->GetCharacteristics()[1];
  BluetoothRemoteGattCharacteristic* char3 = service2->GetCharacteristics()[0];
  BluetoothRemoteGattCharacteristic* char4 = service2->GetCharacteristics()[1];
  BluetoothRemoteGattCharacteristic* char5 = service3->GetCharacteristics()[0];
  BluetoothRemoteGattCharacteristic* char6 = service3->GetCharacteristics()[1];

  // All IDs are unique, even though they have the same UUID.
  EXPECT_NE(char1->GetIdentifier(), char2->GetIdentifier());
  EXPECT_NE(char1->GetIdentifier(), char3->GetIdentifier());
  EXPECT_NE(char1->GetIdentifier(), char4->GetIdentifier());
  EXPECT_NE(char1->GetIdentifier(), char5->GetIdentifier());
  EXPECT_NE(char1->GetIdentifier(), char6->GetIdentifier());

  EXPECT_NE(char2->GetIdentifier(), char3->GetIdentifier());
  EXPECT_NE(char2->GetIdentifier(), char4->GetIdentifier());
  EXPECT_NE(char2->GetIdentifier(), char5->GetIdentifier());
  EXPECT_NE(char2->GetIdentifier(), char6->GetIdentifier());

  EXPECT_NE(char3->GetIdentifier(), char4->GetIdentifier());
  EXPECT_NE(char3->GetIdentifier(), char5->GetIdentifier());
  EXPECT_NE(char3->GetIdentifier(), char6->GetIdentifier());

  EXPECT_NE(char4->GetIdentifier(), char5->GetIdentifier());
  EXPECT_NE(char4->GetIdentifier(), char6->GetIdentifier());

  EXPECT_NE(char5->GetIdentifier(), char6->GetIdentifier());
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)

#if defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)
TEST_F(BluetoothRemoteGattCharacteristicTest, GetUUID) {
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
  std::vector<std::string> services;
  services.push_back("00000000-0000-1000-8000-00805f9b34fb");
  SimulateGattServicesDiscovered(device, services);
  BluetoothRemoteGattService* service = device->GetGattServices()[0];

  // Create 3 characteristics. Two of them are duplicates.
  std::string uuid_str1("11111111-0000-1000-8000-00805f9b34fb");
  std::string uuid_str2("22222222-0000-1000-8000-00805f9b34fb");
  BluetoothUUID uuid1(uuid_str1);
  BluetoothUUID uuid2(uuid_str2);
  SimulateGattCharacteristic(service, uuid_str1, /* properties */ 0);
  SimulateGattCharacteristic(service, uuid_str2, /* properties */ 0);
  SimulateGattCharacteristic(service, uuid_str2, /* properties */ 0);
  BluetoothRemoteGattCharacteristic* char1 = service->GetCharacteristics()[0];
  BluetoothRemoteGattCharacteristic* char2 = service->GetCharacteristics()[1];
  BluetoothRemoteGattCharacteristic* char3 = service->GetCharacteristics()[2];

  // Swap as needed to have char1 point to the the characteristic with uuid1.
  if (char2->GetUUID() == uuid1) {
    std::swap(char1, char2);
  } else if (char3->GetUUID() == uuid1) {
    std::swap(char1, char3);
  }

  EXPECT_EQ(uuid1, char1->GetUUID());
  EXPECT_EQ(uuid2, char2->GetUUID());
  EXPECT_EQ(uuid2, char3->GetUUID());
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)

#if defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)
TEST_F(BluetoothRemoteGattCharacteristicTest, GetProperties) {
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
  std::vector<std::string> services;
  std::string uuid("00000000-0000-1000-8000-00805f9b34fb");
  services.push_back(uuid);
  SimulateGattServicesDiscovered(device, services);
  BluetoothRemoteGattService* service = device->GetGattServices()[0];

  // Create two characteristics with different properties:
  SimulateGattCharacteristic(service, uuid, /* properties */ 0);
  SimulateGattCharacteristic(service, uuid, /* properties */ 7);

  // Read the properties. Because ordering is unknown swap as necessary.
  int properties1 = service->GetCharacteristics()[0]->GetProperties();
  int properties2 = service->GetCharacteristics()[1]->GetProperties();
  if (properties2 == 0)
    std::swap(properties1, properties2);
  EXPECT_EQ(0, properties1);
  EXPECT_EQ(7, properties2);
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)

#if defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)
// Tests GetService.
TEST_F(BluetoothRemoteGattCharacteristicTest, GetService) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate());

  EXPECT_EQ(service_, characteristic1_->GetService());
  EXPECT_EQ(service_, characteristic2_->GetService());
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)

#if defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)
// Tests ReadRemoteCharacteristic and GetValue with empty value buffer.
TEST_F(BluetoothRemoteGattCharacteristicTest, ReadRemoteCharacteristic_Empty) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_READ));

  characteristic1_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  std::vector<uint8_t> empty_vector;
  SimulateGattCharacteristicRead(characteristic1_, empty_vector);

  // Duplicate read reported from OS shouldn't cause a problem:
  SimulateGattCharacteristicRead(characteristic1_, empty_vector);

  EXPECT_EQ(1, gatt_read_characteristic_attempts_);
  EXPECT_EQ(empty_vector, last_read_value_);
  EXPECT_EQ(empty_vector, characteristic1_->GetValue());
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)

#if defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)
// Tests WriteRemoteCharacteristic with empty value buffer.
TEST_F(BluetoothRemoteGattCharacteristicTest, WriteRemoteCharacteristic_Empty) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_WRITE));

  std::vector<uint8_t> empty_vector;
  characteristic1_->WriteRemoteCharacteristic(
      empty_vector, GetCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  SimulateGattCharacteristicWrite(characteristic1_);
  EXPECT_EQ(1, gatt_write_characteristic_attempts_);

  // Duplicate write reported from OS shouldn't cause a problem:
  SimulateGattCharacteristicWrite(characteristic1_);

  EXPECT_EQ(empty_vector, last_write_value_);
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)

#if defined(OS_ANDROID) || defined(OS_WIN)
// Tests ReadRemoteCharacteristic completing after Chrome objects are deleted.
// macOS: Not applicable: This can never happen if CBPeripheral delegate is set
// to nil.
TEST_F(BluetoothRemoteGattCharacteristicTest,
       ReadRemoteCharacteristic_AfterDeleted) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_READ));

  characteristic1_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::NOT_EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));

  RememberCharacteristicForSubsequentAction(characteristic1_);
  DeleteDevice(device_);  // TODO(576906) delete only the characteristic.

  std::vector<uint8_t> empty_vector;
  SimulateGattCharacteristicRead(/* use remembered characteristic */ nullptr,
                                 empty_vector);
  EXPECT_TRUE("Did not crash!");
}
#endif  // defined(OS_ANDROID) || defined(OS_WIN)

#if defined(OS_ANDROID) || defined(OS_WIN)
// Tests WriteRemoteCharacteristic completing after Chrome objects are deleted.
// macOS: Not applicable: This can never happen if CBPeripheral delegate is set
// to nil.
TEST_F(BluetoothRemoteGattCharacteristicTest,
       WriteRemoteCharacteristic_AfterDeleted) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_WRITE));

  std::vector<uint8_t> empty_vector;
  characteristic1_->WriteRemoteCharacteristic(
      empty_vector, GetCallback(Call::NOT_EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));

  RememberCharacteristicForSubsequentAction(characteristic1_);
  DeleteDevice(device_);  // TODO(576906) delete only the characteristic.

  SimulateGattCharacteristicWrite(/* use remembered characteristic */ nullptr);
  EXPECT_TRUE("Did not crash!");
}
#endif  // defined(OS_ANDROID) || defined(OS_WIN)

#if defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)
// Tests ReadRemoteCharacteristic and GetValue with non-empty value buffer.
TEST_F(BluetoothRemoteGattCharacteristicTest, ReadRemoteCharacteristic) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_READ));

  characteristic1_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));

  std::vector<uint8_t> test_vector = {0, 1, 2, 3, 4, 0xf, 0xf0, 0xff};
  SimulateGattCharacteristicRead(characteristic1_, test_vector);

  // Duplicate read reported from OS shouldn't cause a problem:
  std::vector<uint8_t> empty_vector;
  SimulateGattCharacteristicRead(characteristic1_, empty_vector);

  EXPECT_EQ(1, gatt_read_characteristic_attempts_);
  EXPECT_EQ(test_vector, last_read_value_);
  EXPECT_EQ(test_vector, characteristic1_->GetValue());
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)

#if defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)
// Callback that make sure GattCharacteristicValueChanged has been called
// before the callback runs.
static void test_callback(
    BluetoothRemoteGattCharacteristic::ValueCallback callback,
    const TestBluetoothAdapterObserver& callback_observer,
    const std::vector<uint8_t>& value) {
  EXPECT_EQ(1, callback_observer.gatt_characteristic_value_changed_count());
  callback.Run(value);
}

// Tests that ReadRemoteCharacteristic results in a
// GattCharacteristicValueChanged call.
TEST_F(BluetoothRemoteGattCharacteristicTest,
       ReadRemoteCharacteristic_GattCharacteristicValueChanged) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_READ));

  TestBluetoothAdapterObserver observer(adapter_);

  characteristic1_->ReadRemoteCharacteristic(
      base::Bind(test_callback, GetReadValueCallback(Call::EXPECTED),
                 base::ConstRef(observer)),
      GetGattErrorCallback(Call::NOT_EXPECTED));

  std::vector<uint8_t> test_vector = {0, 1, 2, 3, 4, 0xf, 0xf0, 0xff};
  SimulateGattCharacteristicRead(characteristic1_, test_vector);

  EXPECT_EQ(1, observer.gatt_characteristic_value_changed_count());
  EXPECT_EQ(characteristic1_->GetIdentifier(),
            observer.last_gatt_characteristic_id());
  EXPECT_EQ(characteristic1_->GetUUID(),
            observer.last_gatt_characteristic_uuid());
  EXPECT_EQ(test_vector, observer.last_changed_characteristic_value());
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)

#if defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)
// Tests WriteRemoteCharacteristic with non-empty value buffer.
TEST_F(BluetoothRemoteGattCharacteristicTest, WriteRemoteCharacteristic) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_WRITE));

  uint8_t values[] = {0, 1, 2, 3, 4, 0xf, 0xf0, 0xff};
  std::vector<uint8_t> test_vector(values, values + arraysize(values));
  characteristic1_->WriteRemoteCharacteristic(
      test_vector, GetCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));

  SimulateGattCharacteristicWrite(characteristic1_);

  EXPECT_EQ(1, gatt_write_characteristic_attempts_);
  EXPECT_EQ(test_vector, last_write_value_);
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)

#if defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)
// Tests ReadRemoteCharacteristic and GetValue multiple times.
TEST_F(BluetoothRemoteGattCharacteristicTest, ReadRemoteCharacteristic_Twice) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_READ));

  characteristic1_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));

  uint8_t values[] = {0, 1, 2, 3, 4, 0xf, 0xf0, 0xff};
  std::vector<uint8_t> test_vector(values, values + arraysize(values));
  SimulateGattCharacteristicRead(characteristic1_, test_vector);
  EXPECT_EQ(1, gatt_read_characteristic_attempts_);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(test_vector, last_read_value_);
  EXPECT_EQ(test_vector, characteristic1_->GetValue());

  // Read again, with different value:
  ResetEventCounts();
  characteristic1_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  std::vector<uint8_t> empty_vector;
  SimulateGattCharacteristicRead(characteristic1_, empty_vector);
  EXPECT_EQ(1, gatt_read_characteristic_attempts_);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(empty_vector, last_read_value_);
  EXPECT_EQ(empty_vector, characteristic1_->GetValue());
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)

#if defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)
// Tests WriteRemoteCharacteristic multiple times.
TEST_F(BluetoothRemoteGattCharacteristicTest, WriteRemoteCharacteristic_Twice) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_WRITE));

  uint8_t values[] = {0, 1, 2, 3, 4, 0xf, 0xf0, 0xff};
  std::vector<uint8_t> test_vector(values, values + arraysize(values));
  characteristic1_->WriteRemoteCharacteristic(
      test_vector, GetCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));

  SimulateGattCharacteristicWrite(characteristic1_);
  EXPECT_EQ(1, gatt_write_characteristic_attempts_);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(test_vector, last_write_value_);

  // Write again, with different value:
  ResetEventCounts();
  std::vector<uint8_t> empty_vector;
  characteristic1_->WriteRemoteCharacteristic(
      empty_vector, GetCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));

  SimulateGattCharacteristicWrite(characteristic1_);
  EXPECT_EQ(1, gatt_write_characteristic_attempts_);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(empty_vector, last_write_value_);
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)

#if defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)
// Tests ReadRemoteCharacteristic on two characteristics.
TEST_F(BluetoothRemoteGattCharacteristicTest,
       ReadRemoteCharacteristic_MultipleCharacteristics) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_READ));

  characteristic1_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  characteristic2_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  std::vector<uint8_t> test_vector1;
  test_vector1.push_back(111);
  SimulateGattCharacteristicRead(characteristic1_, test_vector1);
  EXPECT_EQ(test_vector1, last_read_value_);

  std::vector<uint8_t> test_vector2;
  test_vector2.push_back(222);
  SimulateGattCharacteristicRead(characteristic2_, test_vector2);
  EXPECT_EQ(test_vector2, last_read_value_);

  EXPECT_EQ(2, gatt_read_characteristic_attempts_);
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(test_vector1, characteristic1_->GetValue());
  EXPECT_EQ(test_vector2, characteristic2_->GetValue());
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)

#if defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)
// Tests WriteRemoteCharacteristic on two characteristics.
TEST_F(BluetoothRemoteGattCharacteristicTest,
       WriteRemoteCharacteristic_MultipleCharacteristics) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_WRITE));

  std::vector<uint8_t> test_vector1;
  test_vector1.push_back(111);
  characteristic1_->WriteRemoteCharacteristic(
      test_vector1, GetCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
#if defined(OS_ANDROID) || defined(OS_MACOSX)
  EXPECT_EQ(test_vector1, last_write_value_);
#endif

  std::vector<uint8_t> test_vector2;
  test_vector2.push_back(222);
  characteristic2_->WriteRemoteCharacteristic(
      test_vector2, GetCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
#if defined(OS_ANDROID) || defined(OS_MACOSX)
  EXPECT_EQ(test_vector2, last_write_value_);
#endif

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  SimulateGattCharacteristicWrite(characteristic1_);
#if !(defined(OS_ANDROID) || defined(OS_MACOSX))
  EXPECT_EQ(test_vector1, last_write_value_);
#endif

  SimulateGattCharacteristicWrite(characteristic2_);
#if !(defined(OS_ANDROID) || defined(OS_MACOSX))
  EXPECT_EQ(test_vector2, last_write_value_);
#endif

  EXPECT_EQ(2, gatt_write_characteristic_attempts_);
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // TODO(591740): Remove if define for OS_ANDROID in this test.
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)

#if defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)
// Tests ReadRemoteCharacteristic asynchronous error.
TEST_F(BluetoothRemoteGattCharacteristicTest, ReadError) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_READ));

  TestBluetoothAdapterObserver observer(adapter_);

  characteristic1_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::NOT_EXPECTED),
      GetGattErrorCallback(Call::EXPECTED));
  SimulateGattCharacteristicReadError(
      characteristic1_, BluetoothRemoteGattService::GATT_ERROR_INVALID_LENGTH);
  SimulateGattCharacteristicReadError(
      characteristic1_, BluetoothRemoteGattService::GATT_ERROR_FAILED);
  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_INVALID_LENGTH,
            last_gatt_error_code_);
  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)

#if defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)
// Tests WriteRemoteCharacteristic asynchronous error.
TEST_F(BluetoothRemoteGattCharacteristicTest, WriteError) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_WRITE));

  std::vector<uint8_t> empty_vector;
  characteristic1_->WriteRemoteCharacteristic(
      empty_vector, GetCallback(Call::NOT_EXPECTED),
      GetGattErrorCallback(Call::EXPECTED));
  SimulateGattCharacteristicWriteError(
      characteristic1_, BluetoothRemoteGattService::GATT_ERROR_INVALID_LENGTH);
  SimulateGattCharacteristicWriteError(
      characteristic1_, BluetoothRemoteGattService::GATT_ERROR_FAILED);

  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_INVALID_LENGTH,
            last_gatt_error_code_);
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)

#if defined(OS_ANDROID)
// Tests ReadRemoteCharacteristic synchronous error.
// Test not relevant for macOS since characteristic read cannot generate
// synchronous error.
TEST_F(BluetoothRemoteGattCharacteristicTest, ReadSynchronousError) {
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate());

  SimulateGattCharacteristicReadWillFailSynchronouslyOnce(characteristic1_);
  characteristic1_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::NOT_EXPECTED),
      GetGattErrorCallback(Call::EXPECTED));
  EXPECT_EQ(0, gatt_read_characteristic_attempts_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_FAILED,
            last_gatt_error_code_);

  // After failing once, can succeed:
  ResetEventCounts();
  characteristic1_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(1, gatt_read_characteristic_attempts_);
  std::vector<uint8_t> empty_vector;
  SimulateGattCharacteristicRead(characteristic1_, empty_vector);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
// Tests WriteRemoteCharacteristic synchronous error.
// This test doesn't apply to macOS synchronous API does exist.
TEST_F(BluetoothRemoteGattCharacteristicTest, WriteSynchronousError) {
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate());

  SimulateGattCharacteristicWriteWillFailSynchronouslyOnce(characteristic1_);
  std::vector<uint8_t> empty_vector;
  characteristic1_->WriteRemoteCharacteristic(
      empty_vector, GetCallback(Call::NOT_EXPECTED),
      GetGattErrorCallback(Call::EXPECTED));
  EXPECT_EQ(0, gatt_write_characteristic_attempts_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_FAILED,
            last_gatt_error_code_);

  // After failing once, can succeed:
  ResetEventCounts();
  characteristic1_->WriteRemoteCharacteristic(
      empty_vector, GetCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(1, gatt_write_characteristic_attempts_);
  SimulateGattCharacteristicWrite(characteristic1_);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)
// Tests ReadRemoteCharacteristic error with a pending read operation.
TEST_F(BluetoothRemoteGattCharacteristicTest,
       ReadRemoteCharacteristic_ReadPending) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_READ));

  characteristic1_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  characteristic1_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::NOT_EXPECTED),
      GetGattErrorCallback(Call::EXPECTED));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_IN_PROGRESS,
            last_gatt_error_code_);

  // Initial read should still succeed:
  ResetEventCounts();
  std::vector<uint8_t> empty_vector;
  SimulateGattCharacteristicRead(characteristic1_, empty_vector);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)

#if defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)
// Tests WriteRemoteCharacteristic error with a pending write operation.
TEST_F(BluetoothRemoteGattCharacteristicTest,
       WriteRemoteCharacteristic_WritePending) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_WRITE));

  std::vector<uint8_t> empty_vector;
  characteristic1_->WriteRemoteCharacteristic(
      empty_vector, GetCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  characteristic1_->WriteRemoteCharacteristic(
      empty_vector, GetCallback(Call::NOT_EXPECTED),
      GetGattErrorCallback(Call::EXPECTED));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_IN_PROGRESS,
            last_gatt_error_code_);

  // Initial write should still succeed:
  ResetEventCounts();
  SimulateGattCharacteristicWrite(characteristic1_);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)

#if defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)
// Tests ReadRemoteCharacteristic error with a pending write operation.
TEST_F(BluetoothRemoteGattCharacteristicTest,
       ReadRemoteCharacteristic_WritePending) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_READ |
      BluetoothRemoteGattCharacteristic::PROPERTY_WRITE));

  std::vector<uint8_t> empty_vector;
  characteristic1_->WriteRemoteCharacteristic(
      empty_vector, GetCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  characteristic1_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::NOT_EXPECTED),
      GetGattErrorCallback(Call::EXPECTED));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_IN_PROGRESS,
            last_gatt_error_code_);

  // Initial write should still succeed:
  ResetEventCounts();
  SimulateGattCharacteristicWrite(characteristic1_);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)

#if defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)
// Tests WriteRemoteCharacteristic error with a pending Read operation.
TEST_F(BluetoothRemoteGattCharacteristicTest,
       WriteRemoteCharacteristic_ReadPending) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_READ |
      BluetoothRemoteGattCharacteristic::PROPERTY_WRITE));

  std::vector<uint8_t> empty_vector;
  characteristic1_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  characteristic1_->WriteRemoteCharacteristic(
      empty_vector, GetCallback(Call::NOT_EXPECTED),
      GetGattErrorCallback(Call::EXPECTED));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_IN_PROGRESS,
            last_gatt_error_code_);

  // Initial read should still succeed:
  ResetEventCounts();
  SimulateGattCharacteristicRead(characteristic1_, empty_vector);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)

#if defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)
// StartNotifySession fails if characteristic doesn't have Notify or Indicate
// property.
TEST_F(BluetoothRemoteGattCharacteristicTest,
       StartNotifySession_NoNotifyOrIndicate) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(StartNotifyBoilerplate(
      /* properties: NOTIFY */ 0x10,
      /* expected_config_descriptor_value: NOTIFY */ 1,
      StartNotifySetupError::CHARACTERISTIC_PROPERTIES));

  EXPECT_EQ(0, gatt_notify_characteristic_attempts_);

  // The expected error callback is asynchronous:
  EXPECT_EQ(0, error_callback_count_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_NOT_SUPPORTED,
            last_gatt_error_code_);
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)

#if defined(OS_ANDROID) || defined(OS_WIN)
// StartNotifySession fails if the characteristic is missing the Client
// Characteristic Configuration descriptor.
// macOS: TODO(crbug.com/624017) Need to implement CCC descriptors.
TEST_F(BluetoothRemoteGattCharacteristicTest,
       StartNotifySession_NoConfigDescriptor) {
  ASSERT_NO_FATAL_FAILURE(StartNotifyBoilerplate(
      /* properties: NOTIFY */ 0x10,
      /* expected_config_descriptor_value: NOTIFY */ 1,
      StartNotifySetupError::CONFIG_DESCRIPTOR_MISSING));

  EXPECT_EQ(0, gatt_notify_characteristic_attempts_);

  // The expected error callback is asynchronous:
  EXPECT_EQ(0, error_callback_count_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_NOT_SUPPORTED,
            last_gatt_error_code_);
}
#endif  // defined(OS_ANDROID) || defined(OS_WIN)

#if defined(OS_ANDROID) || defined(OS_WIN)
// StartNotifySession fails if the characteristic has multiple Client
// Characteristic Configuration descriptors.
// macOS: TODO(crbug.com/624017) Need to implement CCC descriptors.
TEST_F(BluetoothRemoteGattCharacteristicTest,
       StartNotifySession_MultipleConfigDescriptor) {
  ASSERT_NO_FATAL_FAILURE(StartNotifyBoilerplate(
      /* properties: NOTIFY */ 0x10,
      /* expected_config_descriptor_value: NOTIFY */ 1,
      StartNotifySetupError::CONFIG_DESCRIPTOR_DUPLICATE));

  EXPECT_EQ(0, gatt_notify_characteristic_attempts_);

  // The expected error callback is asynchronous:
  EXPECT_EQ(0, error_callback_count_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_FAILED,
            last_gatt_error_code_);
}
#endif  // defined(OS_ANDROID) || defined(OS_WIN)

#if defined(OS_ANDROID)
// StartNotifySession fails synchronously when failing to set a characteristic
// to enable notifications.
// Android: This is mBluetoothGatt.setCharacteristicNotification failing.
// macOS: Not applicable: CoreBluetooth doesn't support synchronous API.
// Windows: Synchronous Test Not Applicable: OS calls are all made
// asynchronously from BluetoothTaskManagerWin.
TEST_F(BluetoothRemoteGattCharacteristicTest,
       StartNotifySession_FailToSetCharacteristicNotification) {
  ASSERT_NO_FATAL_FAILURE(StartNotifyBoilerplate(
      /* properties: NOTIFY */ 0x10,
      /* expected_config_descriptor_value: NOTIFY */ 1,
      StartNotifySetupError::SET_NOTIFY));

  // The expected error callback is asynchronous:
  EXPECT_EQ(0, error_callback_count_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, error_callback_count_);

  EXPECT_EQ(0, gatt_notify_characteristic_attempts_);
  ASSERT_EQ(0u, notify_sessions_.size());
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
// Tests StartNotifySession descriptor write synchronous failure.
// macOS: Not applicable: No need to write to the descriptor manually.
// -[CBPeripheral setNotifyValue:forCharacteristic:] takes care of it.
// Windows: Synchronous Test Not Applicable: OS calls are all made
// asynchronously from BluetoothTaskManagerWin.
TEST_F(BluetoothRemoteGattCharacteristicTest,
       StartNotifySession_WriteDescriptorSynchronousError) {
  ASSERT_NO_FATAL_FAILURE(StartNotifyBoilerplate(
      /* properties: NOTIFY */ 0x10,
      /* expected_config_descriptor_value: NOTIFY */ 1,
      StartNotifySetupError::WRITE_DESCRIPTOR));

  // The expected error callback is asynchronous:
  EXPECT_EQ(0, error_callback_count_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, error_callback_count_);

  EXPECT_EQ(1, gatt_notify_characteristic_attempts_);
  ASSERT_EQ(0u, notify_sessions_.size());
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)
// Tests StartNotifySession success on a characteristic enabling Notify.
TEST_F(BluetoothRemoteGattCharacteristicTest, StartNotifySession) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(StartNotifyBoilerplate(
      /* properties: NOTIFY */ 0x10,
      /* expected_config_descriptor_value: NOTIFY */ 1));
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)

#if defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)
// Tests StartNotifySession success on a characteristic enabling Indicate.
TEST_F(BluetoothRemoteGattCharacteristicTest, StartNotifySession_OnIndicate) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(StartNotifyBoilerplate(
      /* properties: INDICATE */ 0x20,
      /* expected_config_descriptor_value: INDICATE */ 2));
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)

#if defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)
// Tests StartNotifySession success on a characteristic enabling Notify &
// Indicate.
TEST_F(BluetoothRemoteGattCharacteristicTest,
       StartNotifySession_OnNotifyAndIndicate) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(StartNotifyBoilerplate(
      /* properties: NOTIFY and INDICATE bits set */ 0x30,
      /* expected_config_descriptor_value: NOTIFY */ 1));
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)

#if defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)
// Tests multiple StartNotifySession success.
TEST_F(BluetoothRemoteGattCharacteristicTest, StartNotifySession_Multiple) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(
      FakeCharacteristicBoilerplate(/* properties: NOTIFY */ 0x10));
  SimulateGattDescriptor(
      characteristic1_,
      BluetoothRemoteGattDescriptor::ClientCharacteristicConfigurationUuid()
          .canonical_value());
#if !defined(OS_MACOSX)
  // TODO(crbug.com/624017): Need implementation for descriptors.
  ASSERT_EQ(1u, characteristic1_->GetDescriptors().size());
#endif  // !defined(OS_MACOSX)

  characteristic1_->StartNotifySession(
      GetNotifyCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  characteristic1_->StartNotifySession(
      GetNotifyCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(0, callback_count_);
  SimulateGattNotifySessionStarted(characteristic1_);
  EXPECT_EQ(1, gatt_notify_characteristic_attempts_);
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  ASSERT_EQ(2u, notify_sessions_.size());
  ASSERT_TRUE(notify_sessions_[0]);
  ASSERT_TRUE(notify_sessions_[1]);
  EXPECT_EQ(characteristic1_->GetIdentifier(),
            notify_sessions_[0]->GetCharacteristicIdentifier());
  EXPECT_EQ(characteristic1_->GetIdentifier(),
            notify_sessions_[1]->GetCharacteristicIdentifier());
  EXPECT_TRUE(notify_sessions_[0]->IsActive());
  EXPECT_TRUE(notify_sessions_[1]->IsActive());
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)

#if defined(OS_ANDROID) || defined(OS_MACOSX)
// Tests multiple StartNotifySessions pending and then an error.
TEST_F(BluetoothRemoteGattCharacteristicTest,
       StartNotifySessionError_Multiple) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(
      FakeCharacteristicBoilerplate(/* properties: NOTIFY */ 0x10));
  SimulateGattDescriptor(
      characteristic1_,
      BluetoothRemoteGattDescriptor::ClientCharacteristicConfigurationUuid()
          .canonical_value());
#if !defined(OS_MACOSX)
  // TODO(crbug.com/624017): Need implementation for descriptors.
  ASSERT_EQ(1u, characteristic1_->GetDescriptors().size());
#endif  // !defined(OS_MACOSX)

  characteristic1_->StartNotifySession(GetNotifyCallback(Call::NOT_EXPECTED),
                                       GetGattErrorCallback(Call::EXPECTED));
  characteristic1_->StartNotifySession(GetNotifyCallback(Call::NOT_EXPECTED),
                                       GetGattErrorCallback(Call::EXPECTED));
  EXPECT_EQ(1, gatt_notify_characteristic_attempts_);
  EXPECT_EQ(0, callback_count_);
  SimulateGattNotifySessionStartError(
      characteristic1_, BluetoothRemoteGattService::GATT_ERROR_FAILED);
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(2, error_callback_count_);
  ASSERT_EQ(0u, notify_sessions_.size());
  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_FAILED,
            last_gatt_error_code_);
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX)

#if defined(OS_ANDROID)
// Tests StartNotifySession completing after chrome objects are deleted.
// macOS: Not applicable: This can never happen if CBPeripheral delegate is set
// to nil.
TEST_F(BluetoothRemoteGattCharacteristicTest, StartNotifySession_AfterDeleted) {
  ASSERT_NO_FATAL_FAILURE(
      FakeCharacteristicBoilerplate(/* properties: NOTIFY */ 0x10));
  SimulateGattDescriptor(
      characteristic1_,
      BluetoothRemoteGattDescriptor::ClientCharacteristicConfigurationUuid()
          .canonical_value());
  ASSERT_EQ(1u, characteristic1_->GetDescriptors().size());

  characteristic1_->StartNotifySession(GetNotifyCallback(Call::NOT_EXPECTED),
                                       GetGattErrorCallback(Call::EXPECTED));
  EXPECT_EQ(1, gatt_notify_characteristic_attempts_);
  EXPECT_EQ(0, callback_count_);

  RememberCharacteristicForSubsequentAction(characteristic1_);
  RememberCCCDescriptorForSubsequentAction(characteristic1_);
  DeleteDevice(device_);  // TODO(576906) delete only the characteristic.

  SimulateGattNotifySessionStarted(/* use remembered characteristic */ nullptr);
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  ASSERT_EQ(0u, notify_sessions_.size());
  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_FAILED,
            last_gatt_error_code_);
}
#endif  // defined(OS_ANDROID)

#if defined(OS_MACOSX) || defined(OS_WIN)
// Tests StartNotifySession reentrant in start notify session success callback
// and the reentrant start notify session success.
TEST_F(BluetoothRemoteGattCharacteristicTest,
       StartNotifySession_Reentrant_Success_Success) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(
      FakeCharacteristicBoilerplate(/* properties: NOTIFY */ 0x10));
  SimulateGattDescriptor(
      characteristic1_,
      BluetoothGattDescriptor::ClientCharacteristicConfigurationUuid().value());
#if !defined(OS_MACOSX)
  // TODO(crbug.com/624017): Need implementation for descriptors.
  ASSERT_EQ(1u, characteristic1_->GetDescriptors().size());
#endif  // !defined(OS_MACOSX)

  characteristic1_->StartNotifySession(
      GetReentrantStartNotifySessionSuccessCallback(Call::EXPECTED,
                                                    characteristic1_),
      GetReentrantStartNotifySessionErrorCallback(
          Call::NOT_EXPECTED, characteristic1_,
          false /* error_in_reentrant */));
  EXPECT_EQ(0, callback_count_);
  SimulateGattNotifySessionStarted(characteristic1_);
  EXPECT_EQ(1, gatt_notify_characteristic_attempts_);

  // Simulate reentrant StartNotifySession request from
  // BluetoothTestBase::ReentrantStartNotifySessionSuccessCallback.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, gatt_notify_characteristic_attempts_);
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  ASSERT_EQ(2u, notify_sessions_.size());
  for (unsigned int i = 0; i < notify_sessions_.size(); i++) {
    ASSERT_TRUE(notify_sessions_[i]);
    EXPECT_EQ(characteristic1_->GetIdentifier(),
              notify_sessions_[i]->GetCharacteristicIdentifier());
    EXPECT_TRUE(notify_sessions_[i]->IsActive());
  }
}
#endif  // defined(OS_MACOSX) || defined(OS_WIN)

#if defined(OS_WIN)
// Tests StartNotifySession reentrant in start notify session error callback
// and the reentrant start notify session success.
TEST_F(BluetoothRemoteGattCharacteristicTest,
       StartNotifySession_Reentrant_Error_Success) {
  ASSERT_NO_FATAL_FAILURE(
      FakeCharacteristicBoilerplate(/* properties: NOTIFY */ 0x10));
  SimulateGattDescriptor(
      characteristic1_,
      BluetoothGattDescriptor::ClientCharacteristicConfigurationUuid().value());
  ASSERT_EQ(1u, characteristic1_->GetDescriptors().size());

  SimulateGattNotifySessionStartError(
      characteristic1_, BluetoothRemoteGattService::GATT_ERROR_UNKNOWN);

  characteristic1_->StartNotifySession(
      GetReentrantStartNotifySessionSuccessCallback(Call::NOT_EXPECTED,
                                                    characteristic1_),
      GetReentrantStartNotifySessionErrorCallback(
          Call::EXPECTED, characteristic1_, false /* error_in_reentrant */));
  EXPECT_EQ(0, callback_count_);
  SimulateGattNotifySessionStarted(characteristic1_);
  EXPECT_EQ(0, gatt_notify_characteristic_attempts_);
  EXPECT_EQ(1, error_callback_count_);

  // Simulate reentrant StartNotifySession request from
  // BluetoothTestBase::ReentrantStartNotifySessionErrorCallback.
  SimulateGattNotifySessionStarted(characteristic1_);
  EXPECT_EQ(1, gatt_notify_characteristic_attempts_);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  ASSERT_EQ(1u, notify_sessions_.size());
  ASSERT_TRUE(notify_sessions_[0]);
  EXPECT_EQ(characteristic1_->GetIdentifier(),
            notify_sessions_[0]->GetCharacteristicIdentifier());
  EXPECT_TRUE(notify_sessions_[0]->IsActive());
}
#endif  // defined(OS_WIN)

#if defined(OS_WIN)
// Tests StartNotifySession reentrant in start notify session error callback
// and the reentrant start notify session error.
TEST_F(BluetoothRemoteGattCharacteristicTest,
       StartNotifySession_Reentrant_Error_Error) {
  ASSERT_NO_FATAL_FAILURE(
      FakeCharacteristicBoilerplate(/* properties: NOTIFY */ 0x10));
  SimulateGattDescriptor(
      characteristic1_,
      BluetoothGattDescriptor::ClientCharacteristicConfigurationUuid().value());
  ASSERT_EQ(1u, characteristic1_->GetDescriptors().size());

  SimulateGattNotifySessionStartError(
      characteristic1_, BluetoothRemoteGattService::GATT_ERROR_UNKNOWN);

  characteristic1_->StartNotifySession(
      GetReentrantStartNotifySessionSuccessCallback(Call::NOT_EXPECTED,
                                                    characteristic1_),
      GetReentrantStartNotifySessionErrorCallback(
          Call::EXPECTED, characteristic1_, true /* error_in_reentrant */));
  EXPECT_EQ(0, callback_count_);
  SimulateGattNotifySessionStarted(characteristic1_);
  EXPECT_EQ(0, gatt_notify_characteristic_attempts_);

  // Simulate reentrant StartNotifySession request from
  // BluetoothTestBase::ReentrantStartNotifySessionErrorCallback.
  SimulateGattNotifySessionStarted(characteristic1_);
  EXPECT_EQ(0, gatt_notify_characteristic_attempts_);
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(2, error_callback_count_);
  ASSERT_EQ(0u, notify_sessions_.size());
}
#endif  // defined(OS_WIN)

#if defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)
// Tests Characteristic Value changes during a Notify Session.
TEST_F(BluetoothRemoteGattCharacteristicTest, GattCharacteristicValueChanged) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(StartNotifyBoilerplate(
      /* properties: NOTIFY */ 0x10,
      /* expected_config_descriptor_value: NOTIFY */ 1));

  TestBluetoothAdapterObserver observer(adapter_);

  std::vector<uint8_t> test_vector1, test_vector2;
  test_vector1.push_back(111);
  test_vector2.push_back(222);

  SimulateGattCharacteristicChanged(characteristic1_, test_vector1);
  EXPECT_EQ(1, observer.gatt_characteristic_value_changed_count());
  EXPECT_EQ(test_vector1, characteristic1_->GetValue());

  SimulateGattCharacteristicChanged(characteristic1_, test_vector2);
  EXPECT_EQ(2, observer.gatt_characteristic_value_changed_count());
  EXPECT_EQ(test_vector2, characteristic1_->GetValue());
}
#endif  // defined(OS_ANDROID) || defined(OS_MACOSX) || defined(OS_WIN)

#if defined(OS_ANDROID) || defined(OS_WIN)
// Tests Characteristic Value changing after a Notify Session and objects being
// destroyed.
// macOS: Not applicable: This can never happen if CBPeripheral delegate is set
// to nil.
TEST_F(BluetoothRemoteGattCharacteristicTest,
       GattCharacteristicValueChanged_AfterDeleted) {
  ASSERT_NO_FATAL_FAILURE(StartNotifyBoilerplate(
      /* properties: NOTIFY */ 0x10,
      /* expected_config_descriptor_value: NOTIFY */ 1));
  TestBluetoothAdapterObserver observer(adapter_);

  RememberCharacteristicForSubsequentAction(characteristic1_);
  DeleteDevice(device_);  // TODO(576906) delete only the characteristic.

  std::vector<uint8_t> empty_vector;
  SimulateGattCharacteristicChanged(/* use remembered characteristic */ nullptr,
                                    empty_vector);
  EXPECT_TRUE("Did not crash!");
  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());
}
#endif  // defined(OS_ANDROID) || defined(OS_WIN)

#if defined(OS_ANDROID) || defined(OS_WIN)
TEST_F(BluetoothRemoteGattCharacteristicTest, GetDescriptors_FindNone) {
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate());

  EXPECT_EQ(0u, characteristic1_->GetDescriptors().size());
}
#endif  // defined(OS_ANDROID) || defined(OS_WIN)

#if defined(OS_ANDROID) || defined(OS_WIN)
TEST_F(BluetoothRemoteGattCharacteristicTest,
       GetDescriptors_and_GetDescriptor) {
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate());

  // Add several Descriptors:
  BluetoothUUID uuid1("11111111-0000-1000-8000-00805f9b34fb");
  BluetoothUUID uuid2("22222222-0000-1000-8000-00805f9b34fb");
  BluetoothUUID uuid3("33333333-0000-1000-8000-00805f9b34fb");
  BluetoothUUID uuid4("44444444-0000-1000-8000-00805f9b34fb");
  SimulateGattDescriptor(characteristic1_, uuid1.canonical_value());
  SimulateGattDescriptor(characteristic1_, uuid2.canonical_value());
  SimulateGattDescriptor(characteristic2_, uuid3.canonical_value());
  SimulateGattDescriptor(characteristic2_, uuid4.canonical_value());

  // Verify that GetDescriptor can retrieve descriptors again by ID,
  // and that the same Descriptor is returned when searched by ID.
  EXPECT_EQ(2u, characteristic1_->GetDescriptors().size());
  EXPECT_EQ(2u, characteristic2_->GetDescriptors().size());
  std::string c1_id1 = characteristic1_->GetDescriptors()[0]->GetIdentifier();
  std::string c1_id2 = characteristic1_->GetDescriptors()[1]->GetIdentifier();
  std::string c2_id1 = characteristic2_->GetDescriptors()[0]->GetIdentifier();
  std::string c2_id2 = characteristic2_->GetDescriptors()[1]->GetIdentifier();
  BluetoothUUID c1_uuid1 = characteristic1_->GetDescriptors()[0]->GetUUID();
  BluetoothUUID c1_uuid2 = characteristic1_->GetDescriptors()[1]->GetUUID();
  BluetoothUUID c2_uuid1 = characteristic2_->GetDescriptors()[0]->GetUUID();
  BluetoothUUID c2_uuid2 = characteristic2_->GetDescriptors()[1]->GetUUID();
  EXPECT_EQ(c1_uuid1, characteristic1_->GetDescriptor(c1_id1)->GetUUID());
  EXPECT_EQ(c1_uuid2, characteristic1_->GetDescriptor(c1_id2)->GetUUID());
  EXPECT_EQ(c2_uuid1, characteristic2_->GetDescriptor(c2_id1)->GetUUID());
  EXPECT_EQ(c2_uuid2, characteristic2_->GetDescriptor(c2_id2)->GetUUID());

  // GetDescriptors & GetDescriptor return the same object for the same ID:
  EXPECT_EQ(characteristic1_->GetDescriptors()[0],
            characteristic1_->GetDescriptor(c1_id1));
  EXPECT_EQ(characteristic1_->GetDescriptor(c1_id1),
            characteristic1_->GetDescriptor(c1_id1));

  // Characteristic 1 has descriptor uuids 1 and 2 (we don't know the order).
  EXPECT_TRUE(c1_uuid1 == uuid1 || c1_uuid2 == uuid1);
  EXPECT_TRUE(c1_uuid1 == uuid2 || c1_uuid2 == uuid2);
  // ... but not uuid 3
  EXPECT_FALSE(c1_uuid1 == uuid3 || c1_uuid2 == uuid3);
}
#endif  // defined(OS_ANDROID) || defined(OS_WIN)

#if defined(OS_ANDROID) || defined(OS_WIN)
TEST_F(BluetoothRemoteGattCharacteristicTest, GetDescriptorsByUUID) {
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate());

  // Add several Descriptors:
  BluetoothUUID id1("11111111-0000-1000-8000-00805f9b34fb");
  BluetoothUUID id2("22222222-0000-1000-8000-00805f9b34fb");
  BluetoothUUID id3("33333333-0000-1000-8000-00805f9b34fb");
  SimulateGattDescriptor(characteristic1_, id1.canonical_value());
  SimulateGattDescriptor(characteristic1_, id2.canonical_value());
  SimulateGattDescriptor(characteristic2_, id3.canonical_value());
  SimulateGattDescriptor(characteristic2_, id3.canonical_value());

  EXPECT_NE(characteristic2_->GetDescriptorsByUUID(id3).at(0)->GetIdentifier(),
            characteristic2_->GetDescriptorsByUUID(id3).at(1)->GetIdentifier());

  EXPECT_EQ(id1, characteristic1_->GetDescriptorsByUUID(id1).at(0)->GetUUID());
  EXPECT_EQ(id2, characteristic1_->GetDescriptorsByUUID(id2).at(0)->GetUUID());
  EXPECT_EQ(id3, characteristic2_->GetDescriptorsByUUID(id3).at(0)->GetUUID());
  EXPECT_EQ(id3, characteristic2_->GetDescriptorsByUUID(id3).at(1)->GetUUID());
  EXPECT_EQ(1u, characteristic1_->GetDescriptorsByUUID(id1).size());
  EXPECT_EQ(1u, characteristic1_->GetDescriptorsByUUID(id2).size());
  EXPECT_EQ(2u, characteristic2_->GetDescriptorsByUUID(id3).size());

  EXPECT_EQ(0u, characteristic2_->GetDescriptorsByUUID(id1).size());
  EXPECT_EQ(0u, characteristic2_->GetDescriptorsByUUID(id2).size());
  EXPECT_EQ(0u, characteristic1_->GetDescriptorsByUUID(id3).size());
}
#endif  // defined(OS_ANDROID) || defined(OS_WIN)

}  // namespace device
