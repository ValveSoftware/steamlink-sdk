// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef EXTENSIONS_COMMON_PERMISSIONS_USB_DEVICE_PERMISSION_DATA_H_
#define EXTENSIONS_COMMON_PERMISSIONS_USB_DEVICE_PERMISSION_DATA_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "extensions/common/permissions/api_permission.h"

namespace base {

class Value;

}  // namespace base

namespace extensions {

// A pattern that can be used to match a USB device permission.
// Should be of the format: vendorId:productId, where both vendorId and
// productId are decimal strings representing uint16_t values.
class UsbDevicePermissionData {
 public:
  enum SpecialInterfaces {
    // A special interface id for stating permissions for an entire USB device,
    // no specific interface. This value must match value of Rule::ANY_INTERFACE
    // from ChromeOS permission_broker project.
    ANY_INTERFACE = -1,

    // A special interface id for |Check| to indicate that interface field is
    // not to be checked. Not used in manifest file.
    UNSPECIFIED_INTERFACE = -2
  };

  UsbDevicePermissionData();
  UsbDevicePermissionData(uint16_t vendor_id,
                          uint16_t product_id,
                          int interface_id);

  // Check if |param| (which must be a UsbDevicePermissionData::CheckParam)
  // matches the vendor and product IDs associated with |this|.
  bool Check(const APIPermission::CheckParam* param) const;

  // Convert |this| into a base::Value.
  std::unique_ptr<base::Value> ToValue() const;

  // Populate |this| from a base::Value.
  bool FromValue(const base::Value* value);

  bool operator<(const UsbDevicePermissionData& rhs) const;
  bool operator==(const UsbDevicePermissionData& rhs) const;

  const uint16_t& vendor_id() const { return vendor_id_; }
  const uint16_t& product_id() const { return product_id_; }

  // These accessors are provided for IPC_STRUCT_TRAITS_MEMBER.  Please
  // think twice before using them for anything else.
  uint16_t& vendor_id() { return vendor_id_; }
  uint16_t& product_id() { return product_id_; }

 private:
  uint16_t vendor_id_;
  uint16_t product_id_;
  int interface_id_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_PERMISSIONS_USB_DEVICE_PERMISSION_DATA_H_
