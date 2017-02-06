// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigation_controller_impl.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/frame_host/frame_navigation_entry.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/page_state_serialization.h"
#include "content/common/site_isolation_policy.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/shell_switches.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/test_frame_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/url_request/url_request_failed_job.h"

namespace content {

class NavigationControllerBrowserTest : public ContentBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

// Ensure that tests can navigate subframes cross-site in both default mode and
// --site-per-process, but that they only go cross-process in the latter.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest, LoadCrossSiteSubframe) {
  // Load a main frame with a subframe.
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_iframe.html"));
  NavigateToURL(shell(), main_url);
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  ASSERT_EQ(1U, root->child_count());
  ASSERT_NE(nullptr, root->child_at(0));

  // Use NavigateFrameToURL to go cross-site in the subframe.
  GURL foo_url(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_1.html"));
  NavigateFrameToURL(root->child_at(0), foo_url);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // We should only have swapped processes in --site-per-process.
  bool cross_process = root->current_frame_host()->GetProcess() !=
                       root->child_at(0)->current_frame_host()->GetProcess();
  EXPECT_EQ(AreAllSitesIsolatedForTesting(), cross_process);
}

// Verifies that the base, history, and data URLs for LoadDataWithBaseURL end up
// in the expected parts of the NavigationEntry in each stage of navigation, and
// that we don't kill the renderer on reload.  See https://crbug.com/522567.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest, LoadDataWithBaseURL) {
  const GURL base_url("http://baseurl");
  const GURL history_url("http://historyurl");
  const std::string data = "<html><body>foo</body></html>";
  const GURL data_url = GURL("data:text/html;charset=utf-8," + data);

  const NavigationControllerImpl& controller =
      static_cast<const NavigationControllerImpl&>(
          shell()->web_contents()->GetController());

  // Load data, but don't commit yet.
  TestNavigationObserver same_tab_observer(shell()->web_contents(), 1);
  shell()->LoadDataWithBaseURL(history_url, data, base_url);

  // Verify the pending NavigationEntry.
  NavigationEntryImpl* pending_entry = controller.GetPendingEntry();
  EXPECT_EQ(base_url, pending_entry->GetBaseURLForDataURL());
  EXPECT_EQ(history_url, pending_entry->GetVirtualURL());
  EXPECT_EQ(history_url, pending_entry->GetHistoryURLForDataURL());
  EXPECT_EQ(data_url, pending_entry->GetURL());

  // Let the navigation commit.
  same_tab_observer.Wait();

  // Verify the last committed NavigationEntry.
  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(base_url, entry->GetBaseURLForDataURL());
  EXPECT_EQ(history_url, entry->GetVirtualURL());
  EXPECT_EQ(history_url, entry->GetHistoryURLForDataURL());
  EXPECT_EQ(data_url, entry->GetURL());

  // We should use data_url instead of the base_url as the original url of
  // this navigation entry, because base_url is only used for resolving relative
  // paths in the data, or enforcing same origin policy.
  EXPECT_EQ(data_url, entry->GetOriginalRequestURL());

  // Now reload and make sure the renderer isn't killed.
  ReloadBlockUntilNavigationsComplete(shell(), 1);
  EXPECT_TRUE(shell()->web_contents()->GetMainFrame()->IsRenderFrameLive());

  // Verify the last committed NavigationEntry hasn't changed.
  NavigationEntryImpl* reload_entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(entry, reload_entry);
  EXPECT_EQ(base_url, reload_entry->GetBaseURLForDataURL());
  EXPECT_EQ(history_url, reload_entry->GetVirtualURL());
  EXPECT_EQ(history_url, reload_entry->GetHistoryURLForDataURL());
  EXPECT_EQ(data_url, reload_entry->GetOriginalRequestURL());
  EXPECT_EQ(data_url, reload_entry->GetURL());
}

// Verify which page loads when going back to a LoadDataWithBaseURL entry.
// See https://crbug.com/612196.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       LoadDataWithBaseURLTitleAfterBack) {
  const GURL base_url("http://baseurl");
  const GURL history_url(
      embedded_test_server()->GetURL("/navigation_controller/form.html"));
  const std::string data1 = "<html><title>One</title><body>foo</body></html>";
  const GURL data_url1 = GURL("data:text/html;charset=utf-8," + data1);

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  {
    TestNavigationObserver same_tab_observer(shell()->web_contents(), 1);
    shell()->LoadDataWithBaseURL(history_url, data1, base_url);
    same_tab_observer.Wait();
  }

  // Verify the last committed NavigationEntry.
  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(base_url, entry->GetBaseURLForDataURL());
  EXPECT_EQ(history_url, entry->GetVirtualURL());
  EXPECT_EQ(history_url, entry->GetHistoryURLForDataURL());
  EXPECT_EQ(data_url1, entry->GetURL());

  // Navigate again to a different data URL.
  const std::string data2 = "<html><title>Two</title><body>bar</body></html>";
  const GURL data_url2 = GURL("data:text/html;charset=utf-8," + data2);
  {
    TestNavigationObserver same_tab_observer(shell()->web_contents(), 1);
    // Load data, not loaddatawithbaseurl.
    EXPECT_TRUE(NavigateToURL(shell(), data_url2));
    same_tab_observer.Wait();
  }

  // Go back.
  TestNavigationObserver back_load_observer(shell()->web_contents());
  controller.GoBack();
  back_load_observer.Wait();

  // Check title.
  // TODO(creis): The default navigation path incorrectly loads the history_url
  // and claims it loaded the data_url (due to a bug where GoToEntry does not
  // handle this case).  This is confusing.  When using subframe
  // FrameNavigationEntries, we load the data URL when going back, as expected.
  if (SiteIsolationPolicy::UseSubframeNavigationEntries())
    EXPECT_EQ("One", base::UTF16ToUTF8(shell()->web_contents()->GetTitle()));
  else
    EXPECT_EQ("form", base::UTF16ToUTF8(shell()->web_contents()->GetTitle()));

  // Verify the last committed NavigationEntry.
  NavigationEntryImpl* back_entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(base_url, back_entry->GetBaseURLForDataURL());
  EXPECT_EQ(history_url, back_entry->GetVirtualURL());
  EXPECT_EQ(history_url, back_entry->GetHistoryURLForDataURL());
  EXPECT_EQ(data_url1, back_entry->GetOriginalRequestURL());
  EXPECT_EQ(data_url1, back_entry->GetURL());

  EXPECT_EQ(data_url1,
            shell()->web_contents()->GetMainFrame()->GetLastCommittedURL());
}

#if defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       LoadDataWithInvalidBaseURL) {
  const GURL base_url("http://");  // Invalid.
  const GURL history_url("http://historyurl");
  const std::string title = "invalid_base_url";
  const std::string data = base::StringPrintf(
      "<html><head><title>%s</title></head><body>foo</body></html>",
      title.c_str());
  const GURL data_url = GURL("data:text/html;charset=utf-8," + data);

  const NavigationControllerImpl& controller =
      static_cast<const NavigationControllerImpl&>(
          shell()->web_contents()->GetController());

  TestNavigationObserver same_tab_observer(shell()->web_contents(), 1);
  TitleWatcher title_watcher(shell()->web_contents(), base::UTF8ToUTF16(title));
  shell()->LoadDataAsStringWithBaseURL(history_url, data, base_url);
  same_tab_observer.Wait();
  base::string16 actual_title = title_watcher.WaitAndGetTitle();
  EXPECT_EQ(title, base::UTF16ToUTF8(actual_title));

  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
  // What the base URL ends up being is really implementation defined, as
  // using an invalid base URL is already undefined behavior.
  EXPECT_EQ(base_url, entry->GetBaseURLForDataURL());
}
#endif  // defined(OS_ANDROID)

IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       NavigateFromLoadDataWithBaseURL) {
  const GURL base_url("http://baseurl");
  const GURL history_url("http://historyurl");
  const std::string data = "<html><body></body></html>";
  const GURL data_url = GURL("data:text/html;charset=utf-8," + data);

  const NavigationControllerImpl& controller =
      static_cast<const NavigationControllerImpl&>(
          shell()->web_contents()->GetController());

  // Load data and commit.
  {
    TestNavigationObserver same_tab_observer(shell()->web_contents(), 1);
    shell()->LoadDataWithBaseURL(history_url, data, base_url);
    same_tab_observer.Wait();
    EXPECT_EQ(1, controller.GetEntryCount());
    NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
    EXPECT_EQ(base_url, entry->GetBaseURLForDataURL());
    EXPECT_EQ(history_url, entry->GetVirtualURL());
    EXPECT_EQ(history_url, entry->GetHistoryURLForDataURL());
    EXPECT_EQ(data_url, entry->GetURL());
  }

  // TODO(boliu): Add test for in-page fragment navigation. See
  // crbug.com/561034.

  // Navigate with Javascript.
  {
    GURL navigate_url = embedded_test_server()->base_url();
    std::string script = "document.location = '" +
                         navigate_url.spec() + "';";
    TestNavigationObserver same_tab_observer(shell()->web_contents(), 1);
    EXPECT_TRUE(ExecuteScript(shell(), script));
    same_tab_observer.Wait();
    EXPECT_EQ(2, controller.GetEntryCount());
    NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
    EXPECT_TRUE(entry->GetBaseURLForDataURL().is_empty());
    EXPECT_TRUE(entry->GetHistoryURLForDataURL().is_empty());
    EXPECT_EQ(navigate_url, entry->GetVirtualURL());
    EXPECT_EQ(navigate_url, entry->GetURL());
  }
}

IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       FragmentNavigateFromLoadDataWithBaseURL) {
  const GURL base_url("http://baseurl");
  const GURL history_url("http://historyurl");
  const std::string data =
      "<html><body>"
      "  <p id=\"frag\"><a id=\"fraglink\" href=\"#frag\">in-page nav</a></p>"
      "</body></html>";

  const NavigationControllerImpl& controller =
      static_cast<const NavigationControllerImpl&>(
          shell()->web_contents()->GetController());

  // Load data and commit.
  TestNavigationObserver same_tab_observer(shell()->web_contents(), 1);
#if defined(OS_ANDROID)
  shell()->LoadDataAsStringWithBaseURL(history_url, data, base_url);
#else
  shell()->LoadDataWithBaseURL(history_url, data, base_url);
#endif
  same_tab_observer.Wait();
  EXPECT_EQ(1, controller.GetEntryCount());
  const GURL data_url = controller.GetLastCommittedEntry()->GetURL();

  // Perform a fragment navigation using a javascript: URL.
  GURL js_url("javascript:document.location = '#frag';");
  NavigateToURL(shell(), js_url);
  EXPECT_EQ(2, controller.GetEntryCount());
  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
  // TODO(boliu): These expectations maybe incorrect due to crbug.com/561034.
  EXPECT_TRUE(entry->GetBaseURLForDataURL().is_empty());
  EXPECT_TRUE(entry->GetHistoryURLForDataURL().is_empty());
  EXPECT_EQ(data_url, entry->GetVirtualURL());
  EXPECT_EQ(data_url, entry->GetURL());

  // Passes if renderer is still alive.
  EXPECT_TRUE(ExecuteScript(shell(), "console.log('Success');"));
}

IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest, UniqueIDs) {
  const NavigationControllerImpl& controller =
      static_cast<const NavigationControllerImpl&>(
          shell()->web_contents()->GetController());

  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_link_to_load_iframe.html"));
  NavigateToURL(shell(), main_url);
  ASSERT_EQ(1, controller.GetEntryCount());

  // Use JavaScript to click the link and load the iframe.
  std::string script = "document.getElementById('link').click()";
  EXPECT_TRUE(ExecuteScript(shell(), script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  ASSERT_EQ(2, controller.GetEntryCount());

  // Unique IDs should... um... be unique.
  ASSERT_NE(controller.GetEntryAtIndex(0)->GetUniqueID(),
            controller.GetEntryAtIndex(1)->GetUniqueID());
}

// Ensures that RenderFrameHosts end up with the correct nav_entry_id() after
// navigations.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest, UniqueIDsOnFrames) {
  NavigationController& controller = shell()->web_contents()->GetController();

  // Load a main frame with an about:blank subframe.
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_iframe.html"));
  NavigateToURL(shell(), main_url);
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  ASSERT_EQ(1U, root->child_count());
  ASSERT_NE(nullptr, root->child_at(0));

  // The main frame's nav_entry_id should match the last committed entry.
  int unique_id = controller.GetLastCommittedEntry()->GetUniqueID();
  EXPECT_EQ(unique_id, root->current_frame_host()->nav_entry_id());

  // The about:blank iframe should have inherited the same nav_entry_id.
  EXPECT_EQ(unique_id, root->child_at(0)->current_frame_host()->nav_entry_id());

  // Use NavigateFrameToURL to go cross-site in the subframe.
  GURL foo_url(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_1.html"));
  NavigateFrameToURL(root->child_at(0), foo_url);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // The unique ID should have stayed the same for the auto-subframe navigation,
  // since the new page replaces the initial about:blank page in the subframe.
  EXPECT_EQ(unique_id, controller.GetLastCommittedEntry()->GetUniqueID());
  EXPECT_EQ(unique_id, root->current_frame_host()->nav_entry_id());
  EXPECT_EQ(unique_id, root->child_at(0)->current_frame_host()->nav_entry_id());

  // Navigating in the subframe again should create a new entry.
  GURL foo_url2(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_2.html"));
  NavigateFrameToURL(root->child_at(0), foo_url2);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  int unique_id2 = controller.GetLastCommittedEntry()->GetUniqueID();
  EXPECT_NE(unique_id, unique_id2);

  // The unique ID should have updated for the current RenderFrameHost in both
  // frames, not just the subframe.
  EXPECT_EQ(unique_id2, root->current_frame_host()->nav_entry_id());
  EXPECT_EQ(unique_id2,
            root->child_at(0)->current_frame_host()->nav_entry_id());
}

// This test used to make sure that a scheme used to prevent spoofs didn't ever
// interfere with navigations. We switched to a different scheme, so now this is
// just a test to make sure we can still navigate once we prune the history
// list.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       DontIgnoreBackAfterNavEntryLimit) {
  NavigationController& controller =
      shell()->web_contents()->GetController();

  const int kMaxEntryCount =
      static_cast<int>(NavigationControllerImpl::max_entry_count());

  // Load up to the max count, all entries should be there.
  for (int url_index = 0; url_index < kMaxEntryCount; ++url_index) {
    GURL url(base::StringPrintf("data:text/html,page%d", url_index));
    EXPECT_TRUE(NavigateToURL(shell(), url));
  }

  EXPECT_EQ(controller.GetEntryCount(), kMaxEntryCount);

  // Navigate twice more more.
  for (int url_index = kMaxEntryCount;
       url_index < kMaxEntryCount + 2; ++url_index) {
    GURL url(base::StringPrintf("data:text/html,page%d", url_index));
    EXPECT_TRUE(NavigateToURL(shell(), url));
  }

  // We expect page0 and page1 to be gone.
  EXPECT_EQ(kMaxEntryCount, controller.GetEntryCount());
  EXPECT_EQ(GURL("data:text/html,page2"),
            controller.GetEntryAtIndex(0)->GetURL());

  // Now try to go back. This should not hang.
  ASSERT_TRUE(controller.CanGoBack());
  controller.GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // This should have successfully gone back.
  EXPECT_EQ(GURL(base::StringPrintf("data:text/html,page%d", kMaxEntryCount)),
            controller.GetLastCommittedEntry()->GetURL());
}

namespace {

int RendererHistoryLength(Shell* shell) {
  int value = 0;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      shell, "domAutomationController.send(history.length)", &value));
  return value;
}

// Similar to the ones from content_browser_test_utils.
bool NavigateToURLAndReplace(Shell* shell, const GURL& url) {
  WebContents* web_contents = shell->web_contents();
  WaitForLoadStop(web_contents);
  TestNavigationObserver same_tab_observer(web_contents, 1);
  NavigationController::LoadURLParams params(url);
  params.should_replace_current_entry = true;
  web_contents->GetController().LoadURLWithParams(params);
  web_contents->Focus();
  same_tab_observer.Wait();
  if (!IsLastCommittedEntryOfPageType(web_contents, PAGE_TYPE_NORMAL))
    return false;
  return web_contents->GetLastCommittedURL() == url;
}

}  // namespace

// When loading a new page to replace an old page in the history list, make sure
// that the browser and renderer agree, and that both get it right.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       CorrectLengthWithCurrentItemReplacement) {
  NavigationController& controller =
      shell()->web_contents()->GetController();

  EXPECT_TRUE(NavigateToURL(shell(), GURL("data:text/html,page1")));
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(1, RendererHistoryLength(shell()));

  EXPECT_TRUE(NavigateToURLAndReplace(shell(), GURL("data:text/html,page1a")));
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(1, RendererHistoryLength(shell()));

  // Now create two more entries and go back, to test replacing an entry without
  // pruning the forward history.
  EXPECT_TRUE(NavigateToURL(shell(), GURL("data:text/html,page2")));
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(2, RendererHistoryLength(shell()));

  EXPECT_TRUE(NavigateToURL(shell(), GURL("data:text/html,page3")));
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(3, RendererHistoryLength(shell()));

  controller.GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  controller.GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_TRUE(controller.CanGoForward());

  EXPECT_TRUE(NavigateToURLAndReplace(shell(), GURL("data:text/html,page1b")));
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(3, RendererHistoryLength(shell()));
  EXPECT_TRUE(controller.CanGoForward());

  // Note that there's no way to access the renderer's notion of the history
  // offset via JavaScript. Checking just the history length, though, is enough;
  // if the replacement failed, there would be a new history entry and thus an
  // incorrect length.
}

// When spawning a new page from a WebUI page, make sure that the browser and
// renderer agree about the length of the history list, and that both get it
// right.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       CorrectLengthWithNewTabNavigatingFromWebUI) {
  GURL web_ui_page(std::string(kChromeUIScheme) + "://" +
                   std::string(kChromeUIGpuHost));
  EXPECT_TRUE(NavigateToURL(shell(), web_ui_page));
  EXPECT_EQ(BINDINGS_POLICY_WEB_UI,
      shell()->web_contents()->GetRenderViewHost()->GetEnabledBindings());

  ShellAddedObserver observer;
  std::string page_url = embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html").spec();
  EXPECT_TRUE(
      ExecuteScript(shell(), "window.open('" + page_url + "', '_blank')"));
  Shell* shell2 = observer.GetShell();
  EXPECT_TRUE(WaitForLoadStop(shell2->web_contents()));

  EXPECT_EQ(1, shell2->web_contents()->GetController().GetEntryCount());
  EXPECT_EQ(1, RendererHistoryLength(shell2));

  // Again, as above, there's no way to access the renderer's notion of the
  // history offset via JavaScript. Checking just the history length, again,
  // will have to suffice.
}

// Check that we will not trigger a DCHECK in renderer for cross-process
// replacement navigations.
// See https://crbug.com/611679.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       PageIDUpdatedOnPageReplacement) {
  NavigationController& controller = shell()->web_contents()->GetController();
  const GURL page_url = embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html");

  // Use data scheme first so that the next page will be loaded
  // in a separate site instance.
  EXPECT_TRUE(NavigateToURL(shell(), GURL("data:text/html,page1")));
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_NE(-1, shell()->web_contents()->GetMaxPageID());

  // Now navigate and replace the current entry.
  NavigateToURLAndReplace(shell(), page_url);
  EXPECT_EQ(1, controller.GetEntryCount());

  // Page ID should be updated.
  EXPECT_NE(-1, shell()->web_contents()->GetMaxPageID());

  // Reload the page and verify that we don't hit
  // a DCHECK in |RenderFrameImpl::NavigateInternal|.
  controller.Reload(false);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // DCHECK shouldn't be triggered and we should have a valid page ID.
  EXPECT_NE(-1, shell()->web_contents()->GetMaxPageID());
  EXPECT_TRUE(shell()->web_contents()->GetMainFrame()->IsRenderFrameLive());
}

namespace {

class NoNavigationsObserver : public WebContentsObserver {
 public:
  // Observes navigation for the specified |web_contents|.
  explicit NoNavigationsObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

 private:
  void DidNavigateAnyFrame(RenderFrameHost* render_frame_host,
                           const LoadCommittedDetails& details,
                           const FrameNavigateParams& params) override {
    FAIL() << "No navigations should occur";
  }
};

class FrameNavigateParamsCapturer : public WebContentsObserver {
 public:
  // Observes navigation for the specified |node|.
  explicit FrameNavigateParamsCapturer(FrameTreeNode* node)
      : WebContentsObserver(
            node->current_frame_host()->delegate()->GetAsWebContents()),
        frame_tree_node_id_(node->frame_tree_node_id()),
        navigations_remaining_(1),
        wait_for_load_(true),
        message_loop_runner_(new MessageLoopRunner) {}

  void set_navigations_remaining(int count) {
    navigations_remaining_ = count;
  }

  void set_wait_for_load(bool ignore) {
    wait_for_load_ = ignore;
  }

  void Wait() {
    message_loop_runner_->Run();
  }

  const FrameNavigateParams& params() const {
    EXPECT_EQ(1U, params_.size());
    return params_[0];
  }

  const std::vector<FrameNavigateParams>& all_params() const {
    return params_;
  }

  const LoadCommittedDetails& details() const {
    EXPECT_EQ(1U, details_.size());
    return details_[0];
  }

  const std::vector<LoadCommittedDetails>& all_details() const {
    return details_;
  }

 private:
  void DidNavigateAnyFrame(RenderFrameHost* render_frame_host,
                           const LoadCommittedDetails& details,
                           const FrameNavigateParams& params) override {
    RenderFrameHostImpl* rfh =
        static_cast<RenderFrameHostImpl*>(render_frame_host);
    if (rfh->frame_tree_node()->frame_tree_node_id() != frame_tree_node_id_)
      return;

    --navigations_remaining_;
    params_.push_back(params);
    details_.push_back(details);
    if (!navigations_remaining_ &&
        (!web_contents()->IsLoading() || !wait_for_load_))
      message_loop_runner_->Quit();
  }

  void DidStopLoading() override {
    if (!navigations_remaining_)
      message_loop_runner_->Quit();
  }

  // The id of the FrameTreeNode whose navigations to observe.
  int frame_tree_node_id_;

  // How many navigations remain to capture.
  int navigations_remaining_;

  // Whether to also wait for the load to complete.
  bool wait_for_load_;

  // The params of the navigations.
  std::vector<FrameNavigateParams> params_;

  // The details of the navigations.
  std::vector<LoadCommittedDetails> details_;

  // The MessageLoopRunner used to spin the message loop.
  scoped_refptr<MessageLoopRunner> message_loop_runner_;
};

class LoadCommittedCapturer : public WebContentsObserver {
 public:
  // Observes the load commit for the specified |node|.
  explicit LoadCommittedCapturer(FrameTreeNode* node)
      : WebContentsObserver(
            node->current_frame_host()->delegate()->GetAsWebContents()),
        frame_tree_node_id_(node->frame_tree_node_id()),
        message_loop_runner_(new MessageLoopRunner) {}

  // Observes the load commit for the next created frame in the specified
  // |web_contents|.
  explicit LoadCommittedCapturer(WebContents* web_contents)
      : WebContentsObserver(web_contents),
        frame_tree_node_id_(0),
        message_loop_runner_(new MessageLoopRunner) {}

  void Wait() {
    message_loop_runner_->Run();
  }

  ui::PageTransition transition_type() const {
    return transition_type_;
  }

 private:
  void RenderFrameCreated(RenderFrameHost* render_frame_host) override {
    RenderFrameHostImpl* rfh =
        static_cast<RenderFrameHostImpl*>(render_frame_host);

    // Don't pay attention to swapped out RenderFrameHosts in the main frame.
    // TODO(nasko): Remove once swappedout:// is gone.
    // See https://crbug.com/357747.
    if (!rfh->is_active()) {
      DLOG(INFO) << "Skipping swapped out RFH: "
                 << rfh->GetSiteInstance()->GetSiteURL();
      return;
    }

    // If this object was not created with a specified frame tree node, then use
    // the first created active RenderFrameHost.  Once a node is selected, there
    // shouldn't be any other frames being created.
    int frame_tree_node_id = rfh->frame_tree_node()->frame_tree_node_id();
    DCHECK(frame_tree_node_id_ == 0 ||
           frame_tree_node_id_ == frame_tree_node_id);
    frame_tree_node_id_ = frame_tree_node_id;
  }

  void DidCommitProvisionalLoadForFrame(
      RenderFrameHost* render_frame_host,
      const GURL& url,
      ui::PageTransition transition_type) override {
    DCHECK_NE(0, frame_tree_node_id_);
    RenderFrameHostImpl* rfh =
        static_cast<RenderFrameHostImpl*>(render_frame_host);
    if (rfh->frame_tree_node()->frame_tree_node_id() != frame_tree_node_id_)
      return;

    transition_type_ = transition_type;
    if (!web_contents()->IsLoading())
      message_loop_runner_->Quit();
  }

  void DidStopLoading() override { message_loop_runner_->Quit(); }

  // The id of the FrameTreeNode whose navigations to observe.
  int frame_tree_node_id_;

  // The transition_type of the last navigation.
  ui::PageTransition transition_type_;

  // The MessageLoopRunner used to spin the message loop.
  scoped_refptr<MessageLoopRunner> message_loop_runner_;
};

}  // namespace

// Some pages create a popup, then write an iframe into it. This causes a
// subframe navigation without having any committed entry. Such navigations
// just get thrown on the ground, but we shouldn't crash.
//
// This test actually hits NAVIGATION_TYPE_NAV_IGNORE four times. Two of them,
// the initial window.open() and the iframe creation, don't try to create
// navigation entries, and the third and fourth, the new navigations, try to.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest, SubframeOnEmptyPage) {
  // Navigate to a page to force the renderer process to start.
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  // Pop open a new window with no last committed entry.
  ShellAddedObserver new_shell_observer;
  {
    std::string script = "window.open()";
    EXPECT_TRUE(ExecuteScript(root, script));
  }
  Shell* new_shell = new_shell_observer.GetShell();
  ASSERT_NE(new_shell->web_contents(), shell()->web_contents());
  FrameTreeNode* new_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetFrameTree()
          ->root();
  EXPECT_FALSE(
      new_shell->web_contents()->GetController().GetLastCommittedEntry());

  // Make a new iframe in it.
  NoNavigationsObserver observer(new_shell->web_contents());
  {
    LoadCommittedCapturer capturer(new_shell->web_contents());
    std::string script = "var iframe = document.createElement('iframe');"
                         "iframe.src = 'data:text/html,<p>some page</p>';"
                         "document.body.appendChild(iframe);";
    EXPECT_TRUE(ExecuteScript(new_root, script));
    capturer.Wait();
  }
  ASSERT_EQ(1U, new_root->child_count());
  ASSERT_NE(nullptr, new_root->child_at(0));

  // Navigate it cross-site.
  GURL frame_url = embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_2.html");
  {
    LoadCommittedCapturer capturer(new_shell->web_contents());
    std::string script = "location.assign('" + frame_url.spec() + "')";
    EXPECT_TRUE(ExecuteScript(new_root->child_at(0), script));
    capturer.Wait();
  }

  // Success is not crashing, and not navigating.
  EXPECT_EQ(nullptr,
            new_shell->web_contents()->GetController().GetLastCommittedEntry());

  // A nested iframe with a cross-site URL should also be able to commit.
  GURL grandchild_url(embedded_test_server()->GetURL(
      "bar.com", "/navigation_controller/simple_page_1.html"));
  {
    LoadCommittedCapturer capturer(new_shell->web_contents());
    std::string script = "var iframe = document.createElement('iframe');"
                         "iframe.src = '" + grandchild_url.spec() + "';"
                         "document.body.appendChild(iframe);";
    EXPECT_TRUE(ExecuteScript(new_root->child_at(0), script));
    capturer.Wait();
  }
  ASSERT_EQ(1U, new_root->child_at(0)->child_count());
  EXPECT_EQ(grandchild_url, new_root->child_at(0)->child_at(0)->current_url());
}

// Test that the renderer is not killed after an auto subframe navigation if the
// main frame appears to change its origin due to a document.write on an
// about:blank page.  See https://crbug.com/613732.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       OriginChangeAfterDocumentWrite) {
  GURL url1 = embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html");
  EXPECT_TRUE(NavigateToURL(shell(), url1));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  // Pop open a new window to about:blank.
  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(ExecuteScript(root, "var w = window.open('about:blank')"));
  Shell* new_shell = new_shell_observer.GetShell();
  ASSERT_NE(new_shell->web_contents(), shell()->web_contents());
  FrameTreeNode* new_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetFrameTree()
          ->root();
  GURL blank_url(url::kAboutBlankURL);
  EXPECT_EQ(blank_url, new_root->current_url());

  // Make a new iframe in it using document.write from the opener.
  {
    LoadCommittedCapturer capturer(new_shell->web_contents());
    std::string script = "w.document.write("
                         "\"<iframe src='" + url1.spec() + "'></iframe>\");"
                         "w.document.close();";
    EXPECT_TRUE(ExecuteScript(root->current_frame_host(), script));
    capturer.Wait();
  }
  ASSERT_EQ(1U, new_root->child_count());
  EXPECT_EQ(blank_url, new_root->current_url());
  EXPECT_EQ(url1, new_root->child_at(0)->current_url());

  // Navigate the subframe.
  GURL url2 = embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html");
  {
    LoadCommittedCapturer capturer(new_root->child_at(0));
    std::string script = "location.href = '" + url2.spec() + "';";
    EXPECT_TRUE(ExecuteScript(new_root->child_at(0), script));
    capturer.Wait();
  }
  EXPECT_EQ(blank_url, new_root->current_url());
  EXPECT_EQ(url2, new_root->child_at(0)->current_url());
  EXPECT_EQ(2, new_shell->web_contents()->GetController().GetEntryCount());

  // Do a replace state in the main frame, which changes the URL from
  // about:blank to the opener's origin, due to the document.write() call.
  {
    LoadCommittedCapturer capturer(new_root);
    std::string script = "history.replaceState({}, 'foo', 'foo');";
    EXPECT_TRUE(ExecuteScript(new_root, script));
    capturer.Wait();
  }
  EXPECT_EQ(embedded_test_server()->GetURL("/navigation_controller/foo"),
            new_root->current_url());
  EXPECT_EQ(url2, new_root->child_at(0)->current_url());

  // Go back in the subframe.  Note that the main frame's URL looks like a
  // cross-origin change from a web URL to about:blank.
  {
    TestNavigationObserver observer(new_shell->web_contents(), 1);
    new_shell->web_contents()->GetController().GoBack();
    observer.Wait();
  }
  EXPECT_TRUE(new_root->current_frame_host()->IsRenderFrameLive());

  // Go forward in the subframe.  Note that the main frame's URL looks like a
  // cross-origin change from about:blank to a web URL.
  {
    TestNavigationObserver observer(new_shell->web_contents(), 1);
    new_shell->web_contents()->GetController().GoForward();
    observer.Wait();
  }
  EXPECT_TRUE(new_root->current_frame_host()->IsRenderFrameLive());
}

IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       ErrorPageReplacement) {
  NavigationController& controller = shell()->web_contents()->GetController();
  GURL error_url(
      net::URLRequestFailedJob::GetMockHttpUrl(net::ERR_CONNECTION_RESET));
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&net::URLRequestFailedJob::AddUrlHandler));

  NavigateToURL(shell(), GURL(url::kAboutBlankURL));
  EXPECT_EQ(1, controller.GetEntryCount());

  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();

  // Navigate to a page that fails to load. It must result in an error page, the
  // NEW_PAGE navigation type, and an addition to the history list.
  {
    FrameNavigateParamsCapturer capturer(root);
    NavigateFrameToURL(root, error_url);
    capturer.Wait();
    EXPECT_EQ(NAVIGATION_TYPE_NEW_PAGE, capturer.details().type);
    NavigationEntry* entry = controller.GetLastCommittedEntry();
    EXPECT_EQ(PAGE_TYPE_ERROR, entry->GetPageType());
    EXPECT_EQ(2, controller.GetEntryCount());
  }

  // Navigate again to the page that fails to load. It must result in an error
  // page, the EXISTING_PAGE navigation type, and no addition to the history
  // list. We do not use SAME_PAGE here; that case only differs in that it
  // clears the pending entry, and there is no pending entry after a load
  // failure.
  {
    FrameNavigateParamsCapturer capturer(root);
    NavigateFrameToURL(root, error_url);
    capturer.Wait();
    EXPECT_EQ(NAVIGATION_TYPE_EXISTING_PAGE, capturer.details().type);
    NavigationEntry* entry = controller.GetLastCommittedEntry();
    EXPECT_EQ(PAGE_TYPE_ERROR, entry->GetPageType());
    EXPECT_EQ(2, controller.GetEntryCount());
  }

  // Make a new entry ...
  NavigateToURL(shell(), GURL(url::kAboutBlankURL));
  EXPECT_EQ(3, controller.GetEntryCount());

  // ... and replace it with a failed load.
  // TODO(creis): Make this be NEW_PAGE along with the other location.replace
  // cases.  There isn't much impact to having this be EXISTING_PAGE for now.
  // See https://crbug.com/596707.
  {
    FrameNavigateParamsCapturer capturer(root);
    NavigateToURLAndReplace(shell(), error_url);
    capturer.Wait();
    EXPECT_EQ(NAVIGATION_TYPE_EXISTING_PAGE, capturer.details().type);
    NavigationEntry* entry = controller.GetLastCommittedEntry();
    EXPECT_EQ(PAGE_TYPE_ERROR, entry->GetPageType());
    EXPECT_EQ(3, controller.GetEntryCount());
  }

  // Make a new web ui page to force a process swap ...
  GURL web_ui_page(std::string(kChromeUIScheme) + "://" +
                   std::string(kChromeUIGpuHost));
  NavigateToURL(shell(), web_ui_page);
  EXPECT_EQ(4, controller.GetEntryCount());

  // ... and replace it with a failed load. (It is NEW_PAGE for the reason noted
  // above.)
  {
    FrameNavigateParamsCapturer capturer(root);
    NavigateToURLAndReplace(shell(), error_url);
    capturer.Wait();
    EXPECT_EQ(NAVIGATION_TYPE_NEW_PAGE, capturer.details().type);
    NavigationEntry* entry = controller.GetLastCommittedEntry();
    EXPECT_EQ(PAGE_TYPE_ERROR, entry->GetPageType());
    EXPECT_EQ(4, controller.GetEntryCount());
  }
}

// Various tests for navigation type classifications. TODO(avi): It's rather
// bogus that the same info is in two different enums; http://crbug.com/453555.

// Verify that navigations for NAVIGATION_TYPE_NEW_PAGE are correctly
// classified.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       NavigationTypeClassification_NewPage) {
  NavigateToURL(shell(), GURL(url::kAboutBlankURL));

  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();

  {
    // Simple load.
    FrameNavigateParamsCapturer capturer(root);
    GURL frame_url(embedded_test_server()->GetURL(
        "/navigation_controller/page_with_links.html"));
    NavigateFrameToURL(root, frame_url);
    capturer.Wait();
    // TODO(avi,creis): Why is this (and quite a few others below) a "link"
    // transition? Lots of these transitions should be cleaned up.
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.params().transition, ui::PAGE_TRANSITION_LINK));
    EXPECT_EQ(NAVIGATION_TYPE_NEW_PAGE, capturer.details().type);
    EXPECT_FALSE(capturer.details().is_in_page);
  }

  {
    // Load via a fragment link click.
    FrameNavigateParamsCapturer capturer(root);
    std::string script = "document.getElementById('fraglink').click()";
    EXPECT_TRUE(ExecuteScript(root, script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.params().transition, ui::PAGE_TRANSITION_LINK));
    EXPECT_EQ(NAVIGATION_TYPE_NEW_PAGE, capturer.details().type);
    EXPECT_TRUE(capturer.details().is_in_page);
  }

  {
    // Load via link click.
    FrameNavigateParamsCapturer capturer(root);
    std::string script = "document.getElementById('thelink').click()";
    EXPECT_TRUE(ExecuteScript(root, script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.params().transition, ui::PAGE_TRANSITION_LINK));
    EXPECT_EQ(NAVIGATION_TYPE_NEW_PAGE, capturer.details().type);
    EXPECT_FALSE(capturer.details().is_in_page);
  }

  {
    // location.assign().
    FrameNavigateParamsCapturer capturer(root);
    GURL frame_url(embedded_test_server()->GetURL(
        "/navigation_controller/simple_page_2.html"));
    std::string script = "location.assign('" + frame_url.spec() + "')";
    EXPECT_TRUE(ExecuteScript(root, script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.params().transition,
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                  ui::PAGE_TRANSITION_CLIENT_REDIRECT)));
    EXPECT_EQ(NAVIGATION_TYPE_NEW_PAGE, capturer.details().type);
    EXPECT_FALSE(capturer.details().is_in_page);
  }

  {
    // history.pushState().
    FrameNavigateParamsCapturer capturer(root);
    std::string script =
        "history.pushState({}, 'page 1', 'simple_page_1.html')";
    EXPECT_TRUE(ExecuteScript(root, script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.params().transition,
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                  ui::PAGE_TRANSITION_CLIENT_REDIRECT)));
    EXPECT_EQ(NAVIGATION_TYPE_NEW_PAGE, capturer.details().type);
    EXPECT_TRUE(capturer.details().is_in_page);
  }

  if (AreAllSitesIsolatedForTesting()) {
    // Cross-process location.replace().
    FrameNavigateParamsCapturer capturer(root);
    GURL frame_url(embedded_test_server()->GetURL(
        "foo.com", "/navigation_controller/simple_page_1.html"));
    std::string script = "location.replace('" + frame_url.spec() + "')";
    EXPECT_TRUE(ExecuteScript(root, script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.params().transition,
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                  ui::PAGE_TRANSITION_CLIENT_REDIRECT)));
    EXPECT_EQ(NAVIGATION_TYPE_NEW_PAGE, capturer.details().type);
    EXPECT_FALSE(capturer.details().is_in_page);
  }
}

// Verify that navigations for NAVIGATION_TYPE_EXISTING_PAGE are correctly
// classified.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       NavigationTypeClassification_ExistingPage) {
  GURL url1(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  NavigateToURL(shell(), url1);
  GURL url2(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));
  NavigateToURL(shell(), url2);

  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();

  {
    // Back from the browser side.
    FrameNavigateParamsCapturer capturer(root);
    shell()->web_contents()->GetController().GoBack();
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.params().transition,
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                  ui::PAGE_TRANSITION_FORWARD_BACK |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)));
    EXPECT_EQ(NAVIGATION_TYPE_EXISTING_PAGE, capturer.details().type);
    EXPECT_FALSE(capturer.details().is_in_page);
  }

  {
    // Forward from the browser side.
    FrameNavigateParamsCapturer capturer(root);
    shell()->web_contents()->GetController().GoForward();
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.params().transition,
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                  ui::PAGE_TRANSITION_FORWARD_BACK |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)));
    EXPECT_EQ(NAVIGATION_TYPE_EXISTING_PAGE, capturer.details().type);
    EXPECT_FALSE(capturer.details().is_in_page);
  }

  {
    // Back from the renderer side.
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(ExecuteScript(root, "history.back()"));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.params().transition,
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                  ui::PAGE_TRANSITION_FORWARD_BACK |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)));
    EXPECT_EQ(NAVIGATION_TYPE_EXISTING_PAGE, capturer.details().type);
    EXPECT_FALSE(capturer.details().is_in_page);
  }

  {
    // Forward from the renderer side.
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(ExecuteScript(root, "history.forward()"));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.params().transition,
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                  ui::PAGE_TRANSITION_FORWARD_BACK |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)));
    EXPECT_EQ(NAVIGATION_TYPE_EXISTING_PAGE, capturer.details().type);
    EXPECT_FALSE(capturer.details().is_in_page);
  }

  {
    // Back from the renderer side via history.go().
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(ExecuteScript(root, "history.go(-1)"));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.params().transition,
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                  ui::PAGE_TRANSITION_FORWARD_BACK |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)));
    EXPECT_EQ(NAVIGATION_TYPE_EXISTING_PAGE, capturer.details().type);
    EXPECT_FALSE(capturer.details().is_in_page);
  }

  {
    // Forward from the renderer side via history.go().
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(ExecuteScript(root, "history.go(1)"));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.params().transition,
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                  ui::PAGE_TRANSITION_FORWARD_BACK |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)));
    EXPECT_EQ(NAVIGATION_TYPE_EXISTING_PAGE, capturer.details().type);
    EXPECT_FALSE(capturer.details().is_in_page);
  }

  {
    // Reload from the browser side.
    FrameNavigateParamsCapturer capturer(root);
    shell()->web_contents()->GetController().Reload(false);
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.params().transition, ui::PAGE_TRANSITION_RELOAD));
    EXPECT_EQ(NAVIGATION_TYPE_EXISTING_PAGE, capturer.details().type);
    EXPECT_FALSE(capturer.details().is_in_page);
  }

  {
    // Reload from the renderer side.
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(ExecuteScript(root, "location.reload()"));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.params().transition,
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                  ui::PAGE_TRANSITION_CLIENT_REDIRECT)));
    EXPECT_EQ(NAVIGATION_TYPE_EXISTING_PAGE, capturer.details().type);
    EXPECT_FALSE(capturer.details().is_in_page);
  }

  {
    // location.replace().
    // TODO(creis): Change this to be NEW_PAGE with replacement in
    // https://crbug.com/596707.
    FrameNavigateParamsCapturer capturer(root);
    GURL frame_url(embedded_test_server()->GetURL(
        "/navigation_controller/simple_page_1.html"));
    std::string script = "location.replace('" + frame_url.spec() + "')";
    EXPECT_TRUE(ExecuteScript(root, script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.params().transition,
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                  ui::PAGE_TRANSITION_CLIENT_REDIRECT)));
    EXPECT_EQ(NAVIGATION_TYPE_EXISTING_PAGE, capturer.details().type);
    EXPECT_FALSE(capturer.details().is_in_page);
  }

  // Now, various in-page navigations.

  {
    // history.replaceState().
    FrameNavigateParamsCapturer capturer(root);
    std::string script =
        "history.replaceState({}, 'page 2', 'simple_page_2.html')";
    EXPECT_TRUE(ExecuteScript(root, script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.params().transition,
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                  ui::PAGE_TRANSITION_CLIENT_REDIRECT)));
    EXPECT_EQ(NAVIGATION_TYPE_EXISTING_PAGE, capturer.details().type);
    EXPECT_TRUE(capturer.details().is_in_page);
  }

  // Back and forward across a fragment navigation.

  GURL url_links(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_links.html"));
  NavigateToURL(shell(), url_links);
  std::string script = "document.getElementById('fraglink').click()";
  EXPECT_TRUE(ExecuteScript(root, script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  {
    // Back.
    FrameNavigateParamsCapturer capturer(root);
    shell()->web_contents()->GetController().GoBack();
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.params().transition,
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                  ui::PAGE_TRANSITION_FORWARD_BACK |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)));
    EXPECT_EQ(NAVIGATION_TYPE_EXISTING_PAGE, capturer.details().type);
    EXPECT_TRUE(capturer.details().is_in_page);
  }

  {
    // Forward.
    FrameNavigateParamsCapturer capturer(root);
    shell()->web_contents()->GetController().GoForward();
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.params().transition,
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                  ui::PAGE_TRANSITION_FORWARD_BACK)));
    EXPECT_EQ(NAVIGATION_TYPE_EXISTING_PAGE, capturer.details().type);
    EXPECT_TRUE(capturer.details().is_in_page);
  }

  // Back and forward across a pushState-created navigation.

  NavigateToURL(shell(), url1);
  script = "history.pushState({}, 'page 2', 'simple_page_2.html')";
  EXPECT_TRUE(ExecuteScript(root, script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  {
    // Back.
    FrameNavigateParamsCapturer capturer(root);
    shell()->web_contents()->GetController().GoBack();
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.params().transition,
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                  ui::PAGE_TRANSITION_FORWARD_BACK |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)));
    EXPECT_EQ(NAVIGATION_TYPE_EXISTING_PAGE, capturer.details().type);
    EXPECT_TRUE(capturer.details().is_in_page);
  }

  {
    // Forward.
    FrameNavigateParamsCapturer capturer(root);
    shell()->web_contents()->GetController().GoForward();
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.params().transition,
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                  ui::PAGE_TRANSITION_FORWARD_BACK)));
    EXPECT_EQ(NAVIGATION_TYPE_EXISTING_PAGE, capturer.details().type);
    EXPECT_TRUE(capturer.details().is_in_page);
  }
}

// Verify that navigations for NAVIGATION_TYPE_SAME_PAGE are correctly
// classified.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       NavigationTypeClassification_SamePage) {
  GURL url1(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  NavigateToURL(shell(), url1);

  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();

  {
    // Simple load.
    FrameNavigateParamsCapturer capturer(root);
    GURL frame_url(embedded_test_server()->GetURL(
        "/navigation_controller/simple_page_1.html"));
    NavigateFrameToURL(root, frame_url);
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.params().transition, ui::PAGE_TRANSITION_LINK));
    EXPECT_EQ(NAVIGATION_TYPE_SAME_PAGE, capturer.details().type);
  }
}

// Verify that empty GURL navigations are not classified as SAME_PAGE.
// See https://crbug.com/534980.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       NavigationTypeClassification_EmptyGURL) {
  GURL url1(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  NavigateToURL(shell(), url1);

  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();

  {
    // Load an (invalid) empty GURL.  Blink will treat this as an inert commit,
    // but we don't want it to show up as SAME_PAGE.
    FrameNavigateParamsCapturer capturer(root);
    NavigateFrameToURL(root, GURL());
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.params().transition, ui::PAGE_TRANSITION_LINK));
    EXPECT_EQ(NAVIGATION_TYPE_NEW_PAGE, capturer.details().type);
  }
}

// Verify that navigations for NAVIGATION_TYPE_NEW_SUBFRAME and
// NAVIGATION_TYPE_AUTO_SUBFRAME are properly classified.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       NavigationTypeClassification_NewAndAutoSubframe) {
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_iframe.html"));
  NavigateToURL(shell(), main_url);

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();

  ASSERT_EQ(1U, root->child_count());
  ASSERT_NE(nullptr, root->child_at(0));

  {
    // Initial load.
    LoadCommittedCapturer capturer(root->child_at(0));
    GURL frame_url(embedded_test_server()->GetURL(
        "/navigation_controller/simple_page_1.html"));
    NavigateFrameToURL(root->child_at(0), frame_url);
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  {
    // Simple load.
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    GURL frame_url(embedded_test_server()->GetURL(
        "/navigation_controller/simple_page_2.html"));
    NavigateFrameToURL(root->child_at(0), frame_url);
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.params().transition, ui::PAGE_TRANSITION_MANUAL_SUBFRAME));
    EXPECT_EQ(NAVIGATION_TYPE_NEW_SUBFRAME, capturer.details().type);
  }

  {
    // Back.
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    shell()->web_contents()->GetController().GoBack();
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.params().transition, ui::PAGE_TRANSITION_AUTO_SUBFRAME));
    EXPECT_EQ(NAVIGATION_TYPE_AUTO_SUBFRAME, capturer.details().type);
  }

  {
    // Forward.
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    shell()->web_contents()->GetController().GoForward();
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.params().transition, ui::PAGE_TRANSITION_AUTO_SUBFRAME));
    EXPECT_EQ(NAVIGATION_TYPE_AUTO_SUBFRAME, capturer.details().type);
  }

  {
    // Simple load.
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    GURL frame_url(embedded_test_server()->GetURL(
        "/navigation_controller/page_with_links.html"));
    NavigateFrameToURL(root->child_at(0), frame_url);
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.params().transition, ui::PAGE_TRANSITION_MANUAL_SUBFRAME));
    EXPECT_EQ(NAVIGATION_TYPE_NEW_SUBFRAME, capturer.details().type);
  }

  {
    // Load via a fragment link click.
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    std::string script = "document.getElementById('fraglink').click()";
    EXPECT_TRUE(ExecuteScript(root->child_at(0), script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.params().transition, ui::PAGE_TRANSITION_MANUAL_SUBFRAME));
    EXPECT_EQ(NAVIGATION_TYPE_NEW_SUBFRAME, capturer.details().type);
  }

  {
    // location.assign().
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    GURL frame_url(embedded_test_server()->GetURL(
        "/navigation_controller/simple_page_1.html"));
    std::string script = "location.assign('" + frame_url.spec() + "')";
    EXPECT_TRUE(ExecuteScript(root->child_at(0), script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.params().transition, ui::PAGE_TRANSITION_MANUAL_SUBFRAME));
    EXPECT_EQ(NAVIGATION_TYPE_NEW_SUBFRAME, capturer.details().type);
  }

  {
    // location.replace().
    LoadCommittedCapturer capturer(root->child_at(0));
    GURL frame_url(embedded_test_server()->GetURL(
        "/navigation_controller/simple_page_2.html"));
    std::string script = "location.replace('" + frame_url.spec() + "')";
    EXPECT_TRUE(ExecuteScript(root->child_at(0), script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  {
    // history.pushState().
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    std::string script =
        "history.pushState({}, 'page 1', 'simple_page_1.html')";
    EXPECT_TRUE(ExecuteScript(root->child_at(0), script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.params().transition, ui::PAGE_TRANSITION_MANUAL_SUBFRAME));
    EXPECT_EQ(NAVIGATION_TYPE_NEW_SUBFRAME, capturer.details().type);
  }

  {
    // history.replaceState().
    LoadCommittedCapturer capturer(root->child_at(0));
    std::string script =
        "history.replaceState({}, 'page 2', 'simple_page_2.html')";
    EXPECT_TRUE(ExecuteScript(root->child_at(0), script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  {
    // Reload.
    LoadCommittedCapturer capturer(root->child_at(0));
    EXPECT_TRUE(ExecuteScript(root->child_at(0), "location.reload()"));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  {
    // Create an iframe.
    LoadCommittedCapturer capturer(shell()->web_contents());
    GURL frame_url(embedded_test_server()->GetURL(
        "/navigation_controller/simple_page_1.html"));
    std::string script = "var iframe = document.createElement('iframe');"
                         "iframe.src = '" + frame_url.spec() + "';"
                         "document.body.appendChild(iframe);";
    EXPECT_TRUE(ExecuteScript(root, script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }
}

// Verify that navigations caused by client-side redirects are correctly
// classified.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       NavigationTypeClassification_ClientSideRedirect) {
  NavigateToURL(shell(), GURL(url::kAboutBlankURL));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();

  {
    // Load the redirecting page.
    FrameNavigateParamsCapturer capturer(root);
    capturer.set_navigations_remaining(2);
    GURL frame_url(embedded_test_server()->GetURL(
        "/navigation_controller/client_redirect.html"));
    NavigateFrameToURL(root, frame_url);
    capturer.Wait();

    std::vector<FrameNavigateParams> params = capturer.all_params();
    std::vector<LoadCommittedDetails> details = capturer.all_details();
    ASSERT_EQ(2U, params.size());
    ASSERT_EQ(2U, details.size());
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        params[0].transition, ui::PAGE_TRANSITION_LINK));
    EXPECT_EQ(NAVIGATION_TYPE_NEW_PAGE, details[0].type);
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        params[1].transition,
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                  ui::PAGE_TRANSITION_CLIENT_REDIRECT)));
    EXPECT_EQ(NAVIGATION_TYPE_EXISTING_PAGE, details[1].type);
  }
}

// Verify that the LoadCommittedDetails::is_in_page value is properly set for
// non-IN_PAGE navigations. (It's tested for IN_PAGE navigations with the
// NavigationTypeClassification_InPage test.)
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       LoadCommittedDetails_IsInPage) {
  GURL links_url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_links.html"));
  NavigateToURL(shell(), links_url);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();

  {
    // Do a fragment link click.
    FrameNavigateParamsCapturer capturer(root);
    std::string script = "document.getElementById('fraglink').click()";
    EXPECT_TRUE(ExecuteScript(root, script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.params().transition, ui::PAGE_TRANSITION_LINK));
    EXPECT_EQ(NAVIGATION_TYPE_NEW_PAGE, capturer.details().type);
    EXPECT_TRUE(capturer.details().is_in_page);
  }

  {
    // Do a non-fragment link click.
    FrameNavigateParamsCapturer capturer(root);
    std::string script = "document.getElementById('thelink').click()";
    EXPECT_TRUE(ExecuteScript(root, script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.params().transition, ui::PAGE_TRANSITION_LINK));
    EXPECT_EQ(NAVIGATION_TYPE_NEW_PAGE, capturer.details().type);
    EXPECT_FALSE(capturer.details().is_in_page);
  }

  // Second verse, same as the first. (But in a subframe.)

  GURL iframe_url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_iframe.html"));
  NavigateToURL(shell(), iframe_url);

  root = static_cast<WebContentsImpl*>(shell()->web_contents())->
      GetFrameTree()->root();

  ASSERT_EQ(1U, root->child_count());
  ASSERT_NE(nullptr, root->child_at(0));

  NavigateFrameToURL(root->child_at(0), links_url);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  {
    // Do a fragment link click.
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    std::string script = "document.getElementById('fraglink').click()";
    EXPECT_TRUE(ExecuteScript(root->child_at(0), script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.params().transition, ui::PAGE_TRANSITION_MANUAL_SUBFRAME));
    EXPECT_EQ(NAVIGATION_TYPE_NEW_SUBFRAME, capturer.details().type);
    EXPECT_TRUE(capturer.details().is_in_page);
  }

  {
    // Do a non-fragment link click.
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    std::string script = "document.getElementById('thelink').click()";
    EXPECT_TRUE(ExecuteScript(root->child_at(0), script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.params().transition, ui::PAGE_TRANSITION_MANUAL_SUBFRAME));
    EXPECT_EQ(NAVIGATION_TYPE_NEW_SUBFRAME, capturer.details().type);
    EXPECT_FALSE(capturer.details().is_in_page);
  }
}

// Verify the tree of FrameNavigationEntries after initial about:blank commits
// in subframes, which should not count as real committed loads.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       FrameNavigationEntry_BlankAutoSubframe) {
  GURL about_blank_url(url::kAboutBlankURL);
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  NavigateToURL(shell(), main_url);
  const NavigationControllerImpl& controller =
      static_cast<const NavigationControllerImpl&>(
          shell()->web_contents()->GetController());
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();

  // 1. Create a iframe with no URL.
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    std::string script = "var iframe = document.createElement('iframe');"
                         "document.body.appendChild(iframe);";
    EXPECT_TRUE(ExecuteScript(root, script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  // Check last committed NavigationEntry.
  EXPECT_EQ(1, controller.GetEntryCount());
  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(main_url, entry->GetURL());
  FrameNavigationEntry* root_entry = entry->root_node()->frame_entry.get();
  EXPECT_EQ(main_url, root_entry->url());

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // The entry should now have one blank subframe FrameNavigationEntry, but
    // this does not count as committing a real load.
    ASSERT_EQ(1U, entry->root_node()->children.size());
    FrameNavigationEntry* frame_entry =
        entry->root_node()->children[0]->frame_entry.get();
    EXPECT_EQ(about_blank_url, frame_entry->url());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry->root_node()->children.size());
  }
  EXPECT_FALSE(root->child_at(0)->has_committed_real_load());

  // 1a. A nested iframe with no URL should also create a subframe entry but not
  // count as a real load.
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    std::string script = "var iframe = document.createElement('iframe');"
                         "document.body.appendChild(iframe);";
    EXPECT_TRUE(ExecuteScript(root->child_at(0), script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // The nested entry should have one blank subframe FrameNavigationEntry, but
    // this does not count as committing a real load.
    ASSERT_EQ(1U, entry->root_node()->children[0]->children.size());
    FrameNavigationEntry* frame_entry =
        entry->root_node()->children[0]->children[0]->frame_entry.get();
    EXPECT_EQ(about_blank_url, frame_entry->url());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry->root_node()->children.size());
  }
  EXPECT_FALSE(root->child_at(0)->child_at(0)->has_committed_real_load());

  // 2. Create another iframe with an explicit about:blank URL.
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    std::string script = "var iframe = document.createElement('iframe');"
                         "iframe.src = 'about:blank';"
                         "document.body.appendChild(iframe);";
    EXPECT_TRUE(ExecuteScript(root, script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  // Check last committed NavigationEntry.
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(entry, controller.GetLastCommittedEntry());

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // The new entry should have one blank subframe FrameNavigationEntry, but
    // this does not count as committing a real load.
    ASSERT_EQ(2U, entry->root_node()->children.size());
    FrameNavigationEntry* frame_entry =
        entry->root_node()->children[1]->frame_entry.get();
    EXPECT_EQ(about_blank_url, frame_entry->url());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry->root_node()->children.size());
  }
  EXPECT_FALSE(root->child_at(1)->has_committed_real_load());

  // 3. A real same-site navigation in the nested iframe should be AUTO.
  GURL frame_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  {
    LoadCommittedCapturer capturer(root->child_at(0)->child_at(0));
    std::string script = "var frames = document.getElementsByTagName('iframe');"
                         "frames[0].src = '" + frame_url.spec() + "';";
    EXPECT_TRUE(ExecuteScript(root->child_at(0), script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  // Check last committed NavigationEntry.  It should have replaced the previous
  // frame entry in the original NavigationEntry.
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(entry, controller.GetLastCommittedEntry());

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // The entry should still have one nested subframe FrameNavigationEntry.
    ASSERT_EQ(1U, entry->root_node()->children[0]->children.size());
    FrameNavigationEntry* frame_entry =
        entry->root_node()->children[0]->children[0]->frame_entry.get();
    EXPECT_EQ(frame_url, frame_entry->url());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry->root_node()->children.size());
  }
  EXPECT_FALSE(root->child_at(0)->has_committed_real_load());
  EXPECT_TRUE(root->child_at(0)->child_at(0)->has_committed_real_load());
  EXPECT_FALSE(root->child_at(1)->has_committed_real_load());

  // 4. A real cross-site navigation in the second iframe should be AUTO.
  GURL foo_url(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_2.html"));
  {
    LoadCommittedCapturer capturer(root->child_at(1));
    std::string script = "var frames = document.getElementsByTagName('iframe');"
                         "frames[1].src = '" + foo_url.spec() + "';";
    EXPECT_TRUE(ExecuteScript(root, script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  // Check last committed NavigationEntry.
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(entry, controller.GetLastCommittedEntry());

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // The entry should still have two subframe FrameNavigationEntries.
    ASSERT_EQ(2U, entry->root_node()->children.size());
    FrameNavigationEntry* frame_entry =
        entry->root_node()->children[1]->frame_entry.get();
    EXPECT_EQ(foo_url, frame_entry->url());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry->root_node()->children.size());
  }
  EXPECT_FALSE(root->child_at(0)->has_committed_real_load());
  EXPECT_TRUE(root->child_at(0)->child_at(0)->has_committed_real_load());
  EXPECT_TRUE(root->child_at(1)->has_committed_real_load());

  // 5. A new navigation to about:blank in the nested frame should count as a
  // real load, since that frame has already committed a real load and this is
  // not the initial blank page.
  {
    LoadCommittedCapturer capturer(root->child_at(0)->child_at(0));
    std::string script = "var frames = document.getElementsByTagName('iframe');"
                         "frames[0].src = 'about:blank';";
    EXPECT_TRUE(ExecuteScript(root->child_at(0), script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_MANUAL_SUBFRAME));
  }

  // This should have created a new NavigationEntry.
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_NE(entry, controller.GetLastCommittedEntry());
  NavigationEntryImpl* entry2 = controller.GetLastCommittedEntry();

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    ASSERT_EQ(2U, entry->root_node()->children.size());
    FrameNavigationEntry* frame_entry =
        entry2->root_node()->children[0]->children[0]->frame_entry.get();
    EXPECT_EQ(about_blank_url, frame_entry->url());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry->root_node()->children.size());
  }
  EXPECT_FALSE(root->child_at(0)->has_committed_real_load());
  EXPECT_TRUE(root->child_at(0)->child_at(0)->has_committed_real_load());
  EXPECT_TRUE(root->child_at(1)->has_committed_real_load());

  // Check the end result of the frame tree.
  if (AreAllSitesIsolatedForTesting()) {
    FrameTreeVisualizer visualizer;
    EXPECT_EQ(
        " Site A ------------ proxies for B\n"
        "   |--Site A ------- proxies for B\n"
        "   |    +--Site A -- proxies for B\n"
        "   +--Site B ------- proxies for A\n"
        "Where A = http://127.0.0.1/\n"
        "      B = http://foo.com/",
        visualizer.DepictFrameTree(root));
  }
}

// Verify the tree of FrameNavigationEntries when a nested iframe commits inside
// the initial blank page of a loading iframe.  Prevents regression of
// https://crbug.com/600743.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       FrameNavigationEntry_SlowNestedAutoSubframe) {
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  // 1. Create a iframe with a URL that doesn't commit.
  GURL slow_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));
  TestNavigationManager subframe_delayer(shell()->web_contents(), slow_url);
  {
    std::string script = "var iframe = document.createElement('iframe');"
                         "iframe.src = '" + slow_url.spec() + "';"
                         "document.body.appendChild(iframe);";
    EXPECT_TRUE(ExecuteScript(root, script));
  }
  subframe_delayer.WaitForWillStartRequest();

  // Stop the request so that we can wait for load stop below, without ending up
  // with a commit for this frame.
  shell()->web_contents()->Stop();

  // 2. A nested iframe with a cross-site URL should be able to commit.
  GURL foo_url(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_1.html"));
  {
    std::string script = "var iframe = document.createElement('iframe');"
                         "iframe.src = '" + foo_url.spec() + "';"
                         "document.body.appendChild(iframe);";
    EXPECT_TRUE(ExecuteScript(root->child_at(0), script));
    WaitForLoadStopWithoutSuccessCheck(shell()->web_contents());
  }

  // TODO(creis): Check subframe entries once we create them in this case.
  // See https://crbug.com/608402.
  EXPECT_EQ(foo_url, root->child_at(0)->child_at(0)->current_url());
}

// Verify the tree of FrameNavigationEntries when a nested iframe commits inside
// the initial blank page of an iframe with no committed entry.  Prevents
// regression of https://crbug.com/600743.
// Flaky test: See https://crbug.com/610801
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       DISABLED_FrameNavigationEntry_NoCommitNestedAutoSubframe) {
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  // 1. Create a iframe with a URL that doesn't commit.
  GURL no_commit_url(embedded_test_server()->GetURL("/nocontent"));
  {
    std::string script = "var iframe = document.createElement('iframe');"
                         "iframe.src = '" + no_commit_url.spec() + "';"
                         "document.body.appendChild(iframe);";
    EXPECT_TRUE(ExecuteScript(root, script));
  }
  EXPECT_EQ(GURL(), root->child_at(0)->current_url());

  // 2. A nested iframe with a cross-site URL should be able to commit.
  GURL foo_url(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_1.html"));
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    std::string script = "var iframe = document.createElement('iframe');"
                         "iframe.src = '" + foo_url.spec() + "';"
                         "document.body.appendChild(iframe);";
    EXPECT_TRUE(ExecuteScript(root->child_at(0), script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  // TODO(creis): Check subframe entries once we create them in this case.
  // See https://crbug.com/608402.
  EXPECT_EQ(foo_url, root->child_at(0)->child_at(0)->current_url());
}

// Verify the tree of FrameNavigationEntries when a nested iframe commits after
// going back in-page, in which case its parent might not have been in the
// NavigationEntry.  Prevents regression of https://crbug.com/600743.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       FrameNavigationEntry_BackNestedAutoSubframe) {
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  // 1. Navigate in-page.
  {
    FrameNavigateParamsCapturer capturer(root);
    std::string script = "history.pushState({}, 'foo', 'foo')";
    EXPECT_TRUE(ExecuteScript(root, script));
    capturer.Wait();
    EXPECT_EQ(NAVIGATION_TYPE_NEW_PAGE, capturer.details().type);
    EXPECT_TRUE(capturer.details().is_in_page);
  }

  // 2. Create an iframe.
  GURL child_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    std::string script = "var iframe = document.createElement('iframe');"
                         "iframe.src = '" + child_url.spec() + "';"
                         "document.body.appendChild(iframe);";
    EXPECT_TRUE(ExecuteScript(root, script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  // 3. Go back in-page.
  {
    TestNavigationObserver back_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_load_observer.Wait();
  }

  // 4. A nested iframe with a cross-site URL should be able to commit.
  GURL grandchild_url(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_1.html"));
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    std::string script = "var iframe = document.createElement('iframe');"
                         "iframe.src = '" + grandchild_url.spec() + "';"
                         "document.body.appendChild(iframe);";
    EXPECT_TRUE(ExecuteScript(root->child_at(0), script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  // TODO(creis): Check subframe entries once we create them in this case.
  // See https://crbug.com/608402.
  EXPECT_EQ(grandchild_url, root->child_at(0)->child_at(0)->current_url());
}

// Verify the tree of FrameNavigationEntries when a nested iframe commits after
// its parent changes its name, in which case we might not find the parent
// FrameNavigationEntry.  Prevents regression of https://crbug.com/600743.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       FrameNavigationEntry_RenameNestedAutoSubframe) {
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  // 1. Create an iframe.
  GURL child_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    std::string script = "var iframe = document.createElement('iframe');"
                         "iframe.src = '" + child_url.spec() + "';"
                         "document.body.appendChild(iframe);";
    EXPECT_TRUE(ExecuteScript(root, script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  // 2. Change the iframe's name.
  EXPECT_TRUE(ExecuteScript(root->child_at(0), "window.name = 'foo';"));

  // 3. A nested iframe with a cross-site URL should be able to commit.
  GURL bar_url(embedded_test_server()->GetURL(
      "bar.com", "/navigation_controller/simple_page_1.html"));
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    std::string script = "var iframe = document.createElement('iframe');"
                         "iframe.src = '" + bar_url.spec() + "';"
                         "document.body.appendChild(iframe);";
    EXPECT_TRUE(ExecuteScript(root->child_at(0), script));

    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  // TODO(creis): Check subframe entries once we create them in this case.
  // See https://crbug.com/608402.
  EXPECT_EQ(bar_url, root->child_at(0)->child_at(0)->current_url());
}

// Verify the tree of FrameNavigationEntries after NAVIGATION_TYPE_AUTO_SUBFRAME
// commits.
// TODO(creis): Test updating entries for history auto subframe navigations.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       FrameNavigationEntry_AutoSubframe) {
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  NavigateToURL(shell(), main_url);
  const NavigationControllerImpl& controller =
      static_cast<const NavigationControllerImpl&>(
          shell()->web_contents()->GetController());
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();

  // 1. Create a same-site iframe.
  GURL frame_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    std::string script = "var iframe = document.createElement('iframe');"
                         "iframe.src = '" + frame_url.spec() + "';"
                         "document.body.appendChild(iframe);";
    EXPECT_TRUE(ExecuteScript(root, script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  // Check last committed NavigationEntry.
  EXPECT_EQ(1, controller.GetEntryCount());
  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(main_url, entry->GetURL());
  FrameNavigationEntry* root_entry = entry->root_node()->frame_entry.get();
  EXPECT_EQ(main_url, root_entry->url());
  EXPECT_FALSE(controller.GetPendingEntry());

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // The entry should now have a subframe FrameNavigationEntry.
    ASSERT_EQ(1U, entry->root_node()->children.size());
    FrameNavigationEntry* frame_entry =
        entry->root_node()->children[0]->frame_entry.get();
    EXPECT_EQ(frame_url, frame_entry->url());
    EXPECT_TRUE(root->child_at(0)->has_committed_real_load());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry->root_node()->children.size());
  }

  // 2. Create a second, initially cross-site iframe.
  GURL foo_url(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_1.html"));
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    std::string script = "var iframe = document.createElement('iframe');"
                         "iframe.src = '" + foo_url.spec() + "';"
                         "document.body.appendChild(iframe);";
    EXPECT_TRUE(ExecuteScript(root, script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  // The last committed NavigationEntry shouldn't have changed.
  EXPECT_EQ(1, controller.GetEntryCount());
  entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(main_url, entry->GetURL());
  root_entry = entry->root_node()->frame_entry.get();
  EXPECT_EQ(main_url, root_entry->url());
  EXPECT_FALSE(controller.GetPendingEntry());

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // The entry should now have 2 subframe FrameNavigationEntries.
    ASSERT_EQ(2U, entry->root_node()->children.size());
    FrameNavigationEntry* frame_entry =
        entry->root_node()->children[1]->frame_entry.get();
    EXPECT_EQ(foo_url, frame_entry->url());
    EXPECT_TRUE(root->child_at(1)->has_committed_real_load());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry->root_node()->children.size());
  }

  // 3. Create a nested iframe in the second subframe.
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    std::string script = "var iframe = document.createElement('iframe');"
                         "iframe.src = '" + foo_url.spec() + "';"
                         "document.body.appendChild(iframe);";
    EXPECT_TRUE(ExecuteScript(root->child_at(1), script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  // The last committed NavigationEntry shouldn't have changed.
  EXPECT_EQ(1, controller.GetEntryCount());
  entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(main_url, entry->GetURL());
  root_entry = entry->root_node()->frame_entry.get();
  EXPECT_EQ(main_url, root_entry->url());

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // The entry should now have 2 subframe FrameNavigationEntries.
    ASSERT_EQ(2U, entry->root_node()->children.size());
    ASSERT_EQ(1U, entry->root_node()->children[1]->children.size());
    FrameNavigationEntry* frame_entry =
        entry->root_node()->children[1]->children[0]->frame_entry.get();
    EXPECT_EQ(foo_url, frame_entry->url());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry->root_node()->children.size());
  }

  // 4. Create a third iframe on the same site as the second.  This ensures that
  // the commit type is correct even when the subframe process already exists.
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    std::string script = "var iframe = document.createElement('iframe');"
                         "iframe.src = '" + foo_url.spec() + "';"
                         "document.body.appendChild(iframe);";
    EXPECT_TRUE(ExecuteScript(root, script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  // The last committed NavigationEntry shouldn't have changed.
  EXPECT_EQ(1, controller.GetEntryCount());
  entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(main_url, entry->GetURL());
  root_entry = entry->root_node()->frame_entry.get();
  EXPECT_EQ(main_url, root_entry->url());

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // The entry should now have 3 subframe FrameNavigationEntries.
    ASSERT_EQ(3U, entry->root_node()->children.size());
    FrameNavigationEntry* frame_entry =
        entry->root_node()->children[2]->frame_entry.get();
    EXPECT_EQ(foo_url, frame_entry->url());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry->root_node()->children.size());
  }

  // 5. Create a nested iframe on the original site (A-B-A).
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    std::string script = "var iframe = document.createElement('iframe');"
                         "iframe.src = '" + frame_url.spec() + "';"
                         "document.body.appendChild(iframe);";
    FrameTreeNode* child = root->child_at(2);
    EXPECT_TRUE(ExecuteScript(child, script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  // The last committed NavigationEntry shouldn't have changed.
  EXPECT_EQ(1, controller.GetEntryCount());
  entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(main_url, entry->GetURL());
  root_entry = entry->root_node()->frame_entry.get();
  EXPECT_EQ(main_url, root_entry->url());

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // There should be a corresponding FrameNavigationEntry.
    ASSERT_EQ(1U, entry->root_node()->children[2]->children.size());
    FrameNavigationEntry* frame_entry =
        entry->root_node()->children[2]->children[0]->frame_entry.get();
    EXPECT_EQ(frame_url, frame_entry->url());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry->root_node()->children.size());
  }

  // Check the end result of the frame tree.
  if (AreAllSitesIsolatedForTesting()) {
    FrameTreeVisualizer visualizer;
    EXPECT_EQ(
        " Site A ------------ proxies for B\n"
        "   |--Site A ------- proxies for B\n"
        "   |--Site B ------- proxies for A\n"
        "   |    +--Site B -- proxies for A\n"
        "   +--Site B ------- proxies for A\n"
        "        +--Site A -- proxies for B\n"
        "Where A = http://127.0.0.1/\n"
        "      B = http://foo.com/",
        visualizer.DepictFrameTree(root));
  }
}

// Verify the tree of FrameNavigationEntries after NAVIGATION_TYPE_NEW_SUBFRAME
// commits.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       FrameNavigationEntry_NewSubframe) {
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  NavigateToURL(shell(), main_url);
  const NavigationControllerImpl& controller =
      static_cast<const NavigationControllerImpl&>(
          shell()->web_contents()->GetController());
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();

  // 1. Create a same-site iframe.
  GURL frame_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    std::string script = "var iframe = document.createElement('iframe');"
                         "iframe.src = '" + frame_url.spec() + "';"
                         "document.body.appendChild(iframe);";
    EXPECT_TRUE(ExecuteScript(root, script));
    capturer.Wait();
  }
  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();

  // 2. Navigate in the subframe same-site.
  GURL frame_url2(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_links.html"));
  {
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    NavigateFrameToURL(root->child_at(0), frame_url2);
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.params().transition, ui::PAGE_TRANSITION_MANUAL_SUBFRAME));
    EXPECT_EQ(NAVIGATION_TYPE_NEW_SUBFRAME, capturer.details().type);
  }

  // We should have created a new NavigationEntry with the same main frame URL.
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry2 = controller.GetLastCommittedEntry();
  EXPECT_NE(entry, entry2);
  EXPECT_EQ(main_url, entry2->GetURL());
  FrameNavigationEntry* root_entry2 = entry2->root_node()->frame_entry.get();
  EXPECT_EQ(main_url, root_entry2->url());

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // The entry should have a new FrameNavigationEntries for the subframe.
    ASSERT_EQ(1U, entry2->root_node()->children.size());
    EXPECT_EQ(frame_url2, entry2->root_node()->children[0]->frame_entry->url());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry2->root_node()->children.size());
  }

  // 3. Create a second, initially cross-site iframe.
  GURL foo_url(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_1.html"));
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    std::string script = "var iframe = document.createElement('iframe');"
                         "iframe.src = '" + foo_url.spec() + "';"
                         "document.body.appendChild(iframe);";
    EXPECT_TRUE(ExecuteScript(root, script));
    capturer.Wait();
  }

  // 4. Create a nested same-site iframe in the second subframe, wait for it to
  // commit, then navigate it again.
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    std::string script = "var iframe = document.createElement('iframe');"
                         "iframe.src = '" + foo_url.spec() + "';"
                         "document.body.appendChild(iframe);";
    EXPECT_TRUE(ExecuteScript(root->child_at(1), script));
    capturer.Wait();
  }
  GURL bar_url(embedded_test_server()->GetURL(
      "bar.com", "/navigation_controller/simple_page_1.html"));
  {
    FrameNavigateParamsCapturer capturer(root->child_at(1)->child_at(0));
    NavigateFrameToURL(root->child_at(1)->child_at(0), bar_url);
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.params().transition, ui::PAGE_TRANSITION_MANUAL_SUBFRAME));
    EXPECT_EQ(NAVIGATION_TYPE_NEW_SUBFRAME, capturer.details().type);
  }

  // We should have created a new NavigationEntry with the same main frame URL.
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry3 = controller.GetLastCommittedEntry();
  EXPECT_NE(entry, entry3);
  EXPECT_EQ(main_url, entry3->GetURL());
  FrameNavigationEntry* root_entry3 = entry3->root_node()->frame_entry.get();
  EXPECT_EQ(main_url, root_entry3->url());

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // The entry should still have FrameNavigationEntries for all 3 subframes.
    ASSERT_EQ(2U, entry3->root_node()->children.size());
    EXPECT_EQ(frame_url2, entry3->root_node()->children[0]->frame_entry->url());
    EXPECT_EQ(foo_url, entry3->root_node()->children[1]->frame_entry->url());
    ASSERT_EQ(1U, entry3->root_node()->children[1]->children.size());
    EXPECT_EQ(
        bar_url,
        entry3->root_node()->children[1]->children[0]->frame_entry->url());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry3->root_node()->children.size());
  }

  // 6. Navigate the second subframe cross-site, clearing its existing subtree.
  GURL baz_url(embedded_test_server()->GetURL(
      "baz.com", "/navigation_controller/simple_page_1.html"));
  {
    FrameNavigateParamsCapturer capturer(root->child_at(1));
    std::string script = "var frames = document.getElementsByTagName('iframe');"
                         "frames[1].src = '" + baz_url.spec() + "';";
    EXPECT_TRUE(ExecuteScript(root, script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.params().transition, ui::PAGE_TRANSITION_MANUAL_SUBFRAME));
    EXPECT_EQ(NAVIGATION_TYPE_NEW_SUBFRAME, capturer.details().type);
  }

  // We should have created a new NavigationEntry with the same main frame URL.
  EXPECT_EQ(4, controller.GetEntryCount());
  EXPECT_EQ(3, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry4 = controller.GetLastCommittedEntry();
  EXPECT_NE(entry, entry4);
  EXPECT_EQ(main_url, entry4->GetURL());
  FrameNavigationEntry* root_entry4 = entry4->root_node()->frame_entry.get();
  EXPECT_EQ(main_url, root_entry4->url());

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // The entry should still have FrameNavigationEntries for all 3 subframes.
    ASSERT_EQ(2U, entry4->root_node()->children.size());
    EXPECT_EQ(frame_url2, entry4->root_node()->children[0]->frame_entry->url());
    EXPECT_EQ(baz_url, entry4->root_node()->children[1]->frame_entry->url());
    ASSERT_EQ(0U, entry4->root_node()->children[1]->children.size());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry4->root_node()->children.size());
  }

  // Check the end result of the frame tree.
  if (AreAllSitesIsolatedForTesting()) {
    FrameTreeVisualizer visualizer;
    EXPECT_EQ(
        " Site A ------------ proxies for B\n"
        "   |--Site A ------- proxies for B\n"
        "   +--Site B ------- proxies for A\n"
        "Where A = http://127.0.0.1/\n"
        "      B = http://baz.com/",
        visualizer.DepictFrameTree(root));
  }
}

// Ensure that we don't crash when navigating subframes after in-page
// navigations.  See https://crbug.com/522193.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       FrameNavigationEntry_SubframeAfterInPage) {
  // 1. Start on a page with a subframe.
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_iframe.html"));
  NavigateToURL(shell(), main_url);
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();

  ASSERT_EQ(1U, root->child_count());
  ASSERT_NE(nullptr, root->child_at(0));

  // Navigate to a real page in the subframe, so that the next navigation will
  // be MANUAL_SUBFRAME.
  GURL subframe_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  {
    LoadCommittedCapturer capturer(root->child_at(0));
    NavigateFrameToURL(root->child_at(0), subframe_url);
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  // 2. In-page navigation in the main frame.
  std::string push_script = "history.pushState({}, 'page 2', 'page_2.html')";
  EXPECT_TRUE(ExecuteScript(root, push_script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // TODO(creis): Verify subframe entries.  https://crbug.com/522193.

  // 3. Add a nested subframe.
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    std::string script = "var iframe = document.createElement('iframe');"
                         "iframe.src = '" + subframe_url.spec() + "';"
                         "document.body.appendChild(iframe);";
    EXPECT_TRUE(ExecuteScript(root->child_at(0), script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  // TODO(creis): Verify subframe entries.  https://crbug.com/522193.
}

// Verify the tree of FrameNavigationEntries after back/forward navigations in a
// cross-site subframe.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       FrameNavigationEntry_SubframeBackForward) {
  GURL main_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  NavigateToURL(shell(), main_url);
  const NavigationControllerImpl& controller =
      static_cast<const NavigationControllerImpl&>(
          shell()->web_contents()->GetController());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  // 1. Create a same-site iframe.
  GURL frame_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    std::string script = "var iframe = document.createElement('iframe');"
                         "iframe.src = '" + frame_url.spec() + "';"
                         "document.body.appendChild(iframe);";
    EXPECT_TRUE(ExecuteScript(root, script));
    capturer.Wait();
  }
  NavigationEntryImpl* entry1 = controller.GetLastCommittedEntry();

  // 2. Navigate in the subframe cross-site.
  GURL frame_url2(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/page_with_links.html"));
  {
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    NavigateFrameToURL(root->child_at(0), frame_url2);
    capturer.Wait();
  }
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry2 = controller.GetLastCommittedEntry();

  // 3. Navigate in the subframe cross-site again.
  GURL frame_url3(embedded_test_server()->GetURL(
      "bar.com", "/navigation_controller/page_with_links.html"));
  {
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    NavigateFrameToURL(root->child_at(0), frame_url3);
    capturer.Wait();
  }
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry3 = controller.GetLastCommittedEntry();

  // 4. Go back in the subframe.
  {
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    shell()->web_contents()->GetController().GoBack();
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.params().transition, ui::PAGE_TRANSITION_AUTO_SUBFRAME));
    EXPECT_EQ(NAVIGATION_TYPE_AUTO_SUBFRAME, capturer.details().type);
  }
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(entry2, controller.GetLastCommittedEntry());

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // The entry should have a FrameNavigationEntry for the subframe.
    ASSERT_EQ(1U, entry2->root_node()->children.size());
    EXPECT_EQ(frame_url2, entry2->root_node()->children[0]->frame_entry->url());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry2->root_node()->children.size());
  }

  // 5. Go back in the subframe again to the parent page's site.
  {
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    shell()->web_contents()->GetController().GoBack();
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.params().transition, ui::PAGE_TRANSITION_AUTO_SUBFRAME));
    EXPECT_EQ(NAVIGATION_TYPE_AUTO_SUBFRAME, capturer.details().type);
  }
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(entry1, controller.GetLastCommittedEntry());

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // The entry should have a FrameNavigationEntry for the subframe.
    ASSERT_EQ(1U, entry1->root_node()->children.size());
    EXPECT_EQ(frame_url, entry1->root_node()->children[0]->frame_entry->url());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry1->root_node()->children.size());
  }

  // 6. Go forward in the subframe cross-site.
  {
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    shell()->web_contents()->GetController().GoForward();
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.params().transition, ui::PAGE_TRANSITION_AUTO_SUBFRAME));
    EXPECT_EQ(NAVIGATION_TYPE_AUTO_SUBFRAME, capturer.details().type);
  }
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(entry2, controller.GetLastCommittedEntry());

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // The entry should have a FrameNavigationEntry for the subframe.
    ASSERT_EQ(1U, entry2->root_node()->children.size());
    EXPECT_EQ(frame_url2, entry2->root_node()->children[0]->frame_entry->url());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry2->root_node()->children.size());
  }

  // 7. Go forward in the subframe again, cross-site.
  {
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    shell()->web_contents()->GetController().GoForward();
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.params().transition, ui::PAGE_TRANSITION_AUTO_SUBFRAME));
    EXPECT_EQ(NAVIGATION_TYPE_AUTO_SUBFRAME, capturer.details().type);
  }
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(entry3, controller.GetLastCommittedEntry());

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // The entry should have a FrameNavigationEntry for the subframe.
    ASSERT_EQ(1U, entry3->root_node()->children.size());
    EXPECT_EQ(frame_url3, entry3->root_node()->children[0]->frame_entry->url());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry3->root_node()->children.size());
  }
}

// Verify the tree of FrameNavigationEntries after subframes are recreated in
// history navigations, including nested frames.  The history will look like:
// 1. initial_url
// 2. main_url_a (data_url)
// 3. main_url_a (frame_url_b (data_url))
// 4. main_url_a (frame_url_b (frame_url_c))
// 5. main_url_d
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       FrameNavigationEntry_RecreatedSubframeBackForward) {
  // 1. Start on a page with no frames.
  GURL initial_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  NavigateToURL(shell(), initial_url);
  const NavigationControllerImpl& controller =
      static_cast<const NavigationControllerImpl&>(
          shell()->web_contents()->GetController());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  EXPECT_EQ(initial_url, root->current_url());
  NavigationEntryImpl* entry1 = controller.GetLastCommittedEntry();
  EXPECT_EQ(0U, entry1->root_node()->children.size());

  // 2. Navigate to a page with a data URL iframe.
  GURL main_url_a(embedded_test_server()->GetURL(
      "a.com", "/navigation_controller/page_with_data_iframe.html"));
  GURL data_url("data:text/html,Subframe");
  NavigateToURL(shell(), main_url_a);
  ASSERT_EQ(1U, root->child_count());
  ASSERT_EQ(0U, root->child_at(0)->child_count());
  EXPECT_EQ(main_url_a, root->current_url());
  EXPECT_EQ(data_url, root->child_at(0)->current_url());

  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry2 = controller.GetLastCommittedEntry();

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // The entry should have a FrameNavigationEntry for the data subframe.
    ASSERT_EQ(1U, entry2->root_node()->children.size());
    EXPECT_EQ(data_url, entry2->root_node()->children[0]->frame_entry->url());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry2->root_node()->children.size());
  }

  // 3. Navigate the iframe cross-site to a page with a nested iframe.
  GURL frame_url_b(embedded_test_server()->GetURL(
      "b.com", "/navigation_controller/page_with_data_iframe.html"));
  {
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    NavigateFrameToURL(root->child_at(0), frame_url_b);
    capturer.Wait();
  }
  ASSERT_EQ(1U, root->child_count());
  EXPECT_EQ(main_url_a, root->current_url());
  EXPECT_EQ(frame_url_b, root->child_at(0)->current_url());
  EXPECT_EQ(data_url, root->child_at(0)->child_at(0)->current_url());

  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry3 = controller.GetLastCommittedEntry();

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // The entry should have a FrameNavigationEntry for the b.com subframe.
    ASSERT_EQ(1U, entry3->root_node()->children.size());
    ASSERT_EQ(1U, entry3->root_node()->children[0]->children.size());
    EXPECT_EQ(frame_url_b,
              entry3->root_node()->children[0]->frame_entry->url());
    EXPECT_EQ(
        data_url,
        entry3->root_node()->children[0]->children[0]->frame_entry->url());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry3->root_node()->children.size());
  }

  // 4. Navigate the nested iframe cross-site.
  GURL frame_url_c(embedded_test_server()->GetURL(
      "c.com", "/navigation_controller/simple_page_2.html"));
  {
    FrameNavigateParamsCapturer capturer(root->child_at(0)->child_at(0));
    NavigateFrameToURL(root->child_at(0)->child_at(0), frame_url_c);
    capturer.Wait();
  }
  ASSERT_EQ(1U, root->child_count());
  EXPECT_EQ(main_url_a, root->current_url());
  EXPECT_EQ(frame_url_b, root->child_at(0)->current_url());
  EXPECT_EQ(frame_url_c, root->child_at(0)->child_at(0)->current_url());

  EXPECT_EQ(4, controller.GetEntryCount());
  EXPECT_EQ(3, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry4 = controller.GetLastCommittedEntry();

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // The entry should have FrameNavigationEntries for the subframes.
    ASSERT_EQ(1U, entry4->root_node()->children.size());
    ASSERT_EQ(1U, entry4->root_node()->children[0]->children.size());
    EXPECT_EQ(frame_url_b,
              entry4->root_node()->children[0]->frame_entry->url());
    EXPECT_EQ(
        frame_url_c,
        entry4->root_node()->children[0]->children[0]->frame_entry->url());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry4->root_node()->children.size());
  }

  // 5. Navigate main frame cross-site, destroying the frames.
  GURL main_url_d(embedded_test_server()->GetURL(
      "d.com", "/navigation_controller/simple_page_2.html"));
  NavigateToURL(shell(), main_url_d);
  ASSERT_EQ(0U, root->child_count());
  EXPECT_EQ(main_url_d, root->current_url());

  EXPECT_EQ(5, controller.GetEntryCount());
  EXPECT_EQ(4, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry5 = controller.GetLastCommittedEntry();
  EXPECT_EQ(0U, entry5->root_node()->children.size());

  // 6. Go back, recreating the iframe and its nested iframe.
  {
    TestNavigationObserver back_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_load_observer.Wait();
  }
  ASSERT_EQ(1U, root->child_count());
  ASSERT_EQ(1U, root->child_at(0)->child_count());
  EXPECT_EQ(main_url_a, root->current_url());
  EXPECT_EQ(frame_url_b, root->child_at(0)->current_url());
  EXPECT_EQ(frame_url_c, root->child_at(0)->child_at(0)->current_url());

  EXPECT_EQ(5, controller.GetEntryCount());
  EXPECT_EQ(3, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(entry4, controller.GetLastCommittedEntry());

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // The entry should have FrameNavigationEntries for the subframes.
    ASSERT_EQ(1U, entry4->root_node()->children.size());
    ASSERT_EQ(1U, entry4->root_node()->children[0]->children.size());
    EXPECT_EQ(frame_url_b,
              entry4->root_node()->children[0]->frame_entry->url());
    EXPECT_EQ(
        frame_url_c,
        entry4->root_node()->children[0]->children[0]->frame_entry->url());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry4->root_node()->children.size());
  }

  // Inject a JS value so that we can check for it later.
  EXPECT_TRUE(content::ExecuteScript(root, "foo=3;"));

  // 7. Go back again, to the data URL in the nested iframe.
  {
    TestNavigationObserver back_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_load_observer.Wait();
  }
  ASSERT_EQ(1U, root->child_count());
  ASSERT_EQ(1U, root->child_at(0)->child_count());
  EXPECT_EQ(main_url_a, root->current_url());
  EXPECT_EQ(frame_url_b, root->child_at(0)->current_url());
  EXPECT_EQ(data_url, root->child_at(0)->child_at(0)->current_url());

  EXPECT_EQ(5, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(entry3, controller.GetLastCommittedEntry());

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // The entry should have FrameNavigationEntries for the subframes.
    ASSERT_EQ(1U, entry3->root_node()->children.size());
    ASSERT_EQ(1U, entry3->root_node()->children[0]->children.size());
    EXPECT_EQ(frame_url_b,
              entry3->root_node()->children[0]->frame_entry->url());
    EXPECT_EQ(
        data_url,
        entry3->root_node()->children[0]->children[0]->frame_entry->url());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry3->root_node()->children.size());
  }

  // Verify that we did not reload the main frame. See https://crbug.com/586234.
  {
    int value = 0;
    EXPECT_TRUE(ExecuteScriptAndExtractInt(
        root, "domAutomationController.send(foo)", &value));
    EXPECT_EQ(3, value);
  }

  // 8. Go back again, to the data URL in the first subframe.
  {
    TestNavigationObserver back_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_load_observer.Wait();
  }
  ASSERT_EQ(1U, root->child_count());
  ASSERT_EQ(0U, root->child_at(0)->child_count());
  EXPECT_EQ(main_url_a, root->current_url());
  EXPECT_EQ(data_url, root->child_at(0)->current_url());

  EXPECT_EQ(5, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(entry2, controller.GetLastCommittedEntry());

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // The entry should have a FrameNavigationEntry for the subframe.
    ASSERT_EQ(1U, entry2->root_node()->children.size());
    EXPECT_EQ(data_url, entry2->root_node()->children[0]->frame_entry->url());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry2->root_node()->children.size());
  }

  // 9. Go back again, to the initial main frame page.
  {
    TestNavigationObserver back_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_load_observer.Wait();
  }
  ASSERT_EQ(0U, root->child_count());
  EXPECT_EQ(initial_url, root->current_url());

  EXPECT_EQ(5, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(entry1, controller.GetLastCommittedEntry());
  EXPECT_EQ(0U, entry1->root_node()->children.size());

  // 10. Go forward multiple entries and verify the correct subframe URLs load.
  {
    TestNavigationObserver back_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoToOffset(2);
    back_load_observer.Wait();
  }
  ASSERT_EQ(1U, root->child_count());
  EXPECT_EQ(main_url_a, root->current_url());
  EXPECT_EQ(frame_url_b, root->child_at(0)->current_url());
  EXPECT_EQ(data_url, root->child_at(0)->child_at(0)->current_url());

  EXPECT_EQ(5, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(entry3, controller.GetLastCommittedEntry());

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // The entry should have FrameNavigationEntries for the subframes.
    ASSERT_EQ(1U, entry3->root_node()->children.size());
    EXPECT_EQ(frame_url_b,
              entry3->root_node()->children[0]->frame_entry->url());
    EXPECT_EQ(
        data_url,
        entry3->root_node()->children[0]->children[0]->frame_entry->url());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry3->root_node()->children.size());
  }
}

// Verify that we navigate to the fallback (original) URL if a subframe's
// FrameNavigationEntry can't be found during a history navigation.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       FrameNavigationEntry_SubframeHistoryFallback) {
  // This test only makes sense when subframe FrameNavigationEntries are in use.
  if (!SiteIsolationPolicy::UseSubframeNavigationEntries())
    return;

  // 1. Start on a page with a data URL iframe.
  GURL main_url_a(embedded_test_server()->GetURL(
      "a.com", "/navigation_controller/page_with_data_iframe.html"));
  GURL data_url("data:text/html,Subframe");
  NavigateToURL(shell(), main_url_a);
  const NavigationControllerImpl& controller =
      static_cast<const NavigationControllerImpl&>(
          shell()->web_contents()->GetController());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  ASSERT_EQ(1U, root->child_count());
  ASSERT_EQ(0U, root->child_at(0)->child_count());
  EXPECT_EQ(main_url_a, root->current_url());
  EXPECT_EQ(data_url, root->child_at(0)->current_url());

  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry1 = controller.GetLastCommittedEntry();

  // The entry should have a FrameNavigationEntry for the data subframe.
  ASSERT_EQ(1U, entry1->root_node()->children.size());
  EXPECT_EQ(data_url, entry1->root_node()->children[0]->frame_entry->url());

  // 2. Navigate the iframe cross-site.
  GURL frame_url_b(embedded_test_server()->GetURL(
      "b.com", "/navigation_controller/simple_page_1.html"));
  {
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    NavigateFrameToURL(root->child_at(0), frame_url_b);
    capturer.Wait();
  }
  ASSERT_EQ(1U, root->child_count());
  EXPECT_EQ(main_url_a, root->current_url());
  EXPECT_EQ(frame_url_b, root->child_at(0)->current_url());

  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry2 = controller.GetLastCommittedEntry();

  // The entry should have a FrameNavigationEntry for the b.com subframe.
  ASSERT_EQ(1U, entry2->root_node()->children.size());
  EXPECT_EQ(frame_url_b, entry2->root_node()->children[0]->frame_entry->url());

  // 3. Navigate main frame cross-site, destroying the frames.
  GURL main_url_c(embedded_test_server()->GetURL(
      "c.com", "/navigation_controller/simple_page_2.html"));
  NavigateToURL(shell(), main_url_c);
  ASSERT_EQ(0U, root->child_count());
  EXPECT_EQ(main_url_c, root->current_url());

  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry3 = controller.GetLastCommittedEntry();
  EXPECT_EQ(0U, entry3->root_node()->children.size());

  // Force the subframe entry to have the wrong name, so that it isn't found
  // when we go back.
  entry2->root_node()->children[0]->frame_entry->set_frame_unique_name("wrong");

  // 4. Go back, recreating the iframe. The subframe entry won't be found, and
  // we should fall back to the default URL.
  {
    TestNavigationObserver back_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_load_observer.Wait();
  }
  ASSERT_EQ(1U, root->child_count());
  EXPECT_EQ(main_url_a, root->current_url());
  EXPECT_EQ(data_url, root->child_at(0)->current_url());

  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(entry2, controller.GetLastCommittedEntry());

  // The entry should have both the stale FrameNavigationEntry with the old
  // name and the new FrameNavigationEntry for the fallback navigation.
  ASSERT_EQ(2U, entry2->root_node()->children.size());
  EXPECT_EQ(frame_url_b, entry2->root_node()->children[0]->frame_entry->url());
  EXPECT_EQ(data_url, entry2->root_node()->children[1]->frame_entry->url());
}

// Verify that subframes can be restored in a new NavigationController using the
// PageState of an existing NavigationEntry.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       FrameNavigationEntry_RestoreViaPageState) {
  // 1. Start on a page with a data URL iframe.
  GURL main_url_a(embedded_test_server()->GetURL(
      "a.com", "/navigation_controller/page_with_data_iframe.html"));
  GURL data_url("data:text/html,Subframe");
  NavigateToURL(shell(), main_url_a);
  const NavigationControllerImpl& controller =
      static_cast<const NavigationControllerImpl&>(
          shell()->web_contents()->GetController());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  ASSERT_EQ(1U, root->child_count());
  ASSERT_EQ(0U, root->child_at(0)->child_count());
  EXPECT_EQ(main_url_a, root->current_url());
  EXPECT_EQ(data_url, root->child_at(0)->current_url());

  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry1 = controller.GetLastCommittedEntry();

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // The entry should have a FrameNavigationEntry for the data subframe.
    ASSERT_EQ(1U, entry1->root_node()->children.size());
    EXPECT_EQ(data_url, entry1->root_node()->children[0]->frame_entry->url());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry1->root_node()->children.size());
  }

  // 2. Navigate the iframe cross-site.
  GURL frame_url_b(embedded_test_server()->GetURL(
      "b.com", "/navigation_controller/simple_page_1.html"));
  {
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    NavigateFrameToURL(root->child_at(0), frame_url_b);
    capturer.Wait();
  }
  ASSERT_EQ(1U, root->child_count());
  EXPECT_EQ(main_url_a, root->current_url());
  EXPECT_EQ(frame_url_b, root->child_at(0)->current_url());

  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry2 = controller.GetLastCommittedEntry();

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // The entry should have a FrameNavigationEntry for the b.com subframe.
    ASSERT_EQ(1U, entry2->root_node()->children.size());
    EXPECT_EQ(frame_url_b,
              entry2->root_node()->children[0]->frame_entry->url());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, entry2->root_node()->children.size());
  }

  // 3. Navigate main frame cross-site, destroying the frames.
  GURL main_url_c(embedded_test_server()->GetURL(
      "c.com", "/navigation_controller/simple_page_2.html"));
  NavigateToURL(shell(), main_url_c);
  ASSERT_EQ(0U, root->child_count());
  EXPECT_EQ(main_url_c, root->current_url());

  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry3 = controller.GetLastCommittedEntry();
  EXPECT_EQ(0U, entry3->root_node()->children.size());

  // 4. Create a NavigationEntry with the same PageState as |entry2| and verify
  // it has the same FrameNavigationEntry structure.
  std::unique_ptr<NavigationEntryImpl> restored_entry =
      NavigationEntryImpl::FromNavigationEntry(
          NavigationControllerImpl::CreateNavigationEntry(
              main_url_a, Referrer(), ui::PAGE_TRANSITION_RELOAD, false,
              std::string(), controller.GetBrowserContext()));
  restored_entry->SetPageID(0);
  EXPECT_EQ(0U, restored_entry->root_node()->children.size());
  restored_entry->SetPageState(entry2->GetPageState());

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // The entry should have a FrameNavigationEntry for the b.com subframe.
    EXPECT_EQ(main_url_a, restored_entry->root_node()->frame_entry->url());
    ASSERT_EQ(1U, restored_entry->root_node()->children.size());
    EXPECT_EQ(frame_url_b,
              restored_entry->root_node()->children[0]->frame_entry->url());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, restored_entry->root_node()->children.size());
  }

  // 5. Restore the new entry in a new tab and verify the correct URLs load.
  std::vector<std::unique_ptr<NavigationEntry>> entries;
  entries.push_back(std::move(restored_entry));
  Shell* new_shell = Shell::CreateNewWindow(
      controller.GetBrowserContext(), GURL::EmptyGURL(), nullptr, gfx::Size());
  FrameTreeNode* new_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetFrameTree()
          ->root();
  NavigationControllerImpl& new_controller =
      static_cast<NavigationControllerImpl&>(
          new_shell->web_contents()->GetController());
  new_controller.Restore(
      entries.size() - 1,
      NavigationController::RESTORE_LAST_SESSION_EXITED_CLEANLY, &entries);
  ASSERT_EQ(0u, entries.size());
  {
    TestNavigationObserver restore_observer(new_shell->web_contents());
    new_controller.LoadIfNecessary();
    restore_observer.Wait();
  }
  ASSERT_EQ(1U, new_root->child_count());
  EXPECT_EQ(main_url_a, new_root->current_url());
  EXPECT_EQ(frame_url_b, new_root->child_at(0)->current_url());

  EXPECT_EQ(1, new_controller.GetEntryCount());
  EXPECT_EQ(0, new_controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* new_entry = new_controller.GetLastCommittedEntry();

  // Verify subframe entries if they're enabled (e.g. in --site-per-process).
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    // The entry should have a FrameNavigationEntry for the b.com subframe.
    EXPECT_EQ(main_url_a, new_entry->root_node()->frame_entry->url());
    ASSERT_EQ(1U, new_entry->root_node()->children.size());
    EXPECT_EQ(frame_url_b,
              new_entry->root_node()->children[0]->frame_entry->url());
  } else {
    // There are no subframe FrameNavigationEntries by default.
    EXPECT_EQ(0U, new_entry->root_node()->children.size());
  }
}

// Verifies that the |frame_unique_name| is set to the correct frame, so that we
// can match subframe FrameNavigationEntries to newly created frames after
// back/forward and restore.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       FrameNavigationEntry_FrameUniqueName) {
  const NavigationControllerImpl& controller =
      static_cast<const NavigationControllerImpl&>(
          shell()->web_contents()->GetController());

  // 1. Navigate the main frame.
  GURL url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_links.html"));
  NavigateToURL(shell(), url);
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  SiteInstance* main_site_instance =
      root->current_frame_host()->GetSiteInstance();

  // The main frame defaults to an empty name.
  FrameNavigationEntry* frame_entry =
      controller.GetLastCommittedEntry()->GetFrameEntry(root);
  EXPECT_EQ("", frame_entry->frame_unique_name());

  // Test subframe unique names only if enabled, e.g. in --site-per-process.
  if (!SiteIsolationPolicy::UseSubframeNavigationEntries())
    return;

  // 2. Add an unnamed subframe, which does an AUTO_SUBFRAME navigation.
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    std::string script = "var iframe = document.createElement('iframe');"
                         "iframe.src = '" + url.spec() + "';"
                         "document.body.appendChild(iframe);";
    EXPECT_TRUE(ExecuteScript(root, script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  // The root FrameNavigationEntry hasn't changed.
  EXPECT_EQ(frame_entry,
            controller.GetLastCommittedEntry()->GetFrameEntry(root));

  // The subframe should have a generated name.
  FrameTreeNode* subframe = root->child_at(0);
  EXPECT_EQ(main_site_instance,
            subframe->current_frame_host()->GetSiteInstance());
  FrameNavigationEntry* subframe_entry =
      controller.GetLastCommittedEntry()->GetFrameEntry(subframe);
  std::string unnamed_subframe_name = "<!--framePath //<!--frame0-->-->";
  EXPECT_EQ(unnamed_subframe_name, subframe_entry->frame_unique_name());

  // 3. Add a named subframe.
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    std::string script = "var iframe = document.createElement('iframe');"
                         "iframe.src = '" + url.spec() + "';"
                         "iframe.name = 'foo';"
                         "document.body.appendChild(iframe);";
    EXPECT_TRUE(ExecuteScript(root, script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  // The new subframe should have the specified name.
  EXPECT_EQ(frame_entry,
            controller.GetLastCommittedEntry()->GetFrameEntry(root));
  FrameTreeNode* foo_subframe = root->child_at(1);
  EXPECT_EQ(main_site_instance,
            foo_subframe->current_frame_host()->GetSiteInstance());
  FrameNavigationEntry* foo_subframe_entry =
      controller.GetLastCommittedEntry()->GetFrameEntry(foo_subframe);
  std::string named_subframe_name = "foo";
  EXPECT_EQ(named_subframe_name, foo_subframe_entry->frame_unique_name());

  // 4. Navigating in the subframes cross-process shouldn't change their names.
  // TODO(creis): Fix the unnamed case in https://crbug.com/502317.
  GURL bar_url(embedded_test_server()->GetURL(
      "bar.com", "/navigation_controller/simple_page_1.html"));
  NavigateFrameToURL(foo_subframe, bar_url);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // When run just with subframe navigation entries enabled and not in
  // site-per-process-mode the subframe should be in the same SiteInstance as
  // its parent.
  if (!AreAllSitesIsolatedForTesting()) {
    EXPECT_EQ(main_site_instance,
              foo_subframe->current_frame_host()->GetSiteInstance());
  } else {
    EXPECT_NE(main_site_instance,
              foo_subframe->current_frame_host()->GetSiteInstance());
  }

  foo_subframe_entry =
      controller.GetLastCommittedEntry()->GetFrameEntry(foo_subframe);
  EXPECT_EQ(named_subframe_name, foo_subframe_entry->frame_unique_name());
}

// Ensure we don't crash when cloning a named window.  This happened in
// https://crbug.com/603245 because neither the FrameTreeNode ID nor the name of
// the cloned window matched the root FrameNavigationEntry.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest, CloneNamedWindow) {
  // Start on an initial page.
  GURL url_1(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_1));

  // Name the window.
  EXPECT_TRUE(ExecuteScript(shell(), "window.name = 'foo';"));

  // Navigate it.
  GURL url_2(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_2));

  // Clone the tab and load the page.
  std::unique_ptr<WebContentsImpl> new_tab(
      static_cast<WebContentsImpl*>(shell()->web_contents()->Clone()));
  NavigationController& new_controller = new_tab->GetController();
  EXPECT_TRUE(new_controller.IsInitialNavigation());
  EXPECT_TRUE(new_controller.NeedsReload());
  {
    TestNavigationObserver clone_observer(new_tab.get());
    new_controller.LoadIfNecessary();
    clone_observer.Wait();
  }
}

// Ensure we don't crash when going back in a cloned named window.  This
// happened in https://crbug.com/603245 because neither the FrameTreeNode ID nor
// the name of the cloned window matched the root FrameNavigationEntry.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       CloneAndGoBackWithNamedWindow) {
  // Start on an initial page.
  GURL url_1(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_1));

  // Name the window.
  EXPECT_TRUE(ExecuteScript(shell(), "window.name = 'foo';"));

  // Navigate it.
  GURL url_2(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_2));

  // Clear the name.
  EXPECT_TRUE(ExecuteScript(shell(), "window.name = '';"));

  // Navigate it again.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));

  // Clone the tab and load the page.
  std::unique_ptr<WebContentsImpl> new_tab(
      static_cast<WebContentsImpl*>(shell()->web_contents()->Clone()));
  NavigationController& new_controller = new_tab->GetController();
  EXPECT_TRUE(new_controller.IsInitialNavigation());
  EXPECT_TRUE(new_controller.NeedsReload());
  {
    TestNavigationObserver clone_observer(new_tab.get());
    new_controller.LoadIfNecessary();
    clone_observer.Wait();
  }

  // Go back.
  {
    TestNavigationObserver back_load_observer(new_tab.get());
    new_controller.GoBack();
    back_load_observer.Wait();
  }
}

// Ensures that FrameNavigationEntries for dynamically added iframes can be
// found correctly when cloning them during a transfer.  If we don't look for
// them based on unique name in AddOrUpdateFrameEntry, the FrameTreeNode ID
// mismatch will cause us to create a second FrameNavigationEntry during the
// transfer.  Later, we'll find the wrong FrameNavigationEntry (the earlier one
// from the clone which still has a PageState), and this will cause the renderer
// to crash in NavigateInternal because the PageState is present but the page_id
// is -1 (similar to https://crbug.com/568703).  See https://crbug.com/568768.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       FrameNavigationEntry_RepeatCreatedFrame) {
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  // 1. Navigate the main frame.
  GURL url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_links.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  SiteInstance* main_site_instance =
      root->current_frame_host()->GetSiteInstance();

  // 2. Add a cross-site subframe.
  GURL frame_url(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_1.html"));
  std::string script = "var iframe = document.createElement('iframe');"
                       "iframe.src = '" + frame_url.spec() + "';"
                       "document.body.appendChild(iframe);";
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    EXPECT_TRUE(ExecuteScript(root, script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  FrameTreeNode* subframe = root->child_at(0);
  if (AreAllSitesIsolatedForTesting()) {
    EXPECT_NE(main_site_instance,
              subframe->current_frame_host()->GetSiteInstance());
  }
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    FrameNavigationEntry* subframe_entry =
        controller.GetLastCommittedEntry()->GetFrameEntry(subframe);
    EXPECT_EQ(frame_url, subframe_entry->url());
  }

  // 3. Reload the main frame.
  {
    FrameNavigateParamsCapturer capturer(root);
    controller.Reload(false);
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.params().transition, ui::PAGE_TRANSITION_RELOAD));
    EXPECT_EQ(NAVIGATION_TYPE_EXISTING_PAGE, capturer.details().type);
    EXPECT_FALSE(capturer.details().is_in_page);
  }

  // 4. Add the iframe again.
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    EXPECT_TRUE(ExecuteScript(root, script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }
  if (AreAllSitesIsolatedForTesting()) {
    EXPECT_NE(main_site_instance,
              root->child_at(0)->current_frame_host()->GetSiteInstance());
  }
}

// Verifies that item sequence numbers and document sequence numbers update
// properly for main frames and subframes.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       FrameNavigationEntry_SequenceNumbers) {
  const NavigationControllerImpl& controller =
      static_cast<const NavigationControllerImpl&>(
          shell()->web_contents()->GetController());

  // 1. Navigate the main frame.
  GURL url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_links.html"));
  NavigateToURL(shell(), url);
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  FrameNavigationEntry* frame_entry =
      controller.GetLastCommittedEntry()->GetFrameEntry(root);
  int64_t isn_1 = frame_entry->item_sequence_number();
  int64_t dsn_1 = frame_entry->document_sequence_number();
  EXPECT_NE(-1, isn_1);
  EXPECT_NE(-1, dsn_1);

  // 2. Do an in-page fragment navigation.
  std::string script = "document.getElementById('fraglink').click()";
  EXPECT_TRUE(ExecuteScript(root, script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  frame_entry = controller.GetLastCommittedEntry()->GetFrameEntry(root);
  int64_t isn_2 = frame_entry->item_sequence_number();
  int64_t dsn_2 = frame_entry->document_sequence_number();
  EXPECT_NE(-1, isn_2);
  EXPECT_NE(isn_1, isn_2);
  EXPECT_EQ(dsn_1, dsn_2);

  // Test subframe sequence numbers only if enabled, e.g. in --site-per-process.
  if (!SiteIsolationPolicy::UseSubframeNavigationEntries())
    return;

  // 3. Add a subframe, which does an AUTO_SUBFRAME navigation.
  {
    LoadCommittedCapturer capturer(shell()->web_contents());
    std::string add_script = "var iframe = document.createElement('iframe');"
                             "iframe.src = '" + url.spec() + "';"
                             "document.body.appendChild(iframe);";
    EXPECT_TRUE(ExecuteScript(root, add_script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));
  }

  // The root FrameNavigationEntry hasn't changed.
  EXPECT_EQ(frame_entry,
            controller.GetLastCommittedEntry()->GetFrameEntry(root));

  // We should have a unique ISN and DSN for the subframe entry.
  FrameTreeNode* subframe = root->child_at(0);
  FrameNavigationEntry* subframe_entry =
      controller.GetLastCommittedEntry()->GetFrameEntry(subframe);
  int64_t isn_3 = subframe_entry->item_sequence_number();
  int64_t dsn_3 = subframe_entry->document_sequence_number();
  EXPECT_NE(-1, isn_2);
  EXPECT_NE(isn_2, isn_3);
  EXPECT_NE(dsn_2, dsn_3);

  // 4. Do an in-page fragment navigation in the subframe.
  EXPECT_TRUE(ExecuteScript(subframe, script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  subframe_entry = controller.GetLastCommittedEntry()->GetFrameEntry(subframe);
  int64_t isn_4 = subframe_entry->item_sequence_number();
  int64_t dsn_4 = subframe_entry->document_sequence_number();
  EXPECT_NE(-1, isn_4);
  EXPECT_NE(isn_3, isn_4);
  EXPECT_EQ(dsn_3, dsn_4);
}

// Support a set of tests that isolate only a subset of sites with
// out-of-process iframes (OOPIFs).
class NavigationControllerOopifBrowserTest
    : public NavigationControllerBrowserTest {
 public:
  NavigationControllerOopifBrowserTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Enable the OOPIF framework but only isolate sites from a single TLD.
    command_line->AppendSwitchASCII(switches::kIsolateSitesForTesting, "*.is");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NavigationControllerOopifBrowserTest);
};

// Verify that restoring a NavigationEntry with cross-site subframes does not
// create out-of-process iframes unless the current SiteIsolationPolicy says to.
IN_PROC_BROWSER_TEST_F(NavigationControllerOopifBrowserTest,
                       RestoreWithoutExtraOopifs) {
  // This test requires OOPIFs to be possible.
  EXPECT_TRUE(SiteIsolationPolicy::AreCrossProcessFramesPossible());

  // 1. Start on a page with a data URL iframe.
  GURL main_url_a(embedded_test_server()->GetURL(
      "a.com", "/navigation_controller/page_with_data_iframe.html"));
  GURL data_url("data:text/html,Subframe");
  EXPECT_TRUE(NavigateToURL(shell(), main_url_a));
  const NavigationControllerImpl& controller =
      static_cast<const NavigationControllerImpl&>(
          shell()->web_contents()->GetController());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  EXPECT_EQ(main_url_a, root->current_url());
  EXPECT_EQ(data_url, root->child_at(0)->current_url());

  // 2. Navigate the iframe cross-site.
  GURL frame_url_b(embedded_test_server()->GetURL(
      "b.com", "/navigation_controller/simple_page_1.html"));
  NavigateFrameToURL(root->child_at(0), frame_url_b);
  EXPECT_EQ(main_url_a, root->current_url());
  EXPECT_EQ(frame_url_b, root->child_at(0)->current_url());

  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  NavigationEntryImpl* entry2 = controller.GetLastCommittedEntry();

  // 3. Create a NavigationEntry with the same PageState as |entry2|.
  std::unique_ptr<NavigationEntryImpl> restored_entry =
      NavigationEntryImpl::FromNavigationEntry(
          NavigationControllerImpl::CreateNavigationEntry(
              main_url_a, Referrer(), ui::PAGE_TRANSITION_RELOAD, false,
              std::string(), controller.GetBrowserContext()));
  restored_entry->SetPageID(0);
  EXPECT_EQ(0U, restored_entry->root_node()->children.size());
  restored_entry->SetPageState(entry2->GetPageState());

  // The entry should have no SiteInstance in the FrameNavigationEntry for the
  // b.com subframe.
  EXPECT_FALSE(
      restored_entry->root_node()->children[0]->frame_entry->site_instance());

  // 4. Restore the new entry in a new tab and verify the correct URLs load.
  std::vector<std::unique_ptr<NavigationEntry>> entries;
  entries.push_back(std::move(restored_entry));
  Shell* new_shell = Shell::CreateNewWindow(
      controller.GetBrowserContext(), GURL::EmptyGURL(), nullptr, gfx::Size());
  FrameTreeNode* new_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetFrameTree()
          ->root();
  NavigationControllerImpl& new_controller =
      static_cast<NavigationControllerImpl&>(
          new_shell->web_contents()->GetController());
  new_controller.Restore(
      entries.size() - 1,
      NavigationController::RESTORE_LAST_SESSION_EXITED_CLEANLY, &entries);
  ASSERT_EQ(0u, entries.size());
  {
    TestNavigationObserver restore_observer(new_shell->web_contents());
    new_controller.LoadIfNecessary();
    restore_observer.Wait();
  }
  ASSERT_EQ(1U, new_root->child_count());
  EXPECT_EQ(main_url_a, new_root->current_url());
  EXPECT_EQ(frame_url_b, new_root->child_at(0)->current_url());

  // The subframe should only be in a different SiteInstance if OOPIFs are
  // required for all sites.
  if (AreAllSitesIsolatedForTesting()) {
    EXPECT_NE(new_root->current_frame_host()->GetSiteInstance(),
              new_root->child_at(0)->current_frame_host()->GetSiteInstance());
  } else {
    EXPECT_EQ(new_root->current_frame_host()->GetSiteInstance(),
              new_root->child_at(0)->current_frame_host()->GetSiteInstance());
  }
}

namespace {

// Loads |start_url|, then loads |stalled_url| which stalls. While the page is
// stalled, an in-page navigation happens. Make sure that all the navigations
// are properly classified.
void DoReplaceStateWhilePending(Shell* shell,
                                const GURL& start_url,
                                const GURL& stalled_url,
                                const std::string& replace_state_filename) {
  NavigationControllerImpl& controller =
      static_cast<NavigationControllerImpl&>(
          shell->web_contents()->GetController());

  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell->web_contents())->
          GetFrameTree()->root();

  // Start with one page.
  EXPECT_TRUE(NavigateToURL(shell, start_url));

  // Have the user decide to go to a different page which is very slow.
  NavigationStallDelegate stall_delegate(stalled_url);
  ResourceDispatcherHost::Get()->SetDelegate(&stall_delegate);
  controller.LoadURL(
      stalled_url, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());

  // That should be the pending entry.
  NavigationEntryImpl* entry = controller.GetPendingEntry();
  ASSERT_NE(nullptr, entry);
  EXPECT_EQ(stalled_url, entry->GetURL());

  {
    // Now the existing page uses history.replaceState().
    FrameNavigateParamsCapturer capturer(root);
    capturer.set_wait_for_load(false);
    std::string script =
        "history.replaceState({}, '', '" + replace_state_filename + "')";
    EXPECT_TRUE(ExecuteScript(root, script));
    capturer.Wait();

    // The fact that there was a pending entry shouldn't interfere with the
    // classification.
    EXPECT_EQ(NAVIGATION_TYPE_EXISTING_PAGE, capturer.details().type);
    EXPECT_TRUE(capturer.details().is_in_page);
  }

  ResourceDispatcherHost::Get()->SetDelegate(nullptr);
}

}  // namespace

IN_PROC_BROWSER_TEST_F(
    NavigationControllerBrowserTest,
    NavigationTypeClassification_On1InPageToXWhile2Pending) {
  GURL url1(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  GURL url2(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));
  DoReplaceStateWhilePending(shell(), url1, url2, "x");
}

IN_PROC_BROWSER_TEST_F(
    NavigationControllerBrowserTest,
    NavigationTypeClassification_On1InPageTo2While2Pending) {
  GURL url1(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  GURL url2(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));
  DoReplaceStateWhilePending(shell(), url1, url2, "simple_page_2.html");
}

IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       NavigationTypeClassification_On1InPageToXWhile1Pending) {
  GURL url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  DoReplaceStateWhilePending(shell(), url, url, "x");
}

IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       NavigationTypeClassification_On1InPageTo1While1Pending) {
  GURL url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  DoReplaceStateWhilePending(shell(), url, url, "simple_page_1.html");
}

// Ensure that a pending NavigationEntry for a different navigation doesn't
// cause a commit to be incorrectly treated as a replacement.
// See https://crbug.com/593153.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       OtherCommitDuringPendingEntryWithReplacement) {
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  // Load an initial page.
  GURL start_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));
  int entry_count = controller.GetEntryCount();
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(start_url, controller.GetLastCommittedEntry()->GetURL());

  // Start a cross-process navigation with replacement, which never completes.
  GURL foo_url(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/page_with_links.html"));
  NavigationStallDelegate stall_delegate(foo_url);
  ResourceDispatcherHost::Get()->SetDelegate(&stall_delegate);
  NavigationController::LoadURLParams params(foo_url);
  params.should_replace_current_entry = true;
  controller.LoadURLWithParams(params);

  // That should be the pending entry.
  NavigationEntryImpl* entry = controller.GetPendingEntry();
  ASSERT_NE(nullptr, entry);
  EXPECT_EQ(foo_url, entry->GetURL());
  EXPECT_EQ(entry_count, controller.GetEntryCount());

  {
    // Now the existing page uses history.pushState() while the pending entry
    // for the other navigation still exists.
    FrameNavigateParamsCapturer capturer(root);
    capturer.set_wait_for_load(false);
    std::string script = "history.pushState({}, '', 'pushed')";
    EXPECT_TRUE(ExecuteScript(root, script));
    capturer.Wait();
    EXPECT_EQ(NAVIGATION_TYPE_NEW_PAGE, capturer.details().type);
    EXPECT_TRUE(capturer.details().is_in_page);
  }

  // The in-page navigation should not have replaced the previous entry.
  GURL push_state_url(
      embedded_test_server()->GetURL("/navigation_controller/pushed"));
  EXPECT_EQ(entry_count + 1, controller.GetEntryCount());
  EXPECT_EQ(push_state_url, controller.GetLastCommittedEntry()->GetURL());
  EXPECT_EQ(start_url, controller.GetEntryAtIndex(0)->GetURL());

  ResourceDispatcherHost::Get()->SetDelegate(nullptr);
}

// This test ensures that if we go back from a page that has a replaceState()
// call in the window.beforeunload function, we commit to the proper navigation
// entry. https://crbug.com/597239
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       BackFromPageWithReplaceStateInBeforeUnload) {
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  // Load an initial page.
  GURL start_url(embedded_test_server()->GetURL(
      "/navigation_controller/beforeunload_replacestate_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(start_url, controller.GetLastCommittedEntry()->GetURL());

  // Go to the second page.
  std::string script = "document.getElementById('thelink').click()";
  EXPECT_TRUE(ExecuteScript(root, script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());

  // Go back to the first page, which never completes. The attempt to unload the
  // second page, though, causes it to do a replaceState().
  TestNavigationManager manager(shell()->web_contents(), start_url);
  controller.GoBack();
  manager.WaitForWillStartRequest();

  // The navigation that just happened was the replaceState(), which should not
  // have changed the position into the navigation entry list. Make sure that
  // the pending navigation didn't confuse anything.
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
}

// Ensure the renderer process does not get confused about the current entry
// due to subframes and replaced entries.  See https://crbug.com/480201.
// TODO(creis): Re-enable for Site Isolation FYI bots: https://crbug.com/502317.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       PreventSpoofFromSubframeAndReplace) {
  // Start at an initial URL.
  GURL url1(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  NavigateToURL(shell(), url1);

  // Now go to a page with a real iframe.
  GURL url2(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_data_iframe.html"));
  NavigateToURL(shell(), url2);

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  ASSERT_EQ(1U, root->child_count());
  ASSERT_NE(nullptr, root->child_at(0));

  {
    // Navigate in the iframe.
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    GURL frame_url(embedded_test_server()->GetURL(
        "/navigation_controller/simple_page_2.html"));
    NavigateFrameToURL(root->child_at(0), frame_url);
    capturer.Wait();
    EXPECT_EQ(NAVIGATION_TYPE_NEW_SUBFRAME, capturer.details().type);
  }

  {
    // Go back in the iframe.
    TestNavigationObserver back_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_load_observer.Wait();
  }

  {
    // Go forward in the iframe.
    TestNavigationObserver forward_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoForward();
    forward_load_observer.Wait();
  }

  GURL url3(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_iframe.html"));
  {
    // location.replace() to cause an inert commit.
    TestNavigationObserver replace_load_observer(shell()->web_contents());
    std::string script = "location.replace('" + url3.spec() + "')";
    EXPECT_TRUE(ExecuteScript(root, script));
    replace_load_observer.Wait();
  }

  {
    // Go back to url2.
    TestNavigationObserver back_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_load_observer.Wait();

    // Make sure the URL is correct for both the entry and the main frame, and
    // that the process hasn't been killed for showing a spoof.
    EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());
    EXPECT_EQ(url2, shell()->web_contents()->GetLastCommittedURL());
    EXPECT_EQ(url2, root->current_url());
  }

  {
    // Go back to reset main frame entirely.
    TestNavigationObserver back_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_load_observer.Wait();
    EXPECT_EQ(url1, shell()->web_contents()->GetLastCommittedURL());
    EXPECT_EQ(url1, root->current_url());
  }

  {
    // Go forward.
    TestNavigationObserver back_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoForward();
    back_load_observer.Wait();
    EXPECT_EQ(url2, shell()->web_contents()->GetLastCommittedURL());
    EXPECT_EQ(url2, root->current_url());
  }

  {
    // Go forward to the replaced URL.
    TestNavigationObserver forward_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoForward();
    forward_load_observer.Wait();

    // Make sure the URL is correct for both the entry and the main frame, and
    // that the process hasn't been killed for showing a spoof.
    EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());
    EXPECT_EQ(url3, shell()->web_contents()->GetLastCommittedURL());
    EXPECT_EQ(url3, root->current_url());
  }
}

// Ensure the renderer process does not get killed if the main frame URL's path
// changes when going back in a subframe, since this is currently possible after
// a replaceState in the main frame (thanks to https://crbug.com/373041).
// See https:///crbug.com/486916.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       SubframeBackFromReplaceState) {
  // Start at a page with a real iframe.
  GURL url1(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_data_iframe.html"));
  NavigateToURL(shell(), url1);

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  ASSERT_EQ(1U, root->child_count());
  ASSERT_NE(nullptr, root->child_at(0));

  {
    // Navigate in the iframe.
    FrameNavigateParamsCapturer capturer(root->child_at(0));
    GURL frame_url(embedded_test_server()->GetURL(
        "/navigation_controller/simple_page_2.html"));
    NavigateFrameToURL(root->child_at(0), frame_url);
    capturer.Wait();
    EXPECT_EQ(NAVIGATION_TYPE_NEW_SUBFRAME, capturer.details().type);
  }

  {
    // history.replaceState().
    FrameNavigateParamsCapturer capturer(root);
    std::string script =
        "history.replaceState({}, 'replaced', 'replaced')";
    EXPECT_TRUE(ExecuteScript(root, script));
    capturer.Wait();
  }

  {
    // Go back in the iframe.
    TestNavigationObserver back_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_load_observer.Wait();
  }

  // For now, we expect the main frame's URL to revert.  This won't happen once
  // https://crbug.com/373041 is fixed.
  EXPECT_EQ(url1, shell()->web_contents()->GetLastCommittedURL());

  // Make sure the renderer process has not been killed.
  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());
}

namespace {

class FailureWatcher : public WebContentsObserver {
 public:
  // Observes failure for the specified |node|.
  explicit FailureWatcher(FrameTreeNode* node)
      : WebContentsObserver(
            node->current_frame_host()->delegate()->GetAsWebContents()),
        frame_tree_node_id_(node->frame_tree_node_id()),
        message_loop_runner_(new MessageLoopRunner) {}

  void Wait() {
    message_loop_runner_->Run();
  }

 private:
  void DidFailLoad(RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code,
                   const base::string16& error_description,
                   bool was_ignored_by_handler) override {
    RenderFrameHostImpl* rfh =
        static_cast<RenderFrameHostImpl*>(render_frame_host);
    if (rfh->frame_tree_node()->frame_tree_node_id() != frame_tree_node_id_)
      return;

    message_loop_runner_->Quit();
  }

  void DidFailProvisionalLoad(
      RenderFrameHost* render_frame_host,
      const GURL& validated_url,
      int error_code,
      const base::string16& error_description,
      bool was_ignored_by_handler) override {
    RenderFrameHostImpl* rfh =
        static_cast<RenderFrameHostImpl*>(render_frame_host);
    if (rfh->frame_tree_node()->frame_tree_node_id() != frame_tree_node_id_)
      return;

    message_loop_runner_->Quit();
  }

  void DidFinishNavigation(NavigationHandle* handle) override {
    if (handle->GetFrameTreeNodeId() != frame_tree_node_id_)
      return;
    if (handle->HasCommitted())
      return;

    message_loop_runner_->Quit();
  }

  // The id of the FrameTreeNode whose navigations to observe.
  int frame_tree_node_id_;

  // The MessageLoopRunner used to spin the message loop.
  scoped_refptr<MessageLoopRunner> message_loop_runner_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       DISABLED_StopCausesFailureDespiteJavaScriptURL) {
  NavigationControllerImpl& controller =
      static_cast<NavigationControllerImpl&>(
          shell()->web_contents()->GetController());

  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();

  // Start with a normal page.
  GURL url1(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url1));

  // Have the user decide to go to a different page which will not commit.
  GURL url2(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));
  NavigationStallDelegate stall_delegate(url2);
  ResourceDispatcherHost::Get()->SetDelegate(&stall_delegate);
  controller.LoadURL(url2, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());

  // That should be the pending entry.
  NavigationEntryImpl* entry = controller.GetPendingEntry();
  ASSERT_NE(nullptr, entry);
  EXPECT_EQ(url2, entry->GetURL());

  // Loading a JavaScript URL shouldn't affect the ability to stop.
  {
    FailureWatcher watcher(root);
    GURL js("javascript:(function(){})()");
    controller.LoadURL(js, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
    // This LoadURL ends up purging the pending entry, which is why this is
    // tricky.
    EXPECT_EQ(nullptr, controller.GetPendingEntry());
    EXPECT_TRUE(shell()->web_contents()->IsLoading());
    shell()->web_contents()->Stop();
    watcher.Wait();
    EXPECT_FALSE(shell()->web_contents()->IsLoading());
  }

  ResourceDispatcherHost::Get()->SetDelegate(nullptr);
}

namespace {
class RenderProcessKilledObserver : public WebContentsObserver {
 public:
  RenderProcessKilledObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}
  ~RenderProcessKilledObserver() override {}

  void RenderProcessGone(base::TerminationStatus status) override {
    CHECK_NE(status,
             base::TerminationStatus::TERMINATION_STATUS_PROCESS_WAS_KILLED);
  }
};
}

// This tests a race in ReloadOriginalRequest, where a cross-origin reload was
// causing an in-flight replaceState to look like a cross-origin navigation,
// even though it's in-page.  (The reload should not modify the underlying last
// committed entry.)  Not crashing means that the test is successful.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest, ReloadOriginalRequest) {
  GURL original_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  NavigateToURL(shell(), original_url);
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  RenderProcessKilledObserver kill_observer(shell()->web_contents());

  // Redirect so that we can use ReloadOriginalRequest.
  GURL redirect_url(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_1.html"));
  {
    std::string script = "location.replace('" + redirect_url.spec() + "');";
    FrameNavigateParamsCapturer capturer(root);
    EXPECT_TRUE(ExecuteScript(shell(), script));
    capturer.Wait();
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        capturer.params().transition,
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                  ui::PAGE_TRANSITION_CLIENT_REDIRECT)));
    EXPECT_EQ(NAVIGATION_TYPE_EXISTING_PAGE, capturer.details().type);
  }

  // Modify an entry in the session history and reload the original request.
  {
    // We first send a replaceState() to the renderer, which will cause the
    // renderer to send back a DidCommitProvisionalLoad. Immediately after,
    // we send a ReloadOriginalRequest (which in this case is a different
    // origin) and will also cause the renderer to commit the frame. In the
    // end we verify that both navigations committed and that the URLs are
    // correct.
    std::string script = "history.replaceState({}, '', 'foo');";
    root->render_manager()
        ->current_frame_host()
        ->ExecuteJavaScriptWithUserGestureForTests(base::UTF8ToUTF16(script));
    EXPECT_FALSE(shell()->web_contents()->IsLoading());
    shell()->web_contents()->GetController().ReloadOriginalRequestURL(false);
    EXPECT_TRUE(shell()->web_contents()->IsLoading());
    EXPECT_EQ(redirect_url, shell()->web_contents()->GetLastCommittedURL());

    // Wait until there's no more navigations.
    GURL modified_url(embedded_test_server()->GetURL(
        "foo.com", "/navigation_controller/foo"));
    FrameNavigateParamsCapturer capturer(root);
    capturer.set_wait_for_load(false);
    capturer.set_navigations_remaining(2);
    capturer.Wait();
    EXPECT_EQ(2U, capturer.all_details().size());
    EXPECT_EQ(modified_url, capturer.all_params()[0].url);
    EXPECT_EQ(original_url, capturer.all_params()[1].url);
    EXPECT_EQ(original_url, shell()->web_contents()->GetLastCommittedURL());
  }

  // Make sure the renderer is still alive.
  EXPECT_TRUE(ExecuteScript(shell(), "console.log('Success');"));
}

// This test shows that the initial "about:blank" URL is elided from the
// navigation history of a subframe when it is loaded.
//
// It also prevents regression for an in-page navigation renderer kill when
// going back after an in-page navigation in the main frame is followed by an
// auto subframe navigation, due to a bug in HistoryEntry::CloneAndReplace.
// See https://crbug.com/612713.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       BackToAboutBlankIframe) {
  GURL original_url(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  NavigateToURL(shell(), original_url);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  NavigationController& controller = shell()->web_contents()->GetController();
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(1, RendererHistoryLength(shell()));

  // Add an iframe with no 'src'.

  std::string script =
      "var iframe = document.createElement('iframe');"
      "iframe.id = 'frame';"
      "document.body.appendChild(iframe);";
  EXPECT_TRUE(ExecuteScript(root, script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(1, RendererHistoryLength(shell()));
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());

  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* frame = root->child_at(0);
  ASSERT_NE(nullptr, frame);

  GURL blank_url(url::kAboutBlankURL);
  EXPECT_EQ(blank_url, frame->current_url());

  // Now create a new navigation entry. Note that the old navigation entry has
  // "about:blank" as the URL in the iframe.

  script = "history.pushState({}, '', 'notarealurl.html')";
  EXPECT_TRUE(ExecuteScript(root, script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(2, RendererHistoryLength(shell()));
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());

  // Load the iframe; the initial "about:blank" URL should be elided and thus we
  // shouldn't get a new navigation entry.

  GURL frame_url = embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_2.html");
  NavigateFrameToURL(frame, frame_url);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(2, RendererHistoryLength(shell()));
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());

  EXPECT_EQ(frame_url, frame->current_url());

  // Go back.
  {
    TestNavigationObserver observer(shell()->web_contents(), 1);
    ASSERT_TRUE(controller.CanGoBack());
    controller.GoBack();
    observer.Wait();
  }

  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(2, RendererHistoryLength(shell()));
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());

  // There is some open discussion over whether this should send the iframe
  // back to the blank page, but for now it stays in place to preserve
  // compatibility with existing sites. See
  // NavigationControllerImpl::FindFramesToNavigate for more information, as
  // well as http://crbug.com/542299, https://crbug.com/598043 (for the
  // regressions caused by going back), and
  // https://github.com/whatwg/html/issues/546.
  // TODO(avi, creis): Figure out the correct behavior to use here.
  EXPECT_EQ(frame_url, frame->current_url());

  // Now test for https://crbug.com/612713 to prevent an NC_IN_PAGE_NAVIGATION
  // renderer kill.

  // Do an in-page navigation in the subframe.
  std::string fragment_script = "location.href = \"#foo\";";
  EXPECT_TRUE(ExecuteScript(frame->current_frame_host(), fragment_script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(2, RendererHistoryLength(shell()));
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());

  GURL frame_url_2 = embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_2.html#foo");
  EXPECT_EQ(frame_url_2, frame->current_url());

  // Go back.
  {
    TestNavigationObserver observer(shell()->web_contents(), 1);
    controller.GoBack();
    observer.Wait();
  }

  // Verify the process is still alive by running script.  We can't just call
  // IsRenderFrameLive after the navigation since it might not have disconnected
  // yet.
  EXPECT_TRUE(ExecuteScript(root->current_frame_host(), "true;"));
  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());

  // TODO(creis): We should probably go back to frame_url here instead of the
  // initial blank page.  That might require updating all relevant NavEntries to
  // know what the first committed URL is, so that we really elide the initial
  // blank page from history.
  //
  // TODO(creis): This actually goes to frame_url in some cases when subframe
  // FrameNavigationEntries are enabled, due to a mismatch between PageState and
  // the entry's URL.  That should be fixed in https://crbug.com/617239.
  if (!SiteIsolationPolicy::UseSubframeNavigationEntries())
    EXPECT_EQ(blank_url, frame->current_url());
}

// This test is similar to "BackToAboutBlankIframe" above, except that a
// fragment navigation is used rather than pushState (both create an in-page
// navigation, so we need to test both), and an initial 'src' is given to the
// iframe to test proper restoration in that case.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       BackToIframeWithContent) {
  GURL links_url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_links.html"));
  NavigateToURL(shell(), links_url);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  NavigationController& controller = shell()->web_contents()->GetController();
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(1, RendererHistoryLength(shell()));

  // Add an iframe with a 'src'.

  GURL frame_url_1 = embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html");
  std::string script =
      "var iframe = document.createElement('iframe');"
      "iframe.src = '" + frame_url_1.spec() + "';"
      "iframe.id = 'frame';"
      "document.body.appendChild(iframe);";
  EXPECT_TRUE(ExecuteScript(root, script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(1, RendererHistoryLength(shell()));
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());

  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* frame = root->child_at(0);
  ASSERT_NE(nullptr, frame);

  EXPECT_EQ(frame_url_1, frame->current_url());

  // Do a fragment navigation, creating a new navigation entry. Note that the
  // old navigation entry has frame_url_1 as the URL in the iframe.

  script = "document.getElementById('fraglink').click()";
  EXPECT_TRUE(ExecuteScript(root, script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(2, RendererHistoryLength(shell()));
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());

  EXPECT_EQ(frame_url_1, frame->current_url());

  // Navigate the iframe; unlike the test "BackToAboutBlankIframe" above, this
  // _will_ create a new navigation entry.

  GURL frame_url_2 = embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_2.html");
  NavigateFrameToURL(frame, frame_url_2);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_EQ(3, controller.GetEntryCount());
  // TODO(creis): Replicate history length for OOPIFs: https://crbug.com/501116.
  if (!AreAllSitesIsolatedForTesting())
    EXPECT_EQ(3, RendererHistoryLength(shell()));
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());

  EXPECT_EQ(frame_url_2, frame->current_url());

  // Go back two entries.
  {
    TestNavigationObserver observer(shell()->web_contents(), 1);
    ASSERT_TRUE(controller.CanGoToOffset(-2));
    controller.GoToOffset(-2);
    observer.Wait();
  }

  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(3, RendererHistoryLength(shell()));
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());

  // There is some open discussion over whether this should send the iframe back
  // to the original page, but for now it stays in place to preserve
  // compatibility with existing sites.  See
  // NavigationControllerImpl::FindFramesToNavigate for more information, as
  // well as http://crbug.com/542299, https://crbug.com/598043 (for the
  // regressions caused by going back), and
  // https://github.com/whatwg/html/issues/546.
  // TODO(avi, creis): Figure out the correct behavior to use here.
  EXPECT_EQ(frame_url_2, frame->current_url());

  // Now test for https://crbug.com/612713 to prevent an NC_IN_PAGE_NAVIGATION
  // renderer kill.

  // Do an in-page navigation in the subframe.
  std::string fragment_script = "location.href = \"#foo\";";
  EXPECT_TRUE(ExecuteScript(frame->current_frame_host(), fragment_script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_EQ(2, controller.GetEntryCount());
  // TODO(creis): Replicate history length for OOPIFs: https://crbug.com/501116.
  if (!AreAllSitesIsolatedForTesting())
    EXPECT_EQ(2, RendererHistoryLength(shell()));
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());

  // Go back.
  {
    TestNavigationObserver observer(shell()->web_contents(), 1);
    controller.GoBack();
    observer.Wait();
  }

  // Verify the process is still alive by running script.  We can't just call
  // IsRenderFrameLive after the navigation since it might not have disconnected
  // yet.
  EXPECT_TRUE(ExecuteScript(root->current_frame_host(), "true;"));
  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());

  // TODO(creis): It's a bit surprising to go to frame_url_1 here instead of
  // frame_url_2.  Perhaps we should be going back to frame_url_1 when going
  // back two entries above, since it's different than the initial blank case.
  //
  // TODO(creis): This actually goes to frame_url_2 in some cases when subframe
  // FrameNavigationEntries are enabled, due to a mismatch between PageState and
  // the entry's URL.  That should be fixed in https://crbug.com/617239.
  if (!SiteIsolationPolicy::UseSubframeNavigationEntries())
    EXPECT_EQ(frame_url_1, frame->current_url());
}

// Test for in-page navigation kills due to using the wrong history item in
// HistoryController::RecursiveGoToEntry and NavigationControllerImpl::
// FindFramesToNavigate.  See https://crbug.com/612713.
//
// TODO(creis): Enable this test when https://crbug.com/618100 is fixed.
// Disabled for now while we switch to the new navigation path, since this kill
// is exceptionally rare in practice.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       DISABLED_BackTwiceToIframeWithContent) {
  GURL links_url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_links.html"));
  NavigateToURL(shell(), links_url);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  NavigationController& controller = shell()->web_contents()->GetController();
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(1, RendererHistoryLength(shell()));

  // Add an iframe with a 'src'.

  GURL frame_url_1 = embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html");
  std::string script =
      "var iframe = document.createElement('iframe');"
      "iframe.src = '" + frame_url_1.spec() + "';"
      "iframe.id = 'frame';"
      "document.body.appendChild(iframe);";
  EXPECT_TRUE(ExecuteScript(root->current_frame_host(), script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(1, RendererHistoryLength(shell()));
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());

  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* frame = root->child_at(0);
  ASSERT_NE(nullptr, frame);

  EXPECT_EQ(frame_url_1, frame->current_url());

  // Do an in-page navigation in the subframe.
  GURL frame_url_2 = embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html#foo");
  std::string fragment_script = "location.href = \"#foo\";";
  EXPECT_TRUE(ExecuteScript(frame->current_frame_host(), fragment_script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(2, RendererHistoryLength(shell()));
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(frame_url_2, frame->current_url());

  // Do a fragment navigation at the top level.
  std::string link_script = "document.getElementById('fraglink').click()";
  EXPECT_TRUE(ExecuteScript(root->current_frame_host(), link_script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(3, RendererHistoryLength(shell()));
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(frame_url_2, frame->current_url());

  // Go cross-site in the iframe.
  GURL frame_url_3 = embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/simple_page_2.html");
  NavigateFrameToURL(frame, frame_url_3);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(4, controller.GetEntryCount());
  // TODO(creis): Replicate history length for OOPIFs: https://crbug.com/501116.
  if (!AreAllSitesIsolatedForTesting())
    EXPECT_EQ(4, RendererHistoryLength(shell()));
  EXPECT_EQ(3, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(frame_url_3, frame->current_url());

  // Go back two entries.
  {
    TestNavigationObserver observer(shell()->web_contents(), 1);
    ASSERT_TRUE(controller.CanGoToOffset(-2));
    controller.GoToOffset(-2);
    observer.Wait();
  }
  EXPECT_EQ(4, controller.GetEntryCount());
  // TODO(creis): Replicate history length for OOPIFs: https://crbug.com/501116.
  if (!AreAllSitesIsolatedForTesting())
    EXPECT_EQ(4, RendererHistoryLength(shell()));
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(links_url, root->current_url());

  // There is some open discussion over whether this should send the iframe back
  // to the original page, but for now it stays in place to preserve
  // compatibility with existing sites.  See
  // NavigationControllerImpl::FindFramesToNavigate for more information, as
  // well as http://crbug.com/542299, https://crbug.com/598043 (for the
  // regressions caused by going back), and
  // https://github.com/whatwg/html/issues/546.
  // TODO(avi, creis): Figure out the correct behavior to use here.
  EXPECT_EQ(frame_url_3, frame->current_url());

  // Now test for https://crbug.com/612713 to prevent an NC_IN_PAGE_NAVIGATION
  // renderer kill.

  // Go back.
  {
    TestNavigationObserver observer(shell()->web_contents(), 1);
    controller.GoBack();
    observer.Wait();
  }

  // Verify the process is still alive by running script.  We can't just call
  // IsRenderFrameLive after the navigation since it might not have disconnected
  // yet.
  EXPECT_TRUE(ExecuteScript(root->current_frame_host(), "true;"));
  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());

  // TODO(creis): It's a bit surprising to go to frame_url_1 here instead of
  // frame_url_2.  Perhaps we should be going back to frame_url_1 when going
  // back two entries above, since it's different than the initial blank case.
  //
  // TODO(creis): This actually goes to frame_url_2 in some cases when subframe
  // FrameNavigationEntries are enabled, due to a mismatch between PageState and
  // the entry's URL.  That should be fixed in https://crbug.com/617239.
  if (!SiteIsolationPolicy::UseSubframeNavigationEntries())
    EXPECT_EQ(frame_url_1, frame->current_url());
}

// Test for in-page navigation kills when going back to about:blank after a
// document.write.  See https://crbug.com/446959.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       BackAfterIframeDocumentWrite) {
  GURL links_url(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_links.html"));
  NavigateToURL(shell(), links_url);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  NavigationController& controller = shell()->web_contents()->GetController();
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(1, RendererHistoryLength(shell()));

  // Add an iframe with no 'src'.
  GURL blank_url(url::kAboutBlankURL);
  std::string script =
      "var iframe = document.createElement('iframe');"
      "iframe.id = 'frame';"
      "document.body.appendChild(iframe);";
  EXPECT_TRUE(ExecuteScript(root->current_frame_host(), script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(1, RendererHistoryLength(shell()));
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* frame = root->child_at(0);
  ASSERT_NE(nullptr, frame);
  EXPECT_EQ(blank_url, frame->current_url());

  // Do a document.write in the subframe to create a link to click.
  std::string document_write_script =
      "var iframe = document.getElementById('frame');"
      "iframe.contentWindow.document.write("
      "    \"<a id='fraglink' href='#frag'>fragment link</a>\");"
      "iframe.contentWindow.document.close();";
  EXPECT_TRUE(ExecuteScript(root->current_frame_host(), document_write_script));

  // Click the link to do an in-page navigation.  Due to the document.write, the
  // new URL matches the parent frame's URL.
  GURL frame_url_2(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_links.html#frag"));
  std::string link_script = "document.getElementById('fraglink').click()";
  EXPECT_TRUE(ExecuteScript(frame->current_frame_host(), link_script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(2, RendererHistoryLength(shell()));
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(frame_url_2, frame->current_url());

  // Go back.
  {
    TestNavigationObserver observer(shell()->web_contents(), 1);
    controller.GoBack();
    observer.Wait();
  }

  // Verify the process is still alive by running script.  We can't just call
  // IsRenderFrameLive after the navigation since it might not have disconnected
  // yet.
  EXPECT_TRUE(ExecuteScript(root->current_frame_host(), "true;"));
  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());

  EXPECT_EQ(blank_url, frame->current_url());
}

// Test for in-page navigation kills when going back to about:blank in an iframe
// of a data URL, after a document.write.  This differs from
// BackAfterIframeDocumentWrite because both about:blank and the data URL are
// considered unique origins.  See https://crbug.com/446959.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       BackAfterIframeDocumentWriteInDataURL) {
  GURL data_url("data:text/html,Top level page");
  NavigateToURL(shell(), data_url);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  NavigationController& controller = shell()->web_contents()->GetController();
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(1, RendererHistoryLength(shell()));

  // Add an iframe with no 'src'.
  GURL blank_url(url::kAboutBlankURL);
  std::string script =
      "var iframe = document.createElement('iframe');"
      "iframe.id = 'frame';"
      "document.body.appendChild(iframe);";
  EXPECT_TRUE(ExecuteScript(root->current_frame_host(), script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(1, RendererHistoryLength(shell()));
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* frame = root->child_at(0);
  ASSERT_NE(nullptr, frame);
  EXPECT_EQ(blank_url, frame->current_url());

  // Do a document.write in the subframe to create a link to click.
  std::string document_write_script =
      "var iframe = document.getElementById('frame');"
      "iframe.contentWindow.document.write("
      "    \"<a id='fraglink' href='#frag'>fragment link</a>\");"
      "iframe.contentWindow.document.close();";
  EXPECT_TRUE(ExecuteScript(root->current_frame_host(), document_write_script));

  // Click the link to do an in-page navigation.  Due to the document.write, the
  // new URL matches the parent frame's URL.
  GURL frame_url_2("data:text/html,Top level page#frag");
  std::string link_script = "document.getElementById('fraglink').click()";
  EXPECT_TRUE(ExecuteScript(frame->current_frame_host(), link_script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(2, RendererHistoryLength(shell()));
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(frame_url_2, frame->current_url());

  // Go back.
  {
    TestNavigationObserver observer(shell()->web_contents(), 1);
    controller.GoBack();
    observer.Wait();
  }

  // Verify the process is still alive by running script.  We can't just call
  // IsRenderFrameLive after the navigation since it might not have disconnected
  // yet.
  EXPECT_TRUE(ExecuteScript(root->current_frame_host(), "true;"));
  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());

  EXPECT_EQ(blank_url, frame->current_url());
}

// Ensure that we do not corrupt a NavigationEntry's PageState if a subframe
// forward navigation commits after we've already started another forward
// navigation in the main frame.  See https://crbug.com/597322.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       ForwardInSubframeWithPendingForward) {
  // Navigate to a page with an iframe.
  GURL url_a(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_data_iframe.html"));
  GURL frame_url_a1("data:text/html,Subframe");
  EXPECT_TRUE(NavigateToURL(shell(), url_a));

  NavigationController& controller = shell()->web_contents()->GetController();
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  ASSERT_EQ(1U, root->child_count());
  EXPECT_EQ(url_a, root->current_url());
  FrameTreeNode* frame = root->child_at(0);
  EXPECT_EQ(frame_url_a1, frame->current_url());

  // Navigate the iframe to a second page.
  GURL frame_url_a2 = embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html");
  NavigateFrameToURL(frame, frame_url_a2);

  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(url_a, root->current_url());
  EXPECT_EQ(frame_url_a2, frame->current_url());

  // Navigate the top-level frame to another page with an iframe.
  GURL url_b(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_iframe.html"));
  GURL frame_url_b1(url::kAboutBlankURL);
  EXPECT_TRUE(NavigateToURL(shell(), url_b));
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(url_b, root->current_url());
  EXPECT_EQ(frame_url_b1, root->child_at(0)->current_url());

  // Go back two entries. The original frame URL should be back.
  ASSERT_TRUE(controller.CanGoToOffset(-2));
  controller.GoToOffset(-2);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(url_a, root->current_url());
  EXPECT_EQ(frame_url_a1, root->child_at(0)->current_url());

  // Go forward two times in a row, being careful that the subframe commits
  // after the second forward navigation begins but before the main frame
  // commits.
  TestNavigationManager subframe_delayer(shell()->web_contents(), frame_url_a2);
  TestNavigationManager mainframe_delayer(shell()->web_contents(), url_b);
  controller.GoForward();
  subframe_delayer.WaitForWillStartRequest();
  controller.GoForward();
  mainframe_delayer.WaitForWillStartRequest();
  EXPECT_EQ(2, controller.GetPendingEntryIndex());

  // Let the subframe commit.
  subframe_delayer.ResumeNavigation();
  subframe_delayer.WaitForNavigationFinished();
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(url_a, root->current_url());
  EXPECT_EQ(frame_url_a2, root->child_at(0)->current_url());

  // Let the main frame commit.
  mainframe_delayer.ResumeNavigation();
  mainframe_delayer.WaitForNavigationFinished();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(url_b, root->current_url());
  EXPECT_EQ(frame_url_b1, root->child_at(0)->current_url());

  // Check the PageState of the previous entry to ensure it isn't corrupted.
  NavigationEntry* entry = controller.GetEntryAtIndex(1);
  EXPECT_EQ(url_a, entry->GetURL());
  ExplodedPageState exploded_state;
  EXPECT_TRUE(
      DecodePageState(entry->GetPageState().ToEncodedData(), &exploded_state));
  EXPECT_EQ(url_a, GURL(exploded_state.top.url_string.string()));
  EXPECT_EQ(frame_url_a2,
            GURL(exploded_state.top.children.at(0).url_string.string()));
}

// Ensure that we do not corrupt a NavigationEntry's PageState if two forward
// navigations compete in different frames.  See https://crbug.com/623319.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       PageStateAfterForwardInCompetingFrames) {
  // Navigate to a page with an iframe.
  GURL url_a(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_data_iframe.html"));
  GURL frame_url_a1("data:text/html,Subframe");
  EXPECT_TRUE(NavigateToURL(shell(), url_a));

  NavigationController& controller = shell()->web_contents()->GetController();
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  EXPECT_EQ(url_a, root->current_url());
  EXPECT_EQ(frame_url_a1, root->child_at(0)->current_url());

  // Navigate the iframe to a second page.
  GURL frame_url_a2 = embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html");
  NavigateFrameToURL(root->child_at(0), frame_url_a2);

  // Navigate the iframe to about:blank.
  GURL blank_url(url::kAboutBlankURL);
  NavigateFrameToURL(root->child_at(0), blank_url);
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(url_a, root->current_url());
  EXPECT_EQ(blank_url, root->child_at(0)->current_url());

  // Go back to the middle entry.
  controller.GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());

  // Replace the entry with a cross-site top-level page.  By doing a
  // replacement, the main frame pages before and after have the same item
  // sequence number, and thus going between them only requires loads in the
  // subframe.
  GURL url_b(embedded_test_server()->GetURL(
      "b.com", "/navigation_controller/simple_page_2.html"));
  std::string replace_script = "location.replace('" + url_b.spec() + "')";
  TestNavigationObserver replace_observer(shell()->web_contents());
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(), replace_script));
  replace_observer.Wait();
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(url_b, root->current_url());

  // Go back to the original page.
  controller.GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // Navigate forward twice using script.  In https://crbug.com/623319, this
  // caused a mismatch between the NavigationEntry's URL and PageState.
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(),
                            "history.forward(); history.forward();"));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(url_b, root->current_url());
  NavigationEntry* entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(url_b, entry->GetURL());
  ExplodedPageState exploded_state;
  EXPECT_TRUE(
      DecodePageState(entry->GetPageState().ToEncodedData(), &exploded_state));
  EXPECT_EQ(url_b, GURL(exploded_state.top.url_string.string()));
  // TODO(creis): Clear subframe FNEs after location.replace in
  // --isolate-extensions mode.  See https://crbug.com/596707.
  if (!SiteIsolationPolicy::UseSubframeNavigationEntries())
    EXPECT_EQ(0U, exploded_state.top.children.size());

  // Go back and then forward to see if the PageState loads correctly.
  controller.GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  controller.GoForward();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // We should be on url_b, and the renderer process shouldn't be killed.
  ASSERT_TRUE(root->current_frame_host()->IsRenderFrameLive());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(url_b, shell()->web_contents()->GetVisibleURL());
  EXPECT_EQ(url_b, root->current_url());
  EXPECT_EQ(0U, root->child_count());
}

// Ensure that we do not corrupt a NavigationEntry's PageState if two forward
// navigations compete in different frames, and the main frame entry contains an
// iframe of its own.  See https://crbug.com/623319.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       PageStateWithIframeAfterForwardInCompetingFrames) {
  // Navigate to a page with an iframe.
  GURL url_a(embedded_test_server()->GetURL(
      "/navigation_controller/page_with_data_iframe.html"));
  GURL data_url("data:text/html,Subframe");
  EXPECT_TRUE(NavigateToURL(shell(), url_a));

  NavigationController& controller = shell()->web_contents()->GetController();
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  EXPECT_EQ(url_a, root->current_url());
  EXPECT_EQ(data_url, root->child_at(0)->current_url());

  // Navigate the iframe to a first real page.
  GURL frame_url_a1 = embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html");
  NavigateFrameToURL(root->child_at(0), frame_url_a1);

  // Navigate the iframe to a second real page.
  GURL frame_url_a2 = embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html");
  NavigateFrameToURL(root->child_at(0), frame_url_a2);
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(url_a, root->current_url());
  EXPECT_EQ(frame_url_a2, root->child_at(0)->current_url());

  // Go back to the middle entry.
  controller.GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());

  // Replace the entry with a cross-site top-level page with an iframe.  By
  // doing a replacement, the main frame pages before and after have the same
  // item sequence number, and thus going between them only requires loads in
  // the subframe.
  GURL url_b(embedded_test_server()->GetURL(
      "b.com", "/navigation_controller/page_with_data_iframe.html"));
  std::string replace_script = "location.replace('" + url_b.spec() + "')";
  TestNavigationObserver replace_observer(shell()->web_contents());
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(), replace_script));
  replace_observer.Wait();
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(url_b, root->current_url());
  EXPECT_EQ(data_url, root->child_at(0)->current_url());

  // Go back to the original page.
  controller.GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // Navigate forward twice using script.  This will race, but in either outcome
  // we want to ensure that the subframes target entry index 1 and not 2.  In
  // https://crbug.com/623319, the subframes targeted the wrong entry, leading
  // to a URL spoof and renderer kill.
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(),
                            "history.forward(); history.forward();"));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());
  EXPECT_EQ(url_b, root->current_url());
  EXPECT_EQ(data_url, root->child_at(0)->current_url());
  NavigationEntry* entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(url_b, entry->GetURL());
  ExplodedPageState exploded_state;
  EXPECT_TRUE(
      DecodePageState(entry->GetPageState().ToEncodedData(), &exploded_state));
  EXPECT_EQ(url_b, GURL(exploded_state.top.url_string.string()));

  // Go back and then forward to see if the PageState loads correctly.
  controller.GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  controller.GoForward();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // We should be on url_b, and the renderer process shouldn't be killed.
  ASSERT_TRUE(root->current_frame_host()->IsRenderFrameLive());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(url_b, shell()->web_contents()->GetVisibleURL());
  EXPECT_EQ(url_b, root->current_url());
  EXPECT_EQ(data_url, root->child_at(0)->current_url());
}

// Ensure that forward navigations in cloned tabs can commit if they redirect to
// a different site than before.  This causes the navigation's item sequence
// number to change, meaning that we can't use it for determining whether the
// commit matches the history item.  See https://crbug.com/600238.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       ForwardRedirectWithNoCommittedEntry) {
  NavigationController& controller = shell()->web_contents()->GetController();
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  // Put 2 pages in history.
  GURL url_1(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_1));

  GURL url_2(embedded_test_server()->GetURL(
      "/navigation_controller/simple_page_2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_2));

  EXPECT_EQ(url_2, root->current_url());
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());

  // Do a replaceState to a URL that will redirect when we come back to it via
  // session history.
  GURL url_3(embedded_test_server()->GetURL(
      "foo.com", "/navigation_controller/page_with_links.html"));
  {
    TestNavigationObserver observer(shell()->web_contents());
    std::string script =
        "history.replaceState({}, '', '/server-redirect?" + url_3.spec() + "')";
    EXPECT_TRUE(ExecuteScript(root, script));
    observer.Wait();
  }

  // Go back.
  controller.GoBack();
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(url_1, root->current_url());

  // Clone the tab without navigating it.
  std::unique_ptr<WebContentsImpl> new_tab(
      static_cast<WebContentsImpl*>(shell()->web_contents()->Clone()));
  NavigationController& new_controller = new_tab->GetController();
  FrameTreeNode* new_root = new_tab->GetFrameTree()->root();
  EXPECT_TRUE(new_controller.IsInitialNavigation());
  EXPECT_TRUE(new_controller.NeedsReload());

  // Go forward in the new tab.
  {
    TestNavigationObserver observer(new_tab.get());
    new_controller.GoForward();
    observer.Wait();
  }
  EXPECT_TRUE(new_root->current_frame_host()->IsRenderFrameLive());
  EXPECT_EQ(url_3, new_root->current_url());
}

// Tests that when using FrameNavigationEntries, knowledge of POST navigations
// is recorded on a subframe level.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       PostInSubframe) {
  if (!SiteIsolationPolicy::UseSubframeNavigationEntries())
    return;

  GURL page_with_form_url = embedded_test_server()->GetURL(
      "/navigation_controller/subframe_form.html");
  NavigateToURL(shell(), page_with_form_url);

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  FrameTreeNode* frame = root->child_at(0);
  EXPECT_EQ(1, controller.GetEntryCount());

  {
    NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
    FrameNavigationEntry* root_entry = entry->GetFrameEntry(root);
    FrameNavigationEntry* frame_entry = entry->GetFrameEntry(frame);
    EXPECT_NE(nullptr, root_entry);
    EXPECT_NE(nullptr, frame_entry);
    EXPECT_EQ("GET", root_entry->method());
    EXPECT_EQ(-1, root_entry->post_id());
    EXPECT_EQ("GET", frame_entry->method());
    EXPECT_EQ(-1, frame_entry->post_id());
    EXPECT_FALSE(entry->GetHasPostData());
    EXPECT_EQ(-1, entry->GetPostID());
  }

  // Submit the form.
  TestNavigationObserver observer(shell()->web_contents(), 1);
  EXPECT_TRUE(ExecuteScript(
      shell(), "window.domAutomationController.send(submitForm('isubmit'))"));
  observer.Wait();

  EXPECT_EQ(2, controller.GetEntryCount());
  {
    NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
    FrameNavigationEntry* root_entry = entry->GetFrameEntry(root);
    FrameNavigationEntry* frame_entry = entry->GetFrameEntry(frame);
    EXPECT_NE(nullptr, root_entry);
    EXPECT_NE(nullptr, frame_entry);
    EXPECT_EQ("GET", root_entry->method());
    EXPECT_EQ(-1, root_entry->post_id());
    EXPECT_EQ("POST", frame_entry->method());
    // TODO(clamy): Check the post id as well when PlzNavigate handles it
    // properly.
    if (!IsBrowserSideNavigationEnabled())
      EXPECT_NE(-1, frame_entry->post_id());
    EXPECT_FALSE(entry->GetHasPostData());
    EXPECT_EQ(-1, entry->GetPostID());
  }
}

// Tests that POST body is not lost when decidePolicyForNavigation tells the
// renderer to route the request via FrameHostMsg_OpenURL sent to the browser.
// See also https://crbug.com/344348.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest, PostViaOpenUrlMsg) {
  GURL main_url(
      embedded_test_server()->GetURL("/form_that_posts_to_echoall.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Ask the renderer to go through OpenURL FrameHostMsg_OpenURL IPC message.
  // Without this, the test wouldn't repro https://crbug.com/344348.
  shell()
      ->web_contents()
      ->GetMutableRendererPrefs()
      ->browser_handles_all_top_level_requests = true;
  shell()->web_contents()->GetRenderViewHost()->SyncRendererPrefs();

  // Submit the form.
  TestNavigationObserver form_post_observer(shell()->web_contents(), 1);
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(),
                            "document.getElementById('form').submit();"));
  form_post_observer.Wait();

  // Verify that we arrived at the expected location.
  GURL target_url(embedded_test_server()->GetURL("/echoall"));
  EXPECT_EQ(target_url, shell()->web_contents()->GetLastCommittedURL());

  // Verify that POST body was correctly passed to the server and ended up in
  // the body of the page.
  std::string body;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      shell()->web_contents(),
      "window.domAutomationController.send("
      "document.getElementsByTagName('pre')[0].innerText);",
      &body));
  EXPECT_EQ("text=value\n", body);
}

// Test that navigations classified as SAME_PAGE properly update all the
// members of FrameNavigationEntry. If not, it is possible to get a mismatch
// between the origin and URL of a document as seen in
// https://crbug.com/630103.
IN_PROC_BROWSER_TEST_F(NavigationControllerBrowserTest,
                       EnsureSamePageNavigationUpdatesFrameNavigationEntry) {
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = web_contents->GetFrameTree()->root();

  // Navigate to a simple page and then perform an in-page navigation.
  GURL start_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  GURL same_page_url(
      embedded_test_server()->GetURL("a.com", "/title1.html#foo"));
  EXPECT_TRUE(NavigateToURL(shell(), same_page_url));
  EXPECT_EQ(2, web_contents->GetController().GetEntryCount());

  // Replace the URL of the current NavigationEntry with one that will cause
  // a server redirect when loaded.
  {
    GURL redirect_dest_url(
        embedded_test_server()->GetURL("sub.a.com", "/simple_page.html"));
    TestNavigationObserver observer(web_contents);
    std::string script = "history.replaceState({}, '', '/server-redirect?" +
                         redirect_dest_url.spec() + "')";
    EXPECT_TRUE(ExecuteScript(root, script));
    observer.Wait();
  }

  // Simulate the user hitting Enter in the omnibox without changing the URL.
  {
    TestNavigationObserver observer(web_contents);
    web_contents->GetController().LoadURL(web_contents->GetLastCommittedURL(),
                                          Referrer(), ui::PAGE_TRANSITION_LINK,
                                          std::string());
    observer.Wait();
  }

  // Prior to fixing the issue, the above omnibox navigation (which is
  // classified as SAME_PAGE) was leaving the FrameNavigationEntry with the
  // same document sequence number as the previous entry but updates the URL.
  // Doing a back session history navigation now will cause the browser to
  // consider it as in-page because of this matching document sequence number
  // and lead to a mismatch of origin and URL in the renderer process.
  {
    TestNavigationObserver observer(web_contents);
    web_contents->GetController().GoBack();
    observer.Wait();
  }

  // Verify the expected origin through JavaScript. It also has the additional
  // verification of the process also being still alive.
  std::string origin;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      web_contents, "domAutomationController.send(document.origin)", &origin));
  EXPECT_EQ(start_url.GetOrigin().spec(), origin + "/");
}

}  // namespace content
