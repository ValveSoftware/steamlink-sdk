// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_device_info.h"

namespace device {

#if !defined(OS_MACOSX)
const char kInvalidHidDeviceId[] = "";
#endif

HidDeviceInfo::HidDeviceInfo()
    : device_id(kInvalidHidDeviceId),
      bus_type(kHIDBusTypeUSB),
      vendor_id(0),
      product_id(0),
      input_report_size(0),
      output_report_size(0),
      feature_report_size(0),
      has_report_id(false) {}

HidDeviceInfo::~HidDeviceInfo() {}

}  // namespace device
