// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Browser tests targeted at the RenderView that run in browser context.
// Note that these tests rely on single-process mode, and hence may be
// disabled in some configurations (check gyp files).

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_view.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/common/shell_content_client.h"
#include "content/shell/renderer/shell_content_renderer_client.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/failing_http_transaction_factory.h"
#include "net/http/http_cache.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebFrame.h"

namespace content {

namespace {

class TestShellContentRendererClient : public ShellContentRendererClient {
 public:
  TestShellContentRendererClient()
      : latest_error_valid_(false),
        latest_error_reason_(0),
        latest_error_stale_copy_in_cache_(false) {}

  void GetNavigationErrorStrings(content::RenderFrame* render_frame,
                                 const blink::WebURLRequest& failed_request,
                                 const blink::WebURLError& error,
                                 std::string* error_html,
                                 base::string16* error_description) override {
    if (error_html)
      *error_html = "A suffusion of yellow.";
    latest_error_valid_ = true;
    latest_error_reason_ = error.reason;
    latest_error_stale_copy_in_cache_ = error.staleCopyInCache;
  }

  bool GetLatestError(int* error_code, bool* stale_cache_entry_present) {
    if (latest_error_valid_) {
      *error_code = latest_error_reason_;
      *stale_cache_entry_present = latest_error_stale_copy_in_cache_;
    }
    return latest_error_valid_;
  }

 private:
  bool latest_error_valid_;
  int latest_error_reason_;
  bool latest_error_stale_copy_in_cache_;
};

// Must be called on IO thread.
void InterceptNetworkTransactions(net::URLRequestContextGetter* getter,
                                  net::Error error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::HttpCache* cache(
      getter->GetURLRequestContext()->http_transaction_factory()->GetCache());
  DCHECK(cache);
  std::unique_ptr<net::FailingHttpTransactionFactory> factory(
      new net::FailingHttpTransactionFactory(cache->GetSession(), error));
  // Throw away old version; since this is a browser test, there is no
  // need to restore the old state.
  cache->SetHttpNetworkTransactionFactoryForTesting(std::move(factory));
}

void CallOnUIThreadValidatingReturn(const base::Closure& callback,
                                    int rv) {
  DCHECK_EQ(net::OK, rv);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE, callback);
}

// Must be called on IO thread.  The callback will be called on
// completion of cache clearing on the UI thread.
void BackendClearCache(std::unique_ptr<disk_cache::Backend*> backend,
                       const base::Closure& callback,
                       int rv) {
  DCHECK(*backend);
  DCHECK_EQ(net::OK, rv);
  (*backend)->DoomAllEntries(
      base::Bind(&CallOnUIThreadValidatingReturn, callback));
}

// Must be called on IO thread.  The callback will be called on
// completion of cache clearing on the UI thread.
void ClearCache(net::URLRequestContextGetter* getter,
                const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::HttpCache* cache(
      getter->GetURLRequestContext()->http_transaction_factory()->GetCache());
  DCHECK(cache);
  std::unique_ptr<disk_cache::Backend*> backend(new disk_cache::Backend*);
  *backend = NULL;
  disk_cache::Backend** backend_ptr = backend.get();

  net::CompletionCallback backend_callback(base::Bind(
      &BackendClearCache, base::Passed(std::move(backend)), callback));

  // backend_ptr is valid until all copies of backend_callback go out
  // of scope.
  if (net::OK == cache->GetBackend(backend_ptr, backend_callback)) {
    // The call completed synchronously, so GetBackend didn't run the callback.
    backend_callback.Run(net::OK);
  }
}

}  // namespace

class RenderViewBrowserTest : public ContentBrowserTest {
 public:
  RenderViewBrowserTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // This method is needed to allow interaction with in-process renderer
    // and use of a test ContentRendererClient.
    command_line->AppendSwitch(switches::kSingleProcess);
  }

  void SetUpOnMainThread() override {
    // Override setting of renderer client.
    renderer_client_ = new TestShellContentRendererClient();
    // Explicitly leaks ownership; this object will remain alive
    // until process death.  We don't deleted the returned value,
    // since some contexts set the pointer to a non-heap address.
    SetRendererClientForTesting(renderer_client_);
  }

  // Navigates to the given URL and waits for |num_navigations| to occur, and
  // the title to change to |expected_title|.
  void NavigateToURLAndWaitForTitle(const GURL& url,
                                    const std::string& expected_title,
                                    int num_navigations) {
    content::TitleWatcher title_watcher(
        shell()->web_contents(), base::ASCIIToUTF16(expected_title));

    content::NavigateToURLBlockUntilNavigationsComplete(
        shell(), url, num_navigations);

    EXPECT_EQ(base::ASCIIToUTF16(expected_title),
              title_watcher.WaitAndGetTitle());
  }

  // Returns true if there is a valid error stored; in this case
  // |*error_code| and |*stale_cache_entry_present| will be updated
  // appropriately.
  // Must be called after the renderer thread is created.
  bool GetLatestErrorFromRendererClient(
      int* error_code, bool* stale_cache_entry_present) {
    bool result = false;

    PostTaskToInProcessRendererAndWait(
        base::Bind(&RenderViewBrowserTest::GetLatestErrorFromRendererClient0,
                   renderer_client_, &result, error_code,
                   stale_cache_entry_present));
    return result;
  }

 private:
  // Must be run on renderer thread.
  static void GetLatestErrorFromRendererClient0(
      TestShellContentRendererClient* renderer_client,
      bool* result, int* error_code, bool* stale_cache_entry_present) {
    *result = renderer_client->GetLatestError(
        error_code, stale_cache_entry_present);
  }

  TestShellContentRendererClient* renderer_client_;
};

IN_PROC_BROWSER_TEST_F(RenderViewBrowserTest, ConfirmCacheInformationPlumbed) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Load URL with "nocache" set, to create stale cache.
  GURL test_url(embedded_test_server()->GetURL("/nocache.html"));
  NavigateToURLAndWaitForTitle(test_url, "Nocache Test Page", 1);

  // Reload same URL after forcing an error from the the network layer;
  // confirm that the error page is told the cached copy exists.
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter =
      shell()->web_contents()->GetRenderProcessHost()->GetStoragePartition()->
          GetURLRequestContext();
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&InterceptNetworkTransactions,
                 base::RetainedRef(url_request_context_getter),
                 net::ERR_FAILED));

  // An error results in one completed navigation.
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);
  int error_code = net::OK;
  bool stale_cache_entry_present = false;
  ASSERT_TRUE(GetLatestErrorFromRendererClient(
      &error_code, &stale_cache_entry_present));
  EXPECT_EQ(net::ERR_FAILED, error_code);
  EXPECT_TRUE(stale_cache_entry_present);

  // Clear the cache and repeat; confirm lack of entry in cache reported.
  scoped_refptr<MessageLoopRunner> runner = new MessageLoopRunner;
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ClearCache, base::RetainedRef(url_request_context_getter),
                 runner->QuitClosure()));
  runner->Run();

  content::NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);

  error_code = net::OK;
  stale_cache_entry_present = true;
  ASSERT_TRUE(GetLatestErrorFromRendererClient(
      &error_code, &stale_cache_entry_present));
  EXPECT_EQ(net::ERR_FAILED, error_code);
  EXPECT_FALSE(stale_cache_entry_present);
}

}  // namespace content
