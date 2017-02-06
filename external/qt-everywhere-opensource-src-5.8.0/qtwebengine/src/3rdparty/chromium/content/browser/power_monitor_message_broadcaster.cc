// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/power_monitor_message_broadcaster.h"

#include "base/power_monitor/power_monitor.h"
#include "content/common/power_monitor_messages.h"
#include "ipc/ipc_sender.h"

namespace content {

PowerMonitorMessageBroadcaster::PowerMonitorMessageBroadcaster(
    IPC::Sender* sender)
    : sender_(sender) {
  base::PowerMonitor* power_monitor = base::PowerMonitor::Get();
  if (power_monitor)
    power_monitor->AddObserver(this);
}

PowerMonitorMessageBroadcaster::~PowerMonitorMessageBroadcaster() {
  base::PowerMonitor* power_monitor = base::PowerMonitor::Get();
  if (power_monitor)
    power_monitor->RemoveObserver(this);
}

void PowerMonitorMessageBroadcaster::Init() {
  base::PowerMonitor* power_monitor = base::PowerMonitor::Get();
  // Unit tests does not initialize the PowerMonitor.
  if (power_monitor)
    OnPowerStateChange(power_monitor->IsOnBatteryPower());
}

void PowerMonitorMessageBroadcaster::OnPowerStateChange(bool on_battery_power) {
  sender_->Send(new PowerMonitorMsg_PowerStateChange(on_battery_power));
}

void PowerMonitorMessageBroadcaster::OnSuspend() {
  sender_->Send(new PowerMonitorMsg_Suspend());
}

void PowerMonitorMessageBroadcaster::OnResume() {
  sender_->Send(new PowerMonitorMsg_Resume());
}

}  // namespace content
