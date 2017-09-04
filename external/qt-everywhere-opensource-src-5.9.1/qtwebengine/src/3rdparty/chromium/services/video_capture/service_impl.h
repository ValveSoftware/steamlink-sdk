// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_SERVICE_H_
#define SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_SERVICE_H_

#include <memory>

#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/video_capture/public/interfaces/service.mojom.h"

namespace video_capture {

class DeviceFactoryMediaToMojoAdapter;
class MockDeviceFactory;

// Implementation of video_capture::mojom::Service as a Service Manager service.
class ServiceImpl : public service_manager::Service,
                    public service_manager::InterfaceFactory<mojom::Service>,
                    public mojom::Service {
 public:
  ServiceImpl();
  ~ServiceImpl() override;

  // service_manager::Service:
  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) override;

  // service_manager::InterfaceFactory<mojom::VideoCaptureService>:
  void Create(const service_manager::Identity& remote_identity,
              mojom::ServiceRequest request) override;

  // video_capture::mojom::Service
  void ConnectToDeviceFactory(mojom::DeviceFactoryRequest request) override;
  void ConnectToFakeDeviceFactory(mojom::DeviceFactoryRequest request) override;
  void ConnectToMockDeviceFactory(mojom::DeviceFactoryRequest request) override;
  void AddDeviceToMockFactory(
      mojom::MockMediaDevicePtr device,
      const media::VideoCaptureDeviceDescriptor& descriptor,
      const AddDeviceToMockFactoryCallback& callback) override;

 private:
  void LazyInitializeDeviceFactory();
  void LazyInitializeFakeDeviceFactory();
  void LazyInitializeMockDeviceFactory();

  mojo::BindingSet<mojom::Service> service_bindings_;
  mojo::BindingSet<mojom::DeviceFactory> factory_bindings_;
  mojo::BindingSet<mojom::DeviceFactory> fake_factory_bindings_;
  mojo::BindingSet<mojom::DeviceFactory> mock_factory_bindings_;
  std::unique_ptr<DeviceFactoryMediaToMojoAdapter> device_factory_;
  std::unique_ptr<DeviceFactoryMediaToMojoAdapter> fake_device_factory_;
  std::unique_ptr<DeviceFactoryMediaToMojoAdapter> mock_device_factory_adapter_;
  MockDeviceFactory* mock_device_factory_;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_SERVICE_H_
