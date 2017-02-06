// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/callback_list.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "device/battery/battery_monitor.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/shell/public/cpp/interface_registry.h"

// These tests run against a dummy implementation of the BatteryMonitor service.
// That is, they verify that the service implementation is correctly exposed to
// the renderer, whatever the implementation is.

namespace content {

namespace {

typedef base::CallbackList<void(const device::BatteryStatus&)>
    BatteryUpdateCallbackList;
typedef BatteryUpdateCallbackList::Subscription BatteryUpdateSubscription;

// Global battery state used in the tests.
device::BatteryStatus g_battery_status;
// Global list of test battery monitors to notify when |g_battery_status|
// changes.
base::LazyInstance<BatteryUpdateCallbackList> g_callback_list =
    LAZY_INSTANCE_INITIALIZER;

// Updates the global battery state and notifies existing test monitors.
void UpdateBattery(const device::BatteryStatus& battery_status) {
  g_battery_status = battery_status;
  g_callback_list.Get().Notify(battery_status);
}

class FakeBatteryMonitor : public device::BatteryMonitor {
 public:
  static void Create(mojo::InterfaceRequest<BatteryMonitor> request) {
    new FakeBatteryMonitor(std::move(request));
  }

 private:
  FakeBatteryMonitor(mojo::InterfaceRequest<BatteryMonitor> request)
      : binding_(this, std::move(request)) {}
  ~FakeBatteryMonitor() override {}

  void QueryNextStatus(const QueryNextStatusCallback& callback) override {
    // We don't expect overlapped calls to QueryNextStatus.
    DCHECK(callback_.is_null());

    callback_ = callback;

    if (!subscription_) {
      subscription_ =
          g_callback_list.Get().Add(base::Bind(&FakeBatteryMonitor::DidChange,
                                               base::Unretained(this)));
      // Report initial value.
      DidChange(g_battery_status);
    }
  }

  void DidChange(const device::BatteryStatus& battery_status) {
    if (!callback_.is_null()) {
      callback_.Run(battery_status.Clone());
      callback_.Reset();
    }
  }

  std::unique_ptr<BatteryUpdateSubscription> subscription_;
  mojo::StrongBinding<BatteryMonitor> binding_;
  QueryNextStatusCallback callback_;
};

// Overrides the default service implementation with the test implementation
// declared above.
class TestContentBrowserClient : public ContentBrowserClient {
 public:
  void ExposeInterfacesToRenderer(
      shell::InterfaceRegistry* registry,
      RenderProcessHost* render_process_host) override {
    registry->AddInterface(base::Bind(&FakeBatteryMonitor::Create));
  }

  void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                      int child_process_id) override {
    // Necessary for passing kIsolateSitesForTesting flag to the renderer.
    ShellContentBrowserClient::Get()->AppendExtraCommandLineSwitches(
        command_line, child_process_id);
  }

#if defined(OS_ANDROID)
  void GetAdditionalMappedFilesForChildProcess(
      const base::CommandLine& command_line,
      int child_process_id,
      FileDescriptorInfo* mappings,
      std::map<int, base::MemoryMappedFile::Region>* regions) override {
    ShellContentBrowserClient::Get()->GetAdditionalMappedFilesForChildProcess(
        command_line, child_process_id, mappings, regions);
  }
#endif  // defined(OS_ANDROID)
};

class BatteryMonitorIntegrationTest : public ContentBrowserTest {
 public:
  BatteryMonitorIntegrationTest() {}

  void SetUpOnMainThread() override {
    old_client_ = SetBrowserClientForTesting(&test_client_);
  }

  void TearDownOnMainThread() override {
    SetBrowserClientForTesting(old_client_);
  }

 private:
  TestContentBrowserClient test_client_;
  ContentBrowserClient* old_client_;

  DISALLOW_COPY_AND_ASSIGN(BatteryMonitorIntegrationTest);
};

IN_PROC_BROWSER_TEST_F(BatteryMonitorIntegrationTest, DefaultValues) {
  // From JavaScript request a promise for the battery status information and
  // once it resolves check the default values and navigate to #pass.
  UpdateBattery(device::BatteryStatus());
  GURL test_url =
      GetTestUrl("battery_status", "battery_status_default_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);
  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
}

IN_PROC_BROWSER_TEST_F(BatteryMonitorIntegrationTest, ResolvePromise) {
  // Set the fake battery monitor to return predefined battery status values.
  // From JavaScript request a promise for the battery status information and
  // once it resolves check the values and navigate to #pass.
  device::BatteryStatus status;
  status.charging = true;
  status.charging_time = 100;
  status.discharging_time = std::numeric_limits<double>::infinity();
  status.level = 0.5;
  UpdateBattery(status);

  GURL test_url = GetTestUrl("battery_status",
                             "battery_status_promise_resolution_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);
  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
}

IN_PROC_BROWSER_TEST_F(BatteryMonitorIntegrationTest, EventListener) {
  // Set the fake battery monitor to return default battery status values.
  // From JavaScript request a promise for the battery status information.
  // Once it resolves add an event listener for battery level change. Set
  // battery level to 0.6 and invoke update. Check that the event listener
  // is invoked with the correct value for level and navigate to #pass.
  device::BatteryStatus status;
  UpdateBattery(status);

  TestNavigationObserver same_tab_observer(shell()->web_contents(), 2);
  GURL test_url =
      GetTestUrl("battery_status", "battery_status_event_listener_test.html");
  shell()->LoadURL(test_url);
  same_tab_observer.Wait();
  EXPECT_EQ("resolved", shell()->web_contents()->GetLastCommittedURL().ref());

  TestNavigationObserver same_tab_observer2(shell()->web_contents(), 1);
  status.level = 0.6;
  UpdateBattery(status);
  same_tab_observer2.Wait();
  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
}

}  //  namespace

}  //  namespace content
