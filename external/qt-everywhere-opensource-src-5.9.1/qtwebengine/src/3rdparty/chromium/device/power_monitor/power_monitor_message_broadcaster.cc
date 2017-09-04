// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/power_monitor/power_monitor_message_broadcaster.h"

#include "base/power_monitor/power_monitor.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace device {

PowerMonitorMessageBroadcaster::PowerMonitorMessageBroadcaster() {
  base::PowerMonitor* power_monitor = base::PowerMonitor::Get();
  if (power_monitor)
    power_monitor->AddObserver(this);
}

PowerMonitorMessageBroadcaster::~PowerMonitorMessageBroadcaster() {
  base::PowerMonitor* power_monitor = base::PowerMonitor::Get();
  if (power_monitor)
    power_monitor->RemoveObserver(this);
}

// static
void PowerMonitorMessageBroadcaster::Create(
    device::mojom::PowerMonitorRequest request) {
  mojo::MakeStrongBinding(base::MakeUnique<PowerMonitorMessageBroadcaster>(),
                          std::move(request));
}

void PowerMonitorMessageBroadcaster::SetClient(
    device::mojom::PowerMonitorClientPtr power_monitor_client) {
  power_monitor_client_ = std::move(power_monitor_client);
  base::PowerMonitor* power_monitor = base::PowerMonitor::Get();
  // Unit tests does not initialize the PowerMonitor.
  if (power_monitor)
    OnPowerStateChange(power_monitor->IsOnBatteryPower());
}

void PowerMonitorMessageBroadcaster::OnPowerStateChange(bool on_battery_power) {
  if (power_monitor_client_) {
    power_monitor_client_->PowerStateChange(on_battery_power);
  }
}

void PowerMonitorMessageBroadcaster::OnSuspend() {
  if (power_monitor_client_) {
    power_monitor_client_->Suspend();
  }
}

void PowerMonitorMessageBroadcaster::OnResume() {
  if (power_monitor_client_) {
    power_monitor_client_->Resume();
  }
}

}  // namespace device
