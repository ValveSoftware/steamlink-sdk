// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_BATTERY_BATTERY_STATUS_MANAGER_LINUX_H_
#define DEVICE_BATTERY_BATTERY_STATUS_MANAGER_LINUX_H_

#include "device/battery/battery_export.h"
#include "device/battery/battery_status.mojom.h"

namespace base {
class DictionaryValue;
}

namespace device {

// UPowerDeviceState reflects the possible UPower.Device.State values,
// see upower.freedesktop.org/docs/Device.html#Device:State.
enum UPowerDeviceState {
  UPOWER_DEVICE_STATE_UNKNOWN = 0,
  UPOWER_DEVICE_STATE_CHARGING = 1,
  UPOWER_DEVICE_STATE_DISCHARGING = 2,
  UPOWER_DEVICE_STATE_EMPTY = 3,
  UPOWER_DEVICE_STATE_FULL = 4,
  UPOWER_DEVICE_STATE_PENDING_CHARGE = 5,
  UPOWER_DEVICE_STATE_PENDING_DISCHARGE = 6,
};

// Returns the BatteryStatus computed using the provided dictionary.
DEVICE_BATTERY_EXPORT BatteryStatus ComputeWebBatteryStatus(
    const base::DictionaryValue& dictionary);

}  // namespace device

#endif  // DEVICE_BATTERY_BATTERY_STATUS_MANAGER_LINUX_H_
