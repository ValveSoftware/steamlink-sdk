// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "device/bluetooth/test/bluetooth_test_android.h"
#elif defined(OS_MACOSX)
#include "device/bluetooth/test/bluetooth_test_mac.h"
#endif

namespace device {

#if defined(OS_ANDROID) || defined(OS_MACOSX)
class BluetoothRemoteGattDescriptorTest : public BluetoothTest {
 public:
  // Creates adapter_, device_, service_, characteristic_,
  // descriptor1_, & descriptor2_.
  void FakeDescriptorBoilerplate() {
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
    SimulateGattCharacteristic(service_, uuid, 0);
    ASSERT_EQ(1u, service_->GetCharacteristics().size());
    characteristic_ = service_->GetCharacteristics()[0];
    SimulateGattDescriptor(characteristic_,
                           "00000001-0000-1000-8000-00805f9b34fb");
    SimulateGattDescriptor(characteristic_,
                           "00000002-0000-1000-8000-00805f9b34fb");
    ASSERT_EQ(2u, characteristic_->GetDescriptors().size());
    descriptor1_ = characteristic_->GetDescriptors()[0];
    descriptor2_ = characteristic_->GetDescriptors()[1];
    ResetEventCounts();
  }

  BluetoothDevice* device_ = nullptr;
  BluetoothRemoteGattService* service_ = nullptr;
  BluetoothRemoteGattCharacteristic* characteristic_ = nullptr;
  BluetoothRemoteGattDescriptor* descriptor1_ = nullptr;
  BluetoothRemoteGattDescriptor* descriptor2_ = nullptr;
};
#endif

#if defined(OS_ANDROID)
TEST_F(BluetoothRemoteGattDescriptorTest, GetIdentifier) {
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  // 2 devices to verify that descriptors on them have distinct IDs.
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
  // 6 descriptors (same UUID), 1 on each characteristic
  // TODO(576900) Test multiple descriptors with same UUID on one
  // characteristic.
  SimulateGattDescriptor(char1, uuid);
  SimulateGattDescriptor(char2, uuid);
  SimulateGattDescriptor(char3, uuid);
  SimulateGattDescriptor(char4, uuid);
  SimulateGattDescriptor(char5, uuid);
  SimulateGattDescriptor(char6, uuid);
  BluetoothRemoteGattDescriptor* desc1 = char1->GetDescriptors()[0];
  BluetoothRemoteGattDescriptor* desc2 = char2->GetDescriptors()[0];
  BluetoothRemoteGattDescriptor* desc3 = char3->GetDescriptors()[0];
  BluetoothRemoteGattDescriptor* desc4 = char4->GetDescriptors()[0];
  BluetoothRemoteGattDescriptor* desc5 = char5->GetDescriptors()[0];
  BluetoothRemoteGattDescriptor* desc6 = char6->GetDescriptors()[0];

  // All IDs are unique.
  EXPECT_NE(desc1->GetIdentifier(), desc2->GetIdentifier());
  EXPECT_NE(desc1->GetIdentifier(), desc3->GetIdentifier());
  EXPECT_NE(desc1->GetIdentifier(), desc4->GetIdentifier());
  EXPECT_NE(desc1->GetIdentifier(), desc5->GetIdentifier());
  EXPECT_NE(desc1->GetIdentifier(), desc6->GetIdentifier());

  EXPECT_NE(desc2->GetIdentifier(), desc3->GetIdentifier());
  EXPECT_NE(desc2->GetIdentifier(), desc4->GetIdentifier());
  EXPECT_NE(desc2->GetIdentifier(), desc5->GetIdentifier());
  EXPECT_NE(desc2->GetIdentifier(), desc6->GetIdentifier());

  EXPECT_NE(desc3->GetIdentifier(), desc4->GetIdentifier());
  EXPECT_NE(desc3->GetIdentifier(), desc5->GetIdentifier());
  EXPECT_NE(desc3->GetIdentifier(), desc6->GetIdentifier());

  EXPECT_NE(desc4->GetIdentifier(), desc5->GetIdentifier());
  EXPECT_NE(desc4->GetIdentifier(), desc6->GetIdentifier());

  EXPECT_NE(desc5->GetIdentifier(), desc6->GetIdentifier());
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
TEST_F(BluetoothRemoteGattDescriptorTest, GetUUID) {
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
  SimulateGattConnection(device);
  std::vector<std::string> services;
  services.push_back("00000000-0000-1000-8000-00805f9b34fb");
  SimulateGattServicesDiscovered(device, services);
  ASSERT_EQ(1u, device->GetGattServices().size());
  BluetoothRemoteGattService* service = device->GetGattServices()[0];

  SimulateGattCharacteristic(service, "00000000-0000-1000-8000-00805f9b34fb",
                             /* properties */ 0);
  ASSERT_EQ(1u, service->GetCharacteristics().size());
  BluetoothRemoteGattCharacteristic* characteristic =
      service->GetCharacteristics()[0];

  // Create 2 descriptors.
  std::string uuid_str1("11111111-0000-1000-8000-00805f9b34fb");
  std::string uuid_str2("22222222-0000-1000-8000-00805f9b34fb");
  BluetoothUUID uuid1(uuid_str1);
  BluetoothUUID uuid2(uuid_str2);
  SimulateGattDescriptor(characteristic, uuid_str1);
  SimulateGattDescriptor(characteristic, uuid_str2);
  ASSERT_EQ(2u, characteristic->GetDescriptors().size());
  BluetoothRemoteGattDescriptor* descriptor1 =
      characteristic->GetDescriptors()[0];
  BluetoothRemoteGattDescriptor* descriptor2 =
      characteristic->GetDescriptors()[1];

  // Swap as needed to have descriptor1 be the one with uuid1.
  if (descriptor2->GetUUID() == uuid1)
    std::swap(descriptor1, descriptor2);

  EXPECT_EQ(uuid1, descriptor1->GetUUID());
  EXPECT_EQ(uuid2, descriptor2->GetUUID());
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
// Tests ReadRemoteDescriptor and GetValue with empty value buffer.
TEST_F(BluetoothRemoteGattDescriptorTest, ReadRemoteDescriptor_Empty) {
  ASSERT_NO_FATAL_FAILURE(FakeDescriptorBoilerplate());

  descriptor1_->ReadRemoteDescriptor(GetReadValueCallback(Call::EXPECTED),
                                     GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(1, gatt_read_descriptor_attempts_);
  std::vector<uint8_t> empty_vector;
  SimulateGattDescriptorRead(descriptor1_, empty_vector);

  // Duplicate read reported from OS shouldn't cause a problem:
  SimulateGattDescriptorRead(descriptor1_, empty_vector);

  EXPECT_EQ(empty_vector, last_read_value_);
  EXPECT_EQ(empty_vector, descriptor1_->GetValue());
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
// Tests WriteRemoteDescriptor with empty value buffer.
TEST_F(BluetoothRemoteGattDescriptorTest, WriteRemoteDescriptor_Empty) {
  ASSERT_NO_FATAL_FAILURE(FakeDescriptorBoilerplate());

  std::vector<uint8_t> empty_vector;
  descriptor1_->WriteRemoteDescriptor(empty_vector, GetCallback(Call::EXPECTED),
                                      GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(1, gatt_write_descriptor_attempts_);
  SimulateGattDescriptorWrite(descriptor1_);

  // Duplicate write reported from OS shouldn't cause a problem:
  SimulateGattDescriptorWrite(descriptor1_);

  EXPECT_EQ(empty_vector, last_write_value_);
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
// Tests ReadRemoteDescriptor completing after Chrome objects are deleted.
TEST_F(BluetoothRemoteGattDescriptorTest, ReadRemoteDescriptor_AfterDeleted) {
  ASSERT_NO_FATAL_FAILURE(FakeDescriptorBoilerplate());

  descriptor1_->ReadRemoteDescriptor(GetReadValueCallback(Call::NOT_EXPECTED),
                                     GetGattErrorCallback(Call::NOT_EXPECTED));

  RememberDescriptorForSubsequentAction(descriptor1_);
  DeleteDevice(device_);  // TODO(576906) delete only the descriptor.

  std::vector<uint8_t> empty_vector;
  SimulateGattDescriptorRead(/* use remembered descriptor */ nullptr,
                             empty_vector);
  EXPECT_TRUE("Did not crash!");
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
// Tests WriteRemoteDescriptor completing after Chrome objects are deleted.
TEST_F(BluetoothRemoteGattDescriptorTest, WriteRemoteDescriptor_AfterDeleted) {
  ASSERT_NO_FATAL_FAILURE(FakeDescriptorBoilerplate());

  std::vector<uint8_t> empty_vector;
  descriptor1_->WriteRemoteDescriptor(empty_vector,
                                      GetCallback(Call::NOT_EXPECTED),
                                      GetGattErrorCallback(Call::NOT_EXPECTED));

  RememberDescriptorForSubsequentAction(descriptor1_);
  DeleteDevice(device_);  // TODO(576906) delete only the descriptor.

  SimulateGattDescriptorWrite(/* use remembered descriptor */ nullptr);
  EXPECT_TRUE("Did not crash!");
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
// Tests ReadRemoteDescriptor and GetValue with non-empty value buffer.
TEST_F(BluetoothRemoteGattDescriptorTest, ReadRemoteDescriptor) {
  ASSERT_NO_FATAL_FAILURE(FakeDescriptorBoilerplate());

  descriptor1_->ReadRemoteDescriptor(GetReadValueCallback(Call::EXPECTED),
                                     GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(1, gatt_read_descriptor_attempts_);

  uint8_t values[] = {0, 1, 2, 3, 4, 0xf, 0xf0, 0xff};
  std::vector<uint8_t> test_vector(values, values + arraysize(values));
  SimulateGattDescriptorRead(descriptor1_, test_vector);

  // Duplicate read reported from OS shouldn't cause a problem:
  std::vector<uint8_t> empty_vector;
  SimulateGattDescriptorRead(descriptor1_, empty_vector);

  EXPECT_EQ(test_vector, last_read_value_);
  EXPECT_EQ(test_vector, descriptor1_->GetValue());
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
// Tests WriteRemoteDescriptor with non-empty value buffer.
TEST_F(BluetoothRemoteGattDescriptorTest, WriteRemoteDescriptor) {
  ASSERT_NO_FATAL_FAILURE(FakeDescriptorBoilerplate());

  uint8_t values[] = {0, 1, 2, 3, 4, 0xf, 0xf0, 0xff};
  std::vector<uint8_t> test_vector(values, values + arraysize(values));
  descriptor1_->WriteRemoteDescriptor(test_vector, GetCallback(Call::EXPECTED),
                                      GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(1, gatt_write_descriptor_attempts_);

  SimulateGattDescriptorWrite(descriptor1_);

  EXPECT_EQ(test_vector, last_write_value_);
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
// Tests ReadRemoteDescriptor and GetValue multiple times.
TEST_F(BluetoothRemoteGattDescriptorTest, ReadRemoteDescriptor_Twice) {
  ASSERT_NO_FATAL_FAILURE(FakeDescriptorBoilerplate());

  descriptor1_->ReadRemoteDescriptor(GetReadValueCallback(Call::EXPECTED),
                                     GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(1, gatt_read_descriptor_attempts_);

  uint8_t values[] = {0, 1, 2, 3, 4, 0xf, 0xf0, 0xff};
  std::vector<uint8_t> test_vector(values, values + arraysize(values));
  SimulateGattDescriptorRead(descriptor1_, test_vector);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(test_vector, last_read_value_);
  EXPECT_EQ(test_vector, descriptor1_->GetValue());

  // Read again, with different value:
  ResetEventCounts();
  descriptor1_->ReadRemoteDescriptor(GetReadValueCallback(Call::EXPECTED),
                                     GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(1, gatt_read_descriptor_attempts_);
  std::vector<uint8_t> empty_vector;
  SimulateGattDescriptorRead(descriptor1_, empty_vector);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(empty_vector, last_read_value_);
  EXPECT_EQ(empty_vector, descriptor1_->GetValue());
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
// Tests WriteRemoteDescriptor multiple times.
TEST_F(BluetoothRemoteGattDescriptorTest, WriteRemoteDescriptor_Twice) {
  ASSERT_NO_FATAL_FAILURE(FakeDescriptorBoilerplate());

  uint8_t values[] = {0, 1, 2, 3, 4, 0xf, 0xf0, 0xff};
  std::vector<uint8_t> test_vector(values, values + arraysize(values));
  descriptor1_->WriteRemoteDescriptor(test_vector, GetCallback(Call::EXPECTED),
                                      GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(1, gatt_write_descriptor_attempts_);

  SimulateGattDescriptorWrite(descriptor1_);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(test_vector, last_write_value_);

  // Write again, with different value:
  ResetEventCounts();
  std::vector<uint8_t> empty_vector;
  descriptor1_->WriteRemoteDescriptor(empty_vector, GetCallback(Call::EXPECTED),
                                      GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(1, gatt_write_descriptor_attempts_);
  SimulateGattDescriptorWrite(descriptor1_);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(empty_vector, last_write_value_);
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
// Tests ReadRemoteDescriptor on two descriptors.
TEST_F(BluetoothRemoteGattDescriptorTest,
       ReadRemoteDescriptor_MultipleDescriptors) {
  ASSERT_NO_FATAL_FAILURE(FakeDescriptorBoilerplate());

  descriptor1_->ReadRemoteDescriptor(GetReadValueCallback(Call::EXPECTED),
                                     GetGattErrorCallback(Call::NOT_EXPECTED));
  descriptor2_->ReadRemoteDescriptor(GetReadValueCallback(Call::EXPECTED),
                                     GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(2, gatt_read_descriptor_attempts_);
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  std::vector<uint8_t> test_vector1;
  test_vector1.push_back(111);
  SimulateGattDescriptorRead(descriptor1_, test_vector1);
  EXPECT_EQ(test_vector1, last_read_value_);

  std::vector<uint8_t> test_vector2;
  test_vector2.push_back(222);
  SimulateGattDescriptorRead(descriptor2_, test_vector2);
  EXPECT_EQ(test_vector2, last_read_value_);

  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(test_vector1, descriptor1_->GetValue());
  EXPECT_EQ(test_vector2, descriptor2_->GetValue());
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
// Tests WriteRemoteDescriptor on two descriptors.
TEST_F(BluetoothRemoteGattDescriptorTest,
       WriteRemoteDescriptor_MultipleDescriptors) {
  ASSERT_NO_FATAL_FAILURE(FakeDescriptorBoilerplate());

  std::vector<uint8_t> test_vector1;
  test_vector1.push_back(111);
  descriptor1_->WriteRemoteDescriptor(test_vector1, GetCallback(Call::EXPECTED),
                                      GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(test_vector1, last_write_value_);

  std::vector<uint8_t> test_vector2;
  test_vector2.push_back(222);
  descriptor2_->WriteRemoteDescriptor(test_vector2, GetCallback(Call::EXPECTED),
                                      GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(test_vector2, last_write_value_);

  EXPECT_EQ(2, gatt_write_descriptor_attempts_);
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  SimulateGattDescriptorWrite(descriptor1_);
  SimulateGattDescriptorWrite(descriptor2_);

  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
// Tests ReadRemoteDescriptor asynchronous error.
TEST_F(BluetoothRemoteGattDescriptorTest, ReadError) {
  ASSERT_NO_FATAL_FAILURE(FakeDescriptorBoilerplate());

  descriptor1_->ReadRemoteDescriptor(GetReadValueCallback(Call::NOT_EXPECTED),
                                     GetGattErrorCallback(Call::EXPECTED));
  SimulateGattDescriptorReadError(
      descriptor1_, BluetoothRemoteGattService::GATT_ERROR_INVALID_LENGTH);
  SimulateGattDescriptorReadError(
      descriptor1_, BluetoothRemoteGattService::GATT_ERROR_FAILED);
  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_INVALID_LENGTH,
            last_gatt_error_code_);
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
// Tests WriteRemoteDescriptor asynchronous error.
TEST_F(BluetoothRemoteGattDescriptorTest, WriteError) {
  ASSERT_NO_FATAL_FAILURE(FakeDescriptorBoilerplate());

  std::vector<uint8_t> empty_vector;
  descriptor1_->WriteRemoteDescriptor(empty_vector,
                                      GetCallback(Call::NOT_EXPECTED),
                                      GetGattErrorCallback(Call::EXPECTED));
  SimulateGattDescriptorWriteError(
      descriptor1_, BluetoothRemoteGattService::GATT_ERROR_INVALID_LENGTH);
  SimulateGattDescriptorWriteError(
      descriptor1_, BluetoothRemoteGattService::GATT_ERROR_FAILED);

  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_INVALID_LENGTH,
            last_gatt_error_code_);
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
// Tests ReadRemoteDescriptor synchronous error.
TEST_F(BluetoothRemoteGattDescriptorTest, ReadSynchronousError) {
  ASSERT_NO_FATAL_FAILURE(FakeDescriptorBoilerplate());

  SimulateGattDescriptorReadWillFailSynchronouslyOnce(descriptor1_);
  descriptor1_->ReadRemoteDescriptor(GetReadValueCallback(Call::NOT_EXPECTED),
                                     GetGattErrorCallback(Call::EXPECTED));
  EXPECT_EQ(0, gatt_read_descriptor_attempts_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_FAILED,
            last_gatt_error_code_);

  // After failing once, can succeed:
  ResetEventCounts();
  descriptor1_->ReadRemoteDescriptor(GetReadValueCallback(Call::EXPECTED),
                                     GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(1, gatt_read_descriptor_attempts_);
  std::vector<uint8_t> empty_vector;
  SimulateGattDescriptorRead(descriptor1_, empty_vector);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
// Tests WriteRemoteDescriptor synchronous error.
TEST_F(BluetoothRemoteGattDescriptorTest, WriteSynchronousError) {
  ASSERT_NO_FATAL_FAILURE(FakeDescriptorBoilerplate());

  SimulateGattDescriptorWriteWillFailSynchronouslyOnce(descriptor1_);
  std::vector<uint8_t> empty_vector;
  descriptor1_->WriteRemoteDescriptor(empty_vector,
                                      GetCallback(Call::NOT_EXPECTED),
                                      GetGattErrorCallback(Call::EXPECTED));
  EXPECT_EQ(0, gatt_write_descriptor_attempts_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_FAILED,
            last_gatt_error_code_);

  // After failing once, can succeed:
  ResetEventCounts();
  descriptor1_->WriteRemoteDescriptor(empty_vector, GetCallback(Call::EXPECTED),
                                      GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(1, gatt_write_descriptor_attempts_);
  SimulateGattDescriptorWrite(descriptor1_);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
// Tests ReadRemoteDescriptor error with a pending read operation.
TEST_F(BluetoothRemoteGattDescriptorTest, ReadRemoteDescriptor_ReadPending) {
  ASSERT_NO_FATAL_FAILURE(FakeDescriptorBoilerplate());

  descriptor1_->ReadRemoteDescriptor(GetReadValueCallback(Call::EXPECTED),
                                     GetGattErrorCallback(Call::NOT_EXPECTED));
  descriptor1_->ReadRemoteDescriptor(GetReadValueCallback(Call::NOT_EXPECTED),
                                     GetGattErrorCallback(Call::EXPECTED));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_IN_PROGRESS,
            last_gatt_error_code_);

  // Initial read should still succeed:
  ResetEventCounts();
  std::vector<uint8_t> empty_vector;
  SimulateGattDescriptorRead(descriptor1_, empty_vector);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
// Tests WriteRemoteDescriptor error with a pending write operation.
TEST_F(BluetoothRemoteGattDescriptorTest, WriteRemoteDescriptor_WritePending) {
  ASSERT_NO_FATAL_FAILURE(FakeDescriptorBoilerplate());

  std::vector<uint8_t> empty_vector;
  descriptor1_->WriteRemoteDescriptor(empty_vector, GetCallback(Call::EXPECTED),
                                      GetGattErrorCallback(Call::NOT_EXPECTED));
  descriptor1_->WriteRemoteDescriptor(empty_vector,
                                      GetCallback(Call::NOT_EXPECTED),
                                      GetGattErrorCallback(Call::EXPECTED));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_IN_PROGRESS,
            last_gatt_error_code_);

  // Initial write should still succeed:
  ResetEventCounts();
  SimulateGattDescriptorWrite(descriptor1_);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
// Tests ReadRemoteDescriptor error with a pending write operation.
TEST_F(BluetoothRemoteGattDescriptorTest, ReadRemoteDescriptor_WritePending) {
  ASSERT_NO_FATAL_FAILURE(FakeDescriptorBoilerplate());

  std::vector<uint8_t> empty_vector;
  descriptor1_->WriteRemoteDescriptor(empty_vector, GetCallback(Call::EXPECTED),
                                      GetGattErrorCallback(Call::NOT_EXPECTED));
  descriptor1_->ReadRemoteDescriptor(GetReadValueCallback(Call::NOT_EXPECTED),
                                     GetGattErrorCallback(Call::EXPECTED));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_IN_PROGRESS,
            last_gatt_error_code_);

  // Initial write should still succeed:
  ResetEventCounts();
  SimulateGattDescriptorWrite(descriptor1_);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
// Tests WriteRemoteDescriptor error with a pending Read operation.
TEST_F(BluetoothRemoteGattDescriptorTest, WriteRemoteDescriptor_ReadPending) {
  ASSERT_NO_FATAL_FAILURE(FakeDescriptorBoilerplate());

  std::vector<uint8_t> empty_vector;
  descriptor1_->ReadRemoteDescriptor(GetReadValueCallback(Call::EXPECTED),
                                     GetGattErrorCallback(Call::NOT_EXPECTED));
  descriptor1_->WriteRemoteDescriptor(empty_vector,
                                      GetCallback(Call::NOT_EXPECTED),
                                      GetGattErrorCallback(Call::EXPECTED));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_IN_PROGRESS,
            last_gatt_error_code_);

  // Initial read should still succeed:
  ResetEventCounts();
  SimulateGattDescriptorRead(descriptor1_, empty_vector);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
}
#endif  // defined(OS_ANDROID)

}  // namespace device
