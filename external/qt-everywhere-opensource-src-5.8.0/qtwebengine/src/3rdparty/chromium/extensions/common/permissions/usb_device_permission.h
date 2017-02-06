// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_PERMISSIONS_USB_DEVICE_PERMISSION_H_
#define EXTENSIONS_COMMON_PERMISSIONS_USB_DEVICE_PERMISSION_H_

#include <stdint.h>

#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/set_disjunction_permission.h"
#include "extensions/common/permissions/usb_device_permission_data.h"

namespace extensions {

class UsbDevicePermission
  : public SetDisjunctionPermission<UsbDevicePermissionData,
                                    UsbDevicePermission> {
 public:
  struct CheckParam : public APIPermission::CheckParam {
    CheckParam(uint16_t vendor_id, uint16_t product_id, int interface_id)
        : vendor_id(vendor_id),
          product_id(product_id),
          interface_id(interface_id) {}
    const uint16_t vendor_id;
    const uint16_t product_id;
    const int interface_id;
  };

  explicit UsbDevicePermission(const APIPermissionInfo* info);
  ~UsbDevicePermission() override;

  // SetDisjunctionPermission overrides.
  bool FromValue(const base::Value* value,
                 std::string* error,
                 std::vector<std::string>* unhandled_permissions) override;

  // APIPermission overrides
  PermissionIDSet GetPermissions() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_PERMISSIONS_USB_DEVICE_PERMISSION_H_
