// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/power_monitor_test_base.h"
#include "device/power_monitor/power_monitor_message_broadcaster.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class FakePowerMonitorClient : public device::mojom::PowerMonitorClient {
 public:
  FakePowerMonitorClient(device::mojom::PowerMonitorClientRequest request)
      : binding_(this, std::move(request)),
        power_state_changes_(0),
        suspends_(0),
        resumes_(0) {}
  ~FakePowerMonitorClient() override {}

  // Implement device::mojom::PowerMonitorClient
  void PowerStateChange(bool on_battery_power) override {
    power_state_changes_++;
  }
  void Suspend() override { suspends_++; }
  void Resume() override { resumes_++; }

  // Test status counts.
  int power_state_changes() { return power_state_changes_; }
  int suspends() { return suspends_; }
  int resumes() { return resumes_; }

 private:
  mojo::Binding<device::mojom::PowerMonitorClient> binding_;
  int power_state_changes_;  // Count of OnPowerStateChange notifications.
  int suspends_;             // Count of OnSuspend notifications.
  int resumes_;              // Count of OnResume notifications.
};

class PowerMonitorMessageBroadcasterTest : public testing::Test {
 protected:
  PowerMonitorMessageBroadcasterTest() {
    power_monitor_source_ = new base::PowerMonitorTestSource();
    power_monitor_.reset(new base::PowerMonitor(
        std::unique_ptr<base::PowerMonitorSource>(power_monitor_source_)));
  }
  ~PowerMonitorMessageBroadcasterTest() override {}

  base::PowerMonitorTestSource* source() { return power_monitor_source_; }
  base::PowerMonitor* monitor() { return power_monitor_.get(); }

 private:
  base::MessageLoop message_loop_;
  base::PowerMonitorTestSource* power_monitor_source_;
  std::unique_ptr<base::PowerMonitor> power_monitor_;

  DISALLOW_COPY_AND_ASSIGN(PowerMonitorMessageBroadcasterTest);
};

TEST_F(PowerMonitorMessageBroadcasterTest, PowerMessageBroadcast) {
  device::mojom::PowerMonitorClientPtr proxy;
  FakePowerMonitorClient client(mojo::GetProxy(&proxy));
  PowerMonitorMessageBroadcaster broadcaster;

  // Calling SetClient should invoke a power state change.
  broadcaster.SetClient(std::move(proxy));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(client.power_state_changes(), 1);

  // Sending resume when not suspended should have no effect.
  source()->GenerateResumeEvent();
  EXPECT_EQ(client.resumes(), 0);

  // Pretend we suspended.
  source()->GenerateSuspendEvent();
  EXPECT_EQ(client.suspends(), 1);

  // Send a second suspend notification.  This should be suppressed.
  source()->GenerateSuspendEvent();
  EXPECT_EQ(client.suspends(), 1);

  // Pretend we were awakened.
  source()->GenerateResumeEvent();
  EXPECT_EQ(client.resumes(), 1);

  // Send a duplicate resume notification.  This should be suppressed.
  source()->GenerateResumeEvent();
  EXPECT_EQ(client.resumes(), 1);

  // Pretend the device has gone on battery power
  source()->GeneratePowerStateEvent(true);
  EXPECT_EQ(client.power_state_changes(), 2);

  // Repeated indications the device is on battery power should be suppressed.
  source()->GeneratePowerStateEvent(true);
  EXPECT_EQ(client.power_state_changes(), 2);

  // Pretend the device has gone off battery power
  source()->GeneratePowerStateEvent(false);
  EXPECT_EQ(client.power_state_changes(), 3);

  // Repeated indications the device is off battery power should be suppressed.
  source()->GeneratePowerStateEvent(false);
  EXPECT_EQ(client.power_state_changes(), 3);
}

}  // namespace device
