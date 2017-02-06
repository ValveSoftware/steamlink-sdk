// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_local_gatt_service.h"

namespace device {

#if !defined(OS_CHROMEOS) && !defined(OS_LINUX)
// static
base::WeakPtr<BluetoothLocalGattService> BluetoothLocalGattService::Create(
    BluetoothAdapter* adapter,
    const BluetoothUUID& uuid,
    bool is_primary,
    BluetoothLocalGattService* included_service,
    BluetoothLocalGattService::Delegate* delegate) {
  NOTIMPLEMENTED();
  return nullptr;
}
#endif

BluetoothLocalGattService::BluetoothLocalGattService() {}

BluetoothLocalGattService::~BluetoothLocalGattService() {}

}  // namespace device
