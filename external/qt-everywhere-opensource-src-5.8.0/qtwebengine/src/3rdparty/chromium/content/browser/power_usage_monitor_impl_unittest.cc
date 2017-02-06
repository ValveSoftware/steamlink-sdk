// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/power_usage_monitor_impl.h"

#include <utility>

#include "content/public/browser/notification_types.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "device/battery/battery_monitor.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

// Dummy ID to identify a phantom RenderProcessHost in tests.
const int kDummyRenderProcessHostID = 1;

class SystemInterfaceForTest : public PowerUsageMonitor::SystemInterface {
 public:
  SystemInterfaceForTest()
      : num_pending_histogram_reports_(0),
        discharge_percent_per_hour_(0),
        now_(base::Time::FromInternalValue(1000)) {}
  ~SystemInterfaceForTest() override {}

  int num_pending_histogram_reports() const {
    return num_pending_histogram_reports_;
  }

  int discharge_percent_per_hour() const {
    return discharge_percent_per_hour_;
  }

  void AdvanceClockSeconds(int seconds) {
    now_ += base::TimeDelta::FromSeconds(seconds);
  }

  void AdvanceClockMinutes(int minutes) {
    now_ += base::TimeDelta::FromMinutes(minutes);
  }

  void ScheduleHistogramReport(base::TimeDelta delay) override {
    num_pending_histogram_reports_++;
  }

  void CancelPendingHistogramReports() override {
    num_pending_histogram_reports_ = 0;
  }

  void RecordDischargePercentPerHour(int percent_per_hour) override {
    discharge_percent_per_hour_ = percent_per_hour;
  }

  base::Time Now() override { return now_; }

 private:
  int num_pending_histogram_reports_;
  int discharge_percent_per_hour_;
  base::Time now_;
};

class PowerUsageMonitorTest : public testing::Test {
 protected:
  void SetUp() override {
    monitor_.reset(new PowerUsageMonitor);
    // PowerUsageMonitor assumes ownership.
    std::unique_ptr<SystemInterfaceForTest> test_interface(
        new SystemInterfaceForTest());
    system_interface_ = test_interface.get();
    monitor_->SetSystemInterfaceForTest(std::move(test_interface));

    // Without live renderers, the monitor won't do anything.
    monitor_->OnRenderProcessNotification(NOTIFICATION_RENDERER_PROCESS_CREATED,
                                          kDummyRenderProcessHostID);
  }

  void UpdateBatteryStatus(bool charging, double battery_level) {
    device::BatteryStatus battery_status;
    battery_status.charging = charging;
    battery_status.level = battery_level;
    monitor_->OnBatteryStatusUpdate(battery_status);
  }

  void KillTestRenderer() {
    monitor_->OnRenderProcessNotification(
        NOTIFICATION_RENDERER_PROCESS_CLOSED, kDummyRenderProcessHostID);
  }

  std::unique_ptr<PowerUsageMonitor> monitor_;
  SystemInterfaceForTest* system_interface_;
  TestBrowserThreadBundle thread_bundle_;
};

TEST_F(PowerUsageMonitorTest, StartStopQuickly) {
  // Going on battery power.
  UpdateBatteryStatus(false, 1.0);
  int initial_num_histogram_reports =
      system_interface_->num_pending_histogram_reports();
  ASSERT_GT(initial_num_histogram_reports, 0);

  // Battery level goes down a bit.
  system_interface_->AdvanceClockSeconds(1);
  UpdateBatteryStatus(false, 0.9);
  ASSERT_EQ(initial_num_histogram_reports,
            system_interface_->num_pending_histogram_reports());
  ASSERT_EQ(0, system_interface_->discharge_percent_per_hour());

  // Wall power connected.
  system_interface_->AdvanceClockSeconds(30);
  UpdateBatteryStatus(true, 0);
  ASSERT_EQ(0, system_interface_->num_pending_histogram_reports());
  ASSERT_EQ(0, system_interface_->discharge_percent_per_hour());
}

TEST_F(PowerUsageMonitorTest, DischargePercentReported) {
  // Going on battery power.
  UpdateBatteryStatus(false, 1.0);
  int initial_num_histogram_reports =
      system_interface_->num_pending_histogram_reports();
  ASSERT_GT(initial_num_histogram_reports, 0);

  // Battery level goes down a bit.
  system_interface_->AdvanceClockSeconds(30);
  UpdateBatteryStatus(false, 0.9);
  ASSERT_EQ(initial_num_histogram_reports,
            system_interface_->num_pending_histogram_reports());
  ASSERT_EQ(0, system_interface_->discharge_percent_per_hour());

  // Wall power connected.
  system_interface_->AdvanceClockMinutes(31);
  UpdateBatteryStatus(true, 0);
  ASSERT_EQ(0, system_interface_->num_pending_histogram_reports());
  ASSERT_GT(system_interface_->discharge_percent_per_hour(), 0);
}

TEST_F(PowerUsageMonitorTest, NoRenderersDisablesMonitoring) {
  KillTestRenderer();

  // Going on battery power.
  UpdateBatteryStatus(false, 1.0);
  ASSERT_EQ(0, system_interface_->num_pending_histogram_reports());
  ASSERT_EQ(0, system_interface_->discharge_percent_per_hour());

  // Wall power connected.
  system_interface_->AdvanceClockSeconds(30);
  UpdateBatteryStatus(true, 0.5);
  ASSERT_EQ(0, system_interface_->num_pending_histogram_reports());
  ASSERT_EQ(0, system_interface_->discharge_percent_per_hour());
}

TEST_F(PowerUsageMonitorTest, NoRenderersCancelsInProgressMonitoring) {
  // Going on battery power.
  UpdateBatteryStatus(false, 1.0);
  ASSERT_GT(system_interface_->num_pending_histogram_reports(), 0);
  ASSERT_EQ(0, system_interface_->discharge_percent_per_hour());

  // All renderers killed.
  KillTestRenderer();
  ASSERT_EQ(0, system_interface_->num_pending_histogram_reports());

  // Wall power connected.
  system_interface_->AdvanceClockMinutes(31);
  UpdateBatteryStatus(true, 0);
  ASSERT_EQ(0, system_interface_->num_pending_histogram_reports());
  ASSERT_EQ(0, system_interface_->discharge_percent_per_hour());
}

}  // namespace content
