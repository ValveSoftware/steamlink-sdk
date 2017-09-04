// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/test_extension_dir.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/api/runtime/runtime_api.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/notification_types.h"
#include "extensions/test/result_catcher.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

// Tests the privileged components of chrome.runtime.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ChromeRuntimePrivileged) {
  ASSERT_TRUE(RunExtensionTest("runtime/privileged")) << message_;
}

// Tests the unprivileged components of chrome.runtime.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ChromeRuntimeUnprivileged) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("runtime/content_script")));

  // The content script runs on this page.
  extensions::ResultCatcher catcher;
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(catcher.GetNextResult()) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ChromeRuntimeUninstallURL) {
  // Auto-confirm the uninstall dialog.
  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT);
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("runtime")
                                .AppendASCII("uninstall_url")
                                .AppendASCII("sets_uninstall_url")));
  ASSERT_TRUE(RunExtensionTest("runtime/uninstall_url")) << message_;
}

namespace extensions {

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ChromeRuntimeOpenOptionsPage) {
  ASSERT_TRUE(RunExtensionTest("runtime/open_options_page"));
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ChromeRuntimeOpenOptionsPageError) {
  ASSERT_TRUE(RunExtensionTest("runtime/open_options_page_error"));
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ChromeRuntimeGetPlatformInfo) {
  std::unique_ptr<base::Value> result(
      extension_function_test_utils::RunFunctionAndReturnSingleResult(
          new RuntimeGetPlatformInfoFunction(), "[]", browser()));
  ASSERT_TRUE(result.get() != NULL);
  base::DictionaryValue* dict =
      extension_function_test_utils::ToDictionary(result.get());
  ASSERT_TRUE(dict != NULL);
  EXPECT_TRUE(dict->HasKey("os"));
  EXPECT_TRUE(dict->HasKey("arch"));
  EXPECT_TRUE(dict->HasKey("nacl_arch"));
}

// Tests chrome.runtime.getPackageDirectory with an app.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest,
                       ChromeRuntimeGetPackageDirectoryEntryApp) {
  ASSERT_TRUE(RunPlatformAppTest("api_test/runtime/get_package_directory/app"))
      << message_;
}

// Tests chrome.runtime.getPackageDirectory with an extension.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest,
                       ChromeRuntimeGetPackageDirectoryEntryExtension) {
  ASSERT_TRUE(RunExtensionTest("runtime/get_package_directory/extension"))
      << message_;
}

// Tests chrome.runtime.reload
// This test is flaky: crbug.com/366181
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_ChromeRuntimeReload) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  const char kManifest[] =
      "{"
      "  \"name\": \"reload\","
      "  \"version\": \"1.0\","
      "  \"background\": {"
      "    \"scripts\": [\"background.js\"]"
      "  },"
      "  \"manifest_version\": 2"
      "}";

  TestExtensionDir dir;
  dir.WriteManifest(kManifest);
  dir.WriteFile(FILE_PATH_LITERAL("background.js"), "console.log('loaded');");

  const Extension* extension = LoadExtension(dir.UnpackedPath());
  ASSERT_TRUE(extension);
  const std::string extension_id = extension->id();

  // Somewhat arbitrary upper limit of 30 iterations. If the extension manages
  // to reload itself that often without being terminated, the test fails
  // anyway.
  for (int i = 0; i < 30; i++) {
    content::WindowedNotificationObserver unload_observer(
        extensions::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED,
        content::NotificationService::AllSources());
    content::WindowedNotificationObserver load_observer(
        extensions::NOTIFICATION_EXTENSION_LOADED_DEPRECATED,
        content::NotificationService::AllSources());

    ASSERT_TRUE(ExecuteScriptInBackgroundPageNoWait(
        extension_id, "chrome.runtime.reload();"));
    unload_observer.Wait();

    if (registry->GetExtensionById(extension_id,
                                   ExtensionRegistry::TERMINATED)) {
      break;
    } else {
      load_observer.Wait();
      WaitForExtensionViewsToLoad();
    }
  }
  ASSERT_TRUE(
      registry->GetExtensionById(extension_id, ExtensionRegistry::TERMINATED));
}

}  // namespace extensions
