// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_gatt_notify_session_win.h"

namespace device {
BluetoothGattNotifySessionWin::BluetoothGattNotifySessionWin(
    base::WeakPtr<BluetoothRemoteGattCharacteristicWin> characteristic)
    : characteristic_(characteristic) {}

BluetoothGattNotifySessionWin::~BluetoothGattNotifySessionWin() {}

std::string BluetoothGattNotifySessionWin::GetCharacteristicIdentifier() const {
  if (characteristic_.get())
    return characteristic_->GetIdentifier();
  return std::string();
}

bool BluetoothGattNotifySessionWin::IsActive() {
  return characteristic_.get() != nullptr;
}

void BluetoothGattNotifySessionWin::Stop(const base::Closure& callback) {
  NOTIMPLEMENTED();
}

}  // namespace device
