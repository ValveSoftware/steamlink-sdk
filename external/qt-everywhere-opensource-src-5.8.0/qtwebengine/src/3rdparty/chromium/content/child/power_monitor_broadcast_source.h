// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_POWER_MONITOR_BROADCAST_SOURCE_H_
#define CONTENT_CHILD_POWER_MONITOR_BROADCAST_SOURCE_H_

#include "base/macros.h"
#include "base/power_monitor/power_monitor_source.h"
#include "content/common/content_export.h"
#include "ipc/ipc_channel.h"

namespace IPC {
class MessageFilter;
}

namespace content {

class PowerMessageFilter;

// Receives Power Monitor IPC messages sent from the browser process and relays
// them to the PowerMonitor of the current process.
class CONTENT_EXPORT PowerMonitorBroadcastSource :
    public base::PowerMonitorSource {
 public:
  explicit PowerMonitorBroadcastSource();
  ~PowerMonitorBroadcastSource() override;

  IPC::MessageFilter* GetMessageFilter();

 private:
  friend class PowerMessageFilter;

  bool IsOnBatteryPowerImpl() override;

  void OnPowerStateChange(bool on_battery_power);
  void OnSuspend();
  void OnResume();

  bool last_reported_battery_power_state_;
  scoped_refptr<PowerMessageFilter> message_filter_;

  DISALLOW_COPY_AND_ASSIGN(PowerMonitorBroadcastSource);
};

}  // namespace content

#endif  // CONTENT_CHILD_POWER_MONITOR_BROADCAST_SOURCE_H_
