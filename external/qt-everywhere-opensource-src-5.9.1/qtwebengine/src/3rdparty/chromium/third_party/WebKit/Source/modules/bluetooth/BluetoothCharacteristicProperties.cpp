// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/bluetooth/BluetoothCharacteristicProperties.h"

namespace blink {

BluetoothCharacteristicProperties* BluetoothCharacteristicProperties::create(
    uint32_t properties) {
  return new BluetoothCharacteristicProperties(properties);
}

bool BluetoothCharacteristicProperties::broadcast() const {
  return properties & Property::Broadcast;
}

bool BluetoothCharacteristicProperties::read() const {
  return properties & Property::Read;
}

bool BluetoothCharacteristicProperties::writeWithoutResponse() const {
  return properties & Property::WriteWithoutResponse;
}

bool BluetoothCharacteristicProperties::write() const {
  return properties & Property::Write;
}

bool BluetoothCharacteristicProperties::notify() const {
  return properties & Property::Notify;
}

bool BluetoothCharacteristicProperties::indicate() const {
  return properties & Property::Indicate;
}

bool BluetoothCharacteristicProperties::authenticatedSignedWrites() const {
  return properties & Property::AuthenticatedSignedWrites;
}

bool BluetoothCharacteristicProperties::reliableWrite() const {
  return properties & Property::ReliableWrite;
}

bool BluetoothCharacteristicProperties::writableAuxiliaries() const {
  return properties & Property::WritableAuxiliaries;
}

BluetoothCharacteristicProperties::BluetoothCharacteristicProperties(
    uint32_t device_properties) {
  ASSERT(device_properties != Property::None);
  properties = device_properties;
}

}  // namespace blink
