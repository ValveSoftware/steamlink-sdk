// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/feedback_private/feedback_private_api.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/api/feedback_private.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"

using extensions::api::feedback_private::FeedbackFlow;

namespace {

void StopMessageLoopCallback() {
  base::MessageLoopForUI::current()->QuitWhenIdle();
}

}  // namespace

namespace extensions {

class FeedbackTest : public ExtensionBrowserTest {
 public:
  void SetUp() override {
    extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();
    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(::switches::kEnableUserMediaScreenCapturing);
    InProcessBrowserTest::SetUpCommandLine(command_line);
  }

 protected:
  bool IsFeedbackAppAvailable() {
    return extensions::EventRouter::Get(browser()->profile())
        ->ExtensionHasEventListener(
            extension_misc::kFeedbackExtensionId,
            extensions::api::feedback_private::OnFeedbackRequested::kEventName);
  }

  void StartFeedbackUI(FeedbackFlow flow) {
    base::Closure callback = base::Bind(&StopMessageLoopCallback);
    extensions::FeedbackPrivateGetStringsFunction::set_test_callback(&callback);
    InvokeFeedbackUI(flow);
    content::RunMessageLoop();
    extensions::FeedbackPrivateGetStringsFunction::set_test_callback(NULL);
  }

  void VerifyFeedbackAppLaunch() {
    AppWindow* window =
        PlatformAppBrowserTest::GetFirstAppWindowForBrowser(browser());
    ASSERT_TRUE(window);
    const Extension* feedback_app = window->GetExtension();
    ASSERT_TRUE(feedback_app);
    EXPECT_EQ(feedback_app->id(),
              std::string(extension_misc::kFeedbackExtensionId));
  }

 private:
  void InvokeFeedbackUI(FeedbackFlow flow) {
    extensions::FeedbackPrivateAPI* api =
        extensions::FeedbackPrivateAPI::GetFactoryInstance()->Get(
            browser()->profile());
    api->RequestFeedbackForFlow("Test description", "Test tag",
                                GURL("http://www.test.com"), flow);
  }
};

IN_PROC_BROWSER_TEST_F(FeedbackTest, ShowFeedback) {
  WaitForExtensionViewsToLoad();

  ASSERT_TRUE(IsFeedbackAppAvailable());
  StartFeedbackUI(FeedbackFlow::FEEDBACK_FLOW_REGULAR);
  VerifyFeedbackAppLaunch();
}

IN_PROC_BROWSER_TEST_F(FeedbackTest, ShowLoginFeedback) {
  WaitForExtensionViewsToLoad();

  ASSERT_TRUE(IsFeedbackAppAvailable());
  StartFeedbackUI(FeedbackFlow::FEEDBACK_FLOW_LOGIN);
  VerifyFeedbackAppLaunch();

  AppWindow* const window =
      PlatformAppBrowserTest::GetFirstAppWindowForBrowser(browser());
  ASSERT_TRUE(window);
  content::WebContents* const content = window->web_contents();

  bool bool_result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      content,
      "domAutomationController.send("
        "$('page-url').hidden && $('attach-file-container').hidden && "
        "$('attach-file-note').hidden);",
      &bool_result));
  EXPECT_TRUE(bool_result);
}

}  // namespace extensions
