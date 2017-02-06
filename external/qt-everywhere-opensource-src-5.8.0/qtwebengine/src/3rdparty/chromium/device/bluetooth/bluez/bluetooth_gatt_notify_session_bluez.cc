// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/bluetooth_gatt_notify_session_bluez.h"

#include "base/bind.h"
#include "base/logging.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "device/bluetooth/bluez/bluetooth_remote_gatt_characteristic_bluez.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"

namespace bluez {

BluetoothGattNotifySessionBlueZ::BluetoothGattNotifySessionBlueZ(
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

  bluez::BluezDBusManager::Get()
      ->GetBluetoothGattCharacteristicClient()
      ->AddObserver(this);
}

BluetoothGattNotifySessionBlueZ::~BluetoothGattNotifySessionBlueZ() {
  bluez::BluezDBusManager::Get()
      ->GetBluetoothGattCharacteristicClient()
      ->RemoveObserver(this);
  Stop(base::Bind(&base::DoNothing));
}

std::string BluetoothGattNotifySessionBlueZ::GetCharacteristicIdentifier()
    const {
  return characteristic_id_;
}

bool BluetoothGattNotifySessionBlueZ::IsActive() {
  // Determine if the session is active. If |active_| is false, then it's
  // been explicitly marked, so return false.
  if (!active_)
    return false;

  // The fact that |active_| is true doesn't mean that the session is
  // actually active, since the characteristic might have stopped sending
  // notifications yet this method was called before we processed the
  // observer event (e.g. because somebody else called this method in their
  // bluez::BluetoothGattCharacteristicClient::Observer implementation, which
  // was
  // called before ours). Check the client to see if notifications are still
  // being sent.
  bluez::BluetoothGattCharacteristicClient::Properties* properties =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothGattCharacteristicClient()
          ->GetProperties(object_path_);
  if (!properties || !properties->notifying.value())
    active_ = false;

  return active_;
}

void BluetoothGattNotifySessionBlueZ::Stop(const base::Closure& callback) {
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

  device::BluetoothRemoteGattService* service =
      device->GetGattService(service_id_);
  if (!service)
    return;

  BluetoothRemoteGattCharacteristicBlueZ* chrc =
      static_cast<BluetoothRemoteGattCharacteristicBlueZ*>(
          service->GetCharacteristic(characteristic_id_));
  if (!chrc)
    return;

  chrc->RemoveNotifySession(callback);
}

void BluetoothGattNotifySessionBlueZ::GattCharacteristicRemoved(
    const dbus::ObjectPath& object_path) {
  if (object_path != object_path_)
    return;

  active_ = false;
}

void BluetoothGattNotifySessionBlueZ::GattCharacteristicPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  if (object_path != object_path_)
    return;

  if (!active_)
    return;

  bluez::BluetoothGattCharacteristicClient::Properties* properties =
      bluez::BluezDBusManager::Get()
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

}  // namespace bluez
