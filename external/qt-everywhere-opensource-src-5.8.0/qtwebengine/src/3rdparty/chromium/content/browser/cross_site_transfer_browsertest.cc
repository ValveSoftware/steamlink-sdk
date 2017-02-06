// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/browser/shell_resource_dispatcher_host_delegate.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/base/escape.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "url/gurl.h"

namespace content {

// Tracks a single request for a specified URL, and allows waiting until the
// request is destroyed, and then inspecting whether it completed successfully.
class TrackingResourceDispatcherHostDelegate
    : public ShellResourceDispatcherHostDelegate {
 public:
  TrackingResourceDispatcherHostDelegate() : throttle_created_(false) {
  }

  void RequestBeginning(net::URLRequest* request,
                        ResourceContext* resource_context,
                        AppCacheService* appcache_service,
                        ResourceType resource_type,
                        ScopedVector<ResourceThrottle>* throttles) override {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    ShellResourceDispatcherHostDelegate::RequestBeginning(
        request, resource_context, appcache_service, resource_type, throttles);
    // Expect only a single request for the tracked url.
    ASSERT_FALSE(throttle_created_);
    // If this is a request for the tracked URL, add a throttle to track it.
    if (request->url() == tracked_url_)
      throttles->push_back(new TrackingThrottle(request, this));
  }

  // Starts tracking a URL.  The request for previously tracked URL, if any,
  // must have been made and deleted before calling this function.
  void SetTrackedURL(const GURL& tracked_url) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    // Should not currently be tracking any URL.
    ASSERT_FALSE(run_loop_);

    // Create a RunLoop that will be stopped once the request for the tracked
    // URL has been destroyed, to allow tracking the URL while also waiting for
    // other events.
    run_loop_.reset(new base::RunLoop());

    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(
            &TrackingResourceDispatcherHostDelegate::SetTrackedURLOnIOThread,
            base::Unretained(this), tracked_url, run_loop_->QuitClosure()));
  }

  // Waits until the tracked URL has been requested, and the request for it has
  // been destroyed.
  bool WaitForTrackedURLAndGetCompleted() {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    run_loop_->Run();
    run_loop_.reset();
    return tracked_request_completed_;
  }

 private:
  // ResourceThrottle attached to request for the tracked URL.  On destruction,
  // passes the final URLRequestStatus back to the delegate.
  class TrackingThrottle : public ResourceThrottle {
   public:
    TrackingThrottle(net::URLRequest* request,
                     TrackingResourceDispatcherHostDelegate* tracker)
        : request_(request), tracker_(tracker) {
    }

    ~TrackingThrottle() override {
      // If the request is deleted without being cancelled, its status will
      // indicate it succeeded, so have to check if the request is still pending
      // as well.
      tracker_->OnTrackedRequestDestroyed(
          !request_->is_pending() && request_->status().is_success());
    }

    // ResourceThrottle implementation:
    const char* GetNameForLogging() const override {
      return "TrackingThrottle";
    }

   private:
    net::URLRequest* request_;
    TrackingResourceDispatcherHostDelegate* tracker_;

    DISALLOW_COPY_AND_ASSIGN(TrackingThrottle);
  };

  void SetTrackedURLOnIOThread(const GURL& tracked_url,
                               const base::Closure& run_loop_quit_closure) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    throttle_created_ = false;
    tracked_url_ = tracked_url;
    run_loop_quit_closure_ = run_loop_quit_closure;
  }

  void OnTrackedRequestDestroyed(bool completed) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    tracked_request_completed_ = completed;
    tracked_url_ = GURL();

    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            run_loop_quit_closure_);
  }

  // These live on the IO thread.
  GURL tracked_url_;
  bool throttle_created_;
  base::Closure run_loop_quit_closure_;

  // This lives on the UI thread.
  std::unique_ptr<base::RunLoop> run_loop_;

  // Set on the IO thread while |run_loop_| is non-nullptr, read on the UI
  // thread after deleting run_loop_.
  bool tracked_request_completed_;

  DISALLOW_COPY_AND_ASSIGN(TrackingResourceDispatcherHostDelegate);
};

// WebContentsDelegate that fails to open a URL when there's a request that
// needs to be transferred between renderers.
class NoTransferRequestDelegate : public WebContentsDelegate {
 public:
  NoTransferRequestDelegate() {}

  bool ShouldTransferNavigation() override {
    // Intentionally cancel the transfer.
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NoTransferRequestDelegate);
};

class CrossSiteTransferTest : public ContentBrowserTest {
 public:
  CrossSiteTransferTest() : old_delegate_(nullptr) {}

  // ContentBrowserTest implementation:
  void SetUpOnMainThread() override {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&CrossSiteTransferTest::InjectResourceDispatcherHostDelegate,
                   base::Unretained(this)));
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    content::SetupCrossSiteRedirector(embedded_test_server());
  }

  void TearDownOnMainThread() override {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(
            &CrossSiteTransferTest::RestoreResourceDisptcherHostDelegate,
            base::Unretained(this)));
  }

 protected:
  void NavigateToURLContentInitiated(Shell* window,
                                     const GURL& url,
                                     bool should_replace_current_entry,
                                     bool should_wait_for_navigation) {
    std::string script;
    if (should_replace_current_entry)
      script = base::StringPrintf("location.replace('%s')", url.spec().c_str());
    else
      script = base::StringPrintf("location.href = '%s'", url.spec().c_str());
    TestNavigationObserver load_observer(shell()->web_contents(), 1);
    bool result = ExecuteScript(window, script);
    EXPECT_TRUE(result);
    if (should_wait_for_navigation)
      load_observer.Wait();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolateAllSitesForTesting(command_line);
  }

  void InjectResourceDispatcherHostDelegate() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    old_delegate_ = ResourceDispatcherHostImpl::Get()->delegate();
    ResourceDispatcherHostImpl::Get()->SetDelegate(&tracking_delegate_);
  }

  void RestoreResourceDisptcherHostDelegate() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    ResourceDispatcherHostImpl::Get()->SetDelegate(old_delegate_);
    old_delegate_ = nullptr;
  }

  TrackingResourceDispatcherHostDelegate& tracking_delegate() {
    return tracking_delegate_;
  }

 private:
  TrackingResourceDispatcherHostDelegate tracking_delegate_;
  ResourceDispatcherHostDelegate* old_delegate_;
};

// The following tests crash in the ThreadSanitizer runtime,
// http://crbug.com/356758.
#if defined(THREAD_SANITIZER)
#define MAYBE_ReplaceEntryCrossProcessThenTransfer \
    DISABLED_ReplaceEntryCrossProcessThenTransfer
#define MAYBE_ReplaceEntryCrossProcessTwice \
    DISABLED_ReplaceEntryCrossProcessTwice
#else
#define MAYBE_ReplaceEntryCrossProcessThenTransfer \
    ReplaceEntryCrossProcessThenTransfer
#define MAYBE_ReplaceEntryCrossProcessTwice ReplaceEntryCrossProcessTwice
#endif
// Tests that the |should_replace_current_entry| flag persists correctly across
// request transfers that began with a cross-process navigation.
IN_PROC_BROWSER_TEST_F(CrossSiteTransferTest,
                       MAYBE_ReplaceEntryCrossProcessThenTransfer) {
  const NavigationController& controller =
      shell()->web_contents()->GetController();

  // Navigate to a starting URL, so there is a history entry to replace.
  GURL url1 = embedded_test_server()->GetURL("/site_isolation/blank.html?1");
  EXPECT_TRUE(NavigateToURL(shell(), url1));

  // Force all future navigations to transfer. Note that this includes same-site
  // navigiations which may cause double process swaps (via OpenURL and then via
  // transfer). This test intentionally exercises that case.
  ShellContentBrowserClient::SetSwapProcessesForRedirect(true);

  // Navigate to a page on A.com with entry replacement. This navigation is
  // cross-site, so the renderer will send it to the browser via OpenURL to give
  // to a new process. It will then be transferred into yet another process due
  // to the call above.
  GURL url2 =
      embedded_test_server()->GetURL("A.com", "/site_isolation/blank.html?2");
  // Used to make sure the request for url2 succeeds, and there was only one of
  // them.
  tracking_delegate().SetTrackedURL(url2);
  NavigateToURLContentInitiated(shell(), url2, true, true);

  // There should be one history entry. url2 should have replaced url1.
  EXPECT_TRUE(controller.GetPendingEntry() == nullptr);
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetCurrentEntryIndex());
  EXPECT_EQ(url2, controller.GetEntryAtIndex(0)->GetURL());
  // Make sure the request succeeded.
  EXPECT_TRUE(tracking_delegate().WaitForTrackedURLAndGetCompleted());

  // Now navigate as before to a page on B.com, but normally (without
  // replacement). This will still perform a double process-swap as above, via
  // OpenURL and then transfer.
  GURL url3 =
      embedded_test_server()->GetURL("B.com", "/site_isolation/blank.html?3");
  // Used to make sure the request for url3 succeeds, and there was only one of
  // them.
  tracking_delegate().SetTrackedURL(url3);
  NavigateToURLContentInitiated(shell(), url3, false, true);

  // There should be two history entries. url2 should have replaced url1. url2
  // should not have replaced url3.
  EXPECT_TRUE(controller.GetPendingEntry() == nullptr);
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());
  EXPECT_EQ(url2, controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(url3, controller.GetEntryAtIndex(1)->GetURL());

  // Make sure the request succeeded.
  EXPECT_TRUE(tracking_delegate().WaitForTrackedURLAndGetCompleted());
}

// Tests that the |should_replace_current_entry| flag persists correctly across
// request transfers that began with a content-initiated in-process
// navigation. This test is the same as the test above, except transfering from
// in-process.
IN_PROC_BROWSER_TEST_F(CrossSiteTransferTest,
                       ReplaceEntryInProcessThenTransfer) {
  const NavigationController& controller =
      shell()->web_contents()->GetController();

  // Navigate to a starting URL, so there is a history entry to replace.
  GURL url = embedded_test_server()->GetURL("/site_isolation/blank.html?1");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Force all future navigations to transfer. Note that this includes same-site
  // navigiations which may cause double process swaps (via OpenURL and then via
  // transfer). All navigations in this test are same-site, so it only swaps
  // processes via request transfer.
  ShellContentBrowserClient::SetSwapProcessesForRedirect(true);

  // Navigate in-process with entry replacement. It will then be transferred
  // into a new one due to the call above.
  GURL url2 = embedded_test_server()->GetURL("/site_isolation/blank.html?2");
  NavigateToURLContentInitiated(shell(), url2, true, true);

  // There should be one history entry. url2 should have replaced url1.
  EXPECT_TRUE(controller.GetPendingEntry() == nullptr);
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetCurrentEntryIndex());
  EXPECT_EQ(url2, controller.GetEntryAtIndex(0)->GetURL());

  // Now navigate as before, but without replacement.
  GURL url3 = embedded_test_server()->GetURL("/site_isolation/blank.html?3");
  NavigateToURLContentInitiated(shell(), url3, false, true);

  // There should be two history entries. url2 should have replaced url1. url2
  // should not have replaced url3.
  EXPECT_TRUE(controller.GetPendingEntry() == nullptr);
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());
  EXPECT_EQ(url2, controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(url3, controller.GetEntryAtIndex(1)->GetURL());
}

// Tests that the |should_replace_current_entry| flag persists correctly across
// request transfers that cross processes twice from renderer policy.
IN_PROC_BROWSER_TEST_F(CrossSiteTransferTest,
                       MAYBE_ReplaceEntryCrossProcessTwice) {
  const NavigationController& controller =
      shell()->web_contents()->GetController();

  // Navigate to a starting URL, so there is a history entry to replace.
  GURL url1 = embedded_test_server()->GetURL("/site_isolation/blank.html?1");
  EXPECT_TRUE(NavigateToURL(shell(), url1));

  // Navigate to a page on A.com which redirects to B.com with entry
  // replacement. This will switch processes via OpenURL twice. First to A.com,
  // and second in response to the server redirect to B.com. The second swap is
  // also renderer-initiated via OpenURL because decidePolicyForNavigation is
  // currently applied on redirects.
  GURL::Replacements replace_host;
  GURL url2b =
      embedded_test_server()->GetURL("B.com", "/site_isolation/blank.html?2");
  GURL url2a = embedded_test_server()->GetURL(
      "A.com", "/cross-site/" + url2b.host() + url2b.PathForRequest());
  NavigateToURLContentInitiated(shell(), url2a, true, true);

  // There should be one history entry. url2b should have replaced url1.
  EXPECT_TRUE(controller.GetPendingEntry() == nullptr);
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetCurrentEntryIndex());
  EXPECT_EQ(url2b, controller.GetEntryAtIndex(0)->GetURL());

  // Now repeat without replacement.
  GURL url3b =
      embedded_test_server()->GetURL("B.com", "/site_isolation/blank.html?3");
  GURL url3a = embedded_test_server()->GetURL(
      "A.com", "/cross-site/" + url3b.host() + url3b.PathForRequest());
  NavigateToURLContentInitiated(shell(), url3a, false, true);

  // There should be two history entries. url2b should have replaced url1. url3b
  // should not have replaced url2b.
  EXPECT_TRUE(controller.GetPendingEntry() == nullptr);
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());
  EXPECT_EQ(url2b, controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(url3b, controller.GetEntryAtIndex(1)->GetURL());
}

// Tests that the request is destroyed when a cross process navigation is
// cancelled.
IN_PROC_BROWSER_TEST_F(CrossSiteTransferTest, NoLeakOnCrossSiteCancel) {
  const NavigationController& controller =
      shell()->web_contents()->GetController();

  // Navigate to a starting URL, so there is a history entry to replace.
  GURL url1 = embedded_test_server()->GetURL("/site_isolation/blank.html?1");
  EXPECT_TRUE(NavigateToURL(shell(), url1));

  // Force all future navigations to transfer.
  ShellContentBrowserClient::SetSwapProcessesForRedirect(true);

  NoTransferRequestDelegate no_transfer_request_delegate;
  WebContentsDelegate* old_delegate = shell()->web_contents()->GetDelegate();
  shell()->web_contents()->SetDelegate(&no_transfer_request_delegate);

  // Navigate to a page on A.com with entry replacement. This navigation is
  // cross-site, so the renderer will send it to the browser via OpenURL to give
  // to a new process. It will then be transferred into yet another process due
  // to the call above.
  GURL url2 =
      embedded_test_server()->GetURL("A.com", "/site_isolation/blank.html?2");
  // Used to make sure the second request is cancelled, and there is only one
  // request for url2.
  tracking_delegate().SetTrackedURL(url2);

  // Don't wait for the navigation to complete, since that never happens in
  // this case.
  NavigateToURLContentInitiated(shell(), url2, false, false);

  // There should be one history entry, with url1.
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetCurrentEntryIndex());
  EXPECT_EQ(url1, controller.GetEntryAtIndex(0)->GetURL());

  // Make sure the request for url2 did not complete.
  EXPECT_FALSE(tracking_delegate().WaitForTrackedURLAndGetCompleted());

  shell()->web_contents()->SetDelegate(old_delegate);
}

// Test that verifies that a cross-process transfer retains ability to read
// files encapsulated by HTTP POST body that is forwarded to the new renderer.
// Invalid handling of this scenario has been suspected as the cause of at least
// some of the renderer kills tracked in https://crbug.com/613260.
IN_PROC_BROWSER_TEST_F(CrossSiteTransferTest, PostWithFileData) {
  // Navigate to the page with form that posts via 307 redirection to
  // |redirect_target_url| (cross-site from |form_url|).  Using 307 (rather than
  // 302) redirection is important to preserve the HTTP method and POST body.
  GURL form_url(embedded_test_server()->GetURL(
      "a.com", "/form_that_posts_cross_site.html"));
  GURL redirect_target_url(embedded_test_server()->GetURL("x.com", "/echoall"));
  EXPECT_TRUE(NavigateToURL(shell(), form_url));

  // Prepare a file to upload.
  base::ScopedTempDir temp_dir;
  base::FilePath file_path;
  std::string file_content("test-file-content");
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir.path(), &file_path));
  ASSERT_LT(
      0, base::WriteFile(file_path, file_content.data(), file_content.size()));

  // Fill out the form to refer to the test file.
  std::unique_ptr<FileChooserDelegate> delegate(
      new FileChooserDelegate(file_path));
  shell()->web_contents()->SetDelegate(delegate.get());
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(),
                            "document.getElementById('file').click();"));
  EXPECT_TRUE(delegate->file_chosen());

  // Remember the old process id for a sanity check below.
  int old_process_id = shell()->web_contents()->GetRenderProcessHost()->GetID();

  // Submit the form.
  TestNavigationObserver form_post_observer(shell()->web_contents(), 1);
  EXPECT_TRUE(
      ExecuteScript(shell(), "document.getElementById('file-form').submit();"));
  form_post_observer.Wait();

  // Verify that we arrived at the expected, redirected location.
  EXPECT_EQ(redirect_target_url,
            shell()->web_contents()->GetLastCommittedURL());

  // Verify that the test really verifies access of a *new* renderer process.
  int new_process_id = shell()->web_contents()->GetRenderProcessHost()->GetID();
  ASSERT_NE(new_process_id, old_process_id);

  // MAIN VERIFICATION: Check if the new renderer process is able to read the
  // file.
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      new_process_id, file_path));

  // Verify that POST body got preserved by 307 redirect.  This expectation
  // comes from: https://tools.ietf.org/html/rfc7231#section-6.4.7
  std::string actual_page_body;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      shell()->web_contents(),
      "window.domAutomationController.send("
      "document.getElementsByTagName('pre')[0].innerText);",
      &actual_page_body));
  EXPECT_THAT(actual_page_body, ::testing::HasSubstr(file_content));
  EXPECT_THAT(actual_page_body,
              ::testing::HasSubstr(file_path.BaseName().AsUTF8Unsafe()));
  EXPECT_THAT(actual_page_body,
              ::testing::HasSubstr("form-data; name=\"file\""));
}

}  // namespace content
