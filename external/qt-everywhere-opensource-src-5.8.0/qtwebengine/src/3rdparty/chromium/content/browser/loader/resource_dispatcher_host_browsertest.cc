// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/resource_dispatcher_host.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/browser/shell_network_delegate.h"
#include "net/base/net_errors.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/test/url_request/url_request_mock_http_job.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

using base::ASCIIToUTF16;

namespace content {

class ResourceDispatcherHostBrowserTest : public ContentBrowserTest,
                                          public DownloadManager::Observer {
 public:
  ResourceDispatcherHostBrowserTest() : got_downloads_(false) {}

 protected:
  void SetUpOnMainThread() override {
    base::FilePath path = GetTestFilePath("", "");
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(
            &net::URLRequestMockHTTPJob::AddUrlHandlers, path,
            make_scoped_refptr(content::BrowserThread::GetBlockingPool())));
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&net::URLRequestFailedJob::AddUrlHandler));
  }

  void OnDownloadCreated(DownloadManager* manager,
                         DownloadItem* item) override {
    if (!got_downloads_)
      got_downloads_ = !!manager->InProgressCount();
  }

  void CheckTitleTest(const GURL& url,
                      const std::string& expected_title) {
    base::string16 expected_title16(ASCIIToUTF16(expected_title));
    TitleWatcher title_watcher(shell()->web_contents(), expected_title16);
    NavigateToURL(shell(), url);
    EXPECT_EQ(expected_title16, title_watcher.WaitAndGetTitle());
  }

  bool GetPopupTitle(const GURL& url, base::string16* title) {
    NavigateToURL(shell(), url);

    ShellAddedObserver new_shell_observer;

    // Create dynamic popup.
    if (!ExecuteScript(shell(), "OpenPopup();"))
      return false;

    Shell* new_shell = new_shell_observer.GetShell();
    *title = new_shell->web_contents()->GetTitle();
    return true;
  }

  std::string GetCookies(const GURL& url) {
    return content::GetCookies(
        shell()->web_contents()->GetBrowserContext(), url);
  }

  bool got_downloads() const { return got_downloads_; }

 private:
  bool got_downloads_;
};

// Test title for content created by javascript window.open().
// See http://crbug.com/5988
IN_PROC_BROWSER_TEST_F(ResourceDispatcherHostBrowserTest, DynamicTitle1) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/dynamic1.html"));
  base::string16 title;
  ASSERT_TRUE(GetPopupTitle(url, &title));
  EXPECT_TRUE(base::StartsWith(title, ASCIIToUTF16("My Popup Title"),
              base::CompareCase::SENSITIVE))
      << "Actual title: " << title;
}

// Test title for content created by javascript window.open().
// See http://crbug.com/5988
IN_PROC_BROWSER_TEST_F(ResourceDispatcherHostBrowserTest, DynamicTitle2) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/dynamic2.html"));
  base::string16 title;
  ASSERT_TRUE(GetPopupTitle(url, &title));
  EXPECT_TRUE(base::StartsWith(title, ASCIIToUTF16("My Dynamic Title"),
                               base::CompareCase::SENSITIVE))
      << "Actual title: " << title;
}

IN_PROC_BROWSER_TEST_F(ResourceDispatcherHostBrowserTest,
                       SniffHTMLWithNoContentType) {
  CheckTitleTest(
      net::URLRequestMockHTTPJob::GetMockUrl("content-sniffer-test0.html"),
      "Content Sniffer Test 0");
}

IN_PROC_BROWSER_TEST_F(ResourceDispatcherHostBrowserTest,
                       RespectNoSniffDirective) {
  CheckTitleTest(net::URLRequestMockHTTPJob::GetMockUrl("nosniff-test.html"),
                 "mock.http/nosniff-test.html");
}

IN_PROC_BROWSER_TEST_F(ResourceDispatcherHostBrowserTest,
                       DoNotSniffHTMLFromTextPlain) {
  CheckTitleTest(
      net::URLRequestMockHTTPJob::GetMockUrl("content-sniffer-test1.html"),
      "mock.http/content-sniffer-test1.html");
}

IN_PROC_BROWSER_TEST_F(ResourceDispatcherHostBrowserTest,
                       DoNotSniffHTMLFromImageGIF) {
  CheckTitleTest(
      net::URLRequestMockHTTPJob::GetMockUrl("content-sniffer-test2.html"),
      "mock.http/content-sniffer-test2.html");
}

IN_PROC_BROWSER_TEST_F(ResourceDispatcherHostBrowserTest,
                       SniffNoContentTypeNoData) {
  // Make sure no downloads start.
  BrowserContext::GetDownloadManager(
      shell()->web_contents()->GetBrowserContext())->AddObserver(this);
  CheckTitleTest(
      net::URLRequestMockHTTPJob::GetMockUrl("content-sniffer-test3.html"),
      "Content Sniffer Test 3");
  EXPECT_EQ(1u, Shell::windows().size());
  ASSERT_FALSE(got_downloads());
}

IN_PROC_BROWSER_TEST_F(ResourceDispatcherHostBrowserTest,
                       ContentDispositionEmpty) {
  CheckTitleTest(
      net::URLRequestMockHTTPJob::GetMockUrl("content-disposition-empty.html"),
      "success");
}

IN_PROC_BROWSER_TEST_F(ResourceDispatcherHostBrowserTest,
                       ContentDispositionInline) {
  CheckTitleTest(
      net::URLRequestMockHTTPJob::GetMockUrl("content-disposition-inline.html"),
      "success");
}

// Test for bug #1091358.
IN_PROC_BROWSER_TEST_F(ResourceDispatcherHostBrowserTest, SyncXMLHttpRequest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  NavigateToURL(
      shell(), embedded_test_server()->GetURL("/sync_xmlhttprequest.html"));

  // Let's check the XMLHttpRequest ran successfully.
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(), "window.domAutomationController.send(DidSyncRequestSucceed());",
      &success));
  EXPECT_TRUE(success);
}

// If this flakes, use http://crbug.com/62776.
IN_PROC_BROWSER_TEST_F(ResourceDispatcherHostBrowserTest,
                       SyncXMLHttpRequest_Disallowed) {
  ASSERT_TRUE(embedded_test_server()->Start());
  NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("/sync_xmlhttprequest_disallowed.html"));

  // Let's check the XMLHttpRequest ran successfully.
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(), "window.domAutomationController.send(DidSucceed());", &success));
  EXPECT_TRUE(success);
}

// Test for bug #1159553 -- A synchronous xhr (whose content-type is
// downloadable) would trigger download and hang the renderer process,
// if executed while navigating to a new page.
// Disabled on Mac: see http://crbug.com/56264
#if defined(OS_MACOSX)
#define MAYBE_SyncXMLHttpRequest_DuringUnload \
  DISABLED_SyncXMLHttpRequest_DuringUnload
#else
#define MAYBE_SyncXMLHttpRequest_DuringUnload SyncXMLHttpRequest_DuringUnload
#endif
IN_PROC_BROWSER_TEST_F(ResourceDispatcherHostBrowserTest,
                       MAYBE_SyncXMLHttpRequest_DuringUnload) {
  ASSERT_TRUE(embedded_test_server()->Start());
  BrowserContext::GetDownloadManager(
      shell()->web_contents()->GetBrowserContext())->AddObserver(this);

  CheckTitleTest(
      embedded_test_server()->GetURL("/sync_xmlhttprequest_during_unload.html"),
      "sync xhr on unload");

  // Navigate to a new page, to dispatch unload event and trigger xhr.
  // (the bug would make this step hang the renderer).
  CheckTitleTest(
      embedded_test_server()->GetURL("/title2.html"), "Title Of Awesomeness");

  ASSERT_FALSE(got_downloads());
}

// Flaky everywhere. http://crbug.com/130404
// Tests that onunload is run for cross-site requests.  (Bug 1114994)
IN_PROC_BROWSER_TEST_F(ResourceDispatcherHostBrowserTest,
                       DISABLED_CrossSiteOnunloadCookie) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url = embedded_test_server()->GetURL("/onunload_cookie.html");
  CheckTitleTest(url, "set cookie on unload");

  // Navigate to a new cross-site page, to dispatch unload event and set the
  // cookie.
  CheckTitleTest(
      net::URLRequestMockHTTPJob::GetMockUrl("content-sniffer-test0.html"),
      "Content Sniffer Test 0");

  // Check that the cookie was set.
  EXPECT_EQ("onunloadCookie=foo", GetCookies(url));
}

// If this flakes, use http://crbug.com/130404
// Tests that onunload is run for cross-site requests to URLs that complete
// without network loads (e.g., about:blank, data URLs).
IN_PROC_BROWSER_TEST_F(ResourceDispatcherHostBrowserTest,
                       DISABLED_CrossSiteImmediateLoadOnunloadCookie) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url = embedded_test_server()->GetURL("/onunload_cookie.html");
  CheckTitleTest(url, "set cookie on unload");

  // Navigate to a cross-site page that loads immediately without making a
  // network request.  The unload event should still be run.
  NavigateToURL(shell(), GURL(url::kAboutBlankURL));

  // Check that the cookie was set.
  EXPECT_EQ("onunloadCookie=foo", GetCookies(url));
}

namespace {

// Handles |request| by serving a redirect response.
std::unique_ptr<net::test_server::HttpResponse> NoContentResponseHandler(
    const std::string& path,
    const net::test_server::HttpRequest& request) {
  if (!base::StartsWith(path, request.relative_url,
                        base::CompareCase::SENSITIVE))
    return std::unique_ptr<net::test_server::HttpResponse>();

  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_code(net::HTTP_NO_CONTENT);
  return std::move(http_response);
}

}  // namespace

// Tests that the unload handler is not run for 204 responses.
// If this flakes use http://crbug.com/80596.
IN_PROC_BROWSER_TEST_F(ResourceDispatcherHostBrowserTest,
                       CrossSiteNoUnloadOn204) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Start with a URL that sets a cookie in its unload handler.
  GURL url = embedded_test_server()->GetURL("/onunload_cookie.html");
  CheckTitleTest(url, "set cookie on unload");

  // Navigate to a cross-site URL that returns a 204 No Content response.
  const char kNoContentPath[] = "/nocontent";
  embedded_test_server()->RegisterRequestHandler(
      base::Bind(&NoContentResponseHandler, kNoContentPath));
  NavigateToURL(shell(), embedded_test_server()->GetURL(kNoContentPath));

  // Check that the unload cookie was not set.
  EXPECT_EQ("", GetCookies(url));
}

// Tests that the onbeforeunload and onunload logic is short-circuited if the
// old renderer is gone.  In that case, we don't want to wait for the old
// renderer to run the handlers.
// We need to disable this on Mac because the crash causes the OS CrashReporter
// process to kick in to analyze the poor dead renderer.  Unfortunately, if the
// app isn't stripped of debug symbols, this takes about five minutes to
// complete and isn't conducive to quick turnarounds. As we don't currently
// strip the app on the build bots, this is bad times.
#if defined(OS_MACOSX)
#define MAYBE_CrossSiteAfterCrash DISABLED_CrossSiteAfterCrash
#else
#define MAYBE_CrossSiteAfterCrash CrossSiteAfterCrash
#endif
IN_PROC_BROWSER_TEST_F(ResourceDispatcherHostBrowserTest,
                       MAYBE_CrossSiteAfterCrash) {
  // Make sure we have a live process before trying to kill it.
  NavigateToURL(shell(), GURL("about:blank"));

  // Cause the renderer to crash.
  RenderProcessHostWatcher crash_observer(
      shell()->web_contents(),
      RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  NavigateToURL(shell(), GURL(kChromeUICrashURL));
  // Wait for browser to notice the renderer crash.
  crash_observer.Wait();

  // Navigate to a new cross-site page.  The browser should not wait around for
  // the old renderer's on{before}unload handlers to run.
  CheckTitleTest(
      net::URLRequestMockHTTPJob::GetMockUrl("content-sniffer-test0.html"),
      "Content Sniffer Test 0");
}

// Tests that cross-site navigations work when the new page does not go through
// the BufferedEventHandler (e.g., non-http{s} URLs).  (Bug 1225872)
IN_PROC_BROWSER_TEST_F(ResourceDispatcherHostBrowserTest,
                       CrossSiteNavigationNonBuffered) {
  // Start with an HTTP page.
  CheckTitleTest(
      net::URLRequestMockHTTPJob::GetMockUrl("content-sniffer-test0.html"),
      "Content Sniffer Test 0");

  // Now load a file:// page, which does not use the BufferedEventHandler.
  // Make sure that the page loads and displays a title, and doesn't get stuck.
  GURL url = GetTestUrl("", "title2.html");
  CheckTitleTest(url, "Title Of Awesomeness");
}

// Flaky everywhere. http://crbug.com/130404
// Tests that a cross-site navigation to an error page (resulting in the link
// doctor page) still runs the onunload handler and can support navigations
// away from the link doctor page.  (Bug 1235537)
IN_PROC_BROWSER_TEST_F(ResourceDispatcherHostBrowserTest,
                       DISABLED_CrossSiteNavigationErrorPage) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/onunload_cookie.html"));
  CheckTitleTest(url, "set cookie on unload");

  // Navigate to a new cross-site URL that results in an error.
  // TODO(creis): If this causes crashes or hangs, it might be for the same
  // reason as ErrorPageTest::DNSError.  See bug 1199491 and
  // http://crbug.com/22877.
  GURL failed_url = net::URLRequestFailedJob::GetMockHttpUrl(
      net::ERR_NAME_NOT_RESOLVED);
  NavigateToURL(shell(), failed_url);

  EXPECT_NE(ASCIIToUTF16("set cookie on unload"),
            shell()->web_contents()->GetTitle());

  // Check that the cookie was set, meaning that the onunload handler ran.
  EXPECT_EQ("onunloadCookie=foo", GetCookies(url));

  // Check that renderer-initiated navigations still work.  In a previous bug,
  // the ResourceDispatcherHost would think that such navigations were
  // cross-site, because we didn't clean up from the previous request.  Since
  // WebContentsImpl was in the NORMAL state, it would ignore the attempt to run
  // the onunload handler, and the navigation would fail. We can't test by
  // redirecting to javascript:window.location='someURL', since javascript:
  // URLs are prohibited by policy from interacting with sensitive chrome
  // pages of which the error page is one.  Instead, use automation to kick
  // off the navigation, and wait to see that the tab loads.
  base::string16 expected_title16(ASCIIToUTF16("Title Of Awesomeness"));
  TitleWatcher title_watcher(shell()->web_contents(), expected_title16);

  bool success;
  GURL test_url(embedded_test_server()->GetURL("/title2.html"));
  std::string redirect_script = "window.location='" +
      test_url.possibly_invalid_spec() + "';" +
      "window.domAutomationController.send(true);";
  EXPECT_TRUE(ExecuteScriptAndExtractBool(shell(), redirect_script, &success));
  EXPECT_EQ(expected_title16, title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(ResourceDispatcherHostBrowserTest,
                       CrossSiteNavigationErrorPage2) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/title2.html"));
  CheckTitleTest(url, "Title Of Awesomeness");

  // Navigate to a new cross-site URL that results in an error.
  // TODO(creis): If this causes crashes or hangs, it might be for the same
  // reason as ErrorPageTest::DNSError.  See bug 1199491 and
  // http://crbug.com/22877.
  GURL failed_url = net::URLRequestFailedJob::GetMockHttpUrl(
      net::ERR_NAME_NOT_RESOLVED);

  NavigateToURL(shell(), failed_url);
  EXPECT_NE(ASCIIToUTF16("Title Of Awesomeness"),
            shell()->web_contents()->GetTitle());

  // Repeat navigation.  We are testing that this completes.
  NavigateToURL(shell(), failed_url);
  EXPECT_NE(ASCIIToUTF16("Title Of Awesomeness"),
            shell()->web_contents()->GetTitle());
}

IN_PROC_BROWSER_TEST_F(ResourceDispatcherHostBrowserTest,
                       CrossOriginRedirectBlocked) {
  // We expect the following URL requests from this test:
  // 1-  http://mock.http/cross-origin-redirect-blocked.html
  // 2-  http://mock.http/redirect-to-title2.html
  // 3-  http://mock.http/title2.html
  //
  // If the redirect in #2 were not blocked, we'd also see a request
  // for http://mock.http:4000/title2.html, and the title would be different.
  CheckTitleTest(net::URLRequestMockHTTPJob::GetMockUrl(
                     "cross-origin-redirect-blocked.html"),
                 "Title Of More Awesomeness");
}

// Tests that ResourceRequestInfoImpl is updated correctly on failed
// requests, to prevent calling Read on a request that has already failed.
// See bug 40250.
IN_PROC_BROWSER_TEST_F(ResourceDispatcherHostBrowserTest,
                       CrossSiteFailedRequest) {
  // Visit another URL first to trigger a cross-site navigation.
  NavigateToURL(shell(), GetTestUrl("", "simple_page.html"));

  // Visit a URL that fails without calling ResourceDispatcherHost::Read.
  GURL broken_url("chrome://theme");
  NavigateToURL(shell(), broken_url);
}

namespace {

std::unique_ptr<net::test_server::HttpResponse> HandleRedirectRequest(
    const std::string& request_path,
    const net::test_server::HttpRequest& request) {
  if (!base::StartsWith(request.relative_url, request_path,
                        base::CompareCase::SENSITIVE))
    return std::unique_ptr<net::test_server::HttpResponse>();

  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_code(net::HTTP_FOUND);
  http_response->AddCustomHeader(
      "Location", request.relative_url.substr(request_path.length()));
  return std::move(http_response);
}

}  // namespace

// Test that we update the cookie policy URLs correctly when transferring
// navigations.
IN_PROC_BROWSER_TEST_F(ResourceDispatcherHostBrowserTest, CookiePolicy) {
  ASSERT_TRUE(embedded_test_server()->Start());
  embedded_test_server()->RegisterRequestHandler(
      base::Bind(&HandleRedirectRequest, "/redirect?"));

  std::string set_cookie_url(base::StringPrintf(
      "http://localhost:%u/set_cookie.html", embedded_test_server()->port()));
  GURL url(embedded_test_server()->GetURL("/redirect?" + set_cookie_url));

  ShellContentBrowserClient::SetSwapProcessesForRedirect(true);
  ShellNetworkDelegate::SetAcceptAllCookies(false);

  CheckTitleTest(url, "cookie set");
}

class PageTransitionResourceDispatcherHostDelegate
    : public ResourceDispatcherHostDelegate {
 public:
  PageTransitionResourceDispatcherHostDelegate(GURL watch_url)
    : watch_url_(watch_url) {}

  // ResourceDispatcherHostDelegate implementation:
  void RequestBeginning(net::URLRequest* request,
                        ResourceContext* resource_context,
                        AppCacheService* appcache_service,
                        ResourceType resource_type,
                        ScopedVector<ResourceThrottle>* throttles) override {
    if (request->url() == watch_url_) {
      const ResourceRequestInfo* info =
          ResourceRequestInfo::ForRequest(request);
      page_transition_ = info->GetPageTransition();
    }
  }

  ui::PageTransition page_transition() { return page_transition_; }

 private:
  GURL watch_url_;
  ui::PageTransition page_transition_;
};

// Test that ui::PAGE_TRANSITION_CLIENT_REDIRECT is correctly set
// when encountering a meta refresh tag.
IN_PROC_BROWSER_TEST_F(ResourceDispatcherHostBrowserTest,
                       PageTransitionClientRedirect) {
  ASSERT_TRUE(embedded_test_server()->Start());

  PageTransitionResourceDispatcherHostDelegate delegate(
      embedded_test_server()->GetURL("/title1.html"));
  ResourceDispatcherHost::Get()->SetDelegate(&delegate);

  NavigateToURLBlockUntilNavigationsComplete(
      shell(),
      embedded_test_server()->GetURL("/client_redirect.html"),
      2);

  EXPECT_TRUE(
      delegate.page_transition() & ui::PAGE_TRANSITION_CLIENT_REDIRECT);
}

namespace {

// Checks whether the given urls are requested, and that IsUsingLofi() returns
// the appropriate value when the Lo-Fi state is set.
class LoFiModeResourceDispatcherHostDelegate
    : public ResourceDispatcherHostDelegate {
 public:
  LoFiModeResourceDispatcherHostDelegate(const GURL& main_frame_url,
                                         const GURL& subresource_url,
                                         const GURL& iframe_url)
      : main_frame_url_(main_frame_url),
        subresource_url_(subresource_url),
        iframe_url_(iframe_url),
        main_frame_url_seen_(false),
        subresource_url_seen_(false),
        iframe_url_seen_(false),
        use_lofi_(false),
        should_enable_lofi_mode_called_(false) {}

  ~LoFiModeResourceDispatcherHostDelegate() override {}

  // ResourceDispatcherHostDelegate implementation:
  void RequestBeginning(net::URLRequest* request,
                        ResourceContext* resource_context,
                        AppCacheService* appcache_service,
                        ResourceType resource_type,
                        ScopedVector<ResourceThrottle>* throttles) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);
    if (request->url() != main_frame_url_ && request->url() != subresource_url_
        && request->url() != iframe_url_)
      return;
    if (request->url() == main_frame_url_) {
      EXPECT_FALSE(main_frame_url_seen_);
      main_frame_url_seen_ = true;
    } else if (request->url() == subresource_url_) {
      EXPECT_TRUE(main_frame_url_seen_);
      EXPECT_FALSE(subresource_url_seen_);
      subresource_url_seen_ = true;
    } else if (request->url() == iframe_url_) {
      EXPECT_TRUE(main_frame_url_seen_);
      EXPECT_FALSE(iframe_url_seen_);
      iframe_url_seen_ = true;
    }
    EXPECT_EQ(use_lofi_, info->IsUsingLoFi());
  }

  void SetDelegate() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    ResourceDispatcherHost::Get()->SetDelegate(this);
  }

  bool ShouldEnableLoFiMode(
      const net::URLRequest& request,
      content::ResourceContext* resource_context) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    EXPECT_FALSE(should_enable_lofi_mode_called_);
    should_enable_lofi_mode_called_ = true;
    EXPECT_EQ(main_frame_url_, request.url());
    return use_lofi_;
  }

  void Reset(bool use_lofi) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    main_frame_url_seen_ = false;
    subresource_url_seen_ = false;
    iframe_url_seen_ = false;
    use_lofi_ = use_lofi;
    should_enable_lofi_mode_called_ = false;
  }

  void CheckResourcesRequested(bool should_enable_lofi_mode_called) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    EXPECT_EQ(should_enable_lofi_mode_called, should_enable_lofi_mode_called_);
    EXPECT_TRUE(main_frame_url_seen_);
    EXPECT_TRUE(subresource_url_seen_);
    EXPECT_TRUE(iframe_url_seen_);
  }

 private:
  const GURL main_frame_url_;
  const GURL subresource_url_;
  const GURL iframe_url_;

  bool main_frame_url_seen_;
  bool subresource_url_seen_;
  bool iframe_url_seen_;
  bool use_lofi_;
  bool should_enable_lofi_mode_called_;

  DISALLOW_COPY_AND_ASSIGN(LoFiModeResourceDispatcherHostDelegate);
};

}  // namespace

class LoFiResourceDispatcherHostBrowserTest : public ContentBrowserTest {
 public:
  ~LoFiResourceDispatcherHostBrowserTest() override {}

 protected:
  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(embedded_test_server()->Start());

    delegate_.reset(new LoFiModeResourceDispatcherHostDelegate(
        embedded_test_server()->GetURL("/page_with_iframe.html"),
        embedded_test_server()->GetURL("/image.jpg"),
        embedded_test_server()->GetURL("/title1.html")));

    content::BrowserThread::PostTask(
           content::BrowserThread::IO,
           FROM_HERE,
           base::Bind(&LoFiModeResourceDispatcherHostDelegate::SetDelegate,
                      base::Unretained(delegate_.get())));
  }

  void Reset(bool use_lofi) {
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&LoFiModeResourceDispatcherHostDelegate::Reset,
                   base::Unretained(delegate_.get()), use_lofi));
  }

  void CheckResourcesRequested(
      bool should_enable_lofi_mode_called) {
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(
            &LoFiModeResourceDispatcherHostDelegate::CheckResourcesRequested,
            base::Unretained(delegate_.get()), should_enable_lofi_mode_called));
  }

 private:
  std::unique_ptr<LoFiModeResourceDispatcherHostDelegate> delegate_;
};

// Test that navigating with ShouldEnableLoFiMode returning true fetches the
// resources with LOFI_ON.
IN_PROC_BROWSER_TEST_F(LoFiResourceDispatcherHostBrowserTest,
                       ShouldEnableLoFiModeOn) {
  // Navigate with ShouldEnableLoFiMode returning true.
  Reset(true);
  NavigateToURLBlockUntilNavigationsComplete(
      shell(), embedded_test_server()->GetURL("/page_with_iframe.html"), 1);
  CheckResourcesRequested(true);
}

// Test that navigating with ShouldEnableLoFiMode returning false fetches the
// resources with LOFI_OFF.
IN_PROC_BROWSER_TEST_F(LoFiResourceDispatcherHostBrowserTest,
                       ShouldEnableLoFiModeOff) {
  // Navigate with ShouldEnableLoFiMode returning false.
  NavigateToURLBlockUntilNavigationsComplete(
      shell(), embedded_test_server()->GetURL("/page_with_iframe.html"), 1);
  CheckResourcesRequested(true);
}

// Test that reloading calls ShouldEnableLoFiMode again and changes the Lo-Fi
// state.
IN_PROC_BROWSER_TEST_F(LoFiResourceDispatcherHostBrowserTest,
                       ShouldEnableLoFiModeReload) {
  // Navigate with ShouldEnableLoFiMode returning false.
  NavigateToURLBlockUntilNavigationsComplete(
      shell(), embedded_test_server()->GetURL("/page_with_iframe.html"), 1);
  CheckResourcesRequested(true);

  // Reload. ShouldEnableLoFiMode should be called.
  Reset(true);
  ReloadBlockUntilNavigationsComplete(shell(), 1);
  CheckResourcesRequested(true);
}

// Test that navigating backwards calls ShouldEnableLoFiMode again and changes
// the Lo-Fi state.
IN_PROC_BROWSER_TEST_F(LoFiResourceDispatcherHostBrowserTest,
                       ShouldEnableLoFiModeNavigateBackThenForward) {
  // Navigate with ShouldEnableLoFiMode returning false.
  NavigateToURLBlockUntilNavigationsComplete(
      shell(), embedded_test_server()->GetURL("/page_with_iframe.html"), 1);
  CheckResourcesRequested(true);

  // Go to a different page.
  NavigateToURLBlockUntilNavigationsComplete(shell(), GURL("about:blank"), 1);

  // Go back with ShouldEnableLoFiMode returning true.
  Reset(true);
  TestNavigationObserver tab_observer(shell()->web_contents(), 1);
  shell()->GoBackOrForward(-1);
  tab_observer.Wait();
  CheckResourcesRequested(true);
}

// Test that reloading with Lo-Fi disabled doesn't call ShouldEnableLoFiMode and
// already has LOFI_OFF.
IN_PROC_BROWSER_TEST_F(LoFiResourceDispatcherHostBrowserTest,
                       ShouldEnableLoFiModeReloadDisableLoFi) {
  // Navigate with ShouldEnableLoFiMode returning true.
  Reset(true);
  NavigateToURLBlockUntilNavigationsComplete(
      shell(), embedded_test_server()->GetURL("/page_with_iframe.html"), 1);
  CheckResourcesRequested(true);

  // Reload with Lo-Fi disabled.
  Reset(false);
  TestNavigationObserver tab_observer(shell()->web_contents(), 1);
  shell()->web_contents()->GetController().ReloadDisableLoFi(true);
  tab_observer.Wait();
  CheckResourcesRequested(false);
}

namespace {

struct RequestDataForDelegate {
  const GURL url;
  const GURL first_party;
  const url::Origin initiator;

  RequestDataForDelegate(const GURL& url,
                         const GURL& first_party,
                         const url::Origin initiator)
      : url(url), first_party(first_party), initiator(initiator) {}
};

// Captures calls to 'RequestBeginning' and records the URL, first-party for
// cookies, and initiator.
class RequestDataResourceDispatcherHostDelegate
    : public ResourceDispatcherHostDelegate {
 public:
  RequestDataResourceDispatcherHostDelegate() {}

  const ScopedVector<RequestDataForDelegate>& data() { return requests_; }

  // ResourceDispatcherHostDelegate implementation:
  void RequestBeginning(net::URLRequest* request,
                        ResourceContext* resource_context,
                        AppCacheService* appcache_service,
                        ResourceType resource_type,
                        ScopedVector<ResourceThrottle>* throttles) override {
    requests_.push_back(new RequestDataForDelegate(
        request->url(), request->first_party_for_cookies(),
        request->initiator()));
  }

  void SetDelegate() { ResourceDispatcherHost::Get()->SetDelegate(this); }

 private:
  ScopedVector<RequestDataForDelegate> requests_;

  DISALLOW_COPY_AND_ASSIGN(RequestDataResourceDispatcherHostDelegate);
};

const GURL kURLWithUniqueOrigin("data:,");

}  // namespace

class RequestDataResourceDispatcherHostBrowserTest : public ContentBrowserTest {
 public:
  ~RequestDataResourceDispatcherHostBrowserTest() override {}

 protected:
  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(embedded_test_server()->Start());

    delegate_.reset(new RequestDataResourceDispatcherHostDelegate());

    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&RequestDataResourceDispatcherHostDelegate::SetDelegate,
                   base::Unretained(delegate_.get())));
  }

 protected:
  std::unique_ptr<RequestDataResourceDispatcherHostDelegate> delegate_;
};

IN_PROC_BROWSER_TEST_F(RequestDataResourceDispatcherHostBrowserTest, Basic) {
  GURL top_url(embedded_test_server()->GetURL("/simple_page.html"));
  url::Origin top_origin(top_url);

  NavigateToURLBlockUntilNavigationsComplete(shell(), top_url, 1);

  EXPECT_EQ(1u, delegate_->data().size());

  // User-initiated top-level navigations have a first-party and initiator that
  // matches the URL to which they navigate.
  EXPECT_EQ(top_url, delegate_->data()[0]->url);
  EXPECT_EQ(top_url, delegate_->data()[0]->first_party);
  EXPECT_EQ(top_origin, delegate_->data()[0]->initiator);
}

IN_PROC_BROWSER_TEST_F(RequestDataResourceDispatcherHostBrowserTest,
                       SameOriginNested) {
  GURL top_url(embedded_test_server()->GetURL("/page_with_iframe.html"));
  GURL image_url(embedded_test_server()->GetURL("/image.jpg"));
  GURL nested_url(embedded_test_server()->GetURL("/title1.html"));
  url::Origin top_origin(top_url);

  NavigateToURLBlockUntilNavigationsComplete(shell(), top_url, 1);

  EXPECT_EQ(3u, delegate_->data().size());

  // User-initiated top-level navigations have a first-party and initiator that
  // matches the URL to which they navigate.
  EXPECT_EQ(top_url, delegate_->data()[0]->url);
  EXPECT_EQ(top_url, delegate_->data()[0]->first_party);
  EXPECT_EQ(top_origin, delegate_->data()[0]->initiator);

  // Subresource requests have a first-party and initiator that matches the
  // document in which they're embedded.
  EXPECT_EQ(image_url, delegate_->data()[1]->url);
  EXPECT_EQ(top_url, delegate_->data()[1]->first_party);
  EXPECT_EQ(top_origin, delegate_->data()[1]->initiator);

  // Same-origin nested frames have a first-party and initiator that matches
  // the document in which they're embedded.
  EXPECT_EQ(nested_url, delegate_->data()[2]->url);
  EXPECT_EQ(top_url, delegate_->data()[2]->first_party);
  EXPECT_EQ(top_origin, delegate_->data()[2]->initiator);
}

IN_PROC_BROWSER_TEST_F(RequestDataResourceDispatcherHostBrowserTest,
                       SameOriginAuxiliary) {
  GURL top_url(embedded_test_server()->GetURL("/simple_links.html"));
  GURL auxiliary_url(embedded_test_server()->GetURL("/title2.html"));
  url::Origin top_origin(top_url);

  NavigateToURLBlockUntilNavigationsComplete(shell(), top_url, 1);

  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(),
      "window.domAutomationController.send(clickSameSiteNewWindowLink());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();
  WaitForLoadStop(new_shell->web_contents());

  EXPECT_EQ(2u, delegate_->data().size());

  // User-initiated top-level navigations have a first-party and initiator that
  // matches the URL to which they navigate, even if they fail to load.
  EXPECT_EQ(top_url, delegate_->data()[0]->url);
  EXPECT_EQ(top_url, delegate_->data()[0]->first_party);
  EXPECT_EQ(top_origin, delegate_->data()[0]->initiator);

  // Auxiliary navigations have a first-party that matches the URL to which they
  // navigate, and an initiator that matches the document that triggered them.
  EXPECT_EQ(auxiliary_url, delegate_->data()[1]->url);
  EXPECT_EQ(auxiliary_url, delegate_->data()[1]->first_party);
  EXPECT_EQ(top_origin, delegate_->data()[1]->initiator);
}

IN_PROC_BROWSER_TEST_F(RequestDataResourceDispatcherHostBrowserTest,
                       CrossOriginAuxiliary) {
  GURL top_url(embedded_test_server()->GetURL("/simple_links.html"));
  GURL auxiliary_url(embedded_test_server()->GetURL("foo.com", "/title2.html"));
  url::Origin top_origin(top_url);

  NavigateToURLBlockUntilNavigationsComplete(shell(), top_url, 1);

  const char kReplacePortNumber[] =
      "window.domAutomationController.send(setPortNumber(%d));";
  uint16_t port_number = embedded_test_server()->port();
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(), base::StringPrintf(kReplacePortNumber, port_number), &success));
  success = false;

  ShellAddedObserver new_shell_observer;
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(),
      "window.domAutomationController.send(clickCrossSiteNewWindowLink());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();
  WaitForLoadStop(new_shell->web_contents());

  EXPECT_EQ(2u, delegate_->data().size());

  // User-initiated top-level navigations have a first-party and initiator that
  // matches the URL to which they navigate, even if they fail to load.
  EXPECT_EQ(top_url, delegate_->data()[0]->url);
  EXPECT_EQ(top_url, delegate_->data()[0]->first_party);
  EXPECT_EQ(top_origin, delegate_->data()[0]->initiator);

  // Auxiliary navigations have a first-party that matches the URL to which they
  // navigate, and an initiator that matches the document that triggered them.
  EXPECT_EQ(auxiliary_url, delegate_->data()[1]->url);
  EXPECT_EQ(auxiliary_url, delegate_->data()[1]->first_party);
  EXPECT_EQ(top_origin, delegate_->data()[1]->initiator);
}

IN_PROC_BROWSER_TEST_F(RequestDataResourceDispatcherHostBrowserTest,
                       FailedNavigation) {
  // Navigating to this URL will fail, as we haven't taught the host resolver
  // about 'a.com'.
  GURL top_url(embedded_test_server()->GetURL("a.com", "/simple_page.html"));
  url::Origin top_origin(top_url);

  NavigateToURLBlockUntilNavigationsComplete(shell(), top_url, 1);

  EXPECT_EQ(1u, delegate_->data().size());

  // User-initiated top-level navigations have a first-party and initiator that
  // matches the URL to which they navigate, even if they fail to load.
  EXPECT_EQ(top_url, delegate_->data()[0]->url);
  EXPECT_EQ(top_url, delegate_->data()[0]->first_party);
  EXPECT_EQ(top_origin, delegate_->data()[0]->initiator);
}

IN_PROC_BROWSER_TEST_F(RequestDataResourceDispatcherHostBrowserTest,
                       CrossOriginNested) {
  host_resolver()->AddRule("*", "127.0.0.1");
  GURL top_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL top_js_url(
      embedded_test_server()->GetURL("a.com", "/tree_parser_util.js"));
  GURL nested_url(embedded_test_server()->GetURL(
      "b.com", "/cross_site_iframe_factory.html?b()"));
  GURL nested_js_url(
      embedded_test_server()->GetURL("b.com", "/tree_parser_util.js"));
  url::Origin top_origin(top_url);
  url::Origin nested_origin(nested_url);

  NavigateToURLBlockUntilNavigationsComplete(shell(), top_url, 1);

  EXPECT_EQ(4u, delegate_->data().size());

  // User-initiated top-level navigations have a first-party and initiator that
  // matches the URL to which they navigate.
  EXPECT_EQ(top_url, delegate_->data()[0]->url);
  EXPECT_EQ(top_url, delegate_->data()[0]->first_party);
  EXPECT_EQ(top_origin, delegate_->data()[0]->initiator);

  EXPECT_EQ(top_js_url, delegate_->data()[1]->url);
  EXPECT_EQ(top_url, delegate_->data()[1]->first_party);
  EXPECT_EQ(top_origin, delegate_->data()[1]->initiator);

  // Cross-origin frames have a first-party and initiator that matches the URL
  // in which they're embedded.
  EXPECT_EQ(nested_url, delegate_->data()[2]->url);
  EXPECT_EQ(top_url, delegate_->data()[2]->first_party);
  EXPECT_EQ(top_origin, delegate_->data()[2]->initiator);

  // Cross-origin subresource requests have a unique first-party, and an
  // initiator that matches the document in which they're embedded.
  EXPECT_EQ(nested_js_url, delegate_->data()[3]->url);
  EXPECT_EQ(kURLWithUniqueOrigin, delegate_->data()[3]->first_party);
  EXPECT_EQ(nested_origin, delegate_->data()[3]->initiator);
}

}  // namespace content
