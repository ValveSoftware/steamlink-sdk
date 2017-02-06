// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/usb_device_permission_data.h"

#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/usb_device_permission.h"

namespace {

const char* kProductIdKey = "productId";
const char* kVendorIdKey = "vendorId";
const char* kInterfaceIdKey = "interfaceId";

}  // namespace

namespace extensions {

UsbDevicePermissionData::UsbDevicePermissionData()
  : vendor_id_(0), product_id_(0), interface_id_(ANY_INTERFACE) {
}

UsbDevicePermissionData::UsbDevicePermissionData(uint16_t vendor_id,
                                                 uint16_t product_id,
                                                 int interface_id)
    : vendor_id_(vendor_id),
      product_id_(product_id),
      interface_id_(interface_id) {}

bool UsbDevicePermissionData::Check(
    const APIPermission::CheckParam* param) const {
  if (!param)
    return false;
  const UsbDevicePermission::CheckParam& specific_param =
      *static_cast<const UsbDevicePermission::CheckParam*>(param);
  return vendor_id_ == specific_param.vendor_id &&
         product_id_ == specific_param.product_id &&
         (specific_param.interface_id == UNSPECIFIED_INTERFACE ||
          interface_id_ == specific_param.interface_id);
}

std::unique_ptr<base::Value> UsbDevicePermissionData::ToValue() const {
  base::DictionaryValue* result = new base::DictionaryValue();
  result->SetInteger(kVendorIdKey, vendor_id_);
  result->SetInteger(kProductIdKey, product_id_);
  result->SetInteger(kInterfaceIdKey, interface_id_);
  return std::unique_ptr<base::Value>(result);
}

bool UsbDevicePermissionData::FromValue(const base::Value* value) {
  if (!value)
    return false;

  const base::DictionaryValue* dict_value;
  if (!value->GetAsDictionary(&dict_value))
    return false;

  int temp;
  if (!dict_value->GetInteger(kVendorIdKey, &temp))
    return false;
  if (temp < 0 || temp > std::numeric_limits<uint16_t>::max())
    return false;
  vendor_id_ = temp;

  if (!dict_value->GetInteger(kProductIdKey, &temp))
    return false;
  if (temp < 0 || temp > std::numeric_limits<uint16_t>::max())
    return false;
  product_id_ = temp;

  if (!dict_value->GetInteger(kInterfaceIdKey, &temp))
    interface_id_ = ANY_INTERFACE;
  else if (temp < ANY_INTERFACE || temp > std::numeric_limits<uint8_t>::max())
    return false;
  else
    interface_id_ = temp;

  return true;
}

bool UsbDevicePermissionData::operator<(
    const UsbDevicePermissionData& rhs) const {
  return std::tie(vendor_id_, product_id_, interface_id_) <
         std::tie(rhs.vendor_id_, rhs.product_id_, rhs.interface_id_);
}

bool UsbDevicePermissionData::operator==(
    const UsbDevicePermissionData& rhs) const {
  return vendor_id_ == rhs.vendor_id_ &&
      product_id_ == rhs.product_id_ &&
      interface_id_ == rhs.interface_id_;
}

}  // namespace extensions
