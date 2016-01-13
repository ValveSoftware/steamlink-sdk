// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_remote_gatt_characteristic_chromeos.h"

#include <limits>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_gatt_notify_session_chromeos.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor_chromeos.h"
#include "device/bluetooth/bluetooth_remote_gatt_service_chromeos.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

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

BluetoothRemoteGattCharacteristicChromeOS::
    BluetoothRemoteGattCharacteristicChromeOS(
        BluetoothRemoteGattServiceChromeOS* service,
        const dbus::ObjectPath& object_path)
    : object_path_(object_path),
      service_(service),
      num_notify_sessions_(0),
      notify_call_pending_(false),
      weak_ptr_factory_(this) {
  VLOG(1) << "Creating remote GATT characteristic with identifier: "
          << GetIdentifier() << ", UUID: " << GetUUID().canonical_value();
  DBusThreadManager::Get()->GetBluetoothGattCharacteristicClient()->
      AddObserver(this);
  DBusThreadManager::Get()->GetBluetoothGattDescriptorClient()->
      AddObserver(this);

  // Add all known GATT characteristic descriptors.
  const std::vector<dbus::ObjectPath>& gatt_descs =
      DBusThreadManager::Get()->GetBluetoothGattDescriptorClient()->
          GetDescriptors();
  for (std::vector<dbus::ObjectPath>::const_iterator iter = gatt_descs.begin();
       iter != gatt_descs.end(); ++iter)
    GattDescriptorAdded(*iter);
}

BluetoothRemoteGattCharacteristicChromeOS::
    ~BluetoothRemoteGattCharacteristicChromeOS() {
  DBusThreadManager::Get()->GetBluetoothGattDescriptorClient()->
      RemoveObserver(this);
  DBusThreadManager::Get()->GetBluetoothGattCharacteristicClient()->
      RemoveObserver(this);

  // Clean up all the descriptors. There isn't much point in notifying service
  // observers for each descriptor that gets removed, so just delete them.
  for (DescriptorMap::iterator iter = descriptors_.begin();
       iter != descriptors_.end(); ++iter)
    delete iter->second;

  // Report an error for all pending calls to StartNotifySession.
  while (!pending_start_notify_calls_.empty()) {
    PendingStartNotifyCall callbacks = pending_start_notify_calls_.front();
    pending_start_notify_calls_.pop();
    callbacks.second.Run();
  }
}

std::string BluetoothRemoteGattCharacteristicChromeOS::GetIdentifier() const {
  return object_path_.value();
}

device::BluetoothUUID
BluetoothRemoteGattCharacteristicChromeOS::GetUUID() const {
  BluetoothGattCharacteristicClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothGattCharacteristicClient()->
          GetProperties(object_path_);
  DCHECK(properties);
  return device::BluetoothUUID(properties->uuid.value());
}

bool BluetoothRemoteGattCharacteristicChromeOS::IsLocal() const {
  return false;
}

const std::vector<uint8>&
BluetoothRemoteGattCharacteristicChromeOS::GetValue() const {
  return cached_value_;
}

device::BluetoothGattService*
BluetoothRemoteGattCharacteristicChromeOS::GetService() const {
  return service_;
}

device::BluetoothGattCharacteristic::Properties
BluetoothRemoteGattCharacteristicChromeOS::GetProperties() const {
  BluetoothGattCharacteristicClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothGattCharacteristicClient()->
          GetProperties(object_path_);
  DCHECK(properties);

  Properties props = kPropertyNone;
  const std::vector<std::string>& flags = properties->flags.value();
  for (std::vector<std::string>::const_iterator iter = flags.begin();
       iter != flags.end();
       ++iter) {
    if (*iter == bluetooth_gatt_characteristic::kFlagBroadcast)
      props |= kPropertyBroadcast;
    if (*iter == bluetooth_gatt_characteristic::kFlagRead)
      props |= kPropertyRead;
    if (*iter == bluetooth_gatt_characteristic::kFlagWriteWithoutResponse)
      props |= kPropertyWriteWithoutResponse;
    if (*iter == bluetooth_gatt_characteristic::kFlagWrite)
      props |= kPropertyWrite;
    if (*iter == bluetooth_gatt_characteristic::kFlagNotify)
      props |= kPropertyNotify;
    if (*iter == bluetooth_gatt_characteristic::kFlagIndicate)
      props |= kPropertyIndicate;
    if (*iter == bluetooth_gatt_characteristic::kFlagAuthenticatedSignedWrites)
      props |= kPropertyAuthenticatedSignedWrites;
    if (*iter == bluetooth_gatt_characteristic::kFlagExtendedProperties)
      props |= kPropertyExtendedProperties;
    if (*iter == bluetooth_gatt_characteristic::kFlagReliableWrite)
      props |= kPropertyReliableWrite;
    if (*iter == bluetooth_gatt_characteristic::kFlagWritableAuxiliaries)
      props |= kPropertyWritableAuxiliaries;
  }

  return props;
}

device::BluetoothGattCharacteristic::Permissions
BluetoothRemoteGattCharacteristicChromeOS::GetPermissions() const {
  // TODO(armansito): Once BlueZ defines the permissions, return the correct
  // values here.
  return kPermissionNone;
}

bool BluetoothRemoteGattCharacteristicChromeOS::IsNotifying() const {
  BluetoothGattCharacteristicClient::Properties* properties =
      DBusThreadManager::Get()
          ->GetBluetoothGattCharacteristicClient()
          ->GetProperties(object_path_);
  DCHECK(properties);

  return properties->notifying.value();
}

std::vector<device::BluetoothGattDescriptor*>
BluetoothRemoteGattCharacteristicChromeOS::GetDescriptors() const {
  std::vector<device::BluetoothGattDescriptor*> descriptors;
  for (DescriptorMap::const_iterator iter = descriptors_.begin();
       iter != descriptors_.end(); ++iter)
    descriptors.push_back(iter->second);
  return descriptors;
}

device::BluetoothGattDescriptor*
BluetoothRemoteGattCharacteristicChromeOS::GetDescriptor(
    const std::string& identifier) const {
  DescriptorMap::const_iterator iter =
      descriptors_.find(dbus::ObjectPath(identifier));
  if (iter == descriptors_.end())
    return NULL;
  return iter->second;
}

bool BluetoothRemoteGattCharacteristicChromeOS::AddDescriptor(
    device::BluetoothGattDescriptor* descriptor) {
  VLOG(1) << "Descriptors cannot be added to a remote GATT characteristic.";
  return false;
}

bool BluetoothRemoteGattCharacteristicChromeOS::UpdateValue(
    const std::vector<uint8>& value) {
  VLOG(1) << "Cannot update the value of a remote GATT characteristic.";
  return false;
}

void BluetoothRemoteGattCharacteristicChromeOS::ReadRemoteCharacteristic(
    const ValueCallback& callback,
    const ErrorCallback& error_callback) {
  VLOG(1) << "Sending GATT characteristic read request to characteristic: "
          << GetIdentifier() << ", UUID: " << GetUUID().canonical_value()
          << ".";

  DBusThreadManager::Get()->GetBluetoothGattCharacteristicClient()->ReadValue(
      object_path_,
      base::Bind(&BluetoothRemoteGattCharacteristicChromeOS::OnValueSuccess,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback),
      base::Bind(&BluetoothRemoteGattCharacteristicChromeOS::OnError,
                 weak_ptr_factory_.GetWeakPtr(),
                 error_callback));
}

void BluetoothRemoteGattCharacteristicChromeOS::WriteRemoteCharacteristic(
    const std::vector<uint8>& new_value,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  VLOG(1) << "Sending GATT characteristic write request to characteristic: "
          << GetIdentifier() << ", UUID: " << GetUUID().canonical_value()
          << ", with value: " << new_value << ".";

  DBusThreadManager::Get()->GetBluetoothGattCharacteristicClient()->WriteValue(
      object_path_,
      new_value,
      callback,
      base::Bind(&BluetoothRemoteGattCharacteristicChromeOS::OnError,
                 weak_ptr_factory_.GetWeakPtr(),
                 error_callback));
}

void BluetoothRemoteGattCharacteristicChromeOS::StartNotifySession(
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
        error_callback.Run();
        return;
      }

      ++num_notify_sessions_;
      DCHECK(service_);
      DCHECK(service_->GetDevice());
      scoped_ptr<device::BluetoothGattNotifySession> session(
          new BluetoothGattNotifySessionChromeOS(
              service_->GetAdapter(),
              service_->GetDevice()->GetAddress(),
              service_->GetIdentifier(),
              GetIdentifier(),
              object_path_));
      callback.Run(session.Pass());
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
  DBusThreadManager::Get()->GetBluetoothGattCharacteristicClient()->StartNotify(
      object_path_,
      base::Bind(
          &BluetoothRemoteGattCharacteristicChromeOS::OnStartNotifySuccess,
          weak_ptr_factory_.GetWeakPtr(),
          callback),
      base::Bind(&BluetoothRemoteGattCharacteristicChromeOS::OnStartNotifyError,
                 weak_ptr_factory_.GetWeakPtr(),
                 error_callback));
}

void BluetoothRemoteGattCharacteristicChromeOS::RemoveNotifySession(
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
  DBusThreadManager::Get()->GetBluetoothGattCharacteristicClient()->StopNotify(
      object_path_,
      base::Bind(
          &BluetoothRemoteGattCharacteristicChromeOS::OnStopNotifySuccess,
          weak_ptr_factory_.GetWeakPtr(),
          callback),
      base::Bind(&BluetoothRemoteGattCharacteristicChromeOS::OnStopNotifyError,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void BluetoothRemoteGattCharacteristicChromeOS::GattCharacteristicValueUpdated(
    const dbus::ObjectPath& object_path,
    const std::vector<uint8>& value) {
  if (object_path != object_path_)
    return;

  cached_value_ = value;

  VLOG(1) << "GATT characteristic value has changed: " << object_path.value()
          << ": " << value;
  DCHECK(service_);
  service_->NotifyCharacteristicValueChanged(this, value);
}

void BluetoothRemoteGattCharacteristicChromeOS::GattDescriptorAdded(
    const dbus::ObjectPath& object_path) {
  if (descriptors_.find(object_path) != descriptors_.end()) {
    VLOG(1) << "Remote GATT characteristic descriptor already exists: "
            << object_path.value();
    return;
  }

  BluetoothGattDescriptorClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothGattDescriptorClient()->
          GetProperties(object_path);
  DCHECK(properties);
  if (properties->characteristic.value() != object_path_) {
    VLOG(2) << "Remote GATT descriptor does not belong to this characteristic.";
    return;
  }

  VLOG(1) << "Adding new remote GATT descriptor for GATT characteristic: "
          << GetIdentifier() << ", UUID: " << GetUUID().canonical_value();

  BluetoothRemoteGattDescriptorChromeOS* descriptor =
      new BluetoothRemoteGattDescriptorChromeOS(this, object_path);
  descriptors_[object_path] = descriptor;
  DCHECK(descriptor->GetIdentifier() == object_path.value());
  DCHECK(descriptor->GetUUID().IsValid());
  DCHECK(service_);

  service_->NotifyDescriptorAddedOrRemoved(this, descriptor, true /* added */);
  service_->NotifyServiceChanged();
}

void BluetoothRemoteGattCharacteristicChromeOS::GattDescriptorRemoved(
    const dbus::ObjectPath& object_path) {
  DescriptorMap::iterator iter = descriptors_.find(object_path);
  if (iter == descriptors_.end()) {
    VLOG(2) << "Unknown descriptor removed: " << object_path.value();
    return;
  }

  VLOG(1) << "Removing remote GATT descriptor from characteristic: "
          << GetIdentifier() << ", UUID: " << GetUUID().canonical_value();

  BluetoothRemoteGattDescriptorChromeOS* descriptor = iter->second;
  DCHECK(descriptor->object_path() == object_path);
  descriptors_.erase(iter);

  service_->NotifyDescriptorAddedOrRemoved(this, descriptor, false /* added */);
  delete descriptor;

  DCHECK(service_);

  service_->NotifyServiceChanged();
}

void BluetoothRemoteGattCharacteristicChromeOS::OnValueSuccess(
    const ValueCallback& callback,
    const std::vector<uint8>& value) {
  VLOG(1) << "Characteristic value read: " << value;
  cached_value_ = value;

  DCHECK(service_);
  service_->NotifyCharacteristicValueChanged(this, cached_value_);

  callback.Run(value);
}

void BluetoothRemoteGattCharacteristicChromeOS::OnError(
    const ErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  VLOG(1) << "Operation failed: " << error_name << ", message: "
          << error_message;
  error_callback.Run();
}

void BluetoothRemoteGattCharacteristicChromeOS::OnStartNotifySuccess(
    const NotifySessionCallback& callback) {
  VLOG(1) << "Started notifications from characteristic: "
          << object_path_.value();
  DCHECK(num_notify_sessions_ == 0);
  DCHECK(notify_call_pending_);

  ++num_notify_sessions_;
  notify_call_pending_ = false;

  // Invoke the queued callbacks for this operation.
  DCHECK(service_);
  DCHECK(service_->GetDevice());
  scoped_ptr<device::BluetoothGattNotifySession> session(
      new BluetoothGattNotifySessionChromeOS(
          service_->GetAdapter(),
          service_->GetDevice()->GetAddress(),
          service_->GetIdentifier(),
          GetIdentifier(),
          object_path_));
  callback.Run(session.Pass());

  ProcessStartNotifyQueue();
}

void BluetoothRemoteGattCharacteristicChromeOS::OnStartNotifyError(
    const ErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  VLOG(1) << "Failed to start notifications from characteristic: "
          << object_path_.value() << ": " << error_name << ", "
          << error_message;
  DCHECK(num_notify_sessions_ == 0);
  DCHECK(notify_call_pending_);

  notify_call_pending_ = false;
  error_callback.Run();

  ProcessStartNotifyQueue();
}

void BluetoothRemoteGattCharacteristicChromeOS::OnStopNotifySuccess(
    const base::Closure& callback) {
  DCHECK(notify_call_pending_);
  DCHECK(num_notify_sessions_ == 1);

  notify_call_pending_ = false;
  --num_notify_sessions_;
  callback.Run();

  ProcessStartNotifyQueue();
}

void BluetoothRemoteGattCharacteristicChromeOS::OnStopNotifyError(
    const base::Closure& callback,
    const std::string& error_name,
    const std::string& error_message) {
  VLOG(1) << "Call to stop notifications failed for characteristic: "
          << object_path_.value() << ": " << error_name << ", "
          << error_message;

  // Since this is a best effort operation, treat this as success.
  OnStopNotifySuccess(callback);
}

void BluetoothRemoteGattCharacteristicChromeOS::ProcessStartNotifyQueue() {
  while (!pending_start_notify_calls_.empty()) {
    PendingStartNotifyCall callbacks = pending_start_notify_calls_.front();
    pending_start_notify_calls_.pop();
    StartNotifySession(callbacks.first, callbacks.second);
  }
}

}  // namespace chromeos
