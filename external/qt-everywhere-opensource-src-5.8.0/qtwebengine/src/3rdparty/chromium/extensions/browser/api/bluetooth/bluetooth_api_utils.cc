// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/bluetooth/bluetooth_api_utils.h"

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "extensions/common/api/bluetooth.h"

namespace bluetooth = extensions::api::bluetooth;

using device::BluetoothDevice;
using bluetooth::VendorIdSource;

namespace {

bool ConvertVendorIDSourceToApi(const BluetoothDevice::VendorIDSource& input,
                                bluetooth::VendorIdSource* output) {
  switch (input) {
    case BluetoothDevice::VENDOR_ID_UNKNOWN:
      *output = bluetooth::VENDOR_ID_SOURCE_NONE;
      return true;
    case BluetoothDevice::VENDOR_ID_BLUETOOTH:
      *output = bluetooth::VENDOR_ID_SOURCE_BLUETOOTH;
      return true;
    case BluetoothDevice::VENDOR_ID_USB:
      *output = bluetooth::VENDOR_ID_SOURCE_USB;
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

bool ConvertDeviceTypeToApi(const BluetoothDevice::DeviceType& input,
                            bluetooth::DeviceType* output) {
  switch (input) {
    case BluetoothDevice::DEVICE_UNKNOWN:
      *output = bluetooth::DEVICE_TYPE_NONE;
      return true;
    case BluetoothDevice::DEVICE_COMPUTER:
      *output = bluetooth::DEVICE_TYPE_COMPUTER;
      return true;
    case BluetoothDevice::DEVICE_PHONE:
      *output = bluetooth::DEVICE_TYPE_PHONE;
      return true;
    case BluetoothDevice::DEVICE_MODEM:
      *output = bluetooth::DEVICE_TYPE_MODEM;
      return true;
    case BluetoothDevice::DEVICE_AUDIO:
      *output = bluetooth::DEVICE_TYPE_AUDIO;
      return true;
    case BluetoothDevice::DEVICE_CAR_AUDIO:
      *output = bluetooth::DEVICE_TYPE_CARAUDIO;
      return true;
    case BluetoothDevice::DEVICE_VIDEO:
      *output = bluetooth::DEVICE_TYPE_VIDEO;
      return true;
    case BluetoothDevice::DEVICE_PERIPHERAL:
      *output = bluetooth::DEVICE_TYPE_PERIPHERAL;
      return true;
    case BluetoothDevice::DEVICE_JOYSTICK:
      *output = bluetooth::DEVICE_TYPE_JOYSTICK;
      return true;
    case BluetoothDevice::DEVICE_GAMEPAD:
      *output = bluetooth::DEVICE_TYPE_GAMEPAD;
      return true;
    case BluetoothDevice::DEVICE_KEYBOARD:
      *output = bluetooth::DEVICE_TYPE_KEYBOARD;
      return true;
    case BluetoothDevice::DEVICE_MOUSE:
      *output = bluetooth::DEVICE_TYPE_MOUSE;
      return true;
    case BluetoothDevice::DEVICE_TABLET:
      *output = bluetooth::DEVICE_TYPE_TABLET;
      return true;
    case BluetoothDevice::DEVICE_KEYBOARD_MOUSE_COMBO:
      *output = bluetooth::DEVICE_TYPE_KEYBOARDMOUSECOMBO;
      return true;
    default:
      return false;
  }
}

}  // namespace

namespace extensions {
namespace api {
namespace bluetooth {

void BluetoothDeviceToApiDevice(const device::BluetoothDevice& device,
                                Device* out) {
  out->address = device.GetAddress();
  out->name.reset(
      new std::string(base::UTF16ToUTF8(device.GetNameForDisplay())));
  out->device_class.reset(new int(device.GetBluetoothClass()));

  // Only include the Device ID members when one exists for the device, and
  // always include all or none.
  if (ConvertVendorIDSourceToApi(device.GetVendorIDSource(),
                                 &(out->vendor_id_source)) &&
      out->vendor_id_source != VENDOR_ID_SOURCE_NONE) {
    out->vendor_id.reset(new int(device.GetVendorID()));
    out->product_id.reset(new int(device.GetProductID()));
    out->device_id.reset(new int(device.GetDeviceID()));
  }

  ConvertDeviceTypeToApi(device.GetDeviceType(), &(out->type));

  out->paired.reset(new bool(device.IsPaired()));
  out->connected.reset(new bool(device.IsConnected()));
  out->connecting.reset(new bool(device.IsConnecting()));
  out->connectable.reset(new bool(device.IsConnectable()));

  std::vector<std::string>* string_uuids = new std::vector<std::string>();
  const device::BluetoothDevice::UUIDList& uuids = device.GetUUIDs();
  for (device::BluetoothDevice::UUIDList::const_iterator iter = uuids.begin();
       iter != uuids.end(); ++iter)
    string_uuids->push_back(iter->canonical_value());
  out->uuids.reset(string_uuids);

  if (device.GetInquiryRSSI() != device::BluetoothDevice::kUnknownPower)
    out->inquiry_rssi.reset(new int(device.GetInquiryRSSI()));
  else
    out->inquiry_rssi.reset();

  if (device.GetInquiryTxPower() != device::BluetoothDevice::kUnknownPower)
    out->inquiry_tx_power.reset(new int(device.GetInquiryTxPower()));
  else
    out->inquiry_tx_power.reset();
}

void PopulateAdapterState(const device::BluetoothAdapter& adapter,
                          AdapterState* out) {
  out->discovering = adapter.IsDiscovering();
  out->available = adapter.IsPresent();
  out->powered = adapter.IsPowered();
  out->name = adapter.GetName();
  out->address = adapter.GetAddress();
}

}  // namespace bluetooth
}  // namespace api
}  // namespace extensions
