// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DEVICE_H_
#define DEVICE_BLUETOOTH_DEVICE_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/public/interfaces/device.mojom.h"

namespace bluetooth {

// Implementation of Mojo Device located in
// device/bluetooth/public/interfaces/device.mojom.
// It handles requests to interact with Bluetooth Device.
// Uses the platform abstraction of device/bluetooth.
class Device : public mojom::Device {
 public:
  Device(const std::string& address,
         scoped_refptr<device::BluetoothAdapter> adapter);
  ~Device() override;

  // Creates a mojom::DeviceInfo using info from the given |device|.
  static mojom::DeviceInfoPtr ConstructDeviceInfoStruct(
      const device::BluetoothDevice* device);

  // mojom::Device overrides:
  void GetInfo(const GetInfoCallback& callback) override;

 private:
  // The address of the Bluetooth device.
  std::string address_;

  // The current BluetoothAdapter.
  scoped_refptr<device::BluetoothAdapter> adapter_;

  DISALLOW_COPY_AND_ASSIGN(Device);
};

}  // namespace bluetooth

#endif  // DEVICE_BLUETOOTH_DEVICE_H_
