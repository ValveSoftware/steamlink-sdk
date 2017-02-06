// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/test_browser_window.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/test_util.h"

namespace extensions {

namespace {

std::unique_ptr<base::ListValue> RunTabsQueryFunction(
    Browser* browser,
    const Extension* extension,
    const std::string& query_info) {
  scoped_refptr<TabsQueryFunction> function(new TabsQueryFunction());
  function->set_extension(extension);
  std::unique_ptr<base::Value> value(
      extension_function_test_utils::RunFunctionAndReturnSingleResult(
          function.get(), query_info, browser,
          extension_function_test_utils::NONE));
  return base::ListValue::From(std::move(value));
}

}  // namespace

class TabsApiUnitTest : public ExtensionServiceTestBase {
 protected:
  TabsApiUnitTest() {}
  ~TabsApiUnitTest() override {}

  Browser* browser() { return browser_.get(); }

 private:
  // ExtensionServiceTestBase:
  void SetUp() override;
  void TearDown() override;

  // The browser (and accompanying window).
  std::unique_ptr<TestBrowserWindow> browser_window_;
  std::unique_ptr<Browser> browser_;

  DISALLOW_COPY_AND_ASSIGN(TabsApiUnitTest);
};

void TabsApiUnitTest::SetUp() {
  ExtensionServiceTestBase::SetUp();
  InitializeEmptyExtensionService();

  browser_window_.reset(new TestBrowserWindow());
  Browser::CreateParams params(profile());
  params.type = Browser::TYPE_TABBED;
  params.window = browser_window_.get();
  browser_.reset(new Browser(params));
}

void TabsApiUnitTest::TearDown() {
  browser_.reset();
  browser_window_.reset();
  ExtensionServiceTestBase::TearDown();
}

TEST_F(TabsApiUnitTest, QueryWithoutTabsPermission) {
  GURL tab_urls[] = {GURL("http://www.google.com"),
                     GURL("http://www.example.com"),
                     GURL("https://www.google.com")};
  std::string tab_titles[] = {"", "Sample title", "Sample title"};

  // Add 3 web contentses to the browser.
  std::unique_ptr<content::WebContents> web_contentses[arraysize(tab_urls)];
  for (size_t i = 0; i < arraysize(tab_urls); ++i) {
    content::WebContents* web_contents =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
    web_contentses[i].reset(web_contents);
    browser()->tab_strip_model()->AppendWebContents(web_contents, true);
    EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents(),
              web_contents);
    content::WebContentsTester* web_contents_tester =
        content::WebContentsTester::For(web_contents);
    web_contents_tester->NavigateAndCommit(tab_urls[i]);
    web_contents->GetController().GetVisibleEntry()->SetTitle(
        base::ASCIIToUTF16(tab_titles[i]));
  }

  const char* kTitleAndURLQueryInfo =
      "[{\"title\": \"Sample title\", \"url\": \"*://www.google.com/*\"}]";

  // An extension without "tabs" permission will see none of the 3 tabs.
  scoped_refptr<const Extension> extension = test_util::CreateEmptyExtension();
  std::unique_ptr<base::ListValue> tabs_list_without_permission(
      RunTabsQueryFunction(browser(), extension.get(), kTitleAndURLQueryInfo));
  ASSERT_TRUE(tabs_list_without_permission);
  EXPECT_EQ(0u, tabs_list_without_permission->GetSize());

  // An extension with "tabs" permission however will see the third tab.
  scoped_refptr<const Extension> extension_with_permission =
      ExtensionBuilder()
          .SetManifest(
              DictionaryBuilder()
                  .Set("name", "Extension with tabs permission")
                  .Set("version", "1.0")
                  .Set("manifest_version", 2)
                  .Set("permissions", ListBuilder().Append("tabs").Build())
                  .Build())
          .Build();
  std::unique_ptr<base::ListValue> tabs_list_with_permission(
      RunTabsQueryFunction(browser(), extension_with_permission.get(),
                           kTitleAndURLQueryInfo));
  ASSERT_TRUE(tabs_list_with_permission);
  ASSERT_EQ(1u, tabs_list_with_permission->GetSize());

  const base::DictionaryValue* third_tab_info;
  ASSERT_TRUE(tabs_list_with_permission->GetDictionary(0, &third_tab_info));
  int third_tab_id = -1;
  ASSERT_TRUE(third_tab_info->GetInteger("id", &third_tab_id));
  EXPECT_EQ(ExtensionTabUtil::GetTabId(web_contentses[2].get()), third_tab_id);
}

TEST_F(TabsApiUnitTest, QueryWithHostPermission) {
  GURL tab_urls[] = {GURL("http://www.google.com"),
                     GURL("http://www.example.com"),
                     GURL("https://www.google.com/test")};
  std::string tab_titles[] = {"", "Sample title", "Sample title"};

  // Add 3 web contentses to the browser.
  std::unique_ptr<content::WebContents> web_contentses[arraysize(tab_urls)];
  for (size_t i = 0; i < arraysize(tab_urls); ++i) {
    content::WebContents* web_contents =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
    web_contentses[i].reset(web_contents);
    browser()->tab_strip_model()->AppendWebContents(web_contents, true);
    EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents(),
              web_contents);
    content::WebContentsTester* web_contents_tester =
        content::WebContentsTester::For(web_contents);
    web_contents_tester->NavigateAndCommit(tab_urls[i]);
    web_contents->GetController().GetVisibleEntry()->SetTitle(
        base::ASCIIToUTF16(tab_titles[i]));
  }

  const char* kTitleAndURLQueryInfo =
      "[{\"title\": \"Sample title\", \"url\": \"*://www.google.com/*\"}]";

  // An extension with "host" permission will only see the third tab.
  scoped_refptr<const Extension> extension_with_permission =
      ExtensionBuilder()
          .SetManifest(
              DictionaryBuilder()
                  .Set("name", "Extension with tabs permission")
                  .Set("version", "1.0")
                  .Set("manifest_version", 2)
                  .Set("permissions",
                       ListBuilder().Append("*://www.google.com/*").Build())
                  .Build())
          .Build();

  {
    std::unique_ptr<base::ListValue> tabs_list_with_permission(
        RunTabsQueryFunction(browser(), extension_with_permission.get(),
                             kTitleAndURLQueryInfo));
    ASSERT_TRUE(tabs_list_with_permission);
    ASSERT_EQ(1u, tabs_list_with_permission->GetSize());

    const base::DictionaryValue* third_tab_info;
    ASSERT_TRUE(tabs_list_with_permission->GetDictionary(0, &third_tab_info));
    int third_tab_id = -1;
    ASSERT_TRUE(third_tab_info->GetInteger("id", &third_tab_id));
    EXPECT_EQ(ExtensionTabUtil::GetTabId(web_contentses[2].get()),
              third_tab_id);
  }

  // Try the same without title, first and third tabs will match.
  const char* kURLQueryInfo = "[{\"url\": \"*://www.google.com/*\"}]";
  {
    std::unique_ptr<base::ListValue> tabs_list_with_permission(
        RunTabsQueryFunction(browser(), extension_with_permission.get(),
                             kURLQueryInfo));
    ASSERT_TRUE(tabs_list_with_permission);
    ASSERT_EQ(2u, tabs_list_with_permission->GetSize());

    const base::DictionaryValue* first_tab_info;
    const base::DictionaryValue* third_tab_info;
    ASSERT_TRUE(tabs_list_with_permission->GetDictionary(0, &first_tab_info));
    ASSERT_TRUE(tabs_list_with_permission->GetDictionary(1, &third_tab_info));

    std::vector<int> expected_tabs_ids;
    expected_tabs_ids.push_back(
        ExtensionTabUtil::GetTabId(web_contentses[0].get()));
    expected_tabs_ids.push_back(
        ExtensionTabUtil::GetTabId(web_contentses[2].get()));

    int first_tab_id = -1;
    ASSERT_TRUE(first_tab_info->GetInteger("id", &first_tab_id));
    EXPECT_TRUE(ContainsValue(expected_tabs_ids, first_tab_id));

    int third_tab_id = -1;
    ASSERT_TRUE(third_tab_info->GetInteger("id", &third_tab_id));
    EXPECT_TRUE(ContainsValue(expected_tabs_ids, third_tab_id));
  }
}

// Test that using the PDF extension for tab updates is treated as a
// renderer-initiated navigation. crbug.com/660498
TEST_F(TabsApiUnitTest, PDFExtensionNavigation) {
  DictionaryBuilder manifest;
  manifest.Set("name", "pdfext")
      .Set("description", "desc")
      .Set("version", "0.1")
      .Set("manifest_version", 2)
      .Set("permissions", ListBuilder().Append("tabs").Build());
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(manifest.Build())
          .SetID(extension_misc::kPdfExtensionId)
          .Build();
  ASSERT_TRUE(extension);

  content::WebContents* web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  ASSERT_TRUE(web_contents);
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents);
  const GURL kGoogle("http://www.google.com");
  web_contents_tester->NavigateAndCommit(kGoogle);
  EXPECT_EQ(kGoogle, web_contents->GetLastCommittedURL());
  EXPECT_EQ(kGoogle, web_contents->GetVisibleURL());

  SessionTabHelper::CreateForWebContents(web_contents);
  int tab_id = SessionTabHelper::IdForTab(web_contents);
  browser()->tab_strip_model()->AppendWebContents(web_contents, true);

  scoped_refptr<TabsUpdateFunction> function = new TabsUpdateFunction();
  function->set_extension(extension.get());
  function->set_browser_context(profile());
  std::unique_ptr<base::ListValue> args(
      extension_function_test_utils::ParseList(base::StringPrintf(
          "[%d, {\"url\":\"http://example.com\"}]", tab_id)));
  function->SetArgs(args.get());
  api_test_utils::SendResponseHelper response_helper(function.get());
  function->RunWithValidation()->Execute();

  EXPECT_EQ(kGoogle, web_contents->GetLastCommittedURL());
  EXPECT_EQ(kGoogle, web_contents->GetVisibleURL());

  // Clean up.
  response_helper.WaitForResponse();
  while (!browser()->tab_strip_model()->empty())
    browser()->tab_strip_model()->CloseWebContentsAt(0, 0);
  base::RunLoop().RunUntilIdle();
}

}  // namespace extensions
