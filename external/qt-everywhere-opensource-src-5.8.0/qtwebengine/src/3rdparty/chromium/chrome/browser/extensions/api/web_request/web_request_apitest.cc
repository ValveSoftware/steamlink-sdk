// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/active_tab_permission_granter.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/login/login_handler.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/extension_process_policy.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/blocked_action_type.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/features/feature.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

using content::WebContents;

namespace extensions {

namespace {

class CancelLoginDialog : public content::NotificationObserver {
 public:
  CancelLoginDialog() {
    registrar_.Add(this,
                   chrome::NOTIFICATION_AUTH_NEEDED,
                   content::NotificationService::AllSources());
  }

  ~CancelLoginDialog() override {}

  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    LoginHandler* handler =
        content::Details<LoginNotificationDetails>(details).ptr()->handler();
    handler->CancelAuth();
  }

 private:
  content::NotificationRegistrar registrar_;

 DISALLOW_COPY_AND_ASSIGN(CancelLoginDialog);
};

// Sends an XHR request to the provided host, port, and path, and responds when
// the request was sent.
const char kPerformXhrJs[] =
    "var url = 'http://%s:%d/%s';\n"
    "var xhr = new XMLHttpRequest();\n"
    "xhr.open('GET', url);\n"
    "xhr.onload = function() {\n"
    "  window.domAutomationController.send(true);\n"
    "};\n"
    "xhr.onerror = function() {\n"
    "  window.domAutomationController.send(false);\n"
    "};\n"
    "xhr.send();\n";

// Performs an XHR in the given |web_contents|, replying when complete.
void PerformXhrInPage(content::WebContents* web_contents,
                      const std::string& host,
                      int port,
                      const std::string& page) {
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      web_contents,
      base::StringPrintf(kPerformXhrJs, host.c_str(), port, page.c_str()),
      &success));
  EXPECT_TRUE(success);
}

// Returns the current count of webRequests received by the |extension| in
// the background page (assumes the extension stores a value on the window
// object). Returns -1 if something goes awry.
int GetWebRequestCountFromBackgroundPage(const Extension* extension,
                                         content::BrowserContext* context) {
  ExtensionHost* host =
      ProcessManager::Get(context)->GetBackgroundHostForExtension(
          extension->id());
  if (!host || !host->host_contents())
    return -1;

  int count = -1;
  if (!ExecuteScriptAndExtractInt(
          host->host_contents(),
          "window.domAutomationController.send(window.webRequestCount)",
          &count))
    return -1;
  return count;
}

}  // namespace

class ExtensionWebRequestApiTest : public ExtensionApiTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void RunPermissionTest(
      const char* extension_directory,
      bool load_extension_with_incognito_permission,
      bool wait_for_extension_loaded_in_incognito,
      const char* expected_content_regular_window,
      const char* exptected_content_incognito_window);
};

IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest, WebRequestApi) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionSubtest("webrequest", "test_api.html")) << message_;
}

// Fails often on Windows dbg bots. http://crbug.com/177163
#if defined(OS_WIN)
#define MAYBE_WebRequestSimple DISABLED_WebRequestSimple
#else
#define MAYBE_WebRequestSimple WebRequestSimple
#endif  // defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest, MAYBE_WebRequestSimple) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionSubtest("webrequest", "test_simple.html")) <<
      message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest, WebRequestComplex) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionSubtest("webrequest", "test_complex.html")) <<
      message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest, WebRequestTypes) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionSubtest("webrequest", "test_types.html")) << message_;
}

// Test that the webRequest events are dispatched with the expected details when
// a frame or tab is removed while a response is being received.
// Flaky: https://crbug.com/617865
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       DISABLED_WebRequestUnloadAfterRequest) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionSubtest("webrequest", "test_unload.html?1")) <<
      message_;
  ASSERT_TRUE(RunExtensionSubtest("webrequest", "test_unload.html?2")) <<
      message_;
  ASSERT_TRUE(RunExtensionSubtest("webrequest", "test_unload.html?3")) <<
      message_;
  ASSERT_TRUE(RunExtensionSubtest("webrequest", "test_unload.html?4")) <<
      message_;
}

// Test that the webRequest events are dispatched with the expected details when
// a frame or tab is immediately removed after starting a request.
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       WebRequestUnloadImmediately) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionSubtest("webrequest", "test_unload.html?5")) <<
      message_;
  ASSERT_TRUE(RunExtensionSubtest("webrequest", "test_unload.html?6")) <<
      message_;
}

// Flaky (sometimes crash): http://crbug.com/140976
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       DISABLED_WebRequestAuthRequired) {
  CancelLoginDialog login_dialog_helper;

  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionSubtest("webrequest", "test_auth_required.html")) <<
      message_;
}

// This test times out regularly on win_rel trybots. See http://crbug.com/122178
#if defined(OS_WIN)
#define MAYBE_WebRequestBlocking DISABLED_WebRequestBlocking
#else
#define MAYBE_WebRequestBlocking WebRequestBlocking
#endif
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest, MAYBE_WebRequestBlocking) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionSubtest("webrequest", "test_blocking.html")) <<
      message_;
}

// Fails often on Windows dbg bots. http://crbug.com/177163
#if defined(OS_WIN)
#define MAYBE_WebRequestNewTab DISABLED_WebRequestNewTab
#else
#define MAYBE_WebRequestNewTab WebRequestNewTab
#endif  // defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest, MAYBE_WebRequestNewTab) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  // Wait for the extension to set itself up and return control to us.
  ASSERT_TRUE(RunExtensionSubtest("webrequest", "test_newTab.html"))
      << message_;

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::WaitForLoadStop(tab);

  ResultCatcher catcher;

  ExtensionService* service =
      ExtensionSystem::Get(browser()->profile())->extension_service();
  const Extension* extension =
      service->GetExtensionById(last_loaded_extension_id(), false);
  GURL url = extension->GetResourceURL("newTab/a.html");

  ui_test_utils::NavigateToURL(browser(), url);

  // There's a link on a.html with target=_blank. Click on it to open it in a
  // new tab.
  blink::WebMouseEvent mouse_event;
  mouse_event.type = blink::WebInputEvent::MouseDown;
  mouse_event.button = blink::WebMouseEvent::ButtonLeft;
  mouse_event.x = 7;
  mouse_event.y = 7;
  mouse_event.clickCount = 1;
  tab->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(mouse_event);
  mouse_event.type = blink::WebInputEvent::MouseUp;
  tab->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(mouse_event);

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest, WebRequestDeclarative1) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionSubtest("webrequest", "test_declarative1.html"))
      << message_;
}

// This test times out on XP. See http://crbug.com/178296
#if defined(OS_WIN)
#define MAYBE_WebRequestDeclarative2 DISABLED_WebRequestDeclarative2
#else
#define MAYBE_WebRequestDeclarative2 WebRequestDeclarative2
#endif
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       MAYBE_WebRequestDeclarative2) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionSubtest("webrequest", "test_declarative2.html"))
      << message_;
}

void ExtensionWebRequestApiTest::RunPermissionTest(
    const char* extension_directory,
    bool load_extension_with_incognito_permission,
    bool wait_for_extension_loaded_in_incognito,
    const char* expected_content_regular_window,
    const char* exptected_content_incognito_window) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());
  ResultCatcher catcher_incognito;
  catcher_incognito.RestrictToBrowserContext(
      browser()->profile()->GetOffTheRecordProfile());

  ExtensionTestMessageListener listener("done", false);
  ExtensionTestMessageListener listener_incognito("done_incognito", false);

  int load_extension_flags = kFlagNone;
  if (load_extension_with_incognito_permission)
    load_extension_flags |= kFlagEnableIncognito;
  ASSERT_TRUE(LoadExtensionWithFlags(
      test_data_dir_.AppendASCII("webrequest_permissions")
                    .AppendASCII(extension_directory),
      load_extension_flags));

  // Test that navigation in regular window is properly redirected.
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  // This navigation should be redirected.
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/extensions/test_file.html"));

  std::string body;
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
        tab,
        "window.domAutomationController.send(document.body.textContent)",
        &body));
  EXPECT_EQ(expected_content_regular_window, body);

  // Test that navigation in OTR window is properly redirected.
  Browser* otr_browser =
      OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));

  if (wait_for_extension_loaded_in_incognito)
    EXPECT_TRUE(listener_incognito.WaitUntilSatisfied());

  // This navigation should be redirected if
  // load_extension_with_incognito_permission is true.
  ui_test_utils::NavigateToURL(
      otr_browser,
      embedded_test_server()->GetURL("/extensions/test_file.html"));

  body.clear();
  WebContents* otr_tab = otr_browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      otr_tab,
      "window.domAutomationController.send(document.body.textContent)",
      &body));
  EXPECT_EQ(exptected_content_incognito_window, body);
}

IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       WebRequestDeclarativePermissionSpanning1) {
  // Test spanning with incognito permission.
  ASSERT_TRUE(StartEmbeddedTestServer());
  RunPermissionTest("spanning", true, false, "redirected1", "redirected1");
}

IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       WebRequestDeclarativePermissionSpanning2) {
  // Test spanning without incognito permission.
  ASSERT_TRUE(StartEmbeddedTestServer());
  RunPermissionTest("spanning", false, false, "redirected1", "");
}


IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       WebRequestDeclarativePermissionSplit1) {
  // Test split with incognito permission.
  ASSERT_TRUE(StartEmbeddedTestServer());
  RunPermissionTest("split", true, true, "redirected1", "redirected2");
}

IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       WebRequestDeclarativePermissionSplit2) {
  // Test split without incognito permission.
  ASSERT_TRUE(StartEmbeddedTestServer());
  RunPermissionTest("split", false, false, "redirected1", "");
}

// TODO(vabr): Cure these flaky tests, http://crbug.com/238179.
#if !defined(NDEBUG)
#define MAYBE_PostData1 DISABLED_PostData1
#define MAYBE_PostData2 DISABLED_PostData2
#else
#define MAYBE_PostData1 PostData1
#define MAYBE_PostData2 PostData2
#endif
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest, MAYBE_PostData1) {
  // Test HTML form POST data access with the default and "url" encoding.
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionSubtest("webrequest", "test_post1.html")) <<
      message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest, MAYBE_PostData2) {
  // Test HTML form POST data access with the multipart and plaintext encoding.
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionSubtest("webrequest", "test_post2.html")) <<
      message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       DeclarativeSendMessage) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webrequest_sendmessage")) << message_;
}

// Check that reloading an extension that runs in incognito split mode and
// has two active background pages with registered events does not crash the
// browser. Regression test for http://crbug.com/224094
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest, IncognitoSplitModeReload) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  // Wait for rules to be set up.
  ExtensionTestMessageListener listener("done", false);
  ExtensionTestMessageListener listener_incognito("done_incognito", false);

  const Extension* extension = LoadExtensionWithFlags(
      test_data_dir_.AppendASCII("webrequest_reload"), kFlagEnableIncognito);
  ASSERT_TRUE(extension);
  OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));

  EXPECT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_TRUE(listener_incognito.WaitUntilSatisfied());

  // Reload extension and wait for rules to be set up again. This should not
  // crash the browser.
  ExtensionTestMessageListener listener2("done", false);
  ExtensionTestMessageListener listener_incognito2("done_incognito", false);

  ReloadExtension(extension->id());

  EXPECT_TRUE(listener2.WaitUntilSatisfied());
  EXPECT_TRUE(listener_incognito2.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest, ExtensionRequests) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ExtensionTestMessageListener listener_main1("web_request_status1", true);
  ExtensionTestMessageListener listener_main2("web_request_status2", true);

  ExtensionTestMessageListener listener_app("app_done", false);
  ExtensionTestMessageListener listener_extension("extension_done", false);

  // Set up webRequest listener
  ASSERT_TRUE(LoadExtension(
          test_data_dir_.AppendASCII("webrequest_extensions/main")));
  EXPECT_TRUE(listener_main1.WaitUntilSatisfied());
  EXPECT_TRUE(listener_main2.WaitUntilSatisfied());

  // Perform some network activity in an app and another extension.
  ASSERT_TRUE(LoadExtension(
          test_data_dir_.AppendASCII("webrequest_extensions/app")));
  ASSERT_TRUE(LoadExtension(
          test_data_dir_.AppendASCII("webrequest_extensions/extension")));

  EXPECT_TRUE(listener_app.WaitUntilSatisfied());
  EXPECT_TRUE(listener_extension.WaitUntilSatisfied());

  // Load a page, a content script will ping us when it is ready.
  ExtensionTestMessageListener listener_pageready("contentscript_ready", true);
  ui_test_utils::NavigateToURL(browser(), embedded_test_server()->GetURL(
          "/extensions/test_file.html?match_webrequest_test"));
  EXPECT_TRUE(listener_pageready.WaitUntilSatisfied());

  // The extension and app-generated requests should not have triggered any
  // webRequest event filtered by type 'xmlhttprequest'.
  // (check this here instead of before the navigation, in case the webRequest
  // event routing is slow for some reason).
  ExtensionTestMessageListener listener_result(false);
  listener_main1.Reply("");
  EXPECT_TRUE(listener_result.WaitUntilSatisfied());
  EXPECT_EQ("Did not intercept any requests.", listener_result.message());

  ExtensionTestMessageListener listener_contentscript("contentscript_done",
                                                      false);
  ExtensionTestMessageListener listener_framescript("framescript_done", false);

  // Proceed with the final tests: Let the content script fire a request and
  // then load an iframe which also fires a XHR request.
  listener_pageready.Reply("");
  EXPECT_TRUE(listener_contentscript.WaitUntilSatisfied());
  EXPECT_TRUE(listener_framescript.WaitUntilSatisfied());

  // Collect the visited URLs. The content script and subframe does not run in
  // the extension's process, so the requests should be visible to the main
  // extension.
  listener_result.Reset();
  listener_main2.Reply("");
  EXPECT_TRUE(listener_result.WaitUntilSatisfied());
  if (content::AreAllSitesIsolatedForTesting() ||
      IsIsolateExtensionsEnabled()) {
    // With --site-per-process, the extension frame does run in the extension's
    // process.
    EXPECT_EQ("Intercepted requests: ?contentscript",
              listener_result.message());
  } else {
    EXPECT_EQ("Intercepted requests: ?contentscript, ?framescript",
              listener_result.message());
  }
}

IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest, HostedAppRequest) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  GURL hosted_app_url(
      embedded_test_server()->GetURL(
          "/extensions/api_test/webrequest_hosted_app/index.html"));
  scoped_refptr<Extension> hosted_app =
      ExtensionBuilder()
          .SetManifest(
              DictionaryBuilder()
                  .Set("name", "Some hosted app")
                  .Set("version", "1")
                  .Set("manifest_version", 2)
                  .Set("app", DictionaryBuilder()
                                  .Set("launch", DictionaryBuilder()
                                                     .Set("web_url",
                                                          hosted_app_url.spec())
                                                     .Build())
                                  .Build())
                  .Build())
          .Build();
  ExtensionSystem::Get(browser()->profile())
      ->extension_service()
      ->AddExtension(hosted_app.get());

  ExtensionTestMessageListener listener1("main_frame", false);
  ExtensionTestMessageListener listener2("xmlhttprequest", false);

  ASSERT_TRUE(LoadExtension(
          test_data_dir_.AppendASCII("webrequest_hosted_app")));

  ui_test_utils::NavigateToURL(browser(), hosted_app_url);

  EXPECT_TRUE(listener1.WaitUntilSatisfied());
  EXPECT_TRUE(listener2.WaitUntilSatisfied());
}

// Tests that webRequest works with the --scripts-require-action feature.
IN_PROC_BROWSER_TEST_F(ExtensionWebRequestApiTest,
                       WebRequestWithWithheldPermissions) {
  FeatureSwitch::ScopedOverride enable_scripts_require_action(
      FeatureSwitch::scripts_require_action(), true);
  ASSERT_TRUE(StartEmbeddedTestServer());
  // Load an extension that registers a listener for webRequest events, and
  // wait 'til it's initialized.
  ExtensionTestMessageListener listener("ready", false);
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("webrequest_activetab"));
  ASSERT_TRUE(extension) << message_;
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  // Navigate the browser to a page in a new tab.
  const std::string kHost = "example.com";
  GURL url = embedded_test_server()->GetURL(kHost, "/empty.html");
  chrome::NavigateParams params(browser(), url, ui::PAGE_TRANSITION_LINK);
  params.disposition = NEW_FOREGROUND_TAB;
  ui_test_utils::NavigateToURL(&params);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  ExtensionActionRunner* runner =
      ExtensionActionRunner::GetForWebContents(web_contents);
  ASSERT_TRUE(runner);

  int port = embedded_test_server()->port();
  const std::string kXhrPath = "simple.html";

  // The extension shouldn't have currently received any webRequest events,
  // since it doesn't have permission (and shouldn't receive any from an XHR).
  EXPECT_EQ(0, GetWebRequestCountFromBackgroundPage(extension, profile()));
  PerformXhrInPage(web_contents, kHost, port, kXhrPath);
  EXPECT_EQ(0, GetWebRequestCountFromBackgroundPage(extension, profile()));

  // Grant activeTab permission, and perform another XHR. The extension should
  // receive the event.
  EXPECT_EQ(BLOCKED_ACTION_WEB_REQUEST, runner->GetBlockedActions(extension));
  runner->set_default_bubble_close_action_for_testing(
      base::WrapUnique(new ToolbarActionsBarBubbleDelegate::CloseAction(
          ToolbarActionsBarBubbleDelegate::CLOSE_EXECUTE)));
  runner->RunAction(extension, true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  // The runner will have refreshed the page...
  EXPECT_EQ(BLOCKED_ACTION_NONE, runner->GetBlockedActions(extension));
  int xhr_count = GetWebRequestCountFromBackgroundPage(extension, profile());
  // ... which means that we should have a non-zero xhr count.
  EXPECT_GT(xhr_count, 0);
  // And the extension should receive future events.
  PerformXhrInPage(web_contents, kHost, port, kXhrPath);
  ++xhr_count;
  EXPECT_EQ(xhr_count,
            GetWebRequestCountFromBackgroundPage(extension, profile()));

  // If we revoke the extension's tab permissions, it should no longer receive
  // webRequest events.
  ActiveTabPermissionGranter* granter =
      TabHelper::FromWebContents(web_contents)->active_tab_permission_granter();
  ASSERT_TRUE(granter);
  granter->RevokeForTesting();
  base::RunLoop().RunUntilIdle();
  PerformXhrInPage(web_contents, kHost, port, kXhrPath);
  EXPECT_EQ(xhr_count,
            GetWebRequestCountFromBackgroundPage(extension, profile()));
  EXPECT_EQ(BLOCKED_ACTION_WEB_REQUEST, runner->GetBlockedActions(extension));
}

}  // namespace extensions
