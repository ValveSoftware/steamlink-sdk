// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_MAC_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_MAC_H_

#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"

#include "base/mac/scoped_nsobject.h"
#include "base/memory/weak_ptr.h"

#if defined(__OBJC__)
#import <CoreBluetooth/CoreBluetooth.h>
#else
@class CBCharacteristic;
typedef NS_ENUM(NSInteger, CBCharacteristicWriteType);
#endif  // defined(__OBJC__)

namespace device {

class BluetoothRemoteGattServiceMac;

// The BluetoothRemoteGattCharacteristicMac class implements
// BluetoothRemoteGattCharacteristic for remote GATT services on OS X.
class DEVICE_BLUETOOTH_EXPORT BluetoothRemoteGattCharacteristicMac
    : public BluetoothRemoteGattCharacteristic {
 public:
  BluetoothRemoteGattCharacteristicMac(
      BluetoothRemoteGattServiceMac* gatt_service,
      CBCharacteristic* cb_characteristic);
  ~BluetoothRemoteGattCharacteristicMac() override;

  // Override BluetoothGattCharacteristic methods.
  std::string GetIdentifier() const override;
  BluetoothUUID GetUUID() const override;
  Properties GetProperties() const override;
  Permissions GetPermissions() const override;

  // Override BluetoothRemoteGattCharacteristic methods.
  const std::vector<uint8_t>& GetValue() const override;
  BluetoothRemoteGattService* GetService() const override;
  bool IsNotifying() const override;
  std::vector<BluetoothRemoteGattDescriptor*> GetDescriptors() const override;
  BluetoothRemoteGattDescriptor* GetDescriptor(
      const std::string& identifier) const override;
  void StartNotifySession(const NotifySessionCallback& callback,
                          const ErrorCallback& error_callback) override;
  void ReadRemoteCharacteristic(const ValueCallback& callback,
                                const ErrorCallback& error_callback) override;
  void WriteRemoteCharacteristic(const std::vector<uint8_t>& new_value,
                                 const base::Closure& callback,
                                 const ErrorCallback& error_callback) override;

  DISALLOW_COPY_AND_ASSIGN(BluetoothRemoteGattCharacteristicMac);

 private:
  friend class BluetoothRemoteGattServiceMac;
  friend class BluetoothTestMac;

  // Called by the BluetoothRemoteGattServiceMac instance when the
  // characteristics value has been read.
  void DidUpdateValue(NSError* error);
  // Updates value_ and notifies the adapter of the new value.
  void UpdateValueAndNotify();
  // Called by the BluetoothRemoteGattServiceMac instance when the
  // characteristics value has been written.
  void DidWriteValue(NSError* error);
  // Called by the BluetoothRemoteGattServiceMac instance when the notify
  // session has been started or failed to be started.
  void DidUpdateNotificationState(NSError* error);
  // Returns true if the characteristic is readable.
  bool IsReadable() const;
  // Returns true if the characteristic is writable.
  bool IsWritable() const;
  // Returns true if the characteristic supports notifications or indications.
  bool SupportsNotificationsOrIndications() const;
  // Returns the write type (with or without responses).
  CBCharacteristicWriteType GetCBWriteType() const;
  // Returns CoreBluetooth characteristic.
  CBCharacteristic* GetCBCharacteristic() const;

  // gatt_service_ owns instances of this class.
  BluetoothRemoteGattServiceMac* gatt_service_;
  // A characteristic from CBPeripheral.services.characteristics.
  base::scoped_nsobject<CBCharacteristic> cb_characteristic_;
  // Characteristic identifier.
  std::string identifier_;
  // Service UUID.
  BluetoothUUID uuid_;
  // Characteristic value.
  std::vector<uint8_t> value_;
  // True if a gatt read or write request is in progress.
  bool characteristic_value_read_or_write_in_progress_;
  // ReadRemoteCharacteristic request callbacks.
  std::pair<ValueCallback, ErrorCallback> read_characteristic_value_callbacks_;
  // WriteRemoteCharacteristic request callbacks.
  std::pair<base::Closure, ErrorCallback> write_characteristic_value_callbacks_;
  // Stores StartNotifySession request callbacks.
  typedef std::pair<NotifySessionCallback, ErrorCallback>
      PendingStartNotifyCall;
  std::vector<PendingStartNotifyCall> start_notify_session_callbacks_;
  // Flag indicates if GATT event registration is in progress.
  bool start_notifications_in_progress_;
  base::WeakPtrFactory<BluetoothRemoteGattCharacteristicMac> weak_ptr_factory_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_MAC_H_
