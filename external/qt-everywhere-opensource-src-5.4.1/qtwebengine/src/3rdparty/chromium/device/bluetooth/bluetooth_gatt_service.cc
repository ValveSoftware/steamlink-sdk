// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_gatt_service.h"

#include "base/logging.h"

namespace device {

BluetoothGattService::BluetoothGattService() {
}

BluetoothGattService::~BluetoothGattService() {
}

// static
BluetoothGattService* BluetoothGattService::Create(
    const BluetoothUUID& uuid,
    bool is_primary,
    Delegate* delegate) {
  LOG(ERROR) << "Creating local GATT services currently not supported.";
  return NULL;
}

}  // namespace device
