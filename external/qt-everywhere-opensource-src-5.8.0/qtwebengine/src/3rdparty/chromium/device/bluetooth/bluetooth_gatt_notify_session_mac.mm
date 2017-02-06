// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_gatt_notify_session_mac.h"

namespace device {
BluetoothGattNotifySessionMac::BluetoothGattNotifySessionMac(
    base::WeakPtr<BluetoothRemoteGattCharacteristicMac> characteristic)
    : characteristic_(characteristic) {}

BluetoothGattNotifySessionMac::~BluetoothGattNotifySessionMac() {}

std::string BluetoothGattNotifySessionMac::GetCharacteristicIdentifier() const {
  if (characteristic_.get())
    return characteristic_->GetIdentifier();
  return std::string();
}

bool BluetoothGattNotifySessionMac::IsActive() {
  return characteristic_.get() != nullptr;
}

void BluetoothGattNotifySessionMac::Stop(const base::Closure& callback) {
  NOTIMPLEMENTED();
}

}  // namespace device
