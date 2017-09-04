// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "device/vr/vr_device.h"
#include "device/vr/vr_service_impl.h"

namespace device {

VRDisplayImpl::VRDisplayImpl(device::VRDevice* device, VRServiceImpl* service)
    : binding_(this),
      device_(device),
      service_(service),
      weak_ptr_factory_(this) {
  mojom::VRDisplayInfoPtr display_info = device->GetVRDevice();
  // Client might be null in unittest.
  // TODO: setup a mock client in unittest too?
  if (service->client()) {
    service->client()->OnDisplayConnected(binding_.CreateInterfacePtrAndBind(),
                                          mojo::GetProxy(&client_),
                                          std::move(display_info));
  }
}

VRDisplayImpl::~VRDisplayImpl() {}

void VRDisplayImpl::GetPose(const GetPoseCallback& callback) {
  if (!device_->IsAccessAllowed(service_)) {
    callback.Run(nullptr);
    return;
  }

  callback.Run(device_->GetPose());
}

void VRDisplayImpl::ResetPose() {
  if (!device_->IsAccessAllowed(service_))
    return;

  device_->ResetPose();
}

void VRDisplayImpl::RequestPresent(bool secure_origin,
                                   const RequestPresentCallback& callback) {
  if (!device_->IsAccessAllowed(service_)) {
    callback.Run(false);
    return;
  }

  device_->RequestPresent(base::Bind(&VRDisplayImpl::RequestPresentResult,
                                     weak_ptr_factory_.GetWeakPtr(), callback,
                                     secure_origin));
}

void VRDisplayImpl::RequestPresentResult(const RequestPresentCallback& callback,
                                         bool secure_origin,
                                         bool success) {
  if (success) {
    device_->SetPresentingService(service_);
    device_->SetSecureOrigin(secure_origin);
  }
  callback.Run(success);
}

void VRDisplayImpl::ExitPresent() {
  if (device_->IsPresentingService(service_))
    device_->ExitPresent();
}

void VRDisplayImpl::SubmitFrame(mojom::VRPosePtr pose) {
  if (!device_->IsPresentingService(service_))
    return;
  device_->SubmitFrame(std::move(pose));
}

void VRDisplayImpl::UpdateLayerBounds(mojom::VRLayerBoundsPtr left_bounds,
                                      mojom::VRLayerBoundsPtr right_bounds) {
  if (!device_->IsAccessAllowed(service_))
    return;

  device_->UpdateLayerBounds(std::move(left_bounds), std::move(right_bounds));
}
}
