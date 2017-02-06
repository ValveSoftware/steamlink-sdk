// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/bluetooth_remote_gatt_characteristic_bluez.h"

#include <iterator>
#include <limits>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "dbus/property.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_gatt_service.h"
#include "device/bluetooth/bluez/bluetooth_adapter_bluez.h"
#include "device/bluetooth/bluez/bluetooth_gatt_notify_session_bluez.h"
#include "device/bluetooth/bluez/bluetooth_remote_gatt_descriptor_bluez.h"
#include "device/bluetooth/bluez/bluetooth_remote_gatt_service_bluez.h"
#include "device/bluetooth/dbus/bluetooth_gatt_characteristic_client.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

namespace {

// Stream operator for logging vector<uint8_t>.
std::ostream& operator<<(std::ostream& out, const std::vector<uint8_t> bytes) {
  out << "[";
  for (std::vector<uint8_t>::const_iterator iter = bytes.begin();
       iter != bytes.end(); ++iter) {
    out << base::StringPrintf("%02X", *iter);
  }
  return out << "]";
}

}  // namespace

BluetoothRemoteGattCharacteristicBlueZ::BluetoothRemoteGattCharacteristicBlueZ(
    BluetoothRemoteGattServiceBlueZ* service,
    const dbus::ObjectPath& object_path)
    : BluetoothGattCharacteristicBlueZ(object_path),
      num_notify_sessions_(0),
      notify_call_pending_(false),
      service_(service),
      weak_ptr_factory_(this) {
  VLOG(1) << "Creating remote GATT characteristic with identifier: "
          << GetIdentifier() << ", UUID: " << GetUUID().canonical_value();
  bluez::BluezDBusManager::Get()
      ->GetBluetoothGattDescriptorClient()
      ->AddObserver(this);

  // Add all known GATT characteristic descriptors.
  const std::vector<dbus::ObjectPath>& gatt_descs =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothGattDescriptorClient()
          ->GetDescriptors();
  for (std::vector<dbus::ObjectPath>::const_iterator iter = gatt_descs.begin();
       iter != gatt_descs.end(); ++iter)
    GattDescriptorAdded(*iter);
}

BluetoothRemoteGattCharacteristicBlueZ::
    ~BluetoothRemoteGattCharacteristicBlueZ() {
  bluez::BluezDBusManager::Get()
      ->GetBluetoothGattDescriptorClient()
      ->RemoveObserver(this);

  // Clean up all the descriptors. There isn't much point in notifying service
  // observers for each descriptor that gets removed, so just delete them.
  for (DescriptorMap::iterator iter = descriptors_.begin();
       iter != descriptors_.end(); ++iter)
    delete iter->second;

  // Report an error for all pending calls to StartNotifySession.
  while (!pending_start_notify_calls_.empty()) {
    PendingStartNotifyCall callbacks = pending_start_notify_calls_.front();
    pending_start_notify_calls_.pop();
    callbacks.second.Run(device::BluetoothRemoteGattService::GATT_ERROR_FAILED);
  }
}

device::BluetoothUUID BluetoothRemoteGattCharacteristicBlueZ::GetUUID() const {
  bluez::BluetoothGattCharacteristicClient::Properties* properties =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothGattCharacteristicClient()
          ->GetProperties(object_path());
  DCHECK(properties);
  return device::BluetoothUUID(properties->uuid.value());
}

device::BluetoothRemoteGattCharacteristic::Properties
BluetoothRemoteGattCharacteristicBlueZ::GetProperties() const {
  bluez::BluetoothGattCharacteristicClient::Properties* properties =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothGattCharacteristicClient()
          ->GetProperties(object_path());
  DCHECK(properties);

  Properties props = PROPERTY_NONE;
  const std::vector<std::string>& flags = properties->flags.value();
  for (std::vector<std::string>::const_iterator iter = flags.begin();
       iter != flags.end(); ++iter) {
    if (*iter == bluetooth_gatt_characteristic::kFlagBroadcast)
      props |= PROPERTY_BROADCAST;
    if (*iter == bluetooth_gatt_characteristic::kFlagRead)
      props |= PROPERTY_READ;
    if (*iter == bluetooth_gatt_characteristic::kFlagWriteWithoutResponse)
      props |= PROPERTY_WRITE_WITHOUT_RESPONSE;
    if (*iter == bluetooth_gatt_characteristic::kFlagWrite)
      props |= PROPERTY_WRITE;
    if (*iter == bluetooth_gatt_characteristic::kFlagNotify)
      props |= PROPERTY_NOTIFY;
    if (*iter == bluetooth_gatt_characteristic::kFlagIndicate)
      props |= PROPERTY_INDICATE;
    if (*iter == bluetooth_gatt_characteristic::kFlagAuthenticatedSignedWrites)
      props |= PROPERTY_AUTHENTICATED_SIGNED_WRITES;
    if (*iter == bluetooth_gatt_characteristic::kFlagExtendedProperties)
      props |= PROPERTY_EXTENDED_PROPERTIES;
    if (*iter == bluetooth_gatt_characteristic::kFlagReliableWrite)
      props |= PROPERTY_RELIABLE_WRITE;
    if (*iter == bluetooth_gatt_characteristic::kFlagWritableAuxiliaries)
      props |= PROPERTY_WRITABLE_AUXILIARIES;
  }

  return props;
}

device::BluetoothRemoteGattCharacteristic::Permissions
BluetoothRemoteGattCharacteristicBlueZ::GetPermissions() const {
  // TODO(armansito): Once BlueZ defines the permissions, return the correct
  // values here.
  return PERMISSION_NONE;
}

const std::vector<uint8_t>& BluetoothRemoteGattCharacteristicBlueZ::GetValue()
    const {
  bluez::BluetoothGattCharacteristicClient::Properties* properties =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothGattCharacteristicClient()
          ->GetProperties(object_path());

  DCHECK(properties);

  return properties->value.value();
}

device::BluetoothRemoteGattService*
BluetoothRemoteGattCharacteristicBlueZ::GetService() const {
  return service_;
}

bool BluetoothRemoteGattCharacteristicBlueZ::IsNotifying() const {
  bluez::BluetoothGattCharacteristicClient::Properties* properties =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothGattCharacteristicClient()
          ->GetProperties(object_path());
  DCHECK(properties);

  return properties->notifying.value();
}

std::vector<device::BluetoothRemoteGattDescriptor*>
BluetoothRemoteGattCharacteristicBlueZ::GetDescriptors() const {
  std::vector<device::BluetoothRemoteGattDescriptor*> descriptors;
  for (DescriptorMap::const_iterator iter = descriptors_.begin();
       iter != descriptors_.end(); ++iter)
    descriptors.push_back(iter->second);
  return descriptors;
}

device::BluetoothRemoteGattDescriptor*
BluetoothRemoteGattCharacteristicBlueZ::GetDescriptor(
    const std::string& identifier) const {
  DescriptorMap::const_iterator iter =
      descriptors_.find(dbus::ObjectPath(identifier));
  if (iter == descriptors_.end())
    return nullptr;
  return iter->second;
}

void BluetoothRemoteGattCharacteristicBlueZ::StartNotifySession(
    const NotifySessionCallback& callback,
    const ErrorCallback& error_callback) {
  VLOG(1) << __func__;

  if (num_notify_sessions_ > 0) {
    // The characteristic might have stopped notifying even though the session
    // count is nonzero. This means that notifications stopped outside of our
    // control and we should reset the count. If the characteristic is still
    // notifying, then return success. Otherwise, reset the count and treat
    // this call as if the count were 0.
    if (IsNotifying()) {
      // Check for overflows, though unlikely.
      if (num_notify_sessions_ == std::numeric_limits<size_t>::max()) {
        error_callback.Run(
            device::BluetoothRemoteGattService::GATT_ERROR_FAILED);
        return;
      }

      ++num_notify_sessions_;
      DCHECK(service_);
      DCHECK(service_->GetAdapter());
      DCHECK(service_->GetDevice());
      std::unique_ptr<device::BluetoothGattNotifySession> session(
          new BluetoothGattNotifySessionBlueZ(
              service_->GetAdapter(), service_->GetDevice()->GetAddress(),
              service_->GetIdentifier(), GetIdentifier(), object_path()));
      callback.Run(std::move(session));
      return;
    }

    num_notify_sessions_ = 0;
  }

  // Queue the callbacks if there is a pending call to bluetoothd.
  if (notify_call_pending_) {
    pending_start_notify_calls_.push(std::make_pair(callback, error_callback));
    return;
  }

  notify_call_pending_ = true;
  bluez::BluezDBusManager::Get()
      ->GetBluetoothGattCharacteristicClient()
      ->StartNotify(
          object_path(),
          base::Bind(
              &BluetoothRemoteGattCharacteristicBlueZ::OnStartNotifySuccess,
              weak_ptr_factory_.GetWeakPtr(), callback),
          base::Bind(
              &BluetoothRemoteGattCharacteristicBlueZ::OnStartNotifyError,
              weak_ptr_factory_.GetWeakPtr(), error_callback));
}

void BluetoothRemoteGattCharacteristicBlueZ::ReadRemoteCharacteristic(
    const ValueCallback& callback,
    const ErrorCallback& error_callback) {
  VLOG(1) << "Sending GATT characteristic read request to characteristic: "
          << GetIdentifier() << ", UUID: " << GetUUID().canonical_value()
          << ".";

  bluez::BluezDBusManager::Get()
      ->GetBluetoothGattCharacteristicClient()
      ->ReadValue(object_path(), callback,
                  base::Bind(&BluetoothRemoteGattCharacteristicBlueZ::OnError,
                             weak_ptr_factory_.GetWeakPtr(), error_callback));
}

void BluetoothRemoteGattCharacteristicBlueZ::WriteRemoteCharacteristic(
    const std::vector<uint8_t>& new_value,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  VLOG(1) << "Sending GATT characteristic write request to characteristic: "
          << GetIdentifier() << ", UUID: " << GetUUID().canonical_value()
          << ", with value: " << new_value << ".";

  bluez::BluezDBusManager::Get()
      ->GetBluetoothGattCharacteristicClient()
      ->WriteValue(object_path(), new_value, callback,
                   base::Bind(&BluetoothRemoteGattCharacteristicBlueZ::OnError,
                              weak_ptr_factory_.GetWeakPtr(), error_callback));
}

void BluetoothRemoteGattCharacteristicBlueZ::RemoveNotifySession(
    const base::Closure& callback) {
  VLOG(1) << __func__;

  if (num_notify_sessions_ > 1) {
    DCHECK(!notify_call_pending_);
    --num_notify_sessions_;
    callback.Run();
    return;
  }

  // Notifications may have stopped outside our control. If the characteristic
  // is no longer notifying, return success.
  if (!IsNotifying()) {
    num_notify_sessions_ = 0;
    callback.Run();
    return;
  }

  if (notify_call_pending_ || num_notify_sessions_ == 0) {
    callback.Run();
    return;
  }

  DCHECK(num_notify_sessions_ == 1);
  notify_call_pending_ = true;
  bluez::BluezDBusManager::Get()
      ->GetBluetoothGattCharacteristicClient()
      ->StopNotify(
          object_path(),
          base::Bind(
              &BluetoothRemoteGattCharacteristicBlueZ::OnStopNotifySuccess,
              weak_ptr_factory_.GetWeakPtr(), callback),
          base::Bind(&BluetoothRemoteGattCharacteristicBlueZ::OnStopNotifyError,
                     weak_ptr_factory_.GetWeakPtr(), callback));
}

void BluetoothRemoteGattCharacteristicBlueZ::GattDescriptorAdded(
    const dbus::ObjectPath& object_path) {
  if (descriptors_.find(object_path) != descriptors_.end()) {
    VLOG(1) << "Remote GATT characteristic descriptor already exists: "
            << object_path.value();
    return;
  }

  bluez::BluetoothGattDescriptorClient::Properties* properties =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothGattDescriptorClient()
          ->GetProperties(object_path);
  DCHECK(properties);
  if (properties->characteristic.value() != this->object_path()) {
    VLOG(3) << "Remote GATT descriptor does not belong to this characteristic.";
    return;
  }

  VLOG(1) << "Adding new remote GATT descriptor for GATT characteristic: "
          << GetIdentifier() << ", UUID: " << GetUUID().canonical_value();

  BluetoothRemoteGattDescriptorBlueZ* descriptor =
      new BluetoothRemoteGattDescriptorBlueZ(this, object_path);
  descriptors_[object_path] = descriptor;
  DCHECK(descriptor->GetIdentifier() == object_path.value());
  DCHECK(descriptor->GetUUID().IsValid());
  DCHECK(service_);

  static_cast<BluetoothRemoteGattServiceBlueZ*>(service_)
      ->NotifyDescriptorAddedOrRemoved(this, descriptor, true /* added */);
}

void BluetoothRemoteGattCharacteristicBlueZ::GattDescriptorRemoved(
    const dbus::ObjectPath& object_path) {
  DescriptorMap::iterator iter = descriptors_.find(object_path);
  if (iter == descriptors_.end()) {
    VLOG(2) << "Unknown descriptor removed: " << object_path.value();
    return;
  }

  VLOG(1) << "Removing remote GATT descriptor from characteristic: "
          << GetIdentifier() << ", UUID: " << GetUUID().canonical_value();

  BluetoothRemoteGattDescriptorBlueZ* descriptor = iter->second;
  DCHECK(descriptor->object_path() == object_path);
  descriptors_.erase(iter);

  DCHECK(service_);
  static_cast<BluetoothRemoteGattServiceBlueZ*>(service_)
      ->NotifyDescriptorAddedOrRemoved(this, descriptor, false /* added */);

  delete descriptor;
}

void BluetoothRemoteGattCharacteristicBlueZ::GattDescriptorPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  DescriptorMap::iterator iter = descriptors_.find(object_path);
  if (iter == descriptors_.end()) {
    VLOG(2) << "Unknown descriptor removed: " << object_path.value();
    return;
  }

  bluez::BluetoothGattDescriptorClient::Properties* properties =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothGattDescriptorClient()
          ->GetProperties(object_path);

  DCHECK(properties);

  if (property_name != properties->value.name())
    return;

  DCHECK(service_);
  static_cast<BluetoothRemoteGattServiceBlueZ*>(service_)
      ->NotifyDescriptorValueChanged(this, iter->second,
                                     properties->value.value());
}

void BluetoothRemoteGattCharacteristicBlueZ::OnStartNotifySuccess(
    const NotifySessionCallback& callback) {
  VLOG(1) << "Started notifications from characteristic: "
          << object_path().value();
  DCHECK(num_notify_sessions_ == 0);
  DCHECK(notify_call_pending_);

  ++num_notify_sessions_;
  notify_call_pending_ = false;

  // Invoke the queued callbacks for this operation.
  DCHECK(service_);
  DCHECK(service_->GetDevice());
  std::unique_ptr<device::BluetoothGattNotifySession> session(
      new BluetoothGattNotifySessionBlueZ(
          service_->GetAdapter(), service_->GetDevice()->GetAddress(),
          service_->GetIdentifier(), GetIdentifier(), object_path()));
  callback.Run(std::move(session));

  ProcessStartNotifyQueue();
}

void BluetoothRemoteGattCharacteristicBlueZ::OnStartNotifyError(
    const ErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  VLOG(1) << "Failed to start notifications from characteristic: "
          << object_path().value() << ": " << error_name << ", "
          << error_message;
  DCHECK(num_notify_sessions_ == 0);
  DCHECK(notify_call_pending_);

  notify_call_pending_ = false;

  error_callback.Run(
      BluetoothRemoteGattServiceBlueZ::DBusErrorToServiceError(error_name));

  ProcessStartNotifyQueue();
}

void BluetoothRemoteGattCharacteristicBlueZ::OnStopNotifySuccess(
    const base::Closure& callback) {
  DCHECK(notify_call_pending_);
  DCHECK(num_notify_sessions_ == 1);

  notify_call_pending_ = false;
  --num_notify_sessions_;
  callback.Run();

  ProcessStartNotifyQueue();
}

void BluetoothRemoteGattCharacteristicBlueZ::OnStopNotifyError(
    const base::Closure& callback,
    const std::string& error_name,
    const std::string& error_message) {
  VLOG(1) << "Call to stop notifications failed for characteristic: "
          << object_path().value() << ": " << error_name << ", "
          << error_message;

  // Since this is a best effort operation, treat this as success.
  OnStopNotifySuccess(callback);
}

void BluetoothRemoteGattCharacteristicBlueZ::ProcessStartNotifyQueue() {
  while (!pending_start_notify_calls_.empty()) {
    PendingStartNotifyCall callbacks = pending_start_notify_calls_.front();
    pending_start_notify_calls_.pop();
    StartNotifySession(callbacks.first, callbacks.second);
  }
}

void BluetoothRemoteGattCharacteristicBlueZ::OnError(
    const ErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  VLOG(1) << "Operation failed: " << error_name
          << ", message: " << error_message;
  error_callback.Run(
      BluetoothGattServiceBlueZ::DBusErrorToServiceError(error_name));
}

}  // namespace bluez
