// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_remote_gatt_service_chromeos.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "chromeos/dbus/bluetooth_gatt_service_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "device/bluetooth/bluetooth_adapter_chromeos.h"
#include "device/bluetooth/bluetooth_device_chromeos.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic_chromeos.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor_chromeos.h"

namespace chromeos {

BluetoothRemoteGattServiceChromeOS::BluetoothRemoteGattServiceChromeOS(
    BluetoothAdapterChromeOS* adapter,
    BluetoothDeviceChromeOS* device,
    const dbus::ObjectPath& object_path)
    : object_path_(object_path),
      adapter_(adapter),
      device_(device),
      weak_ptr_factory_(this) {
  VLOG(1) << "Creating remote GATT service with identifier: "
          << object_path.value() << ", UUID: " << GetUUID().canonical_value();
  DCHECK(adapter_);

  DBusThreadManager::Get()->GetBluetoothGattServiceClient()->AddObserver(this);
  DBusThreadManager::Get()->GetBluetoothGattCharacteristicClient()->
      AddObserver(this);

  // Add all known GATT characteristics.
  const std::vector<dbus::ObjectPath>& gatt_chars =
      DBusThreadManager::Get()->GetBluetoothGattCharacteristicClient()->
          GetCharacteristics();
  for (std::vector<dbus::ObjectPath>::const_iterator iter = gatt_chars.begin();
       iter != gatt_chars.end(); ++iter)
    GattCharacteristicAdded(*iter);
}

BluetoothRemoteGattServiceChromeOS::~BluetoothRemoteGattServiceChromeOS() {
  DBusThreadManager::Get()->GetBluetoothGattServiceClient()->
      RemoveObserver(this);
  DBusThreadManager::Get()->GetBluetoothGattCharacteristicClient()->
      RemoveObserver(this);

  // Clean up all the characteristics. Copy the characteristics list here and
  // clear the original so that when we send GattCharacteristicRemoved(),
  // GetCharacteristics() returns no characteristics.
  CharacteristicMap characteristics = characteristics_;
  characteristics_.clear();
  for (CharacteristicMap::iterator iter = characteristics.begin();
       iter != characteristics.end(); ++iter) {
    FOR_EACH_OBSERVER(device::BluetoothGattService::Observer, observers_,
                      GattCharacteristicRemoved(this, iter->second));
    delete iter->second;
  }
}

void BluetoothRemoteGattServiceChromeOS::AddObserver(
    device::BluetoothGattService::Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void BluetoothRemoteGattServiceChromeOS::RemoveObserver(
    device::BluetoothGattService::Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

std::string BluetoothRemoteGattServiceChromeOS::GetIdentifier() const {
  return object_path_.value();
}

device::BluetoothUUID BluetoothRemoteGattServiceChromeOS::GetUUID() const {
  BluetoothGattServiceClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothGattServiceClient()->
          GetProperties(object_path_);
  DCHECK(properties);
  return device::BluetoothUUID(properties->uuid.value());
}

bool BluetoothRemoteGattServiceChromeOS::IsLocal() const {
  return false;
}

bool BluetoothRemoteGattServiceChromeOS::IsPrimary() const {
  BluetoothGattServiceClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothGattServiceClient()->
          GetProperties(object_path_);
  DCHECK(properties);
  return properties->primary.value();
}

device::BluetoothDevice* BluetoothRemoteGattServiceChromeOS::GetDevice() const {
  return device_;
}

std::vector<device::BluetoothGattCharacteristic*>
BluetoothRemoteGattServiceChromeOS::GetCharacteristics() const {
  std::vector<device::BluetoothGattCharacteristic*> characteristics;
  for (CharacteristicMap::const_iterator iter = characteristics_.begin();
       iter != characteristics_.end(); ++iter) {
    characteristics.push_back(iter->second);
  }
  return characteristics;
}

std::vector<device::BluetoothGattService*>
BluetoothRemoteGattServiceChromeOS::GetIncludedServices() const {
  // TODO(armansito): Return the actual included services here.
  return std::vector<device::BluetoothGattService*>();
}

device::BluetoothGattCharacteristic*
BluetoothRemoteGattServiceChromeOS::GetCharacteristic(
    const std::string& identifier) const {
  CharacteristicMap::const_iterator iter =
      characteristics_.find(dbus::ObjectPath(identifier));
  if (iter == characteristics_.end())
    return NULL;
  return iter->second;
}

bool BluetoothRemoteGattServiceChromeOS::AddCharacteristic(
    device::BluetoothGattCharacteristic* characteristic) {
  VLOG(1) << "Characteristics cannot be added to a remote GATT service.";
  return false;
}

bool BluetoothRemoteGattServiceChromeOS::AddIncludedService(
    device::BluetoothGattService* service) {
  VLOG(1) << "Included services cannot be added to a remote GATT service.";
  return false;
}

void BluetoothRemoteGattServiceChromeOS::Register(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  VLOG(1) << "A remote GATT service cannot be registered.";
  error_callback.Run();
}

void BluetoothRemoteGattServiceChromeOS::Unregister(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  VLOG(1) << "A remote GATT service cannot be unregistered.";
  error_callback.Run();
}

scoped_refptr<device::BluetoothAdapter>
BluetoothRemoteGattServiceChromeOS::GetAdapter() const {
  return adapter_;
}

void BluetoothRemoteGattServiceChromeOS::NotifyServiceChanged() {
  FOR_EACH_OBSERVER(device::BluetoothGattService::Observer, observers_,
                    GattServiceChanged(this));
}

void BluetoothRemoteGattServiceChromeOS::NotifyCharacteristicValueChanged(
    BluetoothRemoteGattCharacteristicChromeOS* characteristic,
    const std::vector<uint8>& value) {
  DCHECK(characteristic->GetService() == this);
  FOR_EACH_OBSERVER(
      device::BluetoothGattService::Observer,
      observers_,
      GattCharacteristicValueChanged(this, characteristic, value));
}

void BluetoothRemoteGattServiceChromeOS::NotifyDescriptorAddedOrRemoved(
    BluetoothRemoteGattCharacteristicChromeOS* characteristic,
    BluetoothRemoteGattDescriptorChromeOS* descriptor,
    bool added) {
  DCHECK(characteristic->GetService() == this);
  DCHECK(descriptor->GetCharacteristic() == characteristic);
  if (added) {
    FOR_EACH_OBSERVER(device::BluetoothGattService::Observer, observers_,
                      GattDescriptorAdded(characteristic, descriptor));
    return;
  }
  FOR_EACH_OBSERVER(device::BluetoothGattService::Observer, observers_,
                    GattDescriptorRemoved(characteristic, descriptor));
}

void BluetoothRemoteGattServiceChromeOS::NotifyDescriptorValueChanged(
    BluetoothRemoteGattCharacteristicChromeOS* characteristic,
    BluetoothRemoteGattDescriptorChromeOS* descriptor,
    const std::vector<uint8>& value) {
  DCHECK(characteristic->GetService() == this);
  DCHECK(descriptor->GetCharacteristic() == characteristic);
  FOR_EACH_OBSERVER(
      device::BluetoothGattService::Observer, observers_,
      GattDescriptorValueChanged(characteristic, descriptor, value));
}

void BluetoothRemoteGattServiceChromeOS::GattServicePropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name){
  if (object_path != object_path_)
    return;

  VLOG(1) << "Service property changed: " << object_path.value();
  NotifyServiceChanged();
}

void BluetoothRemoteGattServiceChromeOS::GattCharacteristicAdded(
    const dbus::ObjectPath& object_path) {
  if (characteristics_.find(object_path) != characteristics_.end()) {
    VLOG(1) << "Remote GATT characteristic already exists: "
            << object_path.value();
    return;
  }

  BluetoothGattCharacteristicClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothGattCharacteristicClient()->
          GetProperties(object_path);
  DCHECK(properties);
  if (properties->service.value() != object_path_) {
    VLOG(2) << "Remote GATT characteristic does not belong to this service.";
    return;
  }

  VLOG(1) << "Adding new remote GATT characteristic for GATT service: "
          << GetIdentifier() << ", UUID: " << GetUUID().canonical_value();

  BluetoothRemoteGattCharacteristicChromeOS* characteristic =
      new BluetoothRemoteGattCharacteristicChromeOS(this, object_path);
  characteristics_[object_path] = characteristic;
  DCHECK(characteristic->GetIdentifier() == object_path.value());
  DCHECK(characteristic->GetUUID().IsValid());

  FOR_EACH_OBSERVER(device::BluetoothGattService::Observer, observers_,
                    GattCharacteristicAdded(this, characteristic));
  NotifyServiceChanged();
}

void BluetoothRemoteGattServiceChromeOS::GattCharacteristicRemoved(
    const dbus::ObjectPath& object_path) {
  CharacteristicMap::iterator iter = characteristics_.find(object_path);
  if (iter == characteristics_.end()) {
    VLOG(2) << "Unknown GATT characteristic removed: " << object_path.value();
    return;
  }

  VLOG(1) << "Removing remote GATT characteristic from service: "
          << GetIdentifier() << ", UUID: " << GetUUID().canonical_value();

  BluetoothRemoteGattCharacteristicChromeOS* characteristic = iter->second;
  DCHECK(characteristic->object_path() == object_path);
  characteristics_.erase(iter);

  FOR_EACH_OBSERVER(device::BluetoothGattService::Observer, observers_,
                    GattCharacteristicRemoved(this, characteristic));
  NotifyServiceChanged();

  delete characteristic;
}

void BluetoothRemoteGattServiceChromeOS::GattCharacteristicPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  if (characteristics_.find(object_path) == characteristics_.end()) {
    VLOG(2) << "Properties of unknown characteristic changed";
    return;
  }

  // We may receive a property changed event in certain cases, e.g. when the
  // characteristic "Flags" property has been updated with values from the
  // "Characteristic Extended Properties" descriptor. In this case, kick off
  // a service changed observer event to let observers refresh the
  // characteristics.
  BluetoothGattCharacteristicClient::Properties* properties =
      DBusThreadManager::Get()->GetBluetoothGattCharacteristicClient()->
          GetProperties(object_path);
  DCHECK(properties);
  if (property_name != properties->flags.name())
    return;

  NotifyServiceChanged();
}

}  // namespace chromeos
