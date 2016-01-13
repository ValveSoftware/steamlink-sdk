// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_gatt_notify_session_chromeos.h"

#include "base/bind.h"
#include "base/logging.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_gatt_service.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic_chromeos.h"

namespace chromeos {

BluetoothGattNotifySessionChromeOS::BluetoothGattNotifySessionChromeOS(
    scoped_refptr<device::BluetoothAdapter> adapter,
    const std::string& device_address,
    const std::string& service_identifier,
    const std::string& characteristic_identifier,
    const dbus::ObjectPath& characteristic_path)
    : active_(true),
      adapter_(adapter),
      device_address_(device_address),
      service_id_(service_identifier),
      characteristic_id_(characteristic_identifier),
      object_path_(characteristic_path) {
  DCHECK(adapter_.get());
  DCHECK(!device_address_.empty());
  DCHECK(!service_id_.empty());
  DCHECK(!characteristic_id_.empty());
  DCHECK(object_path_.IsValid());

  DBusThreadManager::Get()->GetBluetoothGattCharacteristicClient()->AddObserver(
      this);
}

BluetoothGattNotifySessionChromeOS::~BluetoothGattNotifySessionChromeOS() {
  DBusThreadManager::Get()
      ->GetBluetoothGattCharacteristicClient()
      ->RemoveObserver(this);
  Stop(base::Bind(&base::DoNothing));
}

std::string BluetoothGattNotifySessionChromeOS::GetCharacteristicIdentifier()
    const {
  return characteristic_id_;
}

bool BluetoothGattNotifySessionChromeOS::IsActive() {
  // Determine if the session is active. If |active_| is false, then it's
  // been explicitly marked, so return false.
  if (!active_)
    return false;

  // The fact that |active_| is true doesn't mean that the session is
  // actually active, since the characteristic might have stopped sending
  // notifications yet this method was called before we processed the
  // observer event (e.g. because somebody else called this method in their
  // BluetoothGattCharacteristicClient::Observer implementation, which was
  // called before ours). Check the client to see if notifications are still
  // being sent.
  BluetoothGattCharacteristicClient::Properties* properties =
      DBusThreadManager::Get()
          ->GetBluetoothGattCharacteristicClient()
          ->GetProperties(object_path_);
  if (!properties || !properties->notifying.value())
    active_ = false;

  return active_;
}

void BluetoothGattNotifySessionChromeOS::Stop(const base::Closure& callback) {
  if (!active_) {
    VLOG(1) << "Notify session already inactive.";
    callback.Run();
    return;
  }

  // Mark this session as inactive no matter what.
  active_ = false;

  device::BluetoothDevice* device = adapter_->GetDevice(device_address_);
  if (!device)
    return;

  device::BluetoothGattService* service = device->GetGattService(service_id_);
  if (!service)
    return;

  BluetoothRemoteGattCharacteristicChromeOS* chrc =
      static_cast<BluetoothRemoteGattCharacteristicChromeOS*>(
          service->GetCharacteristic(characteristic_id_));
  if (!chrc)
    return;

  chrc->RemoveNotifySession(callback);
}

void BluetoothGattNotifySessionChromeOS::GattCharacteristicRemoved(
    const dbus::ObjectPath& object_path) {
  if (object_path != object_path_)
    return;

  active_ = false;
}

void BluetoothGattNotifySessionChromeOS::GattCharacteristicPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  if (object_path != object_path_)
    return;

  if (!active_)
    return;

  BluetoothGattCharacteristicClient::Properties* properties =
      DBusThreadManager::Get()
          ->GetBluetoothGattCharacteristicClient()
          ->GetProperties(object_path_);
  if (!properties) {
    active_ = false;
    return;
  }

  if (property_name == properties->notifying.name() &&
      !properties->notifying.value())
    active_ = false;
}

}  // namespace chromeos
