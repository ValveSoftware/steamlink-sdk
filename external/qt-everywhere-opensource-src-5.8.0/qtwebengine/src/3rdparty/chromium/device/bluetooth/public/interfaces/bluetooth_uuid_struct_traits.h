// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_PUBLIC_INTERFACES_BLUETOOTH_UUID_STRUCT_TRAITS_H_
#define DEVICE_BLUETOOTH_PUBLIC_INTERFACES_BLUETOOTH_UUID_STRUCT_TRAITS_H_

#include <string>

#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "device/bluetooth/public/interfaces/bluetooth_uuid.mojom.h"

namespace mojo {

template <>
struct StructTraits<device::mojom::BluetoothUUID,
                    base::Optional<device::BluetoothUUID>> {
  static bool IsNull(const base::Optional<device::BluetoothUUID>& uuid) {
    return !uuid;
  }

  static void SetToNull(base::Optional<device::BluetoothUUID>* output) {
    *output = base::nullopt;
  }

  static std::string uuid(const base::Optional<device::BluetoothUUID>& uuid) {
    DCHECK(uuid);
    return uuid->canonical_value();
  }

  static bool Read(device::mojom::BluetoothUUIDDataView input,
                   base::Optional<device::BluetoothUUID>* output) {
    std::string result;
    if (!input.ReadUuid(&result))
      return false;
    *output = base::make_optional(device::BluetoothUUID(result));

    // If the format isn't 128-bit, .value() would return a different answer
    // than .canonical_value(). Then if browser-side code accidentally checks
    // .value() against a 128-bit string literal, a hostile renderer could use
    // the 16- or 32-bit format and evade the check.
    return output->value().IsValid() &&
           output->value().format() == device::BluetoothUUID::kFormat128Bit;
  }
};

}  // namespace mojo

#endif  // DEVICE_BLUETOOTH_PUBLIC_INTERFACES_BLUETOOTH_UUID_STRUCT_TRAITS_H_
