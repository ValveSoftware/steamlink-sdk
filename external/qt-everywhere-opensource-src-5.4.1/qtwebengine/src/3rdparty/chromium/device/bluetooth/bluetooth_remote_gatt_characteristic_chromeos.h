// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_CHROMEOS_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_CHROMEOS_H_

#include <map>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/bluetooth_gatt_characteristic_client.h"
#include "chromeos/dbus/bluetooth_gatt_descriptor_client.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace device {

class BluetoothGattDescriptor;
class BluetoothGattService;

}  // namespace device

namespace chromeos {

class BluetoothRemoteGattDescriptorChromeOS;
class BluetoothRemoteGattServiceChromeOS;

// The BluetoothRemoteGattCharacteristicChromeOS class implements
// BluetoothGattCharacteristic for remote GATT characteristics on the Chrome OS
// platform.
class BluetoothRemoteGattCharacteristicChromeOS
    : public device::BluetoothGattCharacteristic,
      public BluetoothGattCharacteristicClient::Observer,
      public BluetoothGattDescriptorClient::Observer {
 public:
  // device::BluetoothGattCharacteristic overrides.
  virtual std::string GetIdentifier() const OVERRIDE;
  virtual device::BluetoothUUID GetUUID() const OVERRIDE;
  virtual bool IsLocal() const OVERRIDE;
  virtual const std::vector<uint8>& GetValue() const OVERRIDE;
  virtual device::BluetoothGattService* GetService() const OVERRIDE;
  virtual Properties GetProperties() const OVERRIDE;
  virtual Permissions GetPermissions() const OVERRIDE;
  virtual bool IsNotifying() const OVERRIDE;
  virtual std::vector<device::BluetoothGattDescriptor*>
      GetDescriptors() const OVERRIDE;
  virtual device::BluetoothGattDescriptor* GetDescriptor(
      const std::string& identifier) const OVERRIDE;
  virtual bool AddDescriptor(
      device::BluetoothGattDescriptor* descriptor) OVERRIDE;
  virtual bool UpdateValue(const std::vector<uint8>& value) OVERRIDE;
  virtual void ReadRemoteCharacteristic(
      const ValueCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void WriteRemoteCharacteristic(
      const std::vector<uint8>& new_value,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void StartNotifySession(const NotifySessionCallback& callback,
                                  const ErrorCallback& error_callback) OVERRIDE;

  // Removes one value update session and invokes |callback| on completion. This
  // decrements the session reference count by 1 and if the number reaches 0,
  // makes a call to the subsystem to stop notifications from this
  // characteristic.
  void RemoveNotifySession(const base::Closure& callback);

  // Object path of the underlying D-Bus characteristic.
  const dbus::ObjectPath& object_path() const { return object_path_; }

 private:
  friend class BluetoothRemoteGattServiceChromeOS;

  BluetoothRemoteGattCharacteristicChromeOS(
      BluetoothRemoteGattServiceChromeOS* service,
      const dbus::ObjectPath& object_path);
  virtual ~BluetoothRemoteGattCharacteristicChromeOS();

  // BluetoothGattCharacteristicClient::Observer overrides.
  virtual void GattCharacteristicValueUpdated(
      const dbus::ObjectPath& object_path,
      const std::vector<uint8>& value) OVERRIDE;

  // BluetoothGattDescriptorClient::Observer overrides.
  virtual void GattDescriptorAdded(
      const dbus::ObjectPath& object_path) OVERRIDE;
  virtual void GattDescriptorRemoved(
      const dbus::ObjectPath& object_path) OVERRIDE;

  // Called by dbus:: on successful completion of a request to read
  // the characteristic value.
  void OnValueSuccess(const ValueCallback& callback,
                      const std::vector<uint8>& value);

  // Called by dbus:: on unsuccessful completion of a request to read or write
  // the characteristic value.
  void OnError(const ErrorCallback& error_callback,
               const std::string& error_name,
               const std::string& error_message);

  // Called by dbus:: on successful completion of a request to start
  // notifications.
  void OnStartNotifySuccess(const NotifySessionCallback& callback);

  // Called by dbus:: on unsuccessful completion of a request to start
  // notifications.
  void OnStartNotifyError(const ErrorCallback& error_callback,
                          const std::string& error_name,
                          const std::string& error_message);

  // Called by dbus:: on successful completion of a request to stop
  // notifications.
  void OnStopNotifySuccess(const base::Closure& callback);

  // Called by dbus:: on unsuccessful completion of a request to stop
  // notifications.
  void OnStopNotifyError(const base::Closure& callback,
                         const std::string& error_name,
                         const std::string& error_message);

  // Calls StartNotifySession for each queued request.
  void ProcessStartNotifyQueue();

  // Object path of the D-Bus characteristic object.
  dbus::ObjectPath object_path_;

  // The GATT service this GATT characteristic belongs to.
  BluetoothRemoteGattServiceChromeOS* service_;

  // The cached characteristic value based on the most recent read or
  // notification.
  std::vector<uint8> cached_value_;

  // The total number of currently active value update sessions.
  size_t num_notify_sessions_;

  // Calls to StartNotifySession that are pending. This can happen during the
  // first remote call to start notifications.
  typedef std::pair<NotifySessionCallback, ErrorCallback>
      PendingStartNotifyCall;
  std::queue<PendingStartNotifyCall> pending_start_notify_calls_;

  // True, if a Start or Stop notify call to bluetoothd is currently pending.
  bool notify_call_pending_;

  // Mapping from GATT descriptor object paths to descriptor objects owned by
  // this characteristic. Since the Chrome OS implementation uses object paths
  // as unique identifiers, we also use this mapping to return descriptors by
  // identifier.
  typedef std::map<dbus::ObjectPath, BluetoothRemoteGattDescriptorChromeOS*>
      DescriptorMap;
  DescriptorMap descriptors_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothRemoteGattCharacteristicChromeOS>
      weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothRemoteGattCharacteristicChromeOS);
};

}  // namespace chromeos

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_CHROMEOS_H_
