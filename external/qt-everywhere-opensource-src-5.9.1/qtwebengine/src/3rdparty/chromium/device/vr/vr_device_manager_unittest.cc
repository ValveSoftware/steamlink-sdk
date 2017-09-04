// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/vr_device_manager.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ptr_util.h"
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

  VRDevice* GetDevice(unsigned int index) {
    return device_manager_->GetDevice(index);
  }

 protected:
  FakeVRDeviceProvider* provider_ = nullptr;
  std::unique_ptr<VRDeviceManager> device_manager_;
  std::unique_ptr<VRServiceImpl> vr_service_;

  DISALLOW_COPY_AND_ASSIGN(VRDeviceManagerTest);
};

VRDeviceManagerTest::VRDeviceManagerTest() {}

VRDeviceManagerTest::~VRDeviceManagerTest() {}

void VRDeviceManagerTest::SetUp() {
  provider_ = new FakeVRDeviceProvider();
  device_manager_.reset(new VRDeviceManager(base::WrapUnique(provider_)));
  vr_service_.reset(new VRServiceImpl());
}

TEST_F(VRDeviceManagerTest, InitializationTest) {
  EXPECT_FALSE(provider_->IsInitialized());

  // Calling GetDevices should initialize the service if it hasn't been
  // initialized yet or the providesr have been released.
  // The mojom::VRService should initialize each of it's providers upon it's own
  // initialization.
  device_manager_->GetVRDevices(vr_service_.get());
  EXPECT_TRUE(provider_->IsInitialized());
}

TEST_F(VRDeviceManagerTest, GetDevicesBasicTest) {
  bool success = device_manager_->GetVRDevices(vr_service_.get());
  // Calling GetVRDevices should initialize the providers.
  EXPECT_TRUE(provider_->IsInitialized());
  EXPECT_FALSE(success);

  // GetDeviceByIndex should return nullptr if an invalid index in queried.
  VRDevice* queried_device = GetDevice(1);
  EXPECT_EQ(nullptr, queried_device);

  FakeVRDevice* device1 = new FakeVRDevice();
  provider_->AddDevice(base::WrapUnique(device1));
  success = device_manager_->GetVRDevices(vr_service_.get());
  EXPECT_TRUE(success);
  // Should have successfully returned one device.
  EXPECT_EQ(device1, GetDevice(device1->id()));

  FakeVRDevice* device2 = new FakeVRDevice();
  provider_->AddDevice(base::WrapUnique(device2));
  success = device_manager_->GetVRDevices(vr_service_.get());
  EXPECT_TRUE(success);

  // Querying the WebVRDevice index should return the correct device.
  EXPECT_EQ(device1, GetDevice(device1->id()));
  EXPECT_EQ(device2, GetDevice(device2->id()));
}

}  // namespace device
