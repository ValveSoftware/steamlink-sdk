// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/vr_service_impl.h"

#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "device/vr/test/fake_vr_device.h"
#include "device/vr/test/fake_vr_device_provider.h"
#include "device/vr/vr_device_manager.h"
#include "device/vr/vr_service.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::Mock;

namespace device {

// TODO(shaobo.yan@intel.com) : Update the whole unittest.
class MockVRServiceClient : public mojom::VRServiceClient {};

class VRServiceTestBinding {
 public:
  VRServiceTestBinding() {
    auto request = mojo::GetProxy(&service_ptr_);
    service_impl_.reset(new VRServiceImpl());
    service_impl_->Bind(std::move(request));

    client_binding_.reset(new mojo::Binding<mojom::VRServiceClient>(
        mock_client_, mojo::GetProxy(&client_ptr_)));
  }

  void SetClient() {
    service_impl_->SetClient(
        std::move(client_ptr_),
        base::Bind(&device::VRServiceTestBinding::SetNumberOfDevices,
                   base::Unretained(this)));
  }

  void SetNumberOfDevices(unsigned int number_of_devices) {
    number_of_devices_ = number_of_devices;
  }

  void Close() {
    service_ptr_.reset();
    service_impl_.reset();
  }

  MockVRServiceClient* client() { return mock_client_; }
  VRServiceImpl* service() { return service_impl_.get(); }

 private:
  mojom::VRServiceClientPtr client_ptr_;
  std::unique_ptr<VRServiceImpl> service_impl_;
  mojo::InterfacePtr<mojom::VRService> service_ptr_;

  MockVRServiceClient* mock_client_;
  std::unique_ptr<mojo::Binding<mojom::VRServiceClient>> client_binding_;
  unsigned int number_of_devices_;

  DISALLOW_COPY_AND_ASSIGN(VRServiceTestBinding);
};

class VRServiceImplTest : public testing::Test {
 public:
  VRServiceImplTest() {}
  ~VRServiceImplTest() override {}

 protected:
  void SetUp() override {
    provider_ = new FakeVRDeviceProvider();
    device_manager_.reset(new VRDeviceManager(base::WrapUnique(provider_)));
  }

  void TearDown() override { base::RunLoop().RunUntilIdle(); }

  std::unique_ptr<VRServiceTestBinding> BindService() {
    auto test_binding = base::WrapUnique(new VRServiceTestBinding());
    test_binding->SetClient();
    return test_binding;
  }

  size_t ServiceCount() { return device_manager_->services_.size(); }

  base::MessageLoop message_loop_;
  FakeVRDeviceProvider* provider_ = nullptr;
  std::unique_ptr<VRDeviceManager> device_manager_;

  DISALLOW_COPY_AND_ASSIGN(VRServiceImplTest);
};

// Ensure that services are registered with the device manager as they are
// created and removed from the device manager as their connections are closed.
TEST_F(VRServiceImplTest, DeviceManagerRegistration) {
  EXPECT_EQ(0u, ServiceCount());

  std::unique_ptr<VRServiceTestBinding> service_1 = BindService();

  EXPECT_EQ(1u, ServiceCount());

  std::unique_ptr<VRServiceTestBinding> service_2 = BindService();

  EXPECT_EQ(2u, ServiceCount());

  service_1->Close();

  EXPECT_EQ(1u, ServiceCount());

  service_2->Close();

  EXPECT_EQ(0u, ServiceCount());
}

}
