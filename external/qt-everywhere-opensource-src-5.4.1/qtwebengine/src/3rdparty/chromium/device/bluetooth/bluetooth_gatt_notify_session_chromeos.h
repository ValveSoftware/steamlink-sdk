// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_GATT_NOTIFY_SESSION_CHROMEOS_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_GATT_NOTIFY_SESSION_CHROMEOS_H_

#include <string>

#include "base/callback.h"
#include "chromeos/dbus/bluetooth_gatt_characteristic_client.h"
#include "device/bluetooth/bluetooth_gatt_notify_session.h"

namespace device {

class BluetoothAdapter;

}  // namespace device

namespace chromeos {

class BluetoothRemoteGattCharacteristicChromeOS;

// BluetoothGattNotifySessionChromeOS implements
// BluetoothGattNotifySession for the Chrome OS platform.
class BluetoothGattNotifySessionChromeOS
    : public device::BluetoothGattNotifySession,
      public BluetoothGattCharacteristicClient::Observer {
 public:
  virtual ~BluetoothGattNotifySessionChromeOS();

  // BluetoothGattNotifySession overrides.
  virtual std::string GetCharacteristicIdentifier() const OVERRIDE;
  virtual bool IsActive() OVERRIDE;
  virtual void Stop(const base::Closure& callback) OVERRIDE;

 private:
  friend class BluetoothRemoteGattCharacteristicChromeOS;

  explicit BluetoothGattNotifySessionChromeOS(
      scoped_refptr<device::BluetoothAdapter> adapter,
      const std::string& device_address,
      const std::string& service_identifier,
      const std::string& characteristic_identifier,
      const dbus::ObjectPath& characteristic_path);

  // BluetoothGattCharacteristicClient::Observer overrides.
  virtual void GattCharacteristicRemoved(
      const dbus::ObjectPath& object_path) OVERRIDE;
  virtual void GattCharacteristicPropertyChanged(
      const dbus::ObjectPath& object_path,
      const std::string& property_name) OVERRIDE;

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

  DISALLOW_COPY_AND_ASSIGN(BluetoothGattNotifySessionChromeOS);
};

}  // namespace chromeos

#endif  //  DEVICE_BLUETOOTH_BLUETOOTH_GATT_NOTIFY_SESSION_CHROMEOS_H_
