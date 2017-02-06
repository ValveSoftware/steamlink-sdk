// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/macros.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace device {

class BluetoothGattNotifySession;
class BluetoothRemoteGattDescriptor;
class BluetoothRemoteGattService;

// BluetoothRemoteGattCharacteristic represents a remote GATT characteristic.
// This class is used to represent GATT characteristics that belong to a service
// hosted by a remote device. In this case the characteristic will be
// constructed by the subsystem.
//
// Note: We use virtual inheritance on the GATT characteristic since it will be
// inherited by platform specific versions of the GATT characteristic classes
// also. The platform specific remote GATT characteristic classes will inherit
// both this class and their GATT characteristic class, hence causing an
// inheritance diamond.
class DEVICE_BLUETOOTH_EXPORT BluetoothRemoteGattCharacteristic
    : public virtual BluetoothGattCharacteristic {
 public:
  // The ValueCallback is used to return the value of a remote characteristic
  // upon a read request.
  typedef base::Callback<void(const std::vector<uint8_t>&)> ValueCallback;

  // The NotifySessionCallback is used to return sessions after they have
  // been successfully started.
  typedef base::Callback<void(std::unique_ptr<BluetoothGattNotifySession>)>
      NotifySessionCallback;

  // Returns the value of the characteristic. For remote characteristics, this
  // is the most recently cached value. For local characteristics, this is the
  // most recently updated value or the value retrieved from the delegate.
  virtual const std::vector<uint8_t>& GetValue() const = 0;

  // Returns a pointer to the GATT service this characteristic belongs to.
  virtual BluetoothRemoteGattService* GetService() const = 0;

  // Returns whether or not this characteristic is currently sending value
  // updates in the form of a notification or indication.
  virtual bool IsNotifying() const = 0;

  // Returns the list of GATT characteristic descriptors that provide more
  // information about this characteristic.
  virtual std::vector<BluetoothRemoteGattDescriptor*> GetDescriptors()
      const = 0;

  // Returns the GATT characteristic descriptor with identifier |identifier| if
  // it belongs to this GATT characteristic.
  virtual BluetoothRemoteGattDescriptor* GetDescriptor(
      const std::string& identifier) const = 0;

  // Returns the GATT characteristic descriptors that match |uuid|. There may be
  // multiple, as illustrated by Core Bluetooth Specification [V4.2 Vol 3 Part G
  // 3.3.3.5 Characteristic Presentation Format].
  std::vector<BluetoothRemoteGattDescriptor*> GetDescriptorsByUUID(
      const BluetoothUUID& uuid);

  // Starts a notify session for the remote characteristic, if it supports
  // notifications/indications. On success, the characteristic starts sending
  // value notifications and |callback| is called with a session object whose
  // ownership belongs to the caller. |error_callback| is called on errors.
  //
  // Writes to the Client Characteristic Configuration descriptor to enable
  // notifications/indications. Core Bluetooth Specification [V4.2 Vol 3 Part G
  // Section 3.3.1.1. Characteristic Properties] requires this descriptor to be
  // present when notifications/indications are supported. If the descriptor is
  // not present |error_callback| will be run.
  virtual void StartNotifySession(const NotifySessionCallback& callback,
                                  const ErrorCallback& error_callback) = 0;

  // Sends a read request to a remote characteristic to read its value.
  // |callback| is called to return the read value on success and
  // |error_callback| is called for failures.
  virtual void ReadRemoteCharacteristic(
      const ValueCallback& callback,
      const ErrorCallback& error_callback) = 0;

  // Sends a write request to a remote characteristic, to modify the
  // characteristic's value with the new value |new_value|. |callback| is
  // called to signal success and |error_callback| for failures. This method
  // only applies to remote characteristics and will fail for those that are
  // locally hosted.
  virtual void WriteRemoteCharacteristic(
      const std::vector<uint8_t>& new_value,
      const base::Closure& callback,
      const ErrorCallback& error_callback) = 0;

 protected:
  BluetoothRemoteGattCharacteristic();
  ~BluetoothRemoteGattCharacteristic() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothRemoteGattCharacteristic);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_H_
