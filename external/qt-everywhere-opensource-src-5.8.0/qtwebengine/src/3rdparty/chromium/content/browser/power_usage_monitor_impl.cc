// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/power_usage_monitor_impl.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/power_usage_monitor.h"
#include "content/public/browser/render_process_host.h"

namespace content {

namespace {

// Wait this long after power on before enabling power usage monitoring.
const int kMinUptimeMinutes = 30;

// Minimum discharge time after which we collect the discharge rate.
const int kMinDischargeMinutes = 30;

class PowerUsageMonitorSystemInterface
    : public PowerUsageMonitor::SystemInterface {
 public:
  explicit PowerUsageMonitorSystemInterface(PowerUsageMonitor* owner)
      : power_usage_monitor_(owner),
        weak_ptr_factory_(this) {}
  ~PowerUsageMonitorSystemInterface() override {}

  void ScheduleHistogramReport(base::TimeDelta delay) override {
    BrowserThread::PostDelayedTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(
            &PowerUsageMonitorSystemInterface::ReportBatteryLevelHistogram,
            weak_ptr_factory_.GetWeakPtr(),
            Now(),
            delay),
        delay);
  }

  void CancelPendingHistogramReports() override {
    weak_ptr_factory_.InvalidateWeakPtrs();
  }

  void RecordDischargePercentPerHour(int percent_per_hour) override {
    UMA_HISTOGRAM_PERCENTAGE("Power.BatteryDischargePercentPerHour",
                             percent_per_hour);
  }

  base::Time Now() override { return base::Time::Now(); }

 protected:
  void ReportBatteryLevelHistogram(base::Time start_time,
                                   base::TimeDelta discharge_time) {
    // It's conceivable that the code to cancel pending histogram reports on
    // system suspend, will only get called after the system has woken up.
    // To mitigage this, check whether more time has passed than expected and
    // abort histogram recording in this case.

    // Delayed tasks are subject to timer coalescing and can fire anywhere from
    // delay -> delay * 1.5) . In most cases, the OS should fire the task
    // at the next wakeup and not as late as it can.
    // A threshold of 2 minutes is used, since that should be large enough to
    // take the slop factor due to coalescing into account.
    base::TimeDelta threshold = discharge_time +
        base::TimeDelta::FromMinutes(2);
    if ((Now() - start_time) > threshold) {
      return;
    }

    const std::string histogram_name = base::StringPrintf(
        "Power.BatteryDischarge_%d", discharge_time.InMinutes());
    base::HistogramBase* histogram =
        base::Histogram::FactoryGet(histogram_name,
                                    1,
                                    100,
                                    101,
                                    base::Histogram::kUmaTargetedHistogramFlag);
    double discharge_amount = power_usage_monitor_->discharge_amount();
    histogram->Add(discharge_amount * 100);
  }

 private:
  PowerUsageMonitor* power_usage_monitor_;  // Not owned.

  // Used to cancel in progress delayed tasks.
  base::WeakPtrFactory<PowerUsageMonitorSystemInterface> weak_ptr_factory_;
};

}  // namespace

void StartPowerUsageMonitor() {
  static base::LazyInstance<PowerUsageMonitor>::Leaky monitor =
     LAZY_INSTANCE_INITIALIZER;
  monitor.Get().Start();
}

PowerUsageMonitor::PowerUsageMonitor()
    : callback_(base::Bind(&PowerUsageMonitor::OnBatteryStatusUpdate,
          base::Unretained(this))),
      system_interface_(new PowerUsageMonitorSystemInterface(this)),
      started_(false),
      tracking_discharge_(false),
      on_battery_power_(false),
      initial_battery_level_(0),
      current_battery_level_(0) {
}

PowerUsageMonitor::~PowerUsageMonitor() {
  if (started_)
    base::PowerMonitor::Get()->RemoveObserver(this);
}

void PowerUsageMonitor::Start() {
  // Power monitoring may be delayed based on uptime, but renderer process
  // lifetime tracking needs to start immediately so processes created before
  // then are accounted for.
  registrar_.Add(this,
                 NOTIFICATION_RENDERER_PROCESS_CREATED,
                 NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this,
                 NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 NotificationService::AllBrowserContextsAndSources());
  subscription_ =
      device::BatteryStatusService::GetInstance()->AddCallback(callback_);

  // Delay initialization until the system has been up for a while.
  // This is to mitigate the effect of increased power draw during system start.
  base::TimeDelta uptime = base::SysInfo::Uptime();
  base::TimeDelta min_uptime = base::TimeDelta::FromMinutes(kMinUptimeMinutes);
  if (uptime < min_uptime) {
    base::TimeDelta delay = min_uptime - uptime;
    BrowserThread::PostDelayedTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&PowerUsageMonitor::StartInternal, base::Unretained(this)),
        delay);
  } else {
    StartInternal();
  }
}

void PowerUsageMonitor::StartInternal() {
  DCHECK(!started_);
  started_ = true;

  // PowerMonitor is used to get suspend/resume notifications.
  base::PowerMonitor::Get()->AddObserver(this);
}

void PowerUsageMonitor::DischargeStarted(double battery_level) {
  on_battery_power_ = true;

  // If all browser windows are closed, don't report power metrics since
  // Chrome's power draw is likely not significant.
  if (live_renderer_ids_.empty())
    return;

  // Cancel any in-progress ReportBatteryLevelHistogram() calls.
  system_interface_->CancelPendingHistogramReports();

  tracking_discharge_ = true;
  start_discharge_time_ = system_interface_->Now();

  initial_battery_level_ = battery_level;
  current_battery_level_ = battery_level;

  const int kBatteryReportingIntervalMinutes[] = {5, 15, 30};
  for (auto reporting_interval : kBatteryReportingIntervalMinutes) {
    base::TimeDelta delay = base::TimeDelta::FromMinutes(reporting_interval);
    system_interface_->ScheduleHistogramReport(delay);
  }
}

void PowerUsageMonitor::WallPowerConnected(double battery_level) {
  on_battery_power_ = false;

  if (tracking_discharge_) {
    DCHECK(!start_discharge_time_.is_null());
    base::TimeDelta discharge_time =
        system_interface_->Now() - start_discharge_time_;

    if (discharge_time.InMinutes() > kMinDischargeMinutes) {
      // Record the rate at which the battery discharged over the entire period
      // the system was on battery power.
      double discharge_hours = discharge_time.InSecondsF() / 3600.0;
      int percent_per_hour =
          floor(((discharge_amount() / discharge_hours) * 100.0) + 0.5);
      system_interface_->RecordDischargePercentPerHour(percent_per_hour);
    }
  }

  // Cancel any in-progress ReportBatteryLevelHistogram() calls.
  system_interface_->CancelPendingHistogramReports();

  initial_battery_level_ = 0;
  current_battery_level_ = 0;
  start_discharge_time_ = base::Time();
  tracking_discharge_ = false;
}

void PowerUsageMonitor::OnBatteryStatusUpdate(
    const device::BatteryStatus& status) {
  bool now_on_battery_power = (status.charging == 0);
  bool was_on_battery_power = on_battery_power_;
  double battery_level = status.level;

  if (now_on_battery_power == was_on_battery_power) {
    if (now_on_battery_power)
      current_battery_level_ = battery_level;
    return;
  } else if (now_on_battery_power) {  // Wall power disconnected.
    DischargeStarted(battery_level);
  } else {  // Wall power connected.
    WallPowerConnected(battery_level);
  }
}

void PowerUsageMonitor::OnRenderProcessNotification(int type, int rph_id) {
  size_t previous_num_live_renderers = live_renderer_ids_.size();

  if (type == NOTIFICATION_RENDERER_PROCESS_CREATED) {
    live_renderer_ids_.insert(rph_id);
  } else if (type == NOTIFICATION_RENDERER_PROCESS_CLOSED) {
    live_renderer_ids_.erase(rph_id);
  } else {
    NOTREACHED()  << "Unexpected notification type: " << type;
  }

  if (live_renderer_ids_.empty() && previous_num_live_renderers != 0) {
    // All render processes have died.
    CancelPendingHistogramReporting();
    tracking_discharge_ = false;
  }

}

void PowerUsageMonitor::SetSystemInterfaceForTest(
    std::unique_ptr<SystemInterface> interface) {
  system_interface_ = std::move(interface);
}

void PowerUsageMonitor::OnPowerStateChange(bool on_battery_power) {
}

void PowerUsageMonitor::OnResume() {
}

void PowerUsageMonitor::OnSuspend() {
  CancelPendingHistogramReporting();
}

void PowerUsageMonitor::Observe(int type,
                                const NotificationSource& source,
                                const NotificationDetails& details) {
  RenderProcessHost* rph = Source<RenderProcessHost>(source).ptr();
  OnRenderProcessNotification(type, rph->GetID());
}

void PowerUsageMonitor::CancelPendingHistogramReporting() {
  // Cancel any in-progress histogram reports and reporting of discharge UMA.
  system_interface_->CancelPendingHistogramReports();
}

}  // namespace content
