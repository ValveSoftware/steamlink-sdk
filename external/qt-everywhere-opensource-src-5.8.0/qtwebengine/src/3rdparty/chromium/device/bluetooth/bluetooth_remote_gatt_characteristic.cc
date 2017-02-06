// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"

#include "device/bluetooth/bluetooth_remote_gatt_descriptor.h"

namespace device {

BluetoothRemoteGattCharacteristic::BluetoothRemoteGattCharacteristic() {}

BluetoothRemoteGattCharacteristic::~BluetoothRemoteGattCharacteristic() {}

std::vector<BluetoothRemoteGattDescriptor*>
BluetoothRemoteGattCharacteristic::GetDescriptorsByUUID(
    const BluetoothUUID& uuid) {
  std::vector<BluetoothRemoteGattDescriptor*> descriptors;
  for (BluetoothRemoteGattDescriptor* descriptor : GetDescriptors()) {
    if (descriptor->GetUUID() == uuid) {
      descriptors.push_back(descriptor);
    }
  }
  return descriptors;
}

}  // namespace device
