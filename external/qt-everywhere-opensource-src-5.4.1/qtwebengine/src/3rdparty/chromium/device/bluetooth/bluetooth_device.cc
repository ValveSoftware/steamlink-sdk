// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_device.h"

#include <string>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "device/bluetooth/bluetooth_gatt_service.h"
#include "grit/device_bluetooth_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace device {

BluetoothDevice::BluetoothDevice() {
}

BluetoothDevice::~BluetoothDevice() {
  STLDeleteValues(&gatt_services_);
}

base::string16 BluetoothDevice::GetName() const {
  std::string name = GetDeviceName();
  if (!name.empty()) {
    return base::UTF8ToUTF16(name);
  } else {
    return GetAddressWithLocalizedDeviceTypeName();
  }
}

base::string16 BluetoothDevice::GetAddressWithLocalizedDeviceTypeName() const {
  base::string16 address_utf16 = base::UTF8ToUTF16(GetAddress());
  BluetoothDevice::DeviceType device_type = GetDeviceType();
  switch (device_type) {
    case DEVICE_COMPUTER:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_COMPUTER,
                                        address_utf16);
    case DEVICE_PHONE:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_PHONE,
                                        address_utf16);
    case DEVICE_MODEM:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_MODEM,
                                        address_utf16);
    case DEVICE_AUDIO:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_AUDIO,
                                        address_utf16);
    case DEVICE_CAR_AUDIO:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_CAR_AUDIO,
                                        address_utf16);
    case DEVICE_VIDEO:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_VIDEO,
                                        address_utf16);
    case DEVICE_JOYSTICK:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_JOYSTICK,
                                        address_utf16);
    case DEVICE_GAMEPAD:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_GAMEPAD,
                                        address_utf16);
    case DEVICE_KEYBOARD:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_KEYBOARD,
                                        address_utf16);
    case DEVICE_MOUSE:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_MOUSE,
                                        address_utf16);
    case DEVICE_TABLET:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_TABLET,
                                        address_utf16);
    case DEVICE_KEYBOARD_MOUSE_COMBO:
      return l10n_util::GetStringFUTF16(
          IDS_BLUETOOTH_DEVICE_KEYBOARD_MOUSE_COMBO, address_utf16);
    default:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_UNKNOWN,
                                        address_utf16);
  }
}

BluetoothDevice::DeviceType BluetoothDevice::GetDeviceType() const {
  // https://www.bluetooth.org/Technical/AssignedNumbers/baseband.htm
  uint32 bluetooth_class = GetBluetoothClass();
  switch ((bluetooth_class & 0x1f00) >> 8) {
    case 0x01:
      // Computer major device class.
      return DEVICE_COMPUTER;
    case 0x02:
      // Phone major device class.
      switch ((bluetooth_class & 0xfc) >> 2) {
        case 0x01:
        case 0x02:
        case 0x03:
          // Cellular, cordless and smart phones.
          return DEVICE_PHONE;
        case 0x04:
        case 0x05:
          // Modems: wired or voice gateway and common ISDN access.
          return DEVICE_MODEM;
      }
      break;
    case 0x04:
      // Audio major device class.
      switch ((bluetooth_class & 0xfc) >> 2) {
        case 0x08:
          // Car audio.
          return DEVICE_CAR_AUDIO;
        case 0x0b:
        case 0x0c:
        case 0x0d:
        case 0x0e:
        case 0x0f:
        case 0x010:
          // Video devices.
          return DEVICE_VIDEO;
        default:
          return DEVICE_AUDIO;
      }
      break;
    case 0x05:
      // Peripheral major device class.
      switch ((bluetooth_class & 0xc0) >> 6) {
        case 0x00:
          // "Not a keyboard or pointing device."
          switch ((bluetooth_class & 0x01e) >> 2) {
            case 0x01:
              // Joystick.
              return DEVICE_JOYSTICK;
            case 0x02:
              // Gamepad.
              return DEVICE_GAMEPAD;
            default:
              return DEVICE_PERIPHERAL;
          }
          break;
        case 0x01:
          // Keyboard.
          return DEVICE_KEYBOARD;
        case 0x02:
          // Pointing device.
          switch ((bluetooth_class & 0x01e) >> 2) {
            case 0x05:
              // Digitizer tablet.
              return DEVICE_TABLET;
            default:
              // Mouse.
              return DEVICE_MOUSE;
          }
          break;
        case 0x03:
          // Combo device.
          return DEVICE_KEYBOARD_MOUSE_COMBO;
      }
      break;
  }

  return DEVICE_UNKNOWN;
}

bool BluetoothDevice::IsPairable() const {
  DeviceType type = GetDeviceType();

  // Get the vendor part of the address: "00:11:22" for "00:11:22:33:44:55"
  std::string vendor = GetAddress().substr(0, 8);

  // Verbatim "Bluetooth Mouse", model 96674
  if (type == DEVICE_MOUSE && vendor == "00:12:A1")
    return false;
  // Microsoft "Microsoft Bluetooth Notebook Mouse 5000", model X807028-001
  if (type == DEVICE_MOUSE && vendor == "7C:ED:8D")
    return false;
  // Sony PlayStation Dualshock3
  if (IsTrustable())
    return false;

  // TODO: Move this database into a config file.

  return true;
}

bool BluetoothDevice::IsTrustable() const {
  // Sony PlayStation Dualshock3
  if ((GetVendorID() == 0x054c && GetProductID() == 0x0268 &&
       GetDeviceName() == "PLAYSTATION(R)3 Controller"))
    return true;

  return false;
}

std::vector<BluetoothGattService*>
    BluetoothDevice::GetGattServices() const {
  std::vector<BluetoothGattService*> services;
  for (GattServiceMap::const_iterator iter = gatt_services_.begin();
       iter != gatt_services_.end(); ++iter)
    services.push_back(iter->second);
  return services;
}

BluetoothGattService* BluetoothDevice::GetGattService(
    const std::string& identifier) const {
  GattServiceMap::const_iterator iter = gatt_services_.find(identifier);
  if (iter != gatt_services_.end())
    return iter->second;
  return NULL;
}

// static
std::string BluetoothDevice::CanonicalizeAddress(const std::string& address) {
  std::string canonicalized = address;
  if (address.size() == 12) {
    // Might be an address in the format "1A2B3C4D5E6F". Add separators.
    for (size_t i = 2; i < canonicalized.size(); i += 3) {
      canonicalized.insert(i, ":");
    }
  }

  // Verify that the length matches the canonical format "1A:2B:3C:4D:5E:6F".
  const size_t kCanonicalAddressLength = 17;
  if (canonicalized.size() != kCanonicalAddressLength)
    return std::string();

  const char separator = canonicalized[2];
  for (size_t i = 0; i < canonicalized.size(); ++i) {
    bool is_separator = (i + 1) % 3 == 0;
    if (is_separator) {
      // All separators in the input |address| should be consistent.
      if (canonicalized[i] != separator)
        return std::string();

      canonicalized[i] = ':';
    } else {
      if (!IsHexDigit(canonicalized[i]))
        return std::string();

      canonicalized[i] = base::ToUpperASCII(canonicalized[i]);
    }
  }

  return canonicalized;
}

}  // namespace device
