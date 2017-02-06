// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/strings/pattern.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/page_zoom.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/test_util.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "ui/base/test/scoped_fake_nswindow_fullscreen.h"
#endif

namespace extensions {

namespace keys = tabs_constants;
namespace utils = extension_function_test_utils;

namespace {
using ExtensionTabsTest = PlatformAppBrowserTest;

class ExtensionWindowCreateTest : public InProcessBrowserTest {
 public:
  // Runs chrome.windows.create(), expecting an error.
  std::string RunCreateWindowExpectError(const std::string& args) {
    scoped_refptr<WindowsCreateFunction> function(new WindowsCreateFunction);
    function->set_extension(test_util::CreateEmptyExtension().get());
    return api_test_utils::RunFunctionAndReturnError(function.get(), args,
                                                     browser()->profile());
  }
};

const int kUndefinedId = INT_MIN;

int GetTabId(base::DictionaryValue* tab) {
  int id = kUndefinedId;
  if (tab)
    tab->GetInteger(keys::kIdKey, &id);
  return id;
}

int GetTabWindowId(base::DictionaryValue* tab) {
  int id = kUndefinedId;
  if (tab)
    tab->GetInteger(keys::kWindowIdKey, &id);
  return id;
}

int GetWindowId(base::DictionaryValue* window) {
  int id = kUndefinedId;
  if (window)
    window->GetInteger(keys::kIdKey, &id);
  return id;
}

}  // namespace

#if defined(OS_WIN)
// Crashes under Dr. Memory, see https://crbug.com/605880.
#define MAYBE_GetWindow DISABLED_GetWindow
#else
#define MAYBE_GetWindow GetWindow
#endif
IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, MAYBE_GetWindow) {
  int window_id = ExtensionTabUtil::GetWindowId(browser());

  // Invalid window ID error.
  scoped_refptr<WindowsGetFunction> function = new WindowsGetFunction();
  scoped_refptr<Extension> extension(test_util::CreateEmptyExtension());
  function->set_extension(extension.get());
  EXPECT_TRUE(base::MatchPattern(
      utils::RunFunctionAndReturnError(
          function.get(), base::StringPrintf("[%u]", window_id + 1), browser()),
      keys::kWindowNotFoundError));

  // Basic window details.
  gfx::Rect bounds;
  if (browser()->window()->IsMinimized())
    bounds = browser()->window()->GetRestoredBounds();
  else
    bounds = browser()->window()->GetBounds();

  function = new WindowsGetFunction();
  function->set_extension(extension.get());
  std::unique_ptr<base::DictionaryValue> result(
      utils::ToDictionary(utils::RunFunctionAndReturnSingleResult(
          function.get(), base::StringPrintf("[%u]", window_id), browser())));
  EXPECT_EQ(window_id, GetWindowId(result.get()));
  EXPECT_FALSE(api_test_utils::GetBoolean(result.get(), "incognito"));
  EXPECT_EQ("normal", api_test_utils::GetString(result.get(), "type"));
  EXPECT_EQ(bounds.x(), api_test_utils::GetInteger(result.get(), "left"));
  EXPECT_EQ(bounds.y(), api_test_utils::GetInteger(result.get(), "top"));
  EXPECT_EQ(bounds.width(), api_test_utils::GetInteger(result.get(), "width"));
  EXPECT_EQ(bounds.height(),
            api_test_utils::GetInteger(result.get(), "height"));

  // With "populate" enabled.
  function = new WindowsGetFunction();
  function->set_extension(extension.get());
  result.reset(utils::ToDictionary(
      utils::RunFunctionAndReturnSingleResult(
          function.get(),
          base::StringPrintf("[%u, {\"populate\": true}]", window_id),
          browser())));

  EXPECT_EQ(window_id, GetWindowId(result.get()));
  // "populate" was enabled so tabs should be populated.
  base::ListValue* tabs = nullptr;
  EXPECT_TRUE(result.get()->GetList(keys::kTabsKey, &tabs));

  base::Value* tab0 = nullptr;
  EXPECT_TRUE(tabs->Get(0, &tab0));
  EXPECT_GE(GetTabId(utils::ToDictionary(tab0)), 0);

  // TODO(aa): Can't assume window is focused. On mac, calling Activate() from a
  // browser test doesn't seem to do anything, so can't test the opposite
  // either.
  EXPECT_EQ(browser()->window()->IsActive(),
            api_test_utils::GetBoolean(result.get(), "focused"));

  // TODO(aa): Minimized and maximized dimensions. Is there a way to set
  // minimize/maximize programmatically?

  // Popup.
  Browser* popup_browser = new Browser(
      Browser::CreateParams(Browser::TYPE_POPUP, browser()->profile()));
  function = new WindowsGetFunction();
  function->set_extension(extension.get());
  result.reset(utils::ToDictionary(
      utils::RunFunctionAndReturnSingleResult(
          function.get(),
          base::StringPrintf(
              "[%u]", ExtensionTabUtil::GetWindowId(popup_browser)),
          browser())));
  EXPECT_EQ("popup", api_test_utils::GetString(result.get(), "type"));

  // Incognito.
  Browser* incognito_browser = CreateIncognitoBrowser();
  int incognito_window_id = ExtensionTabUtil::GetWindowId(incognito_browser);

  // Without "include_incognito".
  function = new WindowsGetFunction();
  function->set_extension(extension.get());
  EXPECT_TRUE(base::MatchPattern(
      utils::RunFunctionAndReturnError(
          function.get(), base::StringPrintf("[%u]", incognito_window_id),
          browser()),
      keys::kWindowNotFoundError));

  // With "include_incognito".
  function = new WindowsGetFunction();
  function->set_extension(extension.get());
  result.reset(utils::ToDictionary(
      utils::RunFunctionAndReturnSingleResult(
          function.get(),
          base::StringPrintf("[%u]", incognito_window_id),
          browser(),
          utils::INCLUDE_INCOGNITO)));
  EXPECT_TRUE(api_test_utils::GetBoolean(result.get(), "incognito"));

  // DevTools window.
  DevToolsWindow* devtools = DevToolsWindowTesting::OpenDevToolsWindowSync(
      browser()->tab_strip_model()->GetWebContentsAt(0), false /* is_docked */);

  function = new WindowsGetFunction();
  function->set_extension(extension.get());
  result.reset(utils::ToDictionary(utils::RunFunctionAndReturnSingleResult(
      function.get(),
      base::StringPrintf("[%u, {\"windowTypes\": [\"devtools\"]}]",
                         ExtensionTabUtil::GetWindowId(
                             DevToolsWindowTesting::Get(devtools)->browser())),
      browser(), utils::INCLUDE_INCOGNITO)));
  EXPECT_EQ("devtools", api_test_utils::GetString(result.get(), "type"));

  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, GetCurrentWindow) {
  int window_id = ExtensionTabUtil::GetWindowId(browser());
  Browser* new_browser = CreateBrowser(browser()->profile());
  int new_id = ExtensionTabUtil::GetWindowId(new_browser);

  // Get the current window using new_browser.
  scoped_refptr<WindowsGetCurrentFunction> function =
      new WindowsGetCurrentFunction();
  scoped_refptr<Extension> extension(test_util::CreateEmptyExtension());
  function->set_extension(extension.get());
  std::unique_ptr<base::DictionaryValue> result(
      utils::ToDictionary(utils::RunFunctionAndReturnSingleResult(
          function.get(), "[]", new_browser)));

  // The id should match the window id of the browser instance that was passed
  // to RunFunctionAndReturnSingleResult.
  EXPECT_EQ(new_id, GetWindowId(result.get()));
  base::ListValue* tabs = nullptr;
  EXPECT_FALSE(result.get()->GetList(keys::kTabsKey, &tabs));

  // Get the current window using the old window and make the tabs populated.
  function = new WindowsGetCurrentFunction();
  function->set_extension(extension.get());
  result.reset(utils::ToDictionary(
      utils::RunFunctionAndReturnSingleResult(function.get(),
                                              "[{\"populate\": true}]",
                                              browser())));

  // The id should match the window id of the browser instance that was passed
  // to RunFunctionAndReturnSingleResult.
  EXPECT_EQ(window_id, GetWindowId(result.get()));
  // "populate" was enabled so tabs should be populated.
  EXPECT_TRUE(result.get()->GetList(keys::kTabsKey, &tabs));

  // The tab id should not be -1 as this is a browser window.
  base::Value* tab0 = nullptr;
  EXPECT_TRUE(tabs->Get(0, &tab0));
  EXPECT_GE(GetTabId(utils::ToDictionary(tab0)), 0);
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, GetAllWindows) {
  const size_t NUM_WINDOWS = 5;
  std::set<int> window_ids;
  std::set<int> result_ids;
  window_ids.insert(ExtensionTabUtil::GetWindowId(browser()));

  for (size_t i = 0; i < NUM_WINDOWS - 1; ++i) {
    Browser* new_browser = CreateBrowser(browser()->profile());
    window_ids.insert(ExtensionTabUtil::GetWindowId(new_browser));
  }

  // Application windows should not be accessible, unless allWindowTypes is set
  // to true.
  AppWindow* app_window = CreateTestAppWindow("{}");

  // Undocked DevTools window should not be accessible, unless allWindowTypes is
  // set to true.
  DevToolsWindow* devtools = DevToolsWindowTesting::OpenDevToolsWindowSync(
      browser()->tab_strip_model()->GetWebContentsAt(0), false /* is_docked */);

  scoped_refptr<WindowsGetAllFunction> function = new WindowsGetAllFunction();
  scoped_refptr<Extension> extension(test_util::CreateEmptyExtension());
  function->set_extension(extension.get());
  std::unique_ptr<base::ListValue> result(
      utils::ToList(utils::RunFunctionAndReturnSingleResult(function.get(),
                                                            "[]", browser())));

  base::ListValue* windows = result.get();
  EXPECT_EQ(window_ids.size(), windows->GetSize());
  for (size_t i = 0; i < windows->GetSize(); ++i) {
    base::DictionaryValue* result_window = nullptr;
    EXPECT_TRUE(windows->GetDictionary(i, &result_window));
    result_ids.insert(GetWindowId(result_window));

    // "populate" was not passed in so tabs are not populated.
    base::ListValue* tabs = nullptr;
    EXPECT_FALSE(result_window->GetList(keys::kTabsKey, &tabs));
  }
  // The returned ids should contain all the current browser instance ids.
  EXPECT_EQ(window_ids, result_ids);

  result_ids.clear();
  function = new WindowsGetAllFunction();
  function->set_extension(extension.get());
  result.reset(utils::ToList(
      utils::RunFunctionAndReturnSingleResult(function.get(),
                                              "[{\"populate\": true}]",
                                              browser())));

  windows = result.get();
  EXPECT_EQ(window_ids.size(), windows->GetSize());
  for (size_t i = 0; i < windows->GetSize(); ++i) {
    base::DictionaryValue* result_window = nullptr;
    EXPECT_TRUE(windows->GetDictionary(i, &result_window));
    result_ids.insert(GetWindowId(result_window));

    // "populate" was enabled so tabs should be populated.
    base::ListValue* tabs = nullptr;
    EXPECT_TRUE(result_window->GetList(keys::kTabsKey, &tabs));
  }
  // The returned ids should contain all the current app, browser and
  // devtools instance ids.
  EXPECT_EQ(window_ids, result_ids);

  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);

  CloseAppWindow(app_window);
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, GetAllWindowsAllTypes) {
  const size_t NUM_WINDOWS = 5;
  std::set<int> window_ids;
  std::set<int> result_ids;
  window_ids.insert(ExtensionTabUtil::GetWindowId(browser()));

  for (size_t i = 0; i < NUM_WINDOWS - 1; ++i) {
    Browser* new_browser = CreateBrowser(browser()->profile());
    window_ids.insert(ExtensionTabUtil::GetWindowId(new_browser));
  }

  // Application windows should be accessible.
  AppWindow* app_window = CreateTestAppWindow("{}");
  window_ids.insert(app_window->session_id().id());

  // Undocked DevTools window should be accessible too.
  DevToolsWindow* devtools = DevToolsWindowTesting::OpenDevToolsWindowSync(
      browser()->tab_strip_model()->GetWebContentsAt(0), false /* is_docked */);
  window_ids.insert(ExtensionTabUtil::GetWindowId(
      DevToolsWindowTesting::Get(devtools)->browser()));

  scoped_refptr<WindowsGetAllFunction> function = new WindowsGetAllFunction();
  scoped_refptr<Extension> extension(test_util::CreateEmptyExtension());
  function->set_extension(extension.get());
  std::unique_ptr<base::ListValue> result(
      utils::ToList(utils::RunFunctionAndReturnSingleResult(
          function.get(),
          "[{\"windowTypes\": [\"app\", \"devtools\", \"normal\", \"panel\", "
          "\"popup\"]}]",
          browser())));

  base::ListValue* windows = result.get();
  EXPECT_EQ(window_ids.size(), windows->GetSize());
  for (size_t i = 0; i < windows->GetSize(); ++i) {
    base::DictionaryValue* result_window = nullptr;
    EXPECT_TRUE(windows->GetDictionary(i, &result_window));
    result_ids.insert(GetWindowId(result_window));

    // "populate" was not passed in so tabs are not populated.
    base::ListValue* tabs = nullptr;
    EXPECT_FALSE(result_window->GetList(keys::kTabsKey, &tabs));
  }
  // The returned ids should contain all the current app, browser and
  // devtools instance ids.
  EXPECT_EQ(window_ids, result_ids);

  result_ids.clear();
  function = new WindowsGetAllFunction();
  function->set_extension(extension.get());
  result.reset(utils::ToList(utils::RunFunctionAndReturnSingleResult(
      function.get(),
      "[{\"populate\": true, \"windowTypes\": [\"app\", \"devtools\", "
      "\"normal\", \"panel\", \"popup\"]}]",
      browser())));

  windows = result.get();
  EXPECT_EQ(window_ids.size(), windows->GetSize());
  for (size_t i = 0; i < windows->GetSize(); ++i) {
    base::DictionaryValue* result_window = nullptr;
    EXPECT_TRUE(windows->GetDictionary(i, &result_window));
    result_ids.insert(GetWindowId(result_window));

    // "populate" was enabled so tabs should be populated.
    base::ListValue* tabs = nullptr;
    EXPECT_TRUE(result_window->GetList(keys::kTabsKey, &tabs));
  }
  // The returned ids should contain all the current app, browser and
  // devtools instance ids.
  EXPECT_EQ(window_ids, result_ids);

  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);

  CloseAppWindow(app_window);
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, UpdateNoPermissions) {
  // The test empty extension has no permissions, therefore it should not get
  // tab data in the function result.
  scoped_refptr<TabsUpdateFunction> update_tab_function(
      new TabsUpdateFunction());
  scoped_refptr<Extension> empty_extension(test_util::CreateEmptyExtension());
  update_tab_function->set_extension(empty_extension.get());
  // Without a callback the function will not generate a result.
  update_tab_function->set_has_callback(true);

  std::unique_ptr<base::DictionaryValue> result(
      utils::ToDictionary(utils::RunFunctionAndReturnSingleResult(
          update_tab_function.get(),
          "[null, {\"url\": \"about:blank\", \"pinned\": true}]", browser())));
  // The url is stripped since the extension does not have tab permissions.
  EXPECT_FALSE(result->HasKey("url"));
  EXPECT_TRUE(api_test_utils::GetBoolean(result.get(), "pinned"));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest,
                       DefaultToIncognitoWhenItIsForced) {
  static const char kArgsWithoutExplicitIncognitoParam[] =
      "[{\"url\": \"about:blank\"}]";
  // Force Incognito mode.
  IncognitoModePrefs::SetAvailability(browser()->profile()->GetPrefs(),
                                      IncognitoModePrefs::FORCED);
  // Run without an explicit "incognito" param.
  scoped_refptr<WindowsCreateFunction> function(new WindowsCreateFunction());
  function->SetRenderFrameHost(
      browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame());
  scoped_refptr<Extension> extension(test_util::CreateEmptyExtension());
  function->set_extension(extension.get());
  std::unique_ptr<base::DictionaryValue> result(
      utils::ToDictionary(utils::RunFunctionAndReturnSingleResult(
          function.get(), kArgsWithoutExplicitIncognitoParam, browser(),
          utils::INCLUDE_INCOGNITO)));

  // Make sure it is a new(different) window.
  EXPECT_NE(ExtensionTabUtil::GetWindowId(browser()),
            GetWindowId(result.get()));
  // ... and it is incognito.
  EXPECT_TRUE(api_test_utils::GetBoolean(result.get(), "incognito"));

  // Now try creating a window from incognito window.
  Browser* incognito_browser = CreateIncognitoBrowser();
  // Run without an explicit "incognito" param.
  function = new WindowsCreateFunction();
  function->SetRenderFrameHost(
      browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame());
  function->set_extension(extension.get());
  result.reset(utils::ToDictionary(
      utils::RunFunctionAndReturnSingleResult(
          function.get(),
          kArgsWithoutExplicitIncognitoParam,
          incognito_browser,
          utils::INCLUDE_INCOGNITO)));
  // Make sure it is a new(different) window.
  EXPECT_NE(ExtensionTabUtil::GetWindowId(incognito_browser),
            GetWindowId(result.get()));
  // ... and it is incognito.
  EXPECT_TRUE(api_test_utils::GetBoolean(result.get(), "incognito"));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest,
                       DefaultToIncognitoWhenItIsForcedAndNoArgs) {
  static const char kEmptyArgs[] = "[]";
  // Force Incognito mode.
  IncognitoModePrefs::SetAvailability(browser()->profile()->GetPrefs(),
                                      IncognitoModePrefs::FORCED);
  // Run without an explicit "incognito" param.
  scoped_refptr<WindowsCreateFunction> function = new WindowsCreateFunction();
  scoped_refptr<Extension> extension(test_util::CreateEmptyExtension());
  function->set_extension(extension.get());
  std::unique_ptr<base::DictionaryValue> result(
      utils::ToDictionary(utils::RunFunctionAndReturnSingleResult(
          function.get(), kEmptyArgs, browser(), utils::INCLUDE_INCOGNITO)));

  // Make sure it is a new(different) window.
  EXPECT_NE(ExtensionTabUtil::GetWindowId(browser()),
            GetWindowId(result.get()));
  // ... and it is incognito.
  EXPECT_TRUE(api_test_utils::GetBoolean(result.get(), "incognito"));

  // Now try creating a window from incognito window.
  Browser* incognito_browser = CreateIncognitoBrowser();
  // Run without an explicit "incognito" param.
  function = new WindowsCreateFunction();
  function->set_extension(extension.get());
  result.reset(utils::ToDictionary(
      utils::RunFunctionAndReturnSingleResult(function.get(),
                                              kEmptyArgs,
                                              incognito_browser,
                                              utils::INCLUDE_INCOGNITO)));
  // Make sure it is a new(different) window.
  EXPECT_NE(ExtensionTabUtil::GetWindowId(incognito_browser),
            GetWindowId(result.get()));
  // ... and it is incognito.
  EXPECT_TRUE(api_test_utils::GetBoolean(result.get(), "incognito"));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest,
                       DontCreateNormalWindowWhenIncognitoForced) {
  static const char kArgsWithExplicitIncognitoParam[] =
      "[{\"url\": \"about:blank\", \"incognito\": false }]";
  // Force Incognito mode.
  IncognitoModePrefs::SetAvailability(browser()->profile()->GetPrefs(),
                                      IncognitoModePrefs::FORCED);

  // Run with an explicit "incognito" param.
  scoped_refptr<WindowsCreateFunction> function = new WindowsCreateFunction();
  scoped_refptr<Extension> extension(test_util::CreateEmptyExtension());
  function->set_extension(extension.get());
  EXPECT_TRUE(base::MatchPattern(
      utils::RunFunctionAndReturnError(
          function.get(), kArgsWithExplicitIncognitoParam, browser()),
      keys::kIncognitoModeIsForced));

  // Now try opening a normal window from incognito window.
  Browser* incognito_browser = CreateIncognitoBrowser();
  // Run with an explicit "incognito" param.
  function = new WindowsCreateFunction();
  function->set_extension(extension.get());
  EXPECT_TRUE(base::MatchPattern(
      utils::RunFunctionAndReturnError(
          function.get(), kArgsWithExplicitIncognitoParam, incognito_browser),
      keys::kIncognitoModeIsForced));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest,
                       DontCreateIncognitoWindowWhenIncognitoDisabled) {
  static const char kArgs[] =
      "[{\"url\": \"about:blank\", \"incognito\": true }]";

  Browser* incognito_browser = CreateIncognitoBrowser();
  // Disable Incognito mode.
  IncognitoModePrefs::SetAvailability(browser()->profile()->GetPrefs(),
                                      IncognitoModePrefs::DISABLED);
  // Run in normal window.
  scoped_refptr<WindowsCreateFunction> function = new WindowsCreateFunction();
  scoped_refptr<Extension> extension(test_util::CreateEmptyExtension());
  function->set_extension(extension.get());
  EXPECT_TRUE(base::MatchPattern(
      utils::RunFunctionAndReturnError(function.get(), kArgs, browser()),
      keys::kIncognitoModeIsDisabled));

  // Run in incognito window.
  function = new WindowsCreateFunction();
  function->set_extension(extension.get());
  EXPECT_TRUE(base::MatchPattern(utils::RunFunctionAndReturnError(
                                     function.get(), kArgs, incognito_browser),
                                 keys::kIncognitoModeIsDisabled));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, QueryCurrentWindowTabs) {
  const size_t kExtraWindows = 3;
  for (size_t i = 0; i < kExtraWindows; ++i)
    CreateBrowser(browser()->profile());

  GURL url(url::kAboutBlankURL);
  AddTabAtIndex(0, url, ui::PAGE_TRANSITION_LINK);
  int window_id = ExtensionTabUtil::GetWindowId(browser());

  // Get tabs in the 'current' window called from non-focused browser.
  scoped_refptr<TabsQueryFunction> function = new TabsQueryFunction();
  function->set_extension(test_util::CreateEmptyExtension().get());
  std::unique_ptr<base::ListValue> result(
      utils::ToList(utils::RunFunctionAndReturnSingleResult(
          function.get(), "[{\"currentWindow\":true}]", browser())));

  base::ListValue* result_tabs = result.get();
  // We should have one initial tab and one added tab.
  EXPECT_EQ(2u, result_tabs->GetSize());
  for (size_t i = 0; i < result_tabs->GetSize(); ++i) {
    base::DictionaryValue* result_tab = nullptr;
    EXPECT_TRUE(result_tabs->GetDictionary(i, &result_tab));
    EXPECT_EQ(window_id, GetTabWindowId(result_tab));
  }

  // Get tabs NOT in the 'current' window called from non-focused browser.
  function = new TabsQueryFunction();
  function->set_extension(test_util::CreateEmptyExtension().get());
  result.reset(utils::ToList(
      utils::RunFunctionAndReturnSingleResult(function.get(),
                                              "[{\"currentWindow\":false}]",
                                              browser())));

  result_tabs = result.get();
  // We should have one tab for each extra window.
  EXPECT_EQ(kExtraWindows, result_tabs->GetSize());
  for (size_t i = 0; i < kExtraWindows; ++i) {
    base::DictionaryValue* result_tab = nullptr;
    EXPECT_TRUE(result_tabs->GetDictionary(i, &result_tab));
    EXPECT_NE(window_id, GetTabWindowId(result_tab));
  }
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, QueryAllTabsWithDevTools) {
  const size_t kNumWindows = 3;
  std::set<int> window_ids;
  window_ids.insert(ExtensionTabUtil::GetWindowId(browser()));
  for (size_t i = 0; i < kNumWindows - 1; ++i) {
    Browser* new_browser = CreateBrowser(browser()->profile());
    window_ids.insert(ExtensionTabUtil::GetWindowId(new_browser));
  }

  // Undocked DevTools window should not be accessible.
  DevToolsWindow* devtools = DevToolsWindowTesting::OpenDevToolsWindowSync(
      browser()->tab_strip_model()->GetWebContentsAt(0), false /* is_docked */);

  // Get tabs in the 'current' window called from non-focused browser.
  scoped_refptr<TabsQueryFunction> function = new TabsQueryFunction();
  function->set_extension(test_util::CreateEmptyExtension().get());
  std::unique_ptr<base::ListValue> result(
      utils::ToList(utils::RunFunctionAndReturnSingleResult(
          function.get(), "[{}]", browser())));

  std::set<int> result_ids;
  base::ListValue* result_tabs = result.get();
  // We should have one tab per browser except for DevTools.
  EXPECT_EQ(kNumWindows, result_tabs->GetSize());
  for (size_t i = 0; i < result_tabs->GetSize(); ++i) {
    base::DictionaryValue* result_tab = nullptr;
    EXPECT_TRUE(result_tabs->GetDictionary(i, &result_tab));
    result_ids.insert(GetTabWindowId(result_tab));
  }
  EXPECT_EQ(window_ids, result_ids);

  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, DontCreateTabInClosingPopupWindow) {
  // Test creates new popup window, closes it right away and then tries to open
  // a new tab in it. Tab should not be opened in the popup window, but in a
  // tabbed browser window.
  Browser* popup_browser = new Browser(
      Browser::CreateParams(Browser::TYPE_POPUP, browser()->profile()));
  int window_id = ExtensionTabUtil::GetWindowId(popup_browser);
  chrome::CloseWindow(popup_browser);

  scoped_refptr<TabsCreateFunction> create_tab_function(
      new TabsCreateFunction());
  create_tab_function->set_extension(test_util::CreateEmptyExtension().get());
  // Without a callback the function will not generate a result.
  create_tab_function->set_has_callback(true);

  static const char kNewBlankTabArgs[] =
      "[{\"url\": \"about:blank\", \"windowId\": %u}]";

  std::unique_ptr<base::DictionaryValue> result(
      utils::ToDictionary(utils::RunFunctionAndReturnSingleResult(
          create_tab_function.get(),
          base::StringPrintf(kNewBlankTabArgs, window_id), browser())));

  EXPECT_NE(window_id, GetTabWindowId(result.get()));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, InvalidUpdateWindowState) {
  int window_id = ExtensionTabUtil::GetWindowId(browser());

  static const char kArgsMinimizedWithFocus[] =
      "[%u, {\"state\": \"minimized\", \"focused\": true}]";
  scoped_refptr<WindowsUpdateFunction> function = new WindowsUpdateFunction();
  scoped_refptr<Extension> extension(test_util::CreateEmptyExtension());
  function->set_extension(extension.get());
  EXPECT_TRUE(base::MatchPattern(
      utils::RunFunctionAndReturnError(
          function.get(),
          base::StringPrintf(kArgsMinimizedWithFocus, window_id), browser()),
      keys::kInvalidWindowStateError));

  static const char kArgsMaximizedWithoutFocus[] =
      "[%u, {\"state\": \"maximized\", \"focused\": false}]";
  function = new WindowsUpdateFunction();
  function->set_extension(extension.get());
  EXPECT_TRUE(base::MatchPattern(
      utils::RunFunctionAndReturnError(
          function.get(),
          base::StringPrintf(kArgsMaximizedWithoutFocus, window_id), browser()),
      keys::kInvalidWindowStateError));

  static const char kArgsMinimizedWithBounds[] =
      "[%u, {\"state\": \"minimized\", \"width\": 500}]";
  function = new WindowsUpdateFunction();
  function->set_extension(extension.get());
  EXPECT_TRUE(base::MatchPattern(
      utils::RunFunctionAndReturnError(
          function.get(),
          base::StringPrintf(kArgsMinimizedWithBounds, window_id), browser()),
      keys::kInvalidWindowStateError));

  static const char kArgsMaximizedWithBounds[] =
      "[%u, {\"state\": \"maximized\", \"width\": 500}]";
  function = new WindowsUpdateFunction();
  function->set_extension(extension.get());
  EXPECT_TRUE(base::MatchPattern(
      utils::RunFunctionAndReturnError(
          function.get(),
          base::StringPrintf(kArgsMaximizedWithBounds, window_id), browser()),
      keys::kInvalidWindowStateError));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, UpdateAppWindowSizeConstraint) {
  AppWindow* app_window = CreateTestAppWindow(
      "{\"outerBounds\": "
      "{\"width\": 300, \"height\": 300,"
      " \"minWidth\": 200, \"minHeight\": 200,"
      " \"maxWidth\": 400, \"maxHeight\": 400}}");

  scoped_refptr<WindowsGetFunction> get_function = new WindowsGetFunction();
  scoped_refptr<Extension> extension(test_util::CreateEmptyExtension().get());
  get_function->set_extension(extension.get());
  std::unique_ptr<base::DictionaryValue> result(
      utils::ToDictionary(utils::RunFunctionAndReturnSingleResult(
          get_function.get(),
          base::StringPrintf("[%u, {\"windowTypes\": [\"app\"]}]",
                             app_window->session_id().id()),
          browser())));

  EXPECT_EQ(300, api_test_utils::GetInteger(result.get(), "width"));
  EXPECT_EQ(300, api_test_utils::GetInteger(result.get(), "height"));

  // Verify the min width/height of the application window are
  // respected.
  scoped_refptr<WindowsUpdateFunction> update_min_function =
      new WindowsUpdateFunction();
  result.reset(utils::ToDictionary(utils::RunFunctionAndReturnSingleResult(
      update_min_function.get(),
      base::StringPrintf("[%u, {\"width\": 100, \"height\": 100}]",
                         app_window->session_id().id()),
      browser())));

  EXPECT_EQ(200, api_test_utils::GetInteger(result.get(), "width"));
  EXPECT_EQ(200, api_test_utils::GetInteger(result.get(), "height"));

  // Verify the max width/height of the application window are
  // respected.
  scoped_refptr<WindowsUpdateFunction> update_max_function =
      new WindowsUpdateFunction();
  result.reset(utils::ToDictionary(utils::RunFunctionAndReturnSingleResult(
      update_max_function.get(),
      base::StringPrintf("[%u, {\"width\": 500, \"height\": 500}]",
                         app_window->session_id().id()),
      browser())));

  EXPECT_EQ(400, api_test_utils::GetInteger(result.get(), "width"));
  EXPECT_EQ(400, api_test_utils::GetInteger(result.get(), "height"));

  CloseAppWindow(app_window);
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, UpdateDevToolsWindow) {
  DevToolsWindow* devtools = DevToolsWindowTesting::OpenDevToolsWindowSync(
      browser()->tab_strip_model()->GetWebContentsAt(0), false /* is_docked */);

  scoped_refptr<WindowsGetFunction> get_function = new WindowsGetFunction();
  scoped_refptr<Extension> extension(test_util::CreateEmptyExtension().get());
  get_function->set_extension(extension.get());
  std::unique_ptr<base::DictionaryValue> result(
      utils::ToDictionary(utils::RunFunctionAndReturnSingleResult(
          get_function.get(),
          base::StringPrintf(
              "[%u, {\"windowTypes\": [\"devtools\"]}]",
              ExtensionTabUtil::GetWindowId(
                  DevToolsWindowTesting::Get(devtools)->browser())),
          browser())));

  // Verify the updating width/height works.
  int32_t new_width = api_test_utils::GetInteger(result.get(), "width") - 50;
  int32_t new_height = api_test_utils::GetInteger(result.get(), "height") - 50;

  scoped_refptr<WindowsUpdateFunction> update_function =
      new WindowsUpdateFunction();
  result.reset(utils::ToDictionary(utils::RunFunctionAndReturnSingleResult(
      update_function.get(),
      base::StringPrintf("[%u, {\"width\": %d, \"height\": %d}]",
                         ExtensionTabUtil::GetWindowId(
                             DevToolsWindowTesting::Get(devtools)->browser()),
                         new_width, new_height),
      browser())));

  EXPECT_EQ(new_width, api_test_utils::GetInteger(result.get(), "width"));
  EXPECT_EQ(new_height, api_test_utils::GetInteger(result.get(), "height"));

  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);
}

// TODO(llandwerlin): Activating a browser window and waiting for the
// action to happen requires views::Widget which is not available on
// MacOSX. Deactivate for now.
#if !defined(OS_MACOSX)
class ExtensionWindowLastFocusedTest : public ExtensionTabsTest {
 public:
  void SetUpOnMainThread() override;

  void ActivateAppWindow(AppWindow* app_window);

  void ActivateBrowserWindow(Browser* browser);

  Browser* CreateBrowserWithEmptyTab(bool as_popup);

  int GetTabId(const base::DictionaryValue* value) const;

  base::Value* RunFunction(UIThreadExtensionFunction* function,
                           const std::string& params);

 private:
  // A helper class to wait for an AppWindow to become activated. On
  // window system like X11, for a NativeWidget to be activated, we
  // need to wait for the round trip communication with the X server.
  class AppWindowActivatedWaiter : public AppWindowRegistry::Observer {
   public:
    AppWindowActivatedWaiter(AppWindow* app_window,
                             content::BrowserContext* browser_context)
        : app_window_(app_window),
          browser_context_(browser_context),
          waiting_(false) {
      AppWindowRegistry::Get(browser_context_)->AddObserver(this);
    }
    ~AppWindowActivatedWaiter() override {
      AppWindowRegistry::Get(browser_context_)->RemoveObserver(this);
    }

    void ActivateAndWait() {
      app_window_->GetBaseWindow()->Activate();
      if (!app_window_->GetBaseWindow()->IsActive()) {
        waiting_ = true;
        content::RunMessageLoop();
      }
    }

    // AppWindowRegistry::Observer:
    void OnAppWindowActivated(AppWindow* app_window) override {
      if (app_window_ == app_window && waiting_) {
        base::MessageLoopForUI::current()->QuitWhenIdle();
        waiting_ = false;
      }
    }

   private:
    AppWindow* app_window_;
    content::BrowserContext* browser_context_;
    bool waiting_;
  };

  // A helper class to wait for an views::Widget to become activated.
  class WidgetActivatedWaiter : public views::WidgetObserver {
   public:
    explicit WidgetActivatedWaiter(views::Widget* widget)
        : widget_(widget), waiting_(false) {
      widget_->AddObserver(this);
    }
    ~WidgetActivatedWaiter() override { widget_->RemoveObserver(this); }

    void ActivateAndWait() {
      widget_->Activate();
      if (!widget_->IsActive()) {
        waiting_ = true;
        content::RunMessageLoop();
      }
    }

    // views::WidgetObserver:
    void OnWidgetActivationChanged(views::Widget* widget,
                                   bool active) override {
      if (widget_ == widget && waiting_) {
        base::MessageLoopForUI::current()->QuitWhenIdle();
        waiting_ = false;
      }
    }

   private:
    views::Widget* widget_;
    bool waiting_;
  };

  scoped_refptr<Extension> extension_;
};

void ExtensionWindowLastFocusedTest::SetUpOnMainThread() {
  ExtensionTabsTest::SetUpOnMainThread();
  extension_ = test_util::CreateEmptyExtension();
}

void ExtensionWindowLastFocusedTest::ActivateAppWindow(AppWindow* app_window) {
  AppWindowActivatedWaiter waiter(app_window, browser()->profile());
  waiter.ActivateAndWait();
}

void ExtensionWindowLastFocusedTest::ActivateBrowserWindow(Browser* browser) {
  BrowserView* view = BrowserView::GetBrowserViewForBrowser(browser);
  EXPECT_NE(nullptr, view);
  views::Widget* widget = view->frame();
  EXPECT_NE(nullptr, widget);
  WidgetActivatedWaiter waiter(widget);
  waiter.ActivateAndWait();
}

Browser* ExtensionWindowLastFocusedTest::CreateBrowserWithEmptyTab(
    bool as_popup) {
  Browser* new_browser;
  if (as_popup)
    new_browser = new Browser(
        Browser::CreateParams(Browser::TYPE_POPUP, browser()->profile()));
  else
    new_browser = new Browser(Browser::CreateParams(browser()->profile()));
  AddBlankTabAndShow(new_browser);
  return new_browser;
}

int ExtensionWindowLastFocusedTest::GetTabId(
    const base::DictionaryValue* value) const {
  const base::ListValue* tabs = NULL;
  if (!value->GetList(keys::kTabsKey, &tabs))
    return -2;
  const base::Value* tab = NULL;
  if (!tabs->Get(0, &tab))
    return -2;
  const base::DictionaryValue* tab_dict = NULL;
  if (!tab->GetAsDictionary(&tab_dict))
    return -2;
  int tab_id = 0;
  if (!tab_dict->GetInteger(keys::kIdKey, &tab_id))
    return -2;
  return tab_id;
}

base::Value* ExtensionWindowLastFocusedTest::RunFunction(
    UIThreadExtensionFunction* function,
    const std::string& params) {
  function->set_extension(extension_.get());
  return utils::RunFunctionAndReturnSingleResult(function, params, browser());
}

IN_PROC_BROWSER_TEST_F(ExtensionWindowLastFocusedTest,
                       NoDevtoolsAndAppWindows) {
  DevToolsWindow* devtools = DevToolsWindowTesting::OpenDevToolsWindowSync(
      browser()->tab_strip_model()->GetWebContentsAt(0), false /* is_docked */);
  {
    int devtools_window_id = ExtensionTabUtil::GetWindowId(
        DevToolsWindowTesting::Get(devtools)->browser());
    ActivateBrowserWindow(DevToolsWindowTesting::Get(devtools)->browser());

    scoped_refptr<WindowsGetLastFocusedFunction> function =
        new WindowsGetLastFocusedFunction();
    std::unique_ptr<base::DictionaryValue> result(utils::ToDictionary(
        RunFunction(function.get(), "[{\"populate\": true}]")));
    EXPECT_NE(devtools_window_id,
              api_test_utils::GetInteger(result.get(), "id"));
  }

  AppWindow* app_window = CreateTestAppWindow(
      "{\"outerBounds\": "
      "{\"width\": 300, \"height\": 300,"
      " \"minWidth\": 200, \"minHeight\": 200,"
      " \"maxWidth\": 400, \"maxHeight\": 400}}");
  {
    ActivateAppWindow(app_window);

    scoped_refptr<WindowsGetLastFocusedFunction> get_current_app_function =
        new WindowsGetLastFocusedFunction();
    std::unique_ptr<base::DictionaryValue> result(utils::ToDictionary(
        RunFunction(get_current_app_function.get(), "[{\"populate\": true}]")));
    int app_window_id = app_window->session_id().id();
    EXPECT_NE(app_window_id, api_test_utils::GetInteger(result.get(), "id"));
  }

  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);
  CloseAppWindow(app_window);
}

IN_PROC_BROWSER_TEST_F(ExtensionWindowLastFocusedTest,
                       NoTabIdForDevToolsAndAppWindows) {
  Browser* normal_browser = CreateBrowserWithEmptyTab(false);
  {
    ActivateBrowserWindow(normal_browser);

    scoped_refptr<WindowsGetLastFocusedFunction> function =
        new WindowsGetLastFocusedFunction();
    std::unique_ptr<base::DictionaryValue> result(utils::ToDictionary(
        RunFunction(function.get(), "[{\"populate\": true}]")));
    int normal_browser_window_id =
        ExtensionTabUtil::GetWindowId(normal_browser);
    EXPECT_EQ(normal_browser_window_id,
              api_test_utils::GetInteger(result.get(), "id"));
    EXPECT_NE(-1, GetTabId(result.get()));
    EXPECT_EQ("normal", api_test_utils::GetString(result.get(), "type"));
  }

  Browser* popup_browser = CreateBrowserWithEmptyTab(true);
  {
    ActivateBrowserWindow(popup_browser);

    scoped_refptr<WindowsGetLastFocusedFunction> function =
        new WindowsGetLastFocusedFunction();
    std::unique_ptr<base::DictionaryValue> result(utils::ToDictionary(
        RunFunction(function.get(), "[{\"populate\": true}]")));
    int popup_browser_window_id = ExtensionTabUtil::GetWindowId(popup_browser);
    EXPECT_EQ(popup_browser_window_id,
              api_test_utils::GetInteger(result.get(), "id"));
    EXPECT_NE(-1, GetTabId(result.get()));
    EXPECT_EQ("popup", api_test_utils::GetString(result.get(), "type"));
  }

  DevToolsWindow* devtools = DevToolsWindowTesting::OpenDevToolsWindowSync(
      browser()->tab_strip_model()->GetWebContentsAt(0), false /* is_docked */);
  {
    ActivateBrowserWindow(DevToolsWindowTesting::Get(devtools)->browser());

    scoped_refptr<WindowsGetLastFocusedFunction> function =
        new WindowsGetLastFocusedFunction();
    std::unique_ptr<base::DictionaryValue> result(
        utils::ToDictionary(RunFunction(
            function.get(),
            "[{\"populate\": true, \"windowTypes\": [ \"devtools\" ]}]")));
    int devtools_window_id = ExtensionTabUtil::GetWindowId(
        DevToolsWindowTesting::Get(devtools)->browser());
    EXPECT_EQ(devtools_window_id,
              api_test_utils::GetInteger(result.get(), "id"));
    EXPECT_EQ(-1, GetTabId(result.get()));
    EXPECT_EQ("devtools", api_test_utils::GetString(result.get(), "type"));
  }

  AppWindow* app_window = CreateTestAppWindow(
      "{\"outerBounds\": "
      "{\"width\": 300, \"height\": 300,"
      " \"minWidth\": 200, \"minHeight\": 200,"
      " \"maxWidth\": 400, \"maxHeight\": 400}}");
  {
    ActivateAppWindow(app_window);

    scoped_refptr<WindowsGetLastFocusedFunction> get_current_app_function =
        new WindowsGetLastFocusedFunction();
    std::unique_ptr<base::DictionaryValue> result(utils::ToDictionary(
        RunFunction(get_current_app_function.get(),
                    "[{\"populate\": true, \"windowTypes\": [ \"app\" ]}]")));
    int app_window_id = app_window->session_id().id();
    EXPECT_EQ(app_window_id, api_test_utils::GetInteger(result.get(), "id"));
    EXPECT_EQ(-1, GetTabId(result.get()));
    EXPECT_EQ("app", api_test_utils::GetString(result.get(), "type"));
  }

  chrome::CloseWindow(normal_browser);
  chrome::CloseWindow(popup_browser);
  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);
  CloseAppWindow(app_window);
}
#endif  // !defined(OS_MACOSX)

IN_PROC_BROWSER_TEST_F(ExtensionWindowCreateTest, AcceptState) {
#if defined(OS_MACOSX)
  ui::test::ScopedFakeNSWindowFullscreen fake_fullscreen;
#endif

  scoped_refptr<WindowsCreateFunction> function(new WindowsCreateFunction());
  scoped_refptr<Extension> extension(test_util::CreateEmptyExtension());
  function->set_extension(extension.get());

  std::unique_ptr<base::DictionaryValue> result(
      utils::ToDictionary(utils::RunFunctionAndReturnSingleResult(
          function.get(), "[{\"state\": \"minimized\"}]", browser(),
          utils::INCLUDE_INCOGNITO)));
  int window_id = GetWindowId(result.get());
  std::string error;
  Browser* new_window = ExtensionTabUtil::GetBrowserFromWindowID(
      function.get(), window_id, &error);
  EXPECT_TRUE(error.empty());
#if !defined(OS_LINUX) || defined(OS_CHROMEOS)
  // DesktopWindowTreeHostX11::IsMinimized() relies on an asynchronous update
  // from the window server.
  EXPECT_TRUE(new_window->window()->IsMinimized());
#endif

  function = new WindowsCreateFunction();
  function->set_extension(extension.get());
  result.reset(utils::ToDictionary(utils::RunFunctionAndReturnSingleResult(
      function.get(), "[{\"state\": \"fullscreen\"}]", browser(),
      utils::INCLUDE_INCOGNITO)));
  window_id = GetWindowId(result.get());
  new_window = ExtensionTabUtil::GetBrowserFromWindowID(function.get(),
                                                        window_id, &error);
  EXPECT_TRUE(error.empty());
  EXPECT_TRUE(new_window->window()->IsFullscreen());

  // Let the message loop run so that |fake_fullscreen| finishes transition.
  content::RunAllPendingInMessageLoop();
}

IN_PROC_BROWSER_TEST_F(ExtensionWindowCreateTest, ValidateCreateWindowState) {
  EXPECT_TRUE(base::MatchPattern(
      RunCreateWindowExpectError(
          "[{\"state\": \"fullscreen\", \"type\": \"panel\"}]"),
      keys::kInvalidWindowStateError));
  EXPECT_TRUE(base::MatchPattern(
      RunCreateWindowExpectError(
          "[{\"state\": \"maximized\", \"type\": \"panel\"}]"),
      keys::kInvalidWindowStateError));
  EXPECT_TRUE(base::MatchPattern(
      RunCreateWindowExpectError(
          "[{\"state\": \"minimized\", \"type\": \"panel\"}]"),
      keys::kInvalidWindowStateError));
  EXPECT_TRUE(
      base::MatchPattern(RunCreateWindowExpectError(
                             "[{\"state\": \"minimized\", \"focused\": true}]"),
                         keys::kInvalidWindowStateError));
  EXPECT_TRUE(base::MatchPattern(
      RunCreateWindowExpectError(
          "[{\"state\": \"maximized\", \"focused\": false}]"),
      keys::kInvalidWindowStateError));
  EXPECT_TRUE(base::MatchPattern(
      RunCreateWindowExpectError(
          "[{\"state\": \"fullscreen\", \"focused\": false}]"),
      keys::kInvalidWindowStateError));
  EXPECT_TRUE(
      base::MatchPattern(RunCreateWindowExpectError(
                             "[{\"state\": \"minimized\", \"width\": 500}]"),
                         keys::kInvalidWindowStateError));
  EXPECT_TRUE(
      base::MatchPattern(RunCreateWindowExpectError(
                             "[{\"state\": \"maximized\", \"width\": 500}]"),
                         keys::kInvalidWindowStateError));
  EXPECT_TRUE(
      base::MatchPattern(RunCreateWindowExpectError(
                             "[{\"state\": \"fullscreen\", \"width\": 500}]"),
                         keys::kInvalidWindowStateError));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, DuplicateTab) {
  content::OpenURLParams params(GURL(url::kAboutBlankURL),
                                content::Referrer(),
                                NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK,
                                false);
  content::WebContents* web_contents = browser()->OpenURL(params);
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);
  int window_id = ExtensionTabUtil::GetWindowIdOfTab(web_contents);
  int tab_index = -1;
  TabStripModel* tab_strip;
  ExtensionTabUtil::GetTabStripModel(web_contents, &tab_strip, &tab_index);

  scoped_refptr<TabsDuplicateFunction> duplicate_tab_function(
      new TabsDuplicateFunction());
  std::unique_ptr<base::DictionaryValue> test_extension_value(
      api_test_utils::ParseDictionary(
          "{\"name\": \"Test\", \"version\": \"1.0\", \"permissions\": "
          "[\"tabs\"]}"));
  scoped_refptr<Extension> empty_tab_extension(
      api_test_utils::CreateExtension(test_extension_value.get()));
  duplicate_tab_function->set_extension(empty_tab_extension.get());
  duplicate_tab_function->set_has_callback(true);

  std::unique_ptr<base::DictionaryValue> duplicate_result(
      utils::ToDictionary(utils::RunFunctionAndReturnSingleResult(
          duplicate_tab_function.get(), base::StringPrintf("[%u]", tab_id),
          browser())));

  int duplicate_tab_id = GetTabId(duplicate_result.get());
  int duplicate_tab_window_id = GetTabWindowId(duplicate_result.get());
  int duplicate_tab_index =
      api_test_utils::GetInteger(duplicate_result.get(), "index");
  EXPECT_EQ(base::Value::TYPE_DICTIONARY, duplicate_result->GetType());
  // Duplicate tab id should be different from the original tab id.
  EXPECT_NE(tab_id, duplicate_tab_id);
  EXPECT_EQ(window_id, duplicate_tab_window_id);
  EXPECT_EQ(tab_index + 1, duplicate_tab_index);
  // The test empty tab extension has tabs permissions, therefore
  // |duplicate_result| should contain url, title, and faviconUrl
  // in the function result.
  EXPECT_TRUE(utils::HasPrivacySensitiveFields(duplicate_result.get()));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, DuplicateTabNoPermission) {
  content::OpenURLParams params(GURL(url::kAboutBlankURL),
                                content::Referrer(),
                                NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK,
                                false);
  content::WebContents* web_contents = browser()->OpenURL(params);
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);
  int window_id = ExtensionTabUtil::GetWindowIdOfTab(web_contents);
  int tab_index = -1;
  TabStripModel* tab_strip;
  ExtensionTabUtil::GetTabStripModel(web_contents, &tab_strip, &tab_index);

  scoped_refptr<TabsDuplicateFunction> duplicate_tab_function(
      new TabsDuplicateFunction());
  scoped_refptr<Extension> empty_extension(test_util::CreateEmptyExtension());
  duplicate_tab_function->set_extension(empty_extension.get());
  duplicate_tab_function->set_has_callback(true);

  std::unique_ptr<base::DictionaryValue> duplicate_result(
      utils::ToDictionary(utils::RunFunctionAndReturnSingleResult(
          duplicate_tab_function.get(), base::StringPrintf("[%u]", tab_id),
          browser())));

  int duplicate_tab_id = GetTabId(duplicate_result.get());
  int duplicate_tab_window_id = GetTabWindowId(duplicate_result.get());
  int duplicate_tab_index =
      api_test_utils::GetInteger(duplicate_result.get(), "index");
  EXPECT_EQ(base::Value::TYPE_DICTIONARY, duplicate_result->GetType());
  // Duplicate tab id should be different from the original tab id.
  EXPECT_NE(tab_id, duplicate_tab_id);
  EXPECT_EQ(window_id, duplicate_tab_window_id);
  EXPECT_EQ(tab_index + 1, duplicate_tab_index);
  // The test empty extension has no permissions, therefore |duplicate_result|
  // should not contain url, title, and faviconUrl in the function result.
  EXPECT_FALSE(utils::HasPrivacySensitiveFields(duplicate_result.get()));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, NoTabsEventOnDevTools) {
  extensions::ResultCatcher catcher;
  ExtensionTestMessageListener listener("ready", true);
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("api_test/tabs/no_events")));
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  DevToolsWindow* devtools = DevToolsWindowTesting::OpenDevToolsWindowSync(
      browser()->tab_strip_model()->GetWebContentsAt(0), false /* is_docked */);

  listener.Reply("stop");

  ASSERT_TRUE(catcher.GetNextResult());

  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, NoTabsAppWindow) {
  extensions::ResultCatcher catcher;
  ExtensionTestMessageListener listener("ready", true);
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("api_test/tabs/no_events")));
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  AppWindow* app_window = CreateTestAppWindow(
      "{\"outerBounds\": "
      "{\"width\": 300, \"height\": 300,"
      " \"minWidth\": 200, \"minHeight\": 200,"
      " \"maxWidth\": 400, \"maxHeight\": 400}}");

  listener.Reply("stop");

  ASSERT_TRUE(catcher.GetNextResult());

  CloseAppWindow(app_window);
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, FilteredEvents) {
  extensions::ResultCatcher catcher;
  ExtensionTestMessageListener listener("ready", true);
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("api_test/windows/events")));
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  AppWindow* app_window = CreateTestAppWindow(
      "{\"outerBounds\": "
      "{\"width\": 300, \"height\": 300,"
      " \"minWidth\": 200, \"minHeight\": 200,"
      " \"maxWidth\": 400, \"maxHeight\": 400}}");

  Browser* browser_window =
      new Browser(Browser::CreateParams(browser()->profile()));
  AddBlankTabAndShow(browser_window);

  DevToolsWindow* devtools_window =
      DevToolsWindowTesting::OpenDevToolsWindowSync(
          browser()->tab_strip_model()->GetWebContentsAt(0),
          false /* is_docked */);

  chrome::CloseWindow(browser_window);
  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools_window);
  CloseAppWindow(app_window);

  // TODO(llandwerlin): It seems creating an app window on MacOSX
  // won't create an activation event whereas it does on all other
  // platform. Disable focus event tests for now.
#if defined(OS_MACOSX)
  listener.Reply("");
#else
  listener.Reply("focus");
#endif

  ASSERT_TRUE(catcher.GetNextResult());
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, ExecuteScriptOnDevTools) {
  std::unique_ptr<base::DictionaryValue> test_extension_value(
      api_test_utils::ParseDictionary(
          "{\"name\": \"Test\", \"version\": \"1.0\", \"permissions\": "
          "[\"tabs\"]}"));
  scoped_refptr<Extension> extension(
      api_test_utils::CreateExtension(test_extension_value.get()));

  DevToolsWindow* devtools = DevToolsWindowTesting::OpenDevToolsWindowSync(
      browser()->tab_strip_model()->GetWebContentsAt(0), false /* is_docked */);

  scoped_refptr<TabsExecuteScriptFunction> function =
      new TabsExecuteScriptFunction();
  function->set_extension(extension.get());

  EXPECT_TRUE(base::MatchPattern(
      utils::RunFunctionAndReturnError(
          function.get(), base::StringPrintf("[%u, {\"code\": \"true\"}]",
                                             api::windows::WINDOW_ID_CURRENT),
          DevToolsWindowTesting::Get(devtools)->browser()),
      manifest_errors::kCannotAccessPageWithUrl));

  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);
}

// Tester class for the tabs.zoom* api functions.
class ExtensionTabsZoomTest : public ExtensionTabsTest {
 public:
  void SetUpOnMainThread() override;

  // Runs chrome.tabs.setZoom().
  bool RunSetZoom(int tab_id, double zoom_factor);

  // Runs chrome.tabs.getZoom().
  testing::AssertionResult RunGetZoom(int tab_id, double* zoom_factor);

  // Runs chrome.tabs.setZoomSettings().
  bool RunSetZoomSettings(int tab_id, const char* mode, const char* scope);

  // Runs chrome.tabs.getZoomSettings().
  testing::AssertionResult RunGetZoomSettings(int tab_id,
                                              std::string* mode,
                                              std::string* scope);

  // Runs chrome.tabs.getZoomSettings() and returns default zoom.
  testing::AssertionResult RunGetDefaultZoom(int tab_id,
                                             double* default_zoom_factor);

  // Runs chrome.tabs.setZoom(), expecting an error.
  std::string RunSetZoomExpectError(int tab_id,
                                    double zoom_factor);

  // Runs chrome.tabs.setZoomSettings(), expecting an error.
  std::string RunSetZoomSettingsExpectError(int tab_id,
                                            const char* mode,
                                            const char* scope);

  content::WebContents* OpenUrlAndWaitForLoad(const GURL& url);

 private:
  scoped_refptr<Extension> extension_;
};

void ExtensionTabsZoomTest::SetUpOnMainThread() {
  ExtensionTabsTest::SetUpOnMainThread();
  extension_ = test_util::CreateEmptyExtension();
}

bool ExtensionTabsZoomTest::RunSetZoom(int tab_id, double zoom_factor) {
  scoped_refptr<TabsSetZoomFunction> set_zoom_function(
      new TabsSetZoomFunction());
  set_zoom_function->set_extension(extension_.get());
  set_zoom_function->set_has_callback(true);

  return utils::RunFunction(
      set_zoom_function.get(),
      base::StringPrintf("[%u, %lf]", tab_id, zoom_factor), browser(),
      extension_function_test_utils::NONE);
}

testing::AssertionResult ExtensionTabsZoomTest::RunGetZoom(
    int tab_id,
    double* zoom_factor) {
  scoped_refptr<TabsGetZoomFunction> get_zoom_function(
      new TabsGetZoomFunction());
  get_zoom_function->set_extension(extension_.get());
  get_zoom_function->set_has_callback(true);

  std::unique_ptr<base::Value> get_zoom_result(
      utils::RunFunctionAndReturnSingleResult(
          get_zoom_function.get(), base::StringPrintf("[%u]", tab_id),
          browser()));

  if (!get_zoom_result)
    return testing::AssertionFailure() << "no result";
  if (!get_zoom_result->GetAsDouble(zoom_factor))
    return testing::AssertionFailure() << "result was not a double";

  return testing::AssertionSuccess();
}

bool ExtensionTabsZoomTest::RunSetZoomSettings(int tab_id,
                                               const char* mode,
                                               const char* scope) {
  scoped_refptr<TabsSetZoomSettingsFunction> set_zoom_settings_function(
      new TabsSetZoomSettingsFunction());
  set_zoom_settings_function->set_extension(extension_.get());

  std::string args;
  if (scope) {
    args = base::StringPrintf("[%u, {\"mode\": \"%s\", \"scope\": \"%s\"}]",
                              tab_id, mode, scope);
  } else {
    args = base::StringPrintf("[%u, {\"mode\": \"%s\"}]", tab_id, mode);
  }

  return utils::RunFunction(set_zoom_settings_function.get(),
                            args,
                            browser(),
                            extension_function_test_utils::NONE);
}

testing::AssertionResult ExtensionTabsZoomTest::RunGetZoomSettings(
    int tab_id,
    std::string* mode,
    std::string* scope) {
  DCHECK(mode);
  DCHECK(scope);
  scoped_refptr<TabsGetZoomSettingsFunction> get_zoom_settings_function(
      new TabsGetZoomSettingsFunction());
  get_zoom_settings_function->set_extension(extension_.get());
  get_zoom_settings_function->set_has_callback(true);

  std::unique_ptr<base::DictionaryValue> get_zoom_settings_result(
      utils::ToDictionary(utils::RunFunctionAndReturnSingleResult(
          get_zoom_settings_function.get(), base::StringPrintf("[%u]", tab_id),
          browser())));

  if (!get_zoom_settings_result)
    return testing::AssertionFailure() << "no result";

  *mode = api_test_utils::GetString(get_zoom_settings_result.get(), "mode");
  *scope = api_test_utils::GetString(get_zoom_settings_result.get(), "scope");

  return testing::AssertionSuccess();
}

testing::AssertionResult ExtensionTabsZoomTest::RunGetDefaultZoom(
    int tab_id,
    double* default_zoom_factor) {
  DCHECK(default_zoom_factor);
  scoped_refptr<TabsGetZoomSettingsFunction> get_zoom_settings_function(
      new TabsGetZoomSettingsFunction());
  get_zoom_settings_function->set_extension(extension_.get());
  get_zoom_settings_function->set_has_callback(true);

  std::unique_ptr<base::DictionaryValue> get_zoom_settings_result(
      utils::ToDictionary(utils::RunFunctionAndReturnSingleResult(
          get_zoom_settings_function.get(), base::StringPrintf("[%u]", tab_id),
          browser())));

  if (!get_zoom_settings_result)
    return testing::AssertionFailure() << "no result";

  if (!get_zoom_settings_result->GetDouble("defaultZoomFactor",
                                           default_zoom_factor)) {
    return testing::AssertionFailure()
           << "default zoom factor not found in result";
  }

  return testing::AssertionSuccess();
}

std::string ExtensionTabsZoomTest::RunSetZoomExpectError(int tab_id,
                                                         double zoom_factor) {
  scoped_refptr<TabsSetZoomFunction> set_zoom_function(
      new TabsSetZoomFunction());
  set_zoom_function->set_extension(extension_.get());
  set_zoom_function->set_has_callback(true);

  return utils::RunFunctionAndReturnError(
      set_zoom_function.get(),
      base::StringPrintf("[%u, %lf]", tab_id, zoom_factor),
      browser());
}

std::string ExtensionTabsZoomTest::RunSetZoomSettingsExpectError(
    int tab_id,
    const char* mode,
    const char* scope) {
  scoped_refptr<TabsSetZoomSettingsFunction> set_zoom_settings_function(
      new TabsSetZoomSettingsFunction());
  set_zoom_settings_function->set_extension(extension_.get());

  return utils::RunFunctionAndReturnError(set_zoom_settings_function.get(),
                                          base::StringPrintf(
                                              "[%u, {\"mode\": \"%s\", "
                                              "\"scope\": \"%s\"}]",
                                              tab_id,
                                              mode,
                                              scope),
                                          browser());
}

content::WebContents* ExtensionTabsZoomTest::OpenUrlAndWaitForLoad(
    const GURL& url) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      url,
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  return  browser()->tab_strip_model()->GetActiveWebContents();
}

namespace {

double GetZoomLevel(const content::WebContents* web_contents) {
  return zoom::ZoomController::FromWebContents(web_contents)->GetZoomLevel();
}

content::OpenURLParams GetOpenParams(const char* url) {
  return content::OpenURLParams(GURL(url),
                                content::Referrer(),
                                NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK,
                                false);
}

}  // namespace

IN_PROC_BROWSER_TEST_F(ExtensionTabsZoomTest, SetAndGetZoom) {
  content::OpenURLParams params(GetOpenParams(url::kAboutBlankURL));
  content::WebContents* web_contents = OpenUrlAndWaitForLoad(params.url);
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);

  // Test default values before we set anything.
  double zoom_factor = -1;
  EXPECT_TRUE(RunGetZoom(tab_id, &zoom_factor));
  EXPECT_EQ(1.0, zoom_factor);

  // Test chrome.tabs.setZoom().
  const double kZoomLevel = 0.8;
  EXPECT_TRUE(RunSetZoom(tab_id, kZoomLevel));
  EXPECT_EQ(kZoomLevel,
            content::ZoomLevelToZoomFactor(GetZoomLevel(web_contents)));

  // Test chrome.tabs.getZoom().
  zoom_factor = -1;
  EXPECT_TRUE(RunGetZoom(tab_id, &zoom_factor));
  EXPECT_EQ(kZoomLevel, zoom_factor);
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsZoomTest, GetDefaultZoom) {
  content::OpenURLParams params(GetOpenParams(url::kAboutBlankURL));
  content::WebContents* web_contents = OpenUrlAndWaitForLoad(params.url);
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);

  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(web_contents);
  double default_zoom_factor = -1.0;
  EXPECT_TRUE(RunGetDefaultZoom(tab_id, &default_zoom_factor));
  EXPECT_TRUE(content::ZoomValuesEqual(
      zoom_controller->GetDefaultZoomLevel(),
      content::ZoomFactorToZoomLevel(default_zoom_factor)));

  // Change the default zoom level and verify GetDefaultZoom returns the
  // correct value.
  content::StoragePartition* partition =
      content::BrowserContext::GetStoragePartition(
          web_contents->GetBrowserContext(), web_contents->GetSiteInstance());
  ChromeZoomLevelPrefs* zoom_prefs =
      static_cast<ChromeZoomLevelPrefs*>(partition->GetZoomLevelDelegate());

  double default_zoom_level = zoom_controller->GetDefaultZoomLevel();
  zoom_prefs->SetDefaultZoomLevelPref(default_zoom_level + 0.5);
  default_zoom_factor = -1.0;
  EXPECT_TRUE(RunGetDefaultZoom(tab_id, &default_zoom_factor));
  EXPECT_TRUE(content::ZoomValuesEqual(
      default_zoom_level + 0.5,
      content::ZoomFactorToZoomLevel(default_zoom_factor)));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsZoomTest, SetToDefaultZoom) {
  content::OpenURLParams params(GetOpenParams(url::kAboutBlankURL));
  content::WebContents* web_contents = OpenUrlAndWaitForLoad(params.url);
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);

  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(web_contents);
  double default_zoom_level = zoom_controller->GetDefaultZoomLevel();
  double new_default_zoom_level = default_zoom_level + 0.42;

  content::StoragePartition* partition =
      content::BrowserContext::GetStoragePartition(
          web_contents->GetBrowserContext(), web_contents->GetSiteInstance());
  ChromeZoomLevelPrefs* zoom_prefs =
      static_cast<ChromeZoomLevelPrefs*>(partition->GetZoomLevelDelegate());

  zoom_prefs->SetDefaultZoomLevelPref(new_default_zoom_level);

  double observed_zoom_factor = -1.0;
  EXPECT_TRUE(RunSetZoom(tab_id, 0.0));
  EXPECT_TRUE(RunGetZoom(tab_id, &observed_zoom_factor));
  EXPECT_TRUE(content::ZoomValuesEqual(
      new_default_zoom_level,
      content::ZoomFactorToZoomLevel(observed_zoom_factor)));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsZoomTest, ZoomSettings) {
  // In this test we need two URLs that (1) represent real pages (i.e. they
  // load without causing an error page load), (2) have different domains, and
  // (3) are zoomable by the extension API (this last condition rules out
  // chrome:// urls). We achieve this by noting that about:blank meets these
  // requirements, allowing us to spin up an embedded http server on localhost
  // to get the other domain.
  net::EmbeddedTestServer http_server;
  http_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(http_server.Start());

  GURL url_A = http_server.GetURL("/simple.html");
  GURL url_B("about:blank");

  // Tabs A1 and A2 are navigated to the same origin, while B is navigated
  // to a different one.
  content::WebContents* web_contents_A1 = OpenUrlAndWaitForLoad(url_A);
  content::WebContents* web_contents_A2 = OpenUrlAndWaitForLoad(url_A);
  content::WebContents* web_contents_B = OpenUrlAndWaitForLoad(url_B);

  int tab_id_A1 = ExtensionTabUtil::GetTabId(web_contents_A1);
  int tab_id_A2 = ExtensionTabUtil::GetTabId(web_contents_A2);
  int tab_id_B = ExtensionTabUtil::GetTabId(web_contents_B);

  ASSERT_FLOAT_EQ(
      1.f, content::ZoomLevelToZoomFactor(GetZoomLevel(web_contents_A1)));
  ASSERT_FLOAT_EQ(
      1.f, content::ZoomLevelToZoomFactor(GetZoomLevel(web_contents_A2)));
  ASSERT_FLOAT_EQ(
      1.f, content::ZoomLevelToZoomFactor(GetZoomLevel(web_contents_B)));

  // Test per-origin automatic zoom settings.
  EXPECT_TRUE(RunSetZoom(tab_id_B, 1.f));
  EXPECT_TRUE(RunSetZoom(tab_id_A2, 1.1f));
  EXPECT_FLOAT_EQ(
      1.1f, content::ZoomLevelToZoomFactor(GetZoomLevel(web_contents_A1)));
  EXPECT_FLOAT_EQ(
      1.1f, content::ZoomLevelToZoomFactor(GetZoomLevel(web_contents_A2)));
  EXPECT_FLOAT_EQ(1.f,
                  content::ZoomLevelToZoomFactor(GetZoomLevel(web_contents_B)));

  // Test per-tab automatic zoom settings.
  EXPECT_TRUE(RunSetZoomSettings(tab_id_A1, "automatic", "per-tab"));
  EXPECT_TRUE(RunSetZoom(tab_id_A1, 1.2f));
  EXPECT_FLOAT_EQ(
      1.2f, content::ZoomLevelToZoomFactor(GetZoomLevel(web_contents_A1)));
  EXPECT_FLOAT_EQ(
      1.1f, content::ZoomLevelToZoomFactor(GetZoomLevel(web_contents_A2)));

  // Test 'manual' mode.
  EXPECT_TRUE(RunSetZoomSettings(tab_id_A1, "manual", nullptr));
  EXPECT_TRUE(RunSetZoom(tab_id_A1, 1.3f));
  EXPECT_FLOAT_EQ(
      1.3f, content::ZoomLevelToZoomFactor(GetZoomLevel(web_contents_A1)));
  EXPECT_FLOAT_EQ(
      1.1f, content::ZoomLevelToZoomFactor(GetZoomLevel(web_contents_A2)));

  // Test 'disabled' mode, which will reset A1's zoom to 1.f.
  EXPECT_TRUE(RunSetZoomSettings(tab_id_A1, "disabled", nullptr));
  std::string error = RunSetZoomExpectError(tab_id_A1, 1.4f);
  EXPECT_TRUE(base::MatchPattern(error, keys::kCannotZoomDisabledTabError));
  EXPECT_FLOAT_EQ(
      1.f, content::ZoomLevelToZoomFactor(GetZoomLevel(web_contents_A1)));
  // We should still be able to zoom A2 though.
  EXPECT_TRUE(RunSetZoom(tab_id_A2, 1.4f));
  EXPECT_FLOAT_EQ(
      1.4f, content::ZoomLevelToZoomFactor(GetZoomLevel(web_contents_A2)));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsZoomTest, PerTabResetsOnNavigation) {
  net::EmbeddedTestServer http_server;
  http_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(http_server.Start());

  GURL url_A = http_server.GetURL("/simple.html");
  GURL url_B("about:blank");

  content::WebContents* web_contents = OpenUrlAndWaitForLoad(url_A);
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);
  EXPECT_TRUE(RunSetZoomSettings(tab_id, "automatic", "per-tab"));

  std::string mode;
  std::string scope;
  EXPECT_TRUE(RunGetZoomSettings(tab_id, &mode, &scope));
  EXPECT_EQ("automatic", mode);
  EXPECT_EQ("per-tab", scope);

  // Navigation of tab should reset mode to per-origin.
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url_B,
                                                            1);
  EXPECT_TRUE(RunGetZoomSettings(tab_id, &mode, &scope));
  EXPECT_EQ("automatic", mode);
  EXPECT_EQ("per-origin", scope);
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsZoomTest, GetZoomSettings) {
  content::OpenURLParams params(GetOpenParams(url::kAboutBlankURL));
  content::WebContents* web_contents = OpenUrlAndWaitForLoad(params.url);
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);

  std::string mode;
  std::string scope;

  EXPECT_TRUE(RunGetZoomSettings(tab_id, &mode, &scope));
  EXPECT_EQ("automatic", mode);
  EXPECT_EQ("per-origin", scope);

  EXPECT_TRUE(RunSetZoomSettings(tab_id, "automatic", "per-tab"));
  EXPECT_TRUE(RunGetZoomSettings(tab_id, &mode, &scope));

  EXPECT_EQ("automatic", mode);
  EXPECT_EQ("per-tab", scope);

  std::string error =
      RunSetZoomSettingsExpectError(tab_id, "manual", "per-origin");
  EXPECT_TRUE(base::MatchPattern(error, keys::kPerOriginOnlyInAutomaticError));
  error =
      RunSetZoomSettingsExpectError(tab_id, "disabled", "per-origin");
  EXPECT_TRUE(base::MatchPattern(error, keys::kPerOriginOnlyInAutomaticError));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsZoomTest, CannotZoomInvalidTab) {
  content::OpenURLParams params(GetOpenParams(url::kAboutBlankURL));
  content::WebContents* web_contents = OpenUrlAndWaitForLoad(params.url);
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);

  int bogus_id = tab_id + 100;
  std::string error = RunSetZoomExpectError(bogus_id, 3.14159);
  EXPECT_TRUE(base::MatchPattern(error, keys::kTabNotFoundError));

  error = RunSetZoomSettingsExpectError(bogus_id, "manual", "per-tab");
  EXPECT_TRUE(base::MatchPattern(error, keys::kTabNotFoundError));

  const char kNewTestTabArgs[] = "chrome://version";
  params = GetOpenParams(kNewTestTabArgs);
  web_contents = browser()->OpenURL(params);
  tab_id = ExtensionTabUtil::GetTabId(web_contents);

  // Test chrome.tabs.setZoom().
  error = RunSetZoomExpectError(tab_id, 3.14159);
  EXPECT_TRUE(
      base::MatchPattern(error, manifest_errors::kCannotAccessChromeUrl));

  // chrome.tabs.setZoomSettings().
  error = RunSetZoomSettingsExpectError(tab_id, "manual", "per-tab");
  EXPECT_TRUE(
      base::MatchPattern(error, manifest_errors::kCannotAccessChromeUrl));
}

// Regression test for crbug.com/660498.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, Foo) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  content::WebContents* first_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(first_web_contents);
  chrome::NewTab(browser());
  content::WebContents* second_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(first_web_contents, second_web_contents);
  GURL url = embedded_test_server()->GetURL(
      "/extensions/api_test/tabs/pdf_extension_test.html");
  content::TestNavigationManager navigation_manager(
      second_web_contents, GURL("http://www.facebook.com:83"));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  EXPECT_TRUE(navigation_manager.WaitForRequestStart());

  browser()->tab_strip_model()->ActivateTabAt(0, true);
  EXPECT_EQ(first_web_contents,
            browser()->tab_strip_model()->GetActiveWebContents());
  browser()->tab_strip_model()->ActivateTabAt(1, true);
  EXPECT_EQ(second_web_contents,
            browser()->tab_strip_model()->GetActiveWebContents());

  EXPECT_EQ(url, second_web_contents->GetVisibleURL());
}

}  // namespace extensions
