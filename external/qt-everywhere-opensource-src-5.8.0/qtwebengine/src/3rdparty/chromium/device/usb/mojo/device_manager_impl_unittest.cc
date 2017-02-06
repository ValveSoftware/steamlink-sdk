// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/mojo/device_manager_impl.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "device/core/mock_device_client.h"
#include "device/usb/mock_usb_device.h"
#include "device/usb/mock_usb_device_handle.h"
#include "device/usb/mock_usb_service.h"
#include "device/usb/mojo/device_impl.h"
#include "device/usb/mojo/mock_permission_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Invoke;
using ::testing::_;

namespace device {
namespace usb {

namespace {

ACTION_P2(ExpectGuidAndThen, expected_guid, callback) {
  ASSERT_TRUE(arg0);
  EXPECT_EQ(expected_guid, arg0->guid);
  if (!callback.is_null())
    callback.Run();
};

class USBDeviceManagerImplTest : public testing::Test {
 public:
  USBDeviceManagerImplTest() : message_loop_(new base::MessageLoop) {}
  ~USBDeviceManagerImplTest() override {}

 protected:
  DeviceManagerPtr ConnectToDeviceManager() {
    DeviceManagerPtr device_manager;
    DeviceManagerImpl::Create(permission_provider_.GetWeakPtr(),
                              mojo::GetProxy(&device_manager));
    return device_manager;
  }

  MockDeviceClient device_client_;

 private:
  MockPermissionProvider permission_provider_;
  std::unique_ptr<base::MessageLoop> message_loop_;
};

class MockDeviceManagerClient : public DeviceManagerClient {
 public:
  MockDeviceManagerClient() : m_binding(this) {}
  ~MockDeviceManagerClient() {}

  DeviceManagerClientPtr CreateInterfacePtrAndBind() {
    return m_binding.CreateInterfacePtrAndBind();
  }

  MOCK_METHOD1(DoOnDeviceAdded, void(DeviceInfo*));
  void OnDeviceAdded(DeviceInfoPtr device_info) {
    DoOnDeviceAdded(device_info.get());
  }

  MOCK_METHOD1(DoOnDeviceRemoved, void(DeviceInfo*));
  void OnDeviceRemoved(DeviceInfoPtr device_info) {
    DoOnDeviceRemoved(device_info.get());
  }

 private:
  mojo::Binding<DeviceManagerClient> m_binding;
};

void ExpectDevicesAndThen(const std::set<std::string>& expected_guids,
                          const base::Closure& continuation,
                          mojo::Array<DeviceInfoPtr> results) {
  EXPECT_EQ(expected_guids.size(), results.size());
  std::set<std::string> actual_guids;
  for (size_t i = 0; i < results.size(); ++i)
    actual_guids.insert(results[i]->guid);
  EXPECT_EQ(expected_guids, actual_guids);
  continuation.Run();
}

void ExpectDeviceInfoAndThen(const std::string& expected_guid,
                             const base::Closure& continuation,
                             DeviceInfoPtr device_info) {
  ASSERT_TRUE(device_info);
  EXPECT_EQ(expected_guid, device_info->guid);
  continuation.Run();
}

}  // namespace

// Test basic GetDevices functionality to ensure that all mock devices are
// returned by the service.
TEST_F(USBDeviceManagerImplTest, GetDevices) {
  scoped_refptr<MockUsbDevice> device0 =
      new MockUsbDevice(0x1234, 0x5678, "ACME", "Frobinator", "ABCDEF");
  scoped_refptr<MockUsbDevice> device1 =
      new MockUsbDevice(0x1234, 0x5679, "ACME", "Frobinator+", "GHIJKL");
  scoped_refptr<MockUsbDevice> device2 =
      new MockUsbDevice(0x1234, 0x567a, "ACME", "Frobinator Mk II", "MNOPQR");

  device_client_.usb_service()->AddDevice(device0);
  device_client_.usb_service()->AddDevice(device1);
  device_client_.usb_service()->AddDevice(device2);

  DeviceManagerPtr device_manager = ConnectToDeviceManager();

  EnumerationOptionsPtr options = EnumerationOptions::New();
  options->filters = mojo::Array<DeviceFilterPtr>::New(1);
  options->filters[0] = DeviceFilter::New();
  options->filters[0]->has_vendor_id = true;
  options->filters[0]->vendor_id = 0x1234;

  std::set<std::string> guids;
  guids.insert(device0->guid());
  guids.insert(device1->guid());
  guids.insert(device2->guid());

  base::RunLoop loop;
  device_manager->GetDevices(
      std::move(options),
      base::Bind(&ExpectDevicesAndThen, guids, loop.QuitClosure()));
  loop.Run();
}

// Test requesting a single Device by GUID.
TEST_F(USBDeviceManagerImplTest, GetDevice) {
  scoped_refptr<MockUsbDevice> mock_device =
      new MockUsbDevice(0x1234, 0x5678, "ACME", "Frobinator", "ABCDEF");

  device_client_.usb_service()->AddDevice(mock_device);

  DeviceManagerPtr device_manager = ConnectToDeviceManager();

  {
    base::RunLoop loop;
    DevicePtr device;
    device_manager->GetDevice(mock_device->guid(), mojo::GetProxy(&device));
    device->GetDeviceInfo(base::Bind(&ExpectDeviceInfoAndThen,
                                     mock_device->guid(), loop.QuitClosure()));
    loop.Run();
  }

  DevicePtr bad_device;
  device_manager->GetDevice("not a real guid", mojo::GetProxy(&bad_device));

  {
    base::RunLoop loop;
    bad_device.set_connection_error_handler(loop.QuitClosure());
    loop.Run();
  }
}

// Test requesting device enumeration updates with GetDeviceChanges.
TEST_F(USBDeviceManagerImplTest, Client) {
  scoped_refptr<MockUsbDevice> device0 =
      new MockUsbDevice(0x1234, 0x5678, "ACME", "Frobinator", "ABCDEF");
  scoped_refptr<MockUsbDevice> device1 =
      new MockUsbDevice(0x1234, 0x5679, "ACME", "Frobinator+", "GHIJKL");
  scoped_refptr<MockUsbDevice> device2 =
      new MockUsbDevice(0x1234, 0x567a, "ACME", "Frobinator Mk II", "MNOPQR");
  scoped_refptr<MockUsbDevice> device3 =
      new MockUsbDevice(0x1234, 0x567b, "ACME", "Frobinator Xtreme", "STUVWX");

  device_client_.usb_service()->AddDevice(device0);

  DeviceManagerPtr device_manager = ConnectToDeviceManager();
  MockDeviceManagerClient mock_client;
  device_manager->SetClient(mock_client.CreateInterfacePtrAndBind());

  {
    // Call GetDevices once to make sure the device manager is up and running
    // and the client is set or else we could block forever waiting for calls.
    std::set<std::string> guids;
    guids.insert(device0->guid());
    base::RunLoop loop;
    device_manager->GetDevices(
        nullptr, base::Bind(&ExpectDevicesAndThen, guids, loop.QuitClosure()));
    loop.Run();
  }

  device_client_.usb_service()->AddDevice(device1);
  device_client_.usb_service()->AddDevice(device2);
  device_client_.usb_service()->RemoveDevice(device1);
  device_client_.usb_service()->RemoveDevice(device0);
  device_client_.usb_service()->RemoveDevice(device2);
  device_client_.usb_service()->AddDevice(device3);

  {
    base::RunLoop loop;
    base::Closure barrier = base::BarrierClosure(6, loop.QuitClosure());
    testing::InSequence s;
    EXPECT_CALL(mock_client, DoOnDeviceAdded(_))
        .WillOnce(ExpectGuidAndThen(device1->guid(), barrier))
        .WillOnce(ExpectGuidAndThen(device2->guid(), barrier));
    EXPECT_CALL(mock_client, DoOnDeviceRemoved(_))
        .WillOnce(ExpectGuidAndThen(device1->guid(), barrier))
        .WillOnce(ExpectGuidAndThen(device0->guid(), barrier))
        .WillOnce(ExpectGuidAndThen(device2->guid(), barrier));
    EXPECT_CALL(mock_client, DoOnDeviceAdded(_))
        .WillOnce(ExpectGuidAndThen(device3->guid(), barrier));
    loop.Run();
  }
}

}  // namespace usb
}  // namespace device
