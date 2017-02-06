// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/net/network_portal_detector_impl.h"
#include "chrome/browser/chromeos/net/network_portal_notification_controller.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_profile_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "components/captive_portal/captive_portal_testing_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/test/result_catcher.h"
#include "net/base/net_errors.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"

using chromeos::DBusThreadManager;
using chromeos::NetworkPortalDetector;
using chromeos::NetworkPortalDetectorImpl;
using chromeos::NetworkPortalNotificationController;
using chromeos::ShillDeviceClient;
using chromeos::ShillProfileClient;
using chromeos::ShillServiceClient;
using message_center::MessageCenter;
using message_center::MessageCenterObserver;

namespace {

const char kWifiDevicePath[] = "/device/stub_wifi_device1";
const char kWifi1ServicePath[] = "stub_wifi1";
const char kWifi1ServiceGUID[] = "wifi1_guid";

class TestNotificationObserver : public MessageCenterObserver {
 public:
  TestNotificationObserver() {
    MessageCenter::Get()->AddObserver(this);
  }

  ~TestNotificationObserver() override {
    MessageCenter::Get()->RemoveObserver(this);
  }

  void WaitForNotificationToDisplay() {
    run_loop_.Run();
  }

  void OnNotificationDisplayed(
      const std::string& notification_id,
      const message_center::DisplaySource source) override {
    if (notification_id ==
        NetworkPortalNotificationController::kNotificationId) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                    run_loop_.QuitClosure());
    }
  }

 private:
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(TestNotificationObserver);
};

}  // namespace

class NetworkingConfigTest
    : public ExtensionApiTest,
      public captive_portal::CaptivePortalDetectorTestBase {
 public:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    content::RunAllPendingInMessageLoop();

    DBusThreadManager* const dbus_manager = DBusThreadManager::Get();
    ShillServiceClient::TestInterface* const service_test =
        dbus_manager->GetShillServiceClient()->GetTestInterface();
    ShillDeviceClient::TestInterface* const device_test =
        dbus_manager->GetShillDeviceClient()->GetTestInterface();
    ShillProfileClient::TestInterface* const profile_test =
        dbus_manager->GetShillProfileClient()->GetTestInterface();

    device_test->ClearDevices();
    service_test->ClearServices();

    device_test->AddDevice(kWifiDevicePath, shill::kTypeWifi,
                           "stub_wifi_device1");

    service_test->AddService(kWifi1ServicePath, kWifi1ServiceGUID, "wifi1",
                             shill::kTypeWifi, shill::kStateOnline,
                             true /* add_to_visible */);
    service_test->SetServiceProperty(kWifi1ServicePath, shill::kWifiBSsid,
                                     base::StringValue("01:02:ab:7f:90:00"));
    service_test->SetServiceProperty(kWifi1ServicePath,
                                     shill::kSignalStrengthProperty,
                                     base::FundamentalValue(40));
    profile_test->AddService(ShillProfileClient::GetSharedProfilePath(),
                             kWifi1ServicePath);

    content::RunAllPendingInMessageLoop();

    network_portal_detector_ = new NetworkPortalDetectorImpl(
        g_browser_process->system_request_context(),
        true /* create_notification_controller */);
    chromeos::network_portal_detector::InitializeForTesting(
        network_portal_detector_);
    network_portal_detector_->Enable(false /* start_detection */);
    set_detector(network_portal_detector_->captive_portal_detector_.get());
  }

  void LoadTestExtension() {
    extension_ = LoadExtension(test_data_dir_.AppendASCII("networking_config"));
  }

  bool RunExtensionTest(const std::string& path) {
    return RunExtensionSubtest("networking_config",
                               extension_->GetResourceURL(path).spec());
  }

  void SimulateCaptivePortal() {
    network_portal_detector_->StartDetection();
    content::RunAllPendingInMessageLoop();

    // Simulate a captive portal reply.
    CompleteURLFetch(net::OK, 200, nullptr);
  }

  void SimulateSuccessfulCaptivePortalAuth() {
    content::RunAllPendingInMessageLoop();
    CompleteURLFetch(net::OK, 204, nullptr);
  }

  NetworkPortalDetector::CaptivePortalStatus GetCaptivePortalStatus(
      const std::string& guid) {
    return network_portal_detector_->GetCaptivePortalState(kWifi1ServiceGUID)
        .status;
  }

  NetworkPortalDetectorImpl* network_portal_detector_ = nullptr;

 private:
  const extensions::Extension* extension_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(NetworkingConfigTest, ApiAvailability) {
  ASSERT_TRUE(RunExtensionSubtest("networking_config", "api_availability.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingConfigTest, RegisterNetworks) {
  ASSERT_TRUE(
      RunExtensionSubtest("networking_config", "register_networks.html"))
      << message_;
}

// Test the full, positive flow starting with the extension registration and
// ending with the captive portal being authenticated.
IN_PROC_BROWSER_TEST_F(NetworkingConfigTest, FullTest) {
  LoadTestExtension();
  // This will cause the extension to register for wifi1.
  ASSERT_TRUE(RunExtensionTest("full_test.html")) << message_;

  TestNotificationObserver observer;

  SimulateCaptivePortal();

  // Wait until a captive portal notification is displayed and verify that it is
  // the expected captive portal notification.
  observer.WaitForNotificationToDisplay();
  EXPECT_TRUE(MessageCenter::Get()->FindVisibleNotificationById(
      NetworkPortalNotificationController::kNotificationId));

  // Simulate the user click which leads to the extension being notified.
  MessageCenter::Get()->ClickOnNotificationButton(
      NetworkPortalNotificationController::kNotificationId,
      NetworkPortalNotificationController::kUseExtensionButtonIndex);

  extensions::ResultCatcher catcher;
  EXPECT_TRUE(catcher.GetNextResult());

  // Simulate the captive portal vanishing.
  SimulateSuccessfulCaptivePortalAuth();

  ASSERT_EQ(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE,
            GetCaptivePortalStatus(kWifi1ServiceGUID));
}
