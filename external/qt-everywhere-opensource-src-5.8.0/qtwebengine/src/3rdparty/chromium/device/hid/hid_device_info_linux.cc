// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hid_device_info_linux.h"

namespace device {

HidDeviceInfoLinux::HidDeviceInfoLinux(
    const HidDeviceId& device_id,
    const std::string& device_node,
    uint16_t vendor_id,
    uint16_t product_id,
    const std::string& product_name,
    const std::string& serial_number,
    HidBusType bus_type,
    const std::vector<uint8_t> report_descriptor)
    : HidDeviceInfo(device_id,
                    vendor_id,
                    product_id,
                    product_name,
                    serial_number,
                    bus_type,
                    report_descriptor),
      device_node_(device_node) {}

HidDeviceInfoLinux::~HidDeviceInfoLinux() {
}

}  // namespace device
