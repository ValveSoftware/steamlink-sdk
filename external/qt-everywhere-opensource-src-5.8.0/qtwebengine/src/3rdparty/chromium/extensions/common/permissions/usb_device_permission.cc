// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/usb_device_permission.h"

#include <set>
#include <string>

#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "device/usb/usb_ids.h"
#include "extensions/common/permissions/permissions_info.h"
#include "grit/extensions_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

UsbDevicePermission::UsbDevicePermission(const APIPermissionInfo* info)
    : SetDisjunctionPermission<UsbDevicePermissionData, UsbDevicePermission>(
          info) {}

UsbDevicePermission::~UsbDevicePermission() {}

bool UsbDevicePermission::FromValue(
    const base::Value* value,
    std::string* error,
    std::vector<std::string>* unhandled_permissions) {
  bool parsed_ok =
      SetDisjunctionPermission<UsbDevicePermissionData, UsbDevicePermission>::
          FromValue(value, error, unhandled_permissions);
  if (parsed_ok && data_set_.empty()) {
    if (error)
      *error = "NULL or empty permission list";
    return false;
  }
  return parsed_ok;
}

PermissionIDSet UsbDevicePermission::GetPermissions() const {
  PermissionIDSet ids;

  std::set<uint16_t> unknown_product_vendors;
  bool found_unknown_vendor = false;

  for (const UsbDevicePermissionData& data : data_set_) {
    const char* vendor = device::UsbIds::GetVendorName(data.vendor_id());
    if (vendor) {
      const char* product =
          device::UsbIds::GetProductName(data.vendor_id(), data.product_id());
      if (product) {
        base::string16 product_name_and_vendor = l10n_util::GetStringFUTF16(
            IDS_EXTENSION_USB_DEVICE_PRODUCT_NAME_AND_VENDOR,
            base::UTF8ToUTF16(product), base::UTF8ToUTF16(vendor));
        ids.insert(APIPermission::kUsbDevice, product_name_and_vendor);
      } else {
        unknown_product_vendors.insert(data.vendor_id());
      }
    } else {
      found_unknown_vendor = true;
    }
  }

  for (uint16_t vendor_id : unknown_product_vendors) {
    const char* vendor = device::UsbIds::GetVendorName(vendor_id);
    DCHECK(vendor);
    ids.insert(APIPermission::kUsbDeviceUnknownProduct,
               base::UTF8ToUTF16(vendor));
  }

  if (found_unknown_vendor)
    ids.insert(APIPermission::kUsbDeviceUnknownVendor);

  return ids;
}

}  // namespace extensions
