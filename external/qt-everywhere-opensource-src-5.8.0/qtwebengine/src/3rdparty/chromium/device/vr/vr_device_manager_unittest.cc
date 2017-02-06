// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/vr_device_manager.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "device/vr/test/fake_vr_device.h"
#include "device/vr/test/fake_vr_device_provider.h"
#include "device/vr/vr_device_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class VRDeviceManagerTest : public testing::Test {
 protected:
  VRDeviceManagerTest();
  ~VRDeviceManagerTest() override;

  void SetUp() override;

  bool HasServiceInstance() { return VRDeviceManager::HasInstance(); }

 protected:
  FakeVRDeviceProvider* provider_;
  std::unique_ptr<VRDeviceManager> device_manager_;

  DISALLOW_COPY_AND_ASSIGN(VRDeviceManagerTest);
};

VRDeviceManagerTest::VRDeviceManagerTest() {}

VRDeviceManagerTest::~VRDeviceManagerTest() {}

void VRDeviceManagerTest::SetUp() {
  std::unique_ptr<FakeVRDeviceProvider> provider(new FakeVRDeviceProvider());
  provider_ = provider.get();
  device_manager_.reset(new VRDeviceManager(std::move(provider)));
}

TEST_F(VRDeviceManagerTest, InitializationTest) {
  EXPECT_FALSE(provider_->IsInitialized());

  // Calling GetDevices should initialize the service if it hasn't been
  // initialized yet or the providesr have been released.
  // The mojom::VRService should initialize each of it's providers upon it's own
  // initialization.
  mojo::Array<VRDisplayPtr> webvr_devices;
  webvr_devices = device_manager_->GetVRDevices();
  EXPECT_TRUE(provider_->IsInitialized());
}

TEST_F(VRDeviceManagerTest, GetDevicesBasicTest) {
  mojo::Array<VRDisplayPtr> webvr_devices;
  webvr_devices = device_manager_->GetVRDevices();
  // Calling GetVRDevices should initialize the providers.
  EXPECT_TRUE(provider_->IsInitialized());
  // Should successfully return zero devices when none are available.
  EXPECT_EQ(0u, webvr_devices.size());

  // GetDeviceByIndex should return nullptr if an invalid index in queried.
  VRDevice* queried_device = device_manager_->GetDevice(1);
  EXPECT_EQ(nullptr, queried_device);

  std::unique_ptr<FakeVRDevice> device1(new FakeVRDevice(provider_));
  provider_->AddDevice(device1.get());
  webvr_devices = device_manager_->GetVRDevices();
  // Should have successfully returned one device.
  EXPECT_EQ(1u, webvr_devices.size());
  // The WebVRDevice index should match the device id.
  EXPECT_EQ(webvr_devices[0]->index, device1->id());

  std::unique_ptr<FakeVRDevice> device2(new FakeVRDevice(provider_));
  provider_->AddDevice(device2.get());
  webvr_devices = device_manager_->GetVRDevices();
  // Should have successfully returned two devices.
  EXPECT_EQ(2u, webvr_devices.size());
  // NOTE: Returned WebVRDevices are not required to be in any particular order.

  // Querying the WebVRDevice index should return the correct device.
  queried_device = device_manager_->GetDevice(device1->id());
  EXPECT_EQ(device1.get(), queried_device);
  queried_device = device_manager_->GetDevice(device2->id());
  EXPECT_EQ(device2.get(), queried_device);

  provider_->RemoveDevice(device1.get());
  webvr_devices = device_manager_->GetVRDevices();
  // Should have successfully returned one device.
  EXPECT_EQ(1u, webvr_devices.size());
  // The WebVRDevice index should match the only remaining device id.
  EXPECT_EQ(webvr_devices[0]->index, device2->id());
}

}  // namespace device
