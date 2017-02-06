// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_TYPES_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_TYPES_H_

#include "device/bluetooth/bluetooth_export.h"

// This file is for enums and small types common to several
// parts of bluetooth.

namespace device {

// Devices and adapters can support a number of transports,
// and bluetooth hosts can scan for devices based on the
// transports they support.
enum BluetoothTransport : uint8_t {
  BLUETOOTH_TRANSPORT_INVALID = 0x00,
  // Valid transports are given as a bitset.
  BLUETOOTH_TRANSPORT_CLASSIC = 0x01,
  BLUETOOTH_TRANSPORT_LE = 0x02,
  BLUETOOTH_TRANSPORT_DUAL =
      (BLUETOOTH_TRANSPORT_CLASSIC | BLUETOOTH_TRANSPORT_LE)
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_TYPES_H_
