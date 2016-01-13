// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_GATT_CONNECTION_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_GATT_CONNECTION_H_

#include <string>

#include "base/callback.h"

namespace device {

// BluetoothGattConnection represents a GATT connection to a Bluetooth device
// that has GATT services. Instances are obtained from a BluetoothDevice,
// and the connection is kept alive as long as there is at least one
// active BluetoothGattConnection object. BluetoothGattConnection objects
// automatically update themselves, when the connection is terminated by the
// operating system (e.g. due to user action).
class BluetoothGattConnection {
 public:
  // Destructor automatically closes this GATT connection. If this is the last
  // remaining GATT connection and this results in a call to the OS, that call
  // may not always succeed. Users can make an explicit call to
  // BluetoothGattConnection::Close to make sure that they are notified of
  // a possible error via the callback.
  virtual ~BluetoothGattConnection();

  // Returns the Bluetooth address of the device that this connection is open
  // to.
  virtual std::string GetDeviceAddress() const = 0;

  // Returns true if this connection is open.
  virtual bool IsConnected() = 0;

  // Disconnects this GATT connection and calls |callback| upon completion.
  // After a successful invocation, the device may still remain connected due to
  // other GATT connections.
  virtual void Disconnect(const base::Closure& callback) = 0;

 protected:
  BluetoothGattConnection();

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothGattConnection);
};

}  // namespace device

#endif  //  DEVICE_BLUETOOTH_BLUETOOTH_GATT_CONNECTION_H_
