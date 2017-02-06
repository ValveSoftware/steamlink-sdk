// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_BLUEZ_H_
#define DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_BLUEZ_H_

#include <stddef.h>
#include <stdint.h>
#include <map>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "device/bluetooth/bluez/bluetooth_gatt_characteristic_bluez.h"
#include "device/bluetooth/dbus/bluetooth_gatt_descriptor_client.h"

namespace device {

class BluetoothRemoteGattDescriptor;
class BluetoothRemoteGattService;

}  // namespace device

namespace bluez {

class BluetoothRemoteGattDescriptorBlueZ;
class BluetoothRemoteGattServiceBlueZ;

// The BluetoothRemoteGattCharacteristicBlueZ class implements
// BluetoothRemoteGattCharacteristic for remote GATT characteristics for
// platforms
// that use BlueZ.
class BluetoothRemoteGattCharacteristicBlueZ
    : public BluetoothGattCharacteristicBlueZ,
      public BluetoothGattDescriptorClient::Observer,
      public device::BluetoothRemoteGattCharacteristic {
 public:
  // device::BluetoothGattCharacteristic overrides.
  device::BluetoothUUID GetUUID() const override;
  Properties GetProperties() const override;
  Permissions GetPermissions() const override;

  // device::BluetoothRemoteGattCharacteristic overrides.
  const std::vector<uint8_t>& GetValue() const override;
  device::BluetoothRemoteGattService* GetService() const override;
  bool IsNotifying() const override;
  std::vector<device::BluetoothRemoteGattDescriptor*> GetDescriptors()
      const override;
  device::BluetoothRemoteGattDescriptor* GetDescriptor(
      const std::string& identifier) const override;
  void StartNotifySession(const NotifySessionCallback& callback,
                          const ErrorCallback& error_callback) override;
  void ReadRemoteCharacteristic(const ValueCallback& callback,
                                const ErrorCallback& error_callback) override;
  void WriteRemoteCharacteristic(const std::vector<uint8_t>& new_value,
                                 const base::Closure& callback,
                                 const ErrorCallback& error_callback) override;

  // Removes one value update session and invokes |callback| on completion. This
  // decrements the session reference count by 1 and if the number reaches 0,
  // makes a call to the subsystem to stop notifications from this
  // characteristic.
  void RemoveNotifySession(const base::Closure& callback);

 private:
  friend class BluetoothRemoteGattServiceBlueZ;

  using PendingStartNotifyCall =
      std::pair<NotifySessionCallback, ErrorCallback>;

  BluetoothRemoteGattCharacteristicBlueZ(
      BluetoothRemoteGattServiceBlueZ* service,
      const dbus::ObjectPath& object_path);
  ~BluetoothRemoteGattCharacteristicBlueZ() override;

  // bluez::BluetoothGattDescriptorClient::Observer overrides.
  void GattDescriptorAdded(const dbus::ObjectPath& object_path) override;
  void GattDescriptorRemoved(const dbus::ObjectPath& object_path) override;
  void GattDescriptorPropertyChanged(const dbus::ObjectPath& object_path,
                                     const std::string& property_name) override;

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

  // Called by dbus:: on unsuccessful completion of a request to read or write
  // the characteristic value.
  void OnError(const ErrorCallback& error_callback,
               const std::string& error_name,
               const std::string& error_message);

  // The total number of currently active value update sessions.
  size_t num_notify_sessions_;

  // Calls to StartNotifySession that are pending. This can happen during the
  // first remote call to start notifications.
  std::queue<PendingStartNotifyCall> pending_start_notify_calls_;

  // True, if a Start or Stop notify call to bluetoothd is currently pending.
  bool notify_call_pending_;

  // TODO(rkc): Investigate and fix ownership of the descriptor objects in this
  // map. See crbug.com/604166.
  using DescriptorMap =
      std::map<dbus::ObjectPath, BluetoothRemoteGattDescriptorBlueZ*>;

  // Mapping from GATT descriptor object paths to descriptor objects owned by
  // this characteristic. Since the BlueZ implementation uses object paths
  // as unique identifiers, we also use this mapping to return descriptors by
  // identifier.
  DescriptorMap descriptors_;

  // The GATT service this GATT characteristic belongs to.
  BluetoothRemoteGattServiceBlueZ* service_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothRemoteGattCharacteristicBlueZ>
      weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothRemoteGattCharacteristicBlueZ);
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_BLUEZ_H_
