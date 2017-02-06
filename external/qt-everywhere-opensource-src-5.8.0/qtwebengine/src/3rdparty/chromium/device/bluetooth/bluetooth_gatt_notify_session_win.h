// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_GATT_NOTIFY_SESSION_WIN_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_GATT_NOTIFY_SESSION_WIN_H_

#include "device/bluetooth/bluetooth_gatt_notify_session.h"

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic_win.h"

namespace device {
class DEVICE_BLUETOOTH_EXPORT BluetoothGattNotifySessionWin
    : public BluetoothGattNotifySession {
 public:
  BluetoothGattNotifySessionWin(
      base::WeakPtr<BluetoothRemoteGattCharacteristicWin> characteristic);
  ~BluetoothGattNotifySessionWin() override;

  // Override BluetoothGattNotifySession interfaces.
  std::string GetCharacteristicIdentifier() const override;
  bool IsActive() override;
  void Stop(const base::Closure& callback) override;

 private:
  base::WeakPtr<BluetoothRemoteGattCharacteristicWin> characteristic_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothGattNotifySessionWin);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_GATT_NOTIFY_SESSION_WIN_H_
