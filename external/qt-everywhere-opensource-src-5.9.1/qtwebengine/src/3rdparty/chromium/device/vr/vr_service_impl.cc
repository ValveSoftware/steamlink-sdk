// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/vr_service_impl.h"

#include <utility>

#include "base/bind.h"
#include "device/vr/vr_device.h"
#include "device/vr/vr_device_manager.h"

namespace device {

VRServiceImpl::VRServiceImpl() : listening_for_activate_(false) {}

VRServiceImpl::~VRServiceImpl() {
  RemoveFromDeviceManager();
}

void VRServiceImpl::BindRequest(
    mojo::InterfaceRequest<mojom::VRService> request) {
  VRServiceImpl* service = new VRServiceImpl();
  service->Bind(std::move(request));
}

// Gets a VRDisplayPtr unique to this service so that the associated page can
// communicate with the VRDevice.
VRDisplayImpl* VRServiceImpl::GetVRDisplayImpl(VRDevice* device) {
  DisplayImplMap::iterator it = displays_.find(device);
  if (it != displays_.end())
    return it->second.get();

  VRDisplayImpl* display_impl = new VRDisplayImpl(device, this);
  displays_[device] = base::WrapUnique(display_impl);
  return display_impl;
}

void VRServiceImpl::Bind(mojo::InterfaceRequest<mojom::VRService> request) {
  // TODO(shaobo.yan@intel.com) : Keep one binding_ and use close and rebind.
  binding_.reset(new mojo::Binding<mojom::VRService>(this, std::move(request)));
  binding_->set_connection_error_handler(base::Bind(
      &VRServiceImpl::RemoveFromDeviceManager, base::Unretained(this)));
}

void VRServiceImpl::RemoveFromDeviceManager() {
  displays_.clear();
  VRDeviceManager* device_manager = VRDeviceManager::GetInstance();
  device_manager->RemoveService(this);
}

void VRServiceImpl::RemoveDevice(VRDevice* device) {
  displays_.erase(device);
}

void VRServiceImpl::SetClient(mojom::VRServiceClientPtr service_client,
                              const SetClientCallback& callback) {
  DCHECK(!client_.get());

  client_ = std::move(service_client);
  VRDeviceManager* device_manager = VRDeviceManager::GetInstance();
  // Once a client has been connected AddService will force any VRDisplays to
  // send OnConnected to it so that it's populated with the currently active
  // displays. Thereafer it will stay up to date by virtue of listening for new
  // connected events.
  device_manager->AddService(this);
  callback.Run(device_manager->GetNumberOfConnectedDevices());
}

void VRServiceImpl::SetListeningForActivate(bool listening) {
  listening_for_activate_ = listening;
  VRDeviceManager* device_manager = VRDeviceManager::GetInstance();
  device_manager->ListeningForActivateChanged(listening);
}

}  // namespace device
