// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/notifications/notifications_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/test/test_api.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/test_util.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/notifier_settings.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "ui/base/test/scoped_fake_nswindow_fullscreen.h"
#endif

using extensions::AppWindow;
using extensions::AppWindowRegistry;
using extensions::Extension;
using extensions::ResultCatcher;

namespace utils = extension_function_test_utils;

namespace {

// A class that waits for a |chrome.test.sendMessage| call, ignores the message,
// and writes down the user gesture status of the message.
class UserGestureCatcher : public content::NotificationObserver {
 public:
  UserGestureCatcher() : waiting_(false) {
    registrar_.Add(this,
                   extensions::NOTIFICATION_EXTENSION_TEST_MESSAGE,
                   content::NotificationService::AllSources());
  }

  ~UserGestureCatcher() override {}

  bool GetNextResult() {
    if (results_.empty()) {
      waiting_ = true;
      content::RunMessageLoop();
      waiting_ = false;
    }

    if (!results_.empty()) {
      bool ret = results_.front();
      results_.pop_front();
      return ret;
    }
    NOTREACHED();
    return false;
  }

 private:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    results_.push_back(
        static_cast<content::Source<extensions::TestSendMessageFunction> >(
            source)
            .ptr()
            ->user_gesture());
    if (waiting_)
      base::MessageLoopForUI::current()->QuitWhenIdle();
  }

  content::NotificationRegistrar registrar_;

  // A sequential list of user gesture notifications from the test extension(s).
  std::deque<bool> results_;

  // True if we're in a nested message loop waiting for results from
  // the extension.
  bool waiting_;
};

enum class WindowState {
  FULLSCREEN,
  NORMAL
};

class NotificationsApiTest : public ExtensionApiTest {
 public:
  const Extension* LoadExtensionAndWait(
      const std::string& test_name) {
    base::FilePath extdir = test_data_dir_.AppendASCII(test_name);
    content::WindowedNotificationObserver page_created(
        extensions::NOTIFICATION_EXTENSION_BACKGROUND_PAGE_READY,
        content::NotificationService::AllSources());
    const extensions::Extension* extension = LoadExtension(extdir);
    if (extension) {
      page_created.Wait();
    }
    return extension;
  }

  const Extension* LoadAppWithWindowState(
      const std::string& test_name, WindowState window_state) {
    const char* window_state_string = NULL;
    switch (window_state) {
      case WindowState::FULLSCREEN:
        window_state_string = "fullscreen";
        break;
      case WindowState::NORMAL:
        window_state_string = "normal";
        break;
    }
    const std::string& create_window_options = base::StringPrintf(
        "{\"state\":\"%s\"}", window_state_string);
    base::FilePath extdir = test_data_dir_.AppendASCII(test_name);
    const extensions::Extension* extension = LoadExtension(extdir);
    EXPECT_TRUE(extension);

    ExtensionTestMessageListener launched_listener("launched", true);
    LaunchPlatformApp(extension);
    EXPECT_TRUE(launched_listener.WaitUntilSatisfied());
    launched_listener.Reply(create_window_options);

    return extension;
  }

  AppWindow* GetFirstAppWindow(const std::string& app_id) {
    AppWindowRegistry::AppWindowList app_windows = AppWindowRegistry::Get(
        browser()->profile())->GetAppWindowsForApp(app_id);

    AppWindowRegistry::const_iterator iter = app_windows.begin();
    if (iter != app_windows.end())
      return *iter;

    return NULL;
  }

 protected:
  std::string GetNotificationIdFromDelegateId(const std::string& delegate_id) {
    return g_browser_process->notification_ui_manager()
        ->FindById(
              delegate_id,
              NotificationUIManager::GetProfileID(
                  g_browser_process->profile_manager()->GetLastUsedProfile()))
        ->id();
  }

  void LaunchPlatformApp(const Extension* extension) {
    OpenApplication(AppLaunchParams(
        browser()->profile(), extension, extensions::LAUNCH_CONTAINER_NONE,
        WindowOpenDisposition::NEW_WINDOW, extensions::SOURCE_TEST));
  }

  void EnableFullscreenNotifications() {
    feature_list_.InitWithFeatures({
      features::kPreferHtmlOverPlugins,
      extensions::kAllowFullscreenAppNotificationsFeature}, {});
  }

  void DisableFullscreenNotifications() {
    feature_list_.InitWithFeatures(
        {features::kPreferHtmlOverPlugins},
        {extensions::kAllowFullscreenAppNotificationsFeature});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(NotificationsApiTest, TestBasicUsage) {
  ASSERT_TRUE(RunExtensionTest("notifications/api/basic_usage")) << message_;
}

IN_PROC_BROWSER_TEST_F(NotificationsApiTest, TestEvents) {
  ASSERT_TRUE(RunExtensionTest("notifications/api/events")) << message_;
}

IN_PROC_BROWSER_TEST_F(NotificationsApiTest, TestCSP) {
  ASSERT_TRUE(RunExtensionTest("notifications/api/csp")) << message_;
}

IN_PROC_BROWSER_TEST_F(NotificationsApiTest, TestByUser) {
  const extensions::Extension* extension =
      LoadExtensionAndWait("notifications/api/by_user");
  ASSERT_TRUE(extension) << message_;

  {
    ResultCatcher catcher;
    const std::string notification_id =
        GetNotificationIdFromDelegateId(extension->id() + "-FOO");
    g_browser_process->message_center()->RemoveNotification(notification_id,
                                                            false);
    EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  }

  {
    ResultCatcher catcher;
    const std::string notification_id =
        GetNotificationIdFromDelegateId(extension->id() + "-BAR");
    g_browser_process->message_center()->RemoveNotification(notification_id,
                                                            true);
    EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  }

  {
    ResultCatcher catcher;
    g_browser_process->message_center()->RemoveAllNotifications(
        false /* by_user */, message_center::MessageCenter::RemoveType::ALL);
    EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  }
  {
    ResultCatcher catcher;
    g_browser_process->message_center()->RemoveAllNotifications(
        true /* by_user */, message_center::MessageCenter::RemoveType::ALL);
    EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  }
}

IN_PROC_BROWSER_TEST_F(NotificationsApiTest, TestPartialUpdate) {
  ASSERT_TRUE(RunExtensionTest("notifications/api/partial_update")) << message_;
  const extensions::Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  const char kNewTitle[] = "Changed!";
  const char kNewMessage[] = "Too late! The show ended yesterday";
  int kNewPriority = 2;
  const char kButtonTitle[] = "NewButton";

  const message_center::NotificationList::Notifications& notifications =
      g_browser_process->message_center()->GetVisibleNotifications();
  ASSERT_EQ(1u, notifications.size());
  message_center::Notification* notification = *(notifications.begin());
  ASSERT_EQ(extension->url(), notification->origin_url());

  LOG(INFO) << "Notification ID: " << notification->id();

  EXPECT_EQ(base::ASCIIToUTF16(kNewTitle), notification->title());
  EXPECT_EQ(base::ASCIIToUTF16(kNewMessage), notification->message());
  EXPECT_EQ(kNewPriority, notification->priority());
  EXPECT_EQ(1u, notification->buttons().size());
  EXPECT_EQ(base::ASCIIToUTF16(kButtonTitle), notification->buttons()[0].title);
}

IN_PROC_BROWSER_TEST_F(NotificationsApiTest, TestGetPermissionLevel) {
  scoped_refptr<Extension> empty_extension(
      extensions::test_util::CreateEmptyExtension());

  // Get permission level for the extension whose notifications are enabled.
  {
    scoped_refptr<extensions::NotificationsGetPermissionLevelFunction>
        notification_function(
            new extensions::NotificationsGetPermissionLevelFunction());

    notification_function->set_extension(empty_extension.get());
    notification_function->set_has_callback(true);

    std::unique_ptr<base::Value> result(utils::RunFunctionAndReturnSingleResult(
        notification_function.get(), "[]", browser(), utils::NONE));

    EXPECT_EQ(base::Value::TYPE_STRING, result->GetType());
    std::string permission_level;
    EXPECT_TRUE(result->GetAsString(&permission_level));
    EXPECT_EQ("granted", permission_level);
  }

  // Get permission level for the extension whose notifications are disabled.
  {
    scoped_refptr<extensions::NotificationsGetPermissionLevelFunction>
        notification_function(
            new extensions::NotificationsGetPermissionLevelFunction());

    notification_function->set_extension(empty_extension.get());
    notification_function->set_has_callback(true);

    message_center::NotifierId notifier_id(
        message_center::NotifierId::APPLICATION,
        empty_extension->id());
    message_center::Notifier notifier(notifier_id, base::string16(), true);
    g_browser_process->message_center()->GetNotifierSettingsProvider()->
        SetNotifierEnabled(notifier, false);

    std::unique_ptr<base::Value> result(utils::RunFunctionAndReturnSingleResult(
        notification_function.get(), "[]", browser(), utils::NONE));

    EXPECT_EQ(base::Value::TYPE_STRING, result->GetType());
    std::string permission_level;
    EXPECT_TRUE(result->GetAsString(&permission_level));
    EXPECT_EQ("denied", permission_level);
  }
}

IN_PROC_BROWSER_TEST_F(NotificationsApiTest, TestOnPermissionLevelChanged) {
  const extensions::Extension* extension =
      LoadExtensionAndWait("notifications/api/permission");
  ASSERT_TRUE(extension) << message_;

  // Test permission level changing from granted to denied.
  {
    ResultCatcher catcher;

    message_center::NotifierId notifier_id(
        message_center::NotifierId::APPLICATION,
        extension->id());
    message_center::Notifier notifier(notifier_id, base::string16(), true);
    g_browser_process->message_center()->GetNotifierSettingsProvider()->
        SetNotifierEnabled(notifier, false);

    EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  }

  // Test permission level changing from denied to granted.
  {
    ResultCatcher catcher;

    message_center::NotifierId notifier_id(
        message_center::NotifierId::APPLICATION,
        extension->id());
    message_center::Notifier notifier(notifier_id, base::string16(), false);
    g_browser_process->message_center()->GetNotifierSettingsProvider()->
        SetNotifierEnabled(notifier, true);

    EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  }
}

IN_PROC_BROWSER_TEST_F(NotificationsApiTest, TestUserGesture) {
  const extensions::Extension* extension =
      LoadExtensionAndWait("notifications/api/user_gesture");
  ASSERT_TRUE(extension) << message_;

  const message_center::NotificationList::Notifications& notifications =
      g_browser_process->message_center()->GetVisibleNotifications();
  ASSERT_EQ(1u, notifications.size());
  message_center::Notification* notification = *(notifications.begin());
  ASSERT_EQ(extension->url(), notification->origin_url());

  {
    UserGestureCatcher catcher;
    notification->ButtonClick(0);
    EXPECT_TRUE(catcher.GetNextResult());
    notification->Click();
    EXPECT_TRUE(catcher.GetNextResult());
    notification->Close(true);
    EXPECT_TRUE(catcher.GetNextResult());
    notification->Close(false);
    EXPECT_FALSE(catcher.GetNextResult());
  }
}

IN_PROC_BROWSER_TEST_F(NotificationsApiTest, TestRequireInteraction) {
  const extensions::Extension* extension =
      LoadExtensionAndWait("notifications/api/require_interaction");
  ASSERT_TRUE(extension) << message_;

  const message_center::NotificationList::Notifications& notifications =
      g_browser_process->message_center()->GetVisibleNotifications();
  ASSERT_EQ(1u, notifications.size());
  message_center::Notification* notification = *(notifications.begin());
  ASSERT_EQ(extension->url(), notification->origin_url());

  EXPECT_TRUE(notification->never_timeout());
}

IN_PROC_BROWSER_TEST_F(NotificationsApiTest, TestShouldDisplayNormal) {
  EnableFullscreenNotifications();
  ExtensionTestMessageListener notification_created_listener("created", false);
  const Extension* extension = LoadAppWithWindowState(
      "notifications/api/basic_app", WindowState::NORMAL);
  ASSERT_TRUE(extension) << message_;
  ASSERT_TRUE(notification_created_listener.WaitUntilSatisfied());

  // We start by making sure the window is actually focused.
  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
      GetFirstAppWindow(extension->id())->GetNativeWindow()));

  const message_center::NotificationList::Notifications& notifications =
      g_browser_process->message_center()->GetVisibleNotifications();
  ASSERT_EQ(1u, notifications.size());
  message_center::Notification* notification = *(notifications.begin());
  // If the app hasn't created a fullscreen window, then its notifications
  // shouldn't be displayed when a window is fullscreen.
  ASSERT_FALSE(notification->delegate()->ShouldDisplayOverFullscreen());
}

IN_PROC_BROWSER_TEST_F(NotificationsApiTest, TestShouldDisplayFullscreen) {
#if defined(OS_MACOSX)
  ui::test::ScopedFakeNSWindowFullscreen fake_fullscreen;
#endif
  EnableFullscreenNotifications();
  ExtensionTestMessageListener notification_created_listener("created", false);
  const Extension* extension = LoadAppWithWindowState(
      "notifications/api/basic_app", WindowState::FULLSCREEN);
  ASSERT_TRUE(extension) << message_;
  ASSERT_TRUE(notification_created_listener.WaitUntilSatisfied());

  // We start by making sure the window is actually focused.
  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
      GetFirstAppWindow(extension->id())->GetNativeWindow()));

  ASSERT_TRUE(GetFirstAppWindow(extension->id())->IsFullscreen())
      << "Not Fullscreen";
  ASSERT_TRUE(GetFirstAppWindow(extension->id())->GetBaseWindow()->IsActive())
      << "Not Active";

  const message_center::NotificationList::Notifications& notifications =
      g_browser_process->message_center()->GetVisibleNotifications();
  ASSERT_EQ(1u, notifications.size());
  message_center::Notification* notification = *(notifications.begin());
  // If the app has created a fullscreen window, then its notifications should
  // be displayed when a window is fullscreen.
  ASSERT_TRUE(notification->delegate()->ShouldDisplayOverFullscreen());
}

IN_PROC_BROWSER_TEST_F(NotificationsApiTest, TestShouldDisplayFullscreenOff) {
#if defined(OS_MACOSX)
  ui::test::ScopedFakeNSWindowFullscreen fake_fullscreen;
#endif
  DisableFullscreenNotifications();
  ExtensionTestMessageListener notification_created_listener("created", false);
  const Extension* extension = LoadAppWithWindowState(
      "notifications/api/basic_app", WindowState::FULLSCREEN);
  ASSERT_TRUE(extension) << message_;
  ASSERT_TRUE(notification_created_listener.WaitUntilSatisfied());

  // We start by making sure the window is actually focused.
  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
      GetFirstAppWindow(extension->id())->GetNativeWindow()));

  ASSERT_TRUE(GetFirstAppWindow(extension->id())->IsFullscreen())
      << "Not Fullscreen";
  ASSERT_TRUE(GetFirstAppWindow(extension->id())->GetBaseWindow()->IsActive())
      << "Not Active";

  const message_center::NotificationList::Notifications& notifications =
      g_browser_process->message_center()->GetVisibleNotifications();
  ASSERT_EQ(1u, notifications.size());
  message_center::Notification* notification = *(notifications.begin());
  // When the experiment flag is off, then ShouldDisplayOverFullscreen should
  // return false.
  ASSERT_FALSE(notification->delegate()->ShouldDisplayOverFullscreen());
}

// The Fake OSX fullscreen window doesn't like drawing a second fullscreen
// window when another is visible.
#if !defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(NotificationsApiTest, TestShouldDisplayMultiFullscreen) {
  // Start a fullscreen app, and then start another fullscreen app on top of the
  // first. Notifications from the first should not be displayed because it is
  // not the app actually displaying on the screen.
  EnableFullscreenNotifications();
  ExtensionTestMessageListener notification_created_listener("created", false);
  const Extension* extension1 = LoadAppWithWindowState(
      "notifications/api/basic_app", WindowState::FULLSCREEN);
  ASSERT_TRUE(extension1) << message_;

  ExtensionTestMessageListener window_visible_listener("visible", false);
  const Extension* extension2 = LoadAppWithWindowState(
      "notifications/api/other_app", WindowState::FULLSCREEN);
  ASSERT_TRUE(extension2) << message_;

  ASSERT_TRUE(notification_created_listener.WaitUntilSatisfied());
  ASSERT_TRUE(window_visible_listener.WaitUntilSatisfied());

  // We start by making sure the window is actually focused.
  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
      GetFirstAppWindow(extension2->id())->GetNativeWindow()));

  const message_center::NotificationList::Notifications& notifications =
      g_browser_process->message_center()->GetVisibleNotifications();
  ASSERT_EQ(1u, notifications.size());
  message_center::Notification* notification = *(notifications.begin());
  // The first app window is superseded by the second window, so its
  // notification shouldn't be displayed.
  ASSERT_FALSE(notification->delegate()->ShouldDisplayOverFullscreen());
}
#endif

// Verify that a notification is actually displayed when the app window that
// creates it is fullscreen with the fullscreen notification flag turned on.
IN_PROC_BROWSER_TEST_F(NotificationsApiTest,
                       TestShouldDisplayPopupNotification) {
#if defined(OS_MACOSX)
  ui::test::ScopedFakeNSWindowFullscreen fake_fullscreen;
#endif
  EnableFullscreenNotifications();
  ExtensionTestMessageListener notification_created_listener("created", false);
  const Extension* extension = LoadAppWithWindowState(
      "notifications/api/basic_app", WindowState::FULLSCREEN);
  ASSERT_TRUE(extension) << message_;
  ASSERT_TRUE(notification_created_listener.WaitUntilSatisfied());

  // We start by making sure the window is actually focused.
  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
      GetFirstAppWindow(extension->id())->GetNativeWindow()));

  ASSERT_TRUE(GetFirstAppWindow(extension->id())->IsFullscreen())
      << "Not Fullscreen";
  ASSERT_TRUE(GetFirstAppWindow(extension->id())->GetBaseWindow()->IsActive())
      << "Not Active";

  const message_center::NotificationList::PopupNotifications notifications =
      g_browser_process->message_center()->GetPopupNotifications();
  ASSERT_EQ(1u, notifications.size());
}
