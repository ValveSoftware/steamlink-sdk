// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BATTERY_BATTERY_STATUS_MANAGER_WIN_H_
#define DEVICE_BATTERY_BATTERY_STATUS_MANAGER_WIN_H_

#include <windows.h>
#include "device/battery/battery_export.h"
#include "device/battery/battery_status.mojom.h"

namespace device {

enum WinACLineStatus {
  WIN_AC_LINE_STATUS_OFFLINE = 0,
  WIN_AC_LINE_STATUS_ONLINE = 1,
  WIN_AC_LINE_STATUS_UNKNOWN = 255,
};

// Returns WebBatteryStatus corresponding to the given SYSTEM_POWER_STATUS.
DEVICE_BATTERY_EXPORT BatteryStatus ComputeWebBatteryStatus(
    const SYSTEM_POWER_STATUS& win_status);

}  // namespace device

#endif  // DEVICE_BATTERY_BATTERY_STATUS_MANAGER_WIN_H_
