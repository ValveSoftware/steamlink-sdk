// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "device/battery/battery_status_manager.h"
#include "device/battery/battery_status_service.h"

// These tests run against the default implementation of the BatteryMonitor
// service, with a dummy BatteryManager set as a source of the battery
// information. They can be run only on platforms that use the default service
// implementation, ie. on the platforms where BatteryStatusService is used.

namespace content {

namespace {

class FakeBatteryManager : public device::BatteryStatusManager {
 public:
  explicit FakeBatteryManager(
      const device::BatteryStatusService::BatteryUpdateCallback& callback)
      : callback_(callback), battery_status_available_(true), started_(false) {}
  ~FakeBatteryManager() override {}

  // Methods from BatteryStatusManager.
  bool StartListeningBatteryChange() override {
    started_ = true;
    if (battery_status_available_)
      InvokeUpdateCallback();
    return battery_status_available_;
  }

  void StopListeningBatteryChange() override {}

  void InvokeUpdateCallback() {
    // Invoke asynchronously to mimic the OS-specific battery managers.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback_, status_));
  }

  void set_battery_status(const device::BatteryStatus& status) {
    status_ = status;
  }

  void set_battery_status_available(bool value) {
    battery_status_available_ = value;
  }

  bool started() { return started_; }

 private:
  device::BatteryStatusService::BatteryUpdateCallback callback_;
  bool battery_status_available_;
  bool started_;
  device::BatteryStatus status_;

  DISALLOW_COPY_AND_ASSIGN(FakeBatteryManager);
};

class BatteryMonitorImplTest : public ContentBrowserTest {
 public:
  BatteryMonitorImplTest() : battery_manager_(NULL), battery_service_(NULL) {}

  void SetUpOnMainThread() override {
    battery_service_ = device::BatteryStatusService::GetInstance();

    // We keep a raw pointer to the FakeBatteryManager, which we expect to
    // remain valid for the lifetime of the BatteryStatusService.
    std::unique_ptr<FakeBatteryManager> battery_manager(new FakeBatteryManager(
        battery_service_->GetUpdateCallbackForTesting()));
    battery_manager_ = battery_manager.get();

    battery_service_->SetBatteryManagerForTesting(std::move(battery_manager));
  }

  void TearDown() override {
    battery_service_->SetBatteryManagerForTesting(
        std::unique_ptr<device::BatteryStatusManager>());
    battery_manager_ = NULL;
  }

  FakeBatteryManager* battery_manager() { return battery_manager_; }

 private:
  FakeBatteryManager* battery_manager_;
  device::BatteryStatusService* battery_service_;

  DISALLOW_COPY_AND_ASSIGN(BatteryMonitorImplTest);
};

IN_PROC_BROWSER_TEST_F(BatteryMonitorImplTest, BatteryManagerDefaultValues) {
  // Set the fake battery manager to return false on start. From JavaScript
  // request a promise for the battery status information and once it resolves
  // check the default values and navigate to #pass.
  battery_manager()->set_battery_status_available(false);
  GURL test_url =
      GetTestUrl("battery_status", "battery_status_default_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);
  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
  EXPECT_TRUE(battery_manager()->started());
}

IN_PROC_BROWSER_TEST_F(BatteryMonitorImplTest, BatteryManagerResolvePromise) {
  // Set the fake battery manager to return predefined battery status values.
  // From JavaScript request a promise for the battery status information and
  // once it resolves check the values and navigate to #pass.
  device::BatteryStatus status;
  status.charging = true;
  status.charging_time = 100;
  status.discharging_time = std::numeric_limits<double>::infinity();
  status.level = 0.5;
  battery_manager()->set_battery_status(status);

  GURL test_url = GetTestUrl("battery_status",
                             "battery_status_promise_resolution_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);
  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
  EXPECT_TRUE(battery_manager()->started());
}

IN_PROC_BROWSER_TEST_F(BatteryMonitorImplTest,
                       BatteryManagerWithEventListener) {
  // Set the fake battery manager to return default battery status values.
  // From JavaScript request a promise for the battery status information.
  // Once it resolves add an event listener for battery level change. Set
  // battery level to 0.6 and invoke update. Check that the event listener
  // is invoked with the correct value for level and navigate to #pass.
  device::BatteryStatus status;
  battery_manager()->set_battery_status(status);

  TestNavigationObserver same_tab_observer(shell()->web_contents(), 2);
  GURL test_url =
      GetTestUrl("battery_status", "battery_status_event_listener_test.html");
  shell()->LoadURL(test_url);
  same_tab_observer.Wait();
  EXPECT_EQ("resolved", shell()->web_contents()->GetLastCommittedURL().ref());

  TestNavigationObserver same_tab_observer2(shell()->web_contents(), 1);
  status.level = 0.6;
  battery_manager()->set_battery_status(status);
  battery_manager()->InvokeUpdateCallback();
  same_tab_observer2.Wait();
  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
  EXPECT_TRUE(battery_manager()->started());
}

}  //  namespace

}  //  namespace content
