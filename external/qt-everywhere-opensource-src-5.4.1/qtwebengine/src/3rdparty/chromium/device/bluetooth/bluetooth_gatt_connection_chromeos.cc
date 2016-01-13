// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_gatt_connection_chromeos.h"

#include "base/bind.h"
#include "base/logging.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"

namespace chromeos {

BluetoothGattConnectionChromeOS::BluetoothGattConnectionChromeOS(
    scoped_refptr<device::BluetoothAdapter> adapter,
    const std::string& device_address,
    const dbus::ObjectPath& object_path)
    : connected_(true),
      adapter_(adapter),
      device_address_(device_address),
      object_path_(object_path) {
  DCHECK(adapter_.get());
  DCHECK(!device_address_.empty());
  DCHECK(object_path_.IsValid());

  DBusThreadManager::Get()->GetBluetoothDeviceClient()->AddObserver(this);
}

BluetoothGattConnectionChromeOS::~BluetoothGattConnectionChromeOS() {
  DBusThreadManager::Get()->GetBluetoothDeviceClient()->RemoveObserver(this);
  Disconnect(base::Bind(&base::DoNothing));
}

std::string BluetoothGattConnectionChromeOS::GetDeviceAddress() const {
  return device_address_;
}

bool BluetoothGattConnectionChromeOS::IsConnected() {
  // Lazily determine the activity state of the connection. If already
  // marked as inactive, then return false. Otherwise, explicitly mark
  // |connected_| as false if the device is removed or disconnected. We do this,
  // so that if this method is called during a call to DeviceRemoved or
  // DeviceChanged somewhere else, it returns the correct status.
  if (!connected_)
    return false;

  BluetoothDeviceClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothDeviceClient()->
          GetProperties(object_path_);
  if (!properties || !properties->connected.value())
    connected_ = false;

  return connected_;
}

void BluetoothGattConnectionChromeOS::Disconnect(
    const base::Closure& callback) {
  if (!connected_) {
    VLOG(1) << "Connection already inactive.";
    callback.Run();
    return;
  }

  // TODO(armansito): There isn't currently a good way to manage the ownership
  // of a connection between Chrome and bluetoothd plugins/profiles. Until
  // a proper reference count is kept by bluetoothd, we might unwittingly kill
  // a connection that is managed by the daemon (e.g. HoG). For now, just return
  // success to indicate that this BluetoothGattConnection is no longer active,
  // even though the underlying connection won't actually be disconnected. This
  // technically doesn't violate the contract put forth by this API.
  connected_ = false;
  callback.Run();
}

void BluetoothGattConnectionChromeOS::DeviceRemoved(
    const dbus::ObjectPath& object_path) {
  if (object_path != object_path_)
    return;

  connected_ = false;
}

void BluetoothGattConnectionChromeOS::DevicePropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  if (object_path != object_path_)
    return;

  if (!connected_)
    return;

  BluetoothDeviceClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothDeviceClient()->
          GetProperties(object_path_);

  if (!properties) {
    connected_ = false;
    return;
  }

  if (property_name == properties->connected.name() &&
      !properties->connected.value())
    connected_ = false;
}

}  // namespace chromeos
