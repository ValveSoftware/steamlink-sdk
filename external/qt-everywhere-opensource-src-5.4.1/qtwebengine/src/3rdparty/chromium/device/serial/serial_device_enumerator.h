// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_SERIAL_SERIAL_DEVICE_ENUMERATOR_H_
#define DEVICE_SERIAL_SERIAL_DEVICE_ENUMERATOR_H_

#include "base/memory/scoped_ptr.h"
#include "device/serial/serial.mojom.h"
#include "mojo/public/cpp/bindings/array.h"

namespace device {

// Discovers and enumerates serial devices available to the host.
class SerialDeviceEnumerator {
 public:
  static scoped_ptr<SerialDeviceEnumerator> Create();

  SerialDeviceEnumerator();
  virtual ~SerialDeviceEnumerator();

  virtual mojo::Array<SerialDeviceInfoPtr> GetDevices() = 0;
};

}  // namespace device

#endif  // DEVICE_SERIAL_SERIAL_DEVICE_ENUMERATOR_H_
