// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_GATT_NOTIFY_SESSION_ANDROID_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_GATT_NOTIFY_SESSION_ANDROID_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "device/bluetooth/bluetooth_gatt_notify_session.h"

namespace device {

class BluetoothAdapter;
class BluetoothRemoteGattCharacteristicAndroid;

// BluetoothGattNotifySessionAndroid implements
// BluetoothGattNotifySession for the Android platform.
//
// TODO(crbug.com/551634): Detect destroyed Characteristic or parents objects.
// TODO(crbug.com/551634): Implement Stop.
class DEVICE_BLUETOOTH_EXPORT BluetoothGattNotifySessionAndroid
    : public device::BluetoothGattNotifySession {
 public:
  explicit BluetoothGattNotifySessionAndroid(
      const std::string& characteristic_identifier);
  ~BluetoothGattNotifySessionAndroid() override;

  // BluetoothGattNotifySession overrides.
  std::string GetCharacteristicIdentifier() const override;
  bool IsActive() override;
  void Stop(const base::Closure& callback) override;

 private:
  // Identifier of the associated characteristic.
  std::string characteristic_id_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothGattNotifySessionAndroid);
};

}  // namespace device

#endif  //  DEVICE_BLUETOOTH_BLUETOOTH_GATT_NOTIFY_SESSION_ANDROID_H_
