// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/vr_device.h"
#include "device/vr/vr_device_provider.h"
#include "device/vr/vr_service_impl.h"

namespace device {

unsigned int VRDevice::next_id_ = 1;

VRDevice::VRDevice() : presenting_service_(nullptr), id_(next_id_) {
  // Prevent wraparound. Devices with this ID will be treated as invalid.
  if (next_id_ != VR_DEVICE_LAST_ID)
    next_id_++;
}

VRDevice::~VRDevice() {}

void VRDevice::RequestPresent(const base::Callback<void(bool)>& callback) {
  callback.Run(true);
}

void VRDevice::SetSecureOrigin(bool secure_origin) {}

void VRDevice::AddService(VRServiceImpl* service) {
  // Create a VRDisplayImpl for this service/device pair
  VRDisplayImpl* display_impl = service->GetVRDisplayImpl(this);
  displays_.insert(std::make_pair(service, display_impl));
}

void VRDevice::RemoveService(VRServiceImpl* service) {
  displays_.erase(service);
  if (IsPresentingService(service))
    ExitPresent();
}

bool VRDevice::IsAccessAllowed(VRServiceImpl* service) {
  return (!presenting_service_ || presenting_service_ == service);
}

bool VRDevice::IsPresentingService(VRServiceImpl* service) {
  return (presenting_service_ && presenting_service_ == service);
}

void VRDevice::OnChanged() {
  mojom::VRDisplayInfoPtr vr_device_info = GetVRDevice();
  if (vr_device_info.is_null())
    return;

  for (const auto& display : displays_)
    display.second->client()->OnChanged(vr_device_info.Clone());
}

void VRDevice::OnExitPresent() {
  DisplayClientMap::iterator it = displays_.find(presenting_service_);
  if (it != displays_.end())
    it->second->client()->OnExitPresent();

  SetPresentingService(nullptr);
}

void VRDevice::OnBlur() {
  for (const auto& display : displays_)
    display.second->client()->OnBlur();
}

void VRDevice::OnFocus() {
  for (const auto& display : displays_)
    display.second->client()->OnFocus();
}

void VRDevice::OnActivate(mojom::VRDisplayEventReason reason) {
  for (const auto& display : displays_)
    display.second->client()->OnActivate(reason);
}

void VRDevice::OnDeactivate(mojom::VRDisplayEventReason reason) {
  for (const auto& display : displays_)
    display.second->client()->OnDeactivate(reason);
}

void VRDevice::SetPresentingService(VRServiceImpl* service) {
  presenting_service_ = service;
}

}  // namespace device
