// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_remote_gatt_descriptor_chromeos.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "chromeos/dbus/bluetooth_gatt_descriptor_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic_chromeos.h"
#include "device/bluetooth/bluetooth_remote_gatt_service_chromeos.h"

namespace chromeos {

namespace {

// Stream operator for logging vector<uint8>.
std::ostream& operator<<(std::ostream& out, const std::vector<uint8> bytes) {
  out << "[";
  for (std::vector<uint8>::const_iterator iter = bytes.begin();
       iter != bytes.end(); ++iter) {
    out << base::StringPrintf("%02X", *iter);
  }
  return out << "]";
}

}  // namespace

BluetoothRemoteGattDescriptorChromeOS::BluetoothRemoteGattDescriptorChromeOS(
    BluetoothRemoteGattCharacteristicChromeOS* characteristic,
    const dbus::ObjectPath& object_path)
    : object_path_(object_path),
      characteristic_(characteristic),
      weak_ptr_factory_(this) {
  VLOG(1) << "Creating remote GATT descriptor with identifier: "
          << GetIdentifier() << ", UUID: " << GetUUID().canonical_value();
}

BluetoothRemoteGattDescriptorChromeOS::
    ~BluetoothRemoteGattDescriptorChromeOS() {
}

std::string BluetoothRemoteGattDescriptorChromeOS::GetIdentifier() const {
  return object_path_.value();
}

device::BluetoothUUID BluetoothRemoteGattDescriptorChromeOS::GetUUID() const {
  BluetoothGattDescriptorClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothGattDescriptorClient()->
          GetProperties(object_path_);
  DCHECK(properties);
  return device::BluetoothUUID(properties->uuid.value());
}

bool BluetoothRemoteGattDescriptorChromeOS::IsLocal() const {
  return false;
}

const std::vector<uint8>&
BluetoothRemoteGattDescriptorChromeOS::GetValue() const {
  return cached_value_;
}

device::BluetoothGattCharacteristic*
BluetoothRemoteGattDescriptorChromeOS::GetCharacteristic() const {
  return characteristic_;
}

device::BluetoothGattCharacteristic::Permissions
BluetoothRemoteGattDescriptorChromeOS::GetPermissions() const {
  // TODO(armansito): Once BlueZ defines the permissions, return the correct
  // values here.
  return device::BluetoothGattCharacteristic::kPermissionNone;
}

void BluetoothRemoteGattDescriptorChromeOS::ReadRemoteDescriptor(
    const ValueCallback& callback,
    const ErrorCallback& error_callback) {
  VLOG(1) << "Sending GATT characteristic descriptor read request to "
          << "descriptor: " << GetIdentifier() << ", UUID: "
          << GetUUID().canonical_value();

  DBusThreadManager::Get()->GetBluetoothGattDescriptorClient()->ReadValue(
      object_path_,
      base::Bind(&BluetoothRemoteGattDescriptorChromeOS::OnValueSuccess,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback),
      base::Bind(&BluetoothRemoteGattDescriptorChromeOS::OnError,
                 weak_ptr_factory_.GetWeakPtr(),
                 error_callback));
}

void BluetoothRemoteGattDescriptorChromeOS::WriteRemoteDescriptor(
    const std::vector<uint8>& new_value,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  VLOG(1) << "Sending GATT characteristic descriptor write request to "
          << "characteristic: " << GetIdentifier() << ", UUID: "
          << GetUUID().canonical_value() << ", with value: "
          << new_value << ".";

  DBusThreadManager::Get()->GetBluetoothGattDescriptorClient()->WriteValue(
      object_path_,
      new_value,
      callback,
      base::Bind(&BluetoothRemoteGattDescriptorChromeOS::OnError,
                 weak_ptr_factory_.GetWeakPtr(),
                 error_callback));
}

void BluetoothRemoteGattDescriptorChromeOS::OnValueSuccess(
    const ValueCallback& callback,
    const std::vector<uint8>& value) {
  VLOG(1) << "Descriptor value read: " << value;
  cached_value_ = value;

  DCHECK(characteristic_);
  BluetoothRemoteGattServiceChromeOS* service =
      static_cast<BluetoothRemoteGattServiceChromeOS*>(
          characteristic_->GetService());
  DCHECK(service);
  service->NotifyDescriptorValueChanged(characteristic_, this, value);
  callback.Run(value);
}

void BluetoothRemoteGattDescriptorChromeOS::OnError(
    const ErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  VLOG(1) << "Operation failed: " << error_name
          << ", message: " << error_message;
  error_callback.Run();
}

}  // namespace chromeos
