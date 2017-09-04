// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "device/bluetooth/device.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace bluetooth {

Device::Device(const std::string& address,
               scoped_refptr<device::BluetoothAdapter> adapter)
    : address_(address), adapter_(std::move(adapter)) {}

Device::~Device() {}

// static
mojom::DeviceInfoPtr Device::ConstructDeviceInfoStruct(
    const device::BluetoothDevice* device) {
  mojom::DeviceInfoPtr device_info = mojom::DeviceInfo::New();

  device_info->name = device->GetName();
  device_info->name_for_display =
      base::UTF16ToUTF8(device->GetNameForDisplay());
  device_info->address = device->GetAddress();

  if (device->GetInquiryRSSI()) {
    device_info->rssi = mojom::RSSIWrapper::New();
    device_info->rssi->value = device->GetInquiryRSSI().value();
  }

  return device_info;
}

void Device::GetInfo(const GetInfoCallback& callback) {
  device::BluetoothDevice* device = adapter_->GetDevice(address_);
  if (device) {
    mojom::DeviceInfoPtr device_info = ConstructDeviceInfoStruct(device);
    callback.Run(std::move(device_info));
  } else {
    callback.Run(nullptr);
  }
}

}  // namespace bluetooth
