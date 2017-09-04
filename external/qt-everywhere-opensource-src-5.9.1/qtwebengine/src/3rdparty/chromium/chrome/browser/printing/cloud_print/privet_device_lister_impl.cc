// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/cloud_print/privet_device_lister_impl.h"

#include <utility>

#include "chrome/browser/printing/cloud_print/privet_constants.h"

namespace cloud_print {

PrivetDeviceListerImpl::PrivetDeviceListerImpl(
    local_discovery::ServiceDiscoveryClient* service_discovery_client,
    PrivetDeviceLister::Delegate* delegate)
    : delegate_(delegate),
      device_lister_(this, service_discovery_client, kPrivetDefaultDeviceType) {
}

PrivetDeviceListerImpl::~PrivetDeviceListerImpl() {
}

void PrivetDeviceListerImpl::Start() {
  device_lister_.Start();
}

void PrivetDeviceListerImpl::DiscoverNewDevices(bool force_update) {
  device_lister_.DiscoverNewDevices(force_update);
}

void PrivetDeviceListerImpl::OnDeviceChanged(
    bool added,
    const local_discovery::ServiceDescription& service_description) {
  if (!delegate_)
    return;

  delegate_->DeviceChanged(service_description.service_name,
                           DeviceDescription(service_description));
}

void PrivetDeviceListerImpl::OnDeviceRemoved(const std::string& service_name) {
  if (delegate_)
    delegate_->DeviceRemoved(service_name);
}

void PrivetDeviceListerImpl::OnDeviceCacheFlushed() {
  if (delegate_)
    delegate_->DeviceCacheFlushed();
}

}  // namespace cloud_print
