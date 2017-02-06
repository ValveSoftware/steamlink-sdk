// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_GATT_NOTIFY_SESSION_BLUEZ_H_
#define DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_GATT_NOTIFY_SESSION_BLUEZ_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "device/bluetooth/bluetooth_gatt_notify_session.h"
#include "device/bluetooth/dbus/bluetooth_gatt_characteristic_client.h"

namespace device {

class BluetoothAdapter;

}  // namespace device

namespace bluez {

class BluetoothRemoteGattCharacteristicBlueZ;

// BluetoothGattNotifySessionBlueZ implements
// BluetoothGattNotifySession for platforms that use BlueZ.
class BluetoothGattNotifySessionBlueZ
    : public device::BluetoothGattNotifySession,
      public bluez::BluetoothGattCharacteristicClient::Observer {
 public:
  ~BluetoothGattNotifySessionBlueZ() override;

  // BluetoothGattNotifySession overrides.
  std::string GetCharacteristicIdentifier() const override;
  bool IsActive() override;
  void Stop(const base::Closure& callback) override;

 private:
  friend class BluetoothRemoteGattCharacteristicBlueZ;

  explicit BluetoothGattNotifySessionBlueZ(
      scoped_refptr<device::BluetoothAdapter> adapter,
      const std::string& device_address,
      const std::string& service_identifier,
      const std::string& characteristic_identifier,
      const dbus::ObjectPath& characteristic_path);

  // bluez::BluetoothGattCharacteristicClient::Observer overrides.
  void GattCharacteristicRemoved(const dbus::ObjectPath& object_path) override;
  void GattCharacteristicPropertyChanged(
      const dbus::ObjectPath& object_path,
      const std::string& property_name) override;

  // True, if this session is currently active.
  bool active_;

  // The Bluetooth adapter that this session is associated with.
  scoped_refptr<device::BluetoothAdapter> adapter_;

  // The Bluetooth address of the device hosting the characteristic.
  std::string device_address_;

  // The GATT service that the characteristic belongs to.
  std::string service_id_;

  // Identifier of the associated characteristic.
  std::string characteristic_id_;

  // D-Bus object path of the associated characteristic. This is used to filter
  // observer events.
  dbus::ObjectPath object_path_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothGattNotifySessionBlueZ);
};

}  // namespace bluez

#endif  //  DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_GATT_NOTIFY_SESSION_BLUEZ_H_
