// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_POWER_USAGE_MONITOR_IMPL_H_
#define CONTENT_BROWSER_POWER_USAGE_MONITOR_IMPL_H_

#include "base/containers/hash_tables.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/power_monitor/power_monitor.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "device/battery/battery_status_service.h"

namespace content {

// Record statistics on power usage.
//
// Two main statics are recorded by this class:
// * Power.BatteryDischarge_{5,15,30} - delta between battery level when
//   unplugged from wallpower, over the specified period - in minutes.
// * Power.BatteryDischargeRateWhenUnplugged - the rate of battery discharge
//   from the device being unplugged until it's plugged back in, if said period
//   was longer than 30 minutes.
//
// Heuristics:
// * Data collection starts after system uptime exceeds 30 minutes.
// * If the machine goes to sleep or all renderers are closed then the current
//   measurement is cancelled.
class CONTENT_EXPORT PowerUsageMonitor : public base::PowerObserver,
                                         public NotificationObserver {
 public:
  class SystemInterface {
   public:
    virtual ~SystemInterface() {}

    virtual void ScheduleHistogramReport(base::TimeDelta delay) = 0;
    virtual void CancelPendingHistogramReports() = 0;

    // Record the battery discharge percent per hour over the time the system
    // is on battery power, legal values [0,100].
    virtual void RecordDischargePercentPerHour(int percent_per_hour) = 0;

    // Allow tests to override clock.
    virtual base::Time Now() = 0;
  };

 public:
  PowerUsageMonitor();
  ~PowerUsageMonitor() override;

  double discharge_amount() const {
    return initial_battery_level_ - current_battery_level_;
  }

  // Start monitoring power usage.
  // Note that the actual monitoring will be delayed until 30 minutes after
  // system boot.
  void Start();

  void SetSystemInterfaceForTest(std::unique_ptr<SystemInterface> interface);

  // Overridden from base::PowerObserver:
  void OnPowerStateChange(bool on_battery_power) override;
  void OnResume() override;
  void OnSuspend() override;

  // Overridden from NotificationObserver:
  void Observe(int type,
               const NotificationSource& source,
               const NotificationDetails& details) override;

 private:
  friend class PowerUsageMonitorTest;
  FRIEND_TEST_ALL_PREFIXES(PowerUsageMonitorTest, OnBatteryStatusUpdate);
  FRIEND_TEST_ALL_PREFIXES(PowerUsageMonitorTest, OnRenderProcessNotification);

  // Start monitoring system power usage.
  // This function may be called after a delay, see Start() for details.
  void StartInternal();

  void OnBatteryStatusUpdate(const device::BatteryStatus& status);
  void OnRenderProcessNotification(int type, int rph_id);

  void DischargeStarted(double battery_level);
  void WallPowerConnected(double battery_level);

  void CancelPendingHistogramReporting();

  device::BatteryStatusService::BatteryUpdateCallback callback_;
  std::unique_ptr<device::BatteryStatusService::BatteryUpdateSubscription>
      subscription_;

  NotificationRegistrar registrar_;

  std::unique_ptr<SystemInterface> system_interface_;

  // True if monitoring was started (Start() called).
  bool started_;

  // True if collecting metrics for the current discharge cycle e.g. if no
  // renderers are open we don't keep track of discharge.
  bool tracking_discharge_;

  // True if the system is running on battery power, false if on wall power.
  bool on_battery_power_;

  // Battery level when wall power disconnected. [0.0, 1.0] - 0 if on wall
  // power, 1 means fully charged.
  double initial_battery_level_;

  // Current battery level. [0.0, 1.0] - 0 if on wall power, 1 means fully
  // charged.
  double current_battery_level_;

  // Timestamp when wall power was disconnected, null Time object otherwise.
  base::Time start_discharge_time_;

  // IDs of live renderer processes.
  base::hash_set<int> live_renderer_ids_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PowerUsageMonitor);
};

}  // namespace content

#endif  // CONTENT_BROWSER_POWER_USAGE_MONITOR_IMPL_H_
