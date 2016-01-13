// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_SERVICE_CHROMEOS_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_SERVICE_CHROMEOS_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/dbus/bluetooth_gatt_characteristic_client.h"
#include "chromeos/dbus/bluetooth_gatt_service_client.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_gatt_service.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace device {

class BluetoothAdapter;
class BluetoothGattCharacteristic;

}  // namespace device

namespace chromeos {

class BluetoothAdapterChromeOS;
class BluetoothDeviceChromeOS;
class BluetoothRemoteGattCharacteristicChromeOS;
class BluetoothRemoteGattDescriptorChromeOS;

// The BluetoothRemoteGattServiceChromeOS class implements BluetootGattService
// for remote GATT services on the the Chrome OS platform.
class BluetoothRemoteGattServiceChromeOS
    : public device::BluetoothGattService,
      public BluetoothGattServiceClient::Observer,
      public BluetoothGattCharacteristicClient::Observer {
 public:
  // device::BluetoothGattService overrides.
  virtual void AddObserver(
      device::BluetoothGattService::Observer* observer) OVERRIDE;
  virtual void RemoveObserver(
      device::BluetoothGattService::Observer* observer) OVERRIDE;
  virtual std::string GetIdentifier() const OVERRIDE;
  virtual device::BluetoothUUID GetUUID() const OVERRIDE;
  virtual bool IsLocal() const OVERRIDE;
  virtual bool IsPrimary() const OVERRIDE;
  virtual device::BluetoothDevice* GetDevice() const OVERRIDE;
  virtual std::vector<device::BluetoothGattCharacteristic*>
      GetCharacteristics() const OVERRIDE;
  virtual std::vector<device::BluetoothGattService*>
      GetIncludedServices() const OVERRIDE;
  virtual device::BluetoothGattCharacteristic* GetCharacteristic(
      const std::string& identifier) const OVERRIDE;
  virtual bool AddCharacteristic(
      device::BluetoothGattCharacteristic* characteristic) OVERRIDE;
  virtual bool AddIncludedService(
      device::BluetoothGattService* service) OVERRIDE;
  virtual void Register(const base::Closure& callback,
                        const ErrorCallback& error_callback) OVERRIDE;
  virtual void Unregister(const base::Closure& callback,
                          const ErrorCallback& error_callback) OVERRIDE;

  // Object path of the underlying service.
  const dbus::ObjectPath& object_path() const { return object_path_; }

  // Returns the adapter associated with this service.
  scoped_refptr<device::BluetoothAdapter> GetAdapter() const;

  // Notifies its observers that the GATT service has changed. This is mainly
  // used by BluetoothRemoteGattCharacteristicChromeOS instances to notify
  // service observers when characteristic descriptors get added and removed.
  void NotifyServiceChanged();

  // Notifies its observers that the value of a characteristic has changed.
  // Called by BluetoothRemoteGattCharacteristicChromeOS instances to notify
  // service observers when their cached value is updated after a successful
  // read request or when a "ValueUpdated" signal is received.
  void NotifyCharacteristicValueChanged(
      BluetoothRemoteGattCharacteristicChromeOS* characteristic,
      const std::vector<uint8>& value);

  // Notifies its observers that a descriptor |descriptor| belonging to
  // characteristic |characteristic| has been added or removed. This is used
  // by BluetoothRemoteGattCharacteristicChromeOS instances to notify service
  // observers when characteristic descriptors get added and removed. If |added|
  // is true, an "Added" event will be sent. Otherwise, a "Removed" event will
  // be sent.
  void NotifyDescriptorAddedOrRemoved(
      BluetoothRemoteGattCharacteristicChromeOS* characteristic,
      BluetoothRemoteGattDescriptorChromeOS* descriptor,
      bool added);

  // Notifies its observers that the value of a descriptor has changed. Called
  // by BluetoothRemoteGattDescriptorChromeOS instances to notify service
  // observers when their cached value gets updated after a read request.
  void NotifyDescriptorValueChanged(
      BluetoothRemoteGattCharacteristicChromeOS* characteristic,
      BluetoothRemoteGattDescriptorChromeOS* descriptor,
      const std::vector<uint8>& value);

 private:
  friend class BluetoothDeviceChromeOS;

  BluetoothRemoteGattServiceChromeOS(BluetoothAdapterChromeOS* adapter,
                                     BluetoothDeviceChromeOS* device,
                                     const dbus::ObjectPath& object_path);
  virtual ~BluetoothRemoteGattServiceChromeOS();

  // BluetoothGattServiceClient::Observer override.
  virtual void GattServicePropertyChanged(
      const dbus::ObjectPath& object_path,
      const std::string& property_name) OVERRIDE;

  // BluetoothGattCharacteristicClient::Observer override.
  virtual void GattCharacteristicAdded(
      const dbus::ObjectPath& object_path) OVERRIDE;
  virtual void GattCharacteristicRemoved(
      const dbus::ObjectPath& object_path) OVERRIDE;
  virtual void GattCharacteristicPropertyChanged(
      const dbus::ObjectPath& object_path,
      const std::string& property_name) OVERRIDE;

  // Object path of the GATT service.
  dbus::ObjectPath object_path_;

  // List of observers interested in event notifications from us.
  ObserverList<device::BluetoothGattService::Observer> observers_;

  // The adapter associated with this service. It's ok to store a raw pointer
  // here since |adapter_| indirectly owns this instance.
  BluetoothAdapterChromeOS* adapter_;

  // The device this GATT service belongs to. It's ok to store a raw pointer
  // here since |device_| owns this instance.
  BluetoothDeviceChromeOS* device_;

  // Mapping from GATT characteristic object paths to characteristic objects.
  // owned by this service. Since the Chrome OS implementation uses object
  // paths as unique identifiers, we also use this mapping to return
  // characteristics by identifier.
  typedef std::map<dbus::ObjectPath, BluetoothRemoteGattCharacteristicChromeOS*>
      CharacteristicMap;
  CharacteristicMap characteristics_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothRemoteGattServiceChromeOS> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothRemoteGattServiceChromeOS);
};

}  // namespace chromeos

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_SERVICE_CHROMEOS_H_
