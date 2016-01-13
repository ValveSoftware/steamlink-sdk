// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_connection.h"

namespace device {

PendingHidReport::PendingHidReport() {}

PendingHidReport::~PendingHidReport() {}

PendingHidRead::PendingHidRead() {}

PendingHidRead::~PendingHidRead() {}

HidConnection::HidConnection(const HidDeviceInfo& device_info)
    : device_info_(device_info) {}

HidConnection::~HidConnection() {}

}  // namespace device
