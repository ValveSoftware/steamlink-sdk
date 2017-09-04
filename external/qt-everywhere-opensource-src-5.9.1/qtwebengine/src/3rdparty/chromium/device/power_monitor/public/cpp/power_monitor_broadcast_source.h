// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_POWER_MONITOR_POWER_MONITOR_BROADCAST_SOURCE_H_
#define DEVICE_POWER_MONITOR_POWER_MONITOR_BROADCAST_SOURCE_H_

#include "base/macros.h"
#include "base/power_monitor/power_monitor_source.h"
#include "device/power_monitor/power_monitor_export.h"
#include "device/power_monitor/public/interfaces/power_monitor.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace device {

// Receives state changes from Power Monitor through mojo, and relays them to
// the PowerMonitor of the current process.
class DEVICE_POWER_MONITOR_EXPORT PowerMonitorBroadcastSource
    : public base::PowerMonitorSource,
      NON_EXPORTED_BASE(public device::mojom::PowerMonitorClient) {
 public:
  explicit PowerMonitorBroadcastSource(
      service_manager::InterfaceProvider* interface_provider);
  ~PowerMonitorBroadcastSource() override;

  void PowerStateChange(bool on_battery_power) override;
  void Suspend() override;
  void Resume() override;

 private:
  bool IsOnBatteryPowerImpl() override;
  bool last_reported_battery_power_state_;
  mojo::Binding<device::mojom::PowerMonitorClient> binding_;

  DISALLOW_COPY_AND_ASSIGN(PowerMonitorBroadcastSource);
};

}  // namespace device

#endif  // DEVICE_POWER_MONITOR_POWER_MONITOR_BROADCAST_SOURCE_H_
