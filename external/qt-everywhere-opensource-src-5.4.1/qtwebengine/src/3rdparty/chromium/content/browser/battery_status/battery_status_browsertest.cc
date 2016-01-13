// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/synchronization/waitable_event.h"
#include "content/browser/battery_status/battery_status_manager.h"
#include "content/browser/battery_status/battery_status_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"

namespace content {

namespace {

class FakeBatteryManager : public BatteryStatusManager {
 public:
  explicit FakeBatteryManager(
      const BatteryStatusService::BatteryUpdateCallback& callback)
      : battery_status_available_(true),
        started_(false) {
    callback_ = callback;
  }
  virtual ~FakeBatteryManager() { }

  // Methods from BatteryStatusManager.
  virtual bool StartListeningBatteryChange() OVERRIDE {
    started_ = true;
    if (battery_status_available_)
      InvokeUpdateCallback();
    return battery_status_available_;
  }

  virtual void StopListeningBatteryChange() OVERRIDE { }

  void InvokeUpdateCallback() {
    callback_.Run(status_);
  }

  void set_battery_status(const blink::WebBatteryStatus& status) {
    status_ = status;
  }

  void set_battery_status_available(bool value) {
    battery_status_available_ = value;
  }

  bool started() {
    return started_;
  }

 private:
  bool battery_status_available_;
  bool started_;
  blink::WebBatteryStatus status_;

  DISALLOW_COPY_AND_ASSIGN(FakeBatteryManager);
};

class BatteryStatusBrowserTest : public ContentBrowserTest  {
 public:
    BatteryStatusBrowserTest()
      : battery_manager_(0),
        battery_service_(0),
        io_loop_finished_event_(false, false) {
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&BatteryStatusBrowserTest::SetUpOnIOThread, this));
    io_loop_finished_event_.Wait();
  }

  void SetUpOnIOThread() {
    battery_service_ = BatteryStatusService::GetInstance();
    battery_manager_ = new FakeBatteryManager(
        battery_service_->GetUpdateCallbackForTesting());
    battery_service_->SetBatteryManagerForTesting(battery_manager_);
    io_loop_finished_event_.Signal();
  }

  virtual void TearDown() OVERRIDE {
    battery_service_->SetBatteryManagerForTesting(0);
  }

  FakeBatteryManager* battery_manager() {
    return battery_manager_;
  }

 private:
  FakeBatteryManager* battery_manager_;
  BatteryStatusService* battery_service_;
  base::WaitableEvent io_loop_finished_event_;

  DISALLOW_COPY_AND_ASSIGN(BatteryStatusBrowserTest);
};

IN_PROC_BROWSER_TEST_F(BatteryStatusBrowserTest, BatteryManagerDefaultValues) {
  // Set the fake battery manager to return false on start. From JavaScript
  // request a promise for the battery status information and once it resolves
  // check the default values and navigate to #pass.
  battery_manager()->set_battery_status_available(false);
  GURL test_url = GetTestUrl(
      "battery_status", "battery_status_default_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);
  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
  EXPECT_TRUE(battery_manager()->started());
}

IN_PROC_BROWSER_TEST_F(BatteryStatusBrowserTest, BatteryManagerResolvePromise) {
  // Set the fake battery manager to return predefined battery status values.
  // From JavaScript request a promise for the battery status information and
  // once it resolves check the values and navigate to #pass.
  blink::WebBatteryStatus status;
  status.charging = true;
  status.chargingTime = 100;
  status.dischargingTime = std::numeric_limits<double>::infinity();
  status.level = 0.5;
  battery_manager()->set_battery_status(status);

  GURL test_url = GetTestUrl(
      "battery_status", "battery_status_promise_resolution_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);
  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
  EXPECT_TRUE(battery_manager()->started());
}

IN_PROC_BROWSER_TEST_F(BatteryStatusBrowserTest,
    BatteryManagerWithEventListener) {
  // Set the fake battery manager to return default battery status values.
  // From JavaScript request a promise for the battery status information.
  // Once it resolves add an event listener for battery level change. Set
  // battery level to 0.6 and invoke update. Check that the event listener
  // is invoked with the correct value for level and navigate to #pass.
  blink::WebBatteryStatus status;
  battery_manager()->set_battery_status(status);

  TestNavigationObserver same_tab_observer(shell()->web_contents(), 2);
  GURL test_url = GetTestUrl(
      "battery_status", "battery_status_event_listener_test.html");
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
