// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

using net::test_server::HttpResponse;
using net::test_server::HttpRequest;
using net::test_server::BasicHttpResponse;

const char kCountedHtmlPath[] = "/counted.html";
const char kCookieHtmlPath[] = "/cookie.html";

class AsyncRevalidationManagerBrowserTest : public ContentBrowserTest {
 protected:
  AsyncRevalidationManagerBrowserTest() {}
  ~AsyncRevalidationManagerBrowserTest() override {}

  void SetUp() override {
    base::FeatureList::ClearInstanceForTesting();
    std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
    feature_list->InitializeFromCommandLine(
        "StaleWhileRevalidate2", std::string());
    base::FeatureList::SetInstance(std::move(feature_list));

    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    ContentBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    embedded_test_server()->StartAcceptingConnections();
  }

  base::RunLoop* run_loop() { return &run_loop_; }
  int requests_counted() const { return requests_counted_; }

  // This method lacks diagnostics for the failure case because TitleWatcher
  // will just wait until the test times out if |expected_title| does not
  // appear.
  bool TitleBecomes(const GURL& url, const std::string& expected_title) {
    base::string16 expected_title16(base::ASCIIToUTF16(expected_title));
    TitleWatcher title_watcher(shell()->web_contents(), expected_title16);
    NavigateToURL(shell(), url);
    return title_watcher.WaitAndGetTitle() == expected_title16;
  }

  void RegisterCountingRequestHandler() {
    embedded_test_server()->RegisterRequestHandler(base::Bind(
        &AsyncRevalidationManagerBrowserTest::CountingRequestHandler, this));
  }

  void RegisterCookieRequestHandler() {
    embedded_test_server()->RegisterRequestHandler(base::Bind(
        &AsyncRevalidationManagerBrowserTest::CookieRequestHandler, this));
  }

 private:
  // A request handler which increases the number in the title tag on every
  // request.
  std::unique_ptr<HttpResponse> CountingRequestHandler(
      const HttpRequest& request) {
    if (request.relative_url != kCountedHtmlPath)
      return nullptr;

    int version = ++requests_counted_;

    std::unique_ptr<BasicHttpResponse> http_response(
        StaleWhileRevalidateHeaders());
    http_response->set_content(
        base::StringPrintf("<title>Version %d</title>", version));

    // The second time this handler is run is the async revalidation. Tests can
    // use this for synchronisation.
    if (version == 2)
      run_loop_.Quit();
    return std::move(http_response);
  }

  // A request handler which increases a cookie value on every request.
  std::unique_ptr<HttpResponse> CookieRequestHandler(
      const HttpRequest& request) {
    static const char kHtml[] =
        "<script>\n"
        "var intervalId;\n"
        "function checkCookie() {\n"
        "  if (document.cookie.search(/version=2/) != -1) {\n"
        "    clearInterval(intervalId);\n"
        "    document.title = \"PASS\";\n"
        "  }\n"
        "}\n"
        "intervalId = setInterval(checkCookie, 10);\n"
        "</script>\n"
        "<title>Loaded</title>\n";

    if (request.relative_url != kCookieHtmlPath)
      return nullptr;

    int version = ++requests_counted_;

    std::unique_ptr<BasicHttpResponse> http_response(
        StaleWhileRevalidateHeaders());
    http_response->AddCustomHeader("Set-Cookie",
                                   base::StringPrintf("version=%d", version));
    http_response->set_content(kHtml);

    return std::move(http_response);
  }

  // Generate the standard response headers common to all request handlers.
  std::unique_ptr<BasicHttpResponse> StaleWhileRevalidateHeaders() {
    std::unique_ptr<BasicHttpResponse> http_response(new BasicHttpResponse);
    http_response->set_code(net::HTTP_OK);
    http_response->set_content_type("text/html; charset=utf-8");
    http_response->AddCustomHeader("Cache-Control",
                                   "max-age=0, stale-while-revalidate=86400");
    // A validator is needed for revalidations, and hence
    // stale-while-revalidate, to work.
    std::string etag = base::StringPrintf(
        "\"AsyncRevalidationManagerBrowserTest%d\"", requests_counted_);
    http_response->AddCustomHeader("ETag", etag);
    return http_response;
  }

  base::RunLoop run_loop_;
  int requests_counted_ = 0;

  DISALLOW_COPY_AND_ASSIGN(AsyncRevalidationManagerBrowserTest);
};

// Verify that the "Cache-Control: stale-while-revalidate" directive correctly
// triggers an async revalidation.
IN_PROC_BROWSER_TEST_F(AsyncRevalidationManagerBrowserTest,
                       StaleWhileRevalidateIsApplied) {
  // PlzNavigate: Stale while revalidate is disabled.
  // TODO(clamy): Re-enable the test when there is support.
  if (IsBrowserSideNavigationEnabled())
    return;
  RegisterCountingRequestHandler();
  GURL url(embedded_test_server()->GetURL(kCountedHtmlPath));

  EXPECT_TRUE(TitleBecomes(url, "Version 1"));

  // The first request happens synchronously.
  EXPECT_EQ(1, requests_counted());

  // Force the renderer to be destroyed so that the Blink cache doesn't
  // interfere with the result.
  NavigateToURL(shell(), GURL("about:blank"));

  // Load the page again. We should get the stale version from the cache.
  EXPECT_TRUE(TitleBecomes(url, "Version 1"));

  // Wait for the async revalidation to complete.
  run_loop()->Run();
  EXPECT_EQ(2, requests_counted());
}

// The fresh cache entry must become visible once the async revalidation request
// has been sent.
IN_PROC_BROWSER_TEST_F(AsyncRevalidationManagerBrowserTest, CacheIsUpdated) {
  // PlzNavigate: Stale while revalidate is disabled.
  // TODO(clamy): Re-enable the test when there is support.
  if (IsBrowserSideNavigationEnabled())
    return;
  using base::ASCIIToUTF16;
  RegisterCountingRequestHandler();
  GURL url(embedded_test_server()->GetURL(kCountedHtmlPath));

  EXPECT_TRUE(TitleBecomes(url, "Version 1"));

  // Reset the renderer cache.
  NavigateToURL(shell(), GURL("about:blank"));

  // Load the page again. We should get the stale version from the cache.
  EXPECT_TRUE(TitleBecomes(url, "Version 1"));

  // Wait for the async revalidation request to be processed by the
  // EmbeddedTestServer.
  run_loop()->Run();

  // Reset the renderer cache.
  NavigateToURL(shell(), GURL("about:blank"));

  // Since the async revalidation request has been sent, the cache can no
  // longer return the stale contents.
  EXPECT_TRUE(TitleBecomes(url, "Version 2"));
}

// When the asynchronous revalidation arrives, any cookies it contains must be
// applied immediately.
IN_PROC_BROWSER_TEST_F(AsyncRevalidationManagerBrowserTest,
                       CookieSetAsynchronously) {
  // PlzNavigate: Stale while revalidate is disabled.
  // TODO(clamy): Re-enable the test when there is support.
  if (IsBrowserSideNavigationEnabled())
    return;
  RegisterCookieRequestHandler();
  GURL url(embedded_test_server()->GetURL(kCookieHtmlPath));

  // Set cookie to version=1
  NavigateToURL(shell(), url);

  // Reset render cache.
  NavigateToURL(shell(), GURL("about:blank"));

  // The page will load from the cache, then when the async revalidation
  // completes the cookie will update.
  EXPECT_TRUE(TitleBecomes(url, "PASS"));
}

}  // namespace

}  // namespace content
