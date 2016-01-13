// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/resource_fetcher.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_view.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebView.h"

using blink::WebFrame;
using blink::WebURLRequest;
using blink::WebURLResponse;

namespace {

// The first RenderFrame is routing ID 1, and the first RenderView is 2.
const int kRenderViewRoutingId = 2;

}

namespace content {

static const int kMaxWaitTimeMs = 5000;

class FetcherDelegate {
 public:
  FetcherDelegate()
      : completed_(false),
        timed_out_(false) {
    // Start a repeating timer waiting for the download to complete.  The
    // callback has to be a static function, so we hold on to our instance.
    FetcherDelegate::instance_ = this;
    StartTimer();
  }

  virtual ~FetcherDelegate() {}

  ResourceFetcher::Callback NewCallback() {
    return base::Bind(&FetcherDelegate::OnURLFetchComplete,
                      base::Unretained(this));
  }

  virtual void OnURLFetchComplete(const WebURLResponse& response,
                                  const std::string& data) {
    response_ = response;
    data_ = data;
    completed_ = true;
    timer_.Stop();
    if (!timed_out_)
      quit_task_.Run();
  }

  bool completed() const { return completed_; }
  bool timed_out() const { return timed_out_; }

  std::string data() const { return data_; }
  const WebURLResponse& response() const { return response_; }

  // Wait for the request to complete or timeout.
  void WaitForResponse() {
    scoped_refptr<MessageLoopRunner> runner = new MessageLoopRunner;
    quit_task_ = runner->QuitClosure();
    runner->Run();
  }

  void StartTimer() {
    timer_.Start(FROM_HERE,
                 base::TimeDelta::FromMilliseconds(kMaxWaitTimeMs),
                 this,
                 &FetcherDelegate::TimerFired);
  }

  void TimerFired() {
    ASSERT_FALSE(completed_);

    timed_out_ = true;
    if (!completed_)
      quit_task_.Run();
    FAIL() << "fetch timed out";
  }

  static FetcherDelegate* instance_;

 private:
  base::OneShotTimer<FetcherDelegate> timer_;
  bool completed_;
  bool timed_out_;
  WebURLResponse response_;
  std::string data_;
  base::Closure quit_task_;
};

FetcherDelegate* FetcherDelegate::instance_ = NULL;

class EvilFetcherDelegate : public FetcherDelegate {
 public:
  virtual ~EvilFetcherDelegate() {}

  void SetFetcher(ResourceFetcher* fetcher) {
    fetcher_.reset(fetcher);
  }

  virtual void OnURLFetchComplete(const WebURLResponse& response,
                                  const std::string& data) OVERRIDE {
    FetcherDelegate::OnURLFetchComplete(response, data);

    // Destroy the ResourceFetcher here.  We are testing that upon returning
    // to the ResourceFetcher that it does not crash.  This must be done after
    // calling FetcherDelegate::OnURLFetchComplete, since deleting the fetcher
    // invalidates |response| and |data|.
    fetcher_.reset();
  }

 private:
  scoped_ptr<ResourceFetcher> fetcher_;
};

class ResourceFetcherTests : public ContentBrowserTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kSingleProcess);
#if defined(OS_WIN)
    // Don't want to try to create a GPU process.
    command_line->AppendSwitch(switches::kDisableGpu);
#endif
  }

  RenderView* GetRenderView() {
    // We could have the test on the UI thread get the WebContent's routing ID,
    // but we know this will be the first RV so skip that and just hardcode it.
    return RenderView::FromRoutingID(kRenderViewRoutingId);
  }

  void ResourceFetcherDownloadOnRenderer(const GURL& url) {
    WebFrame* frame = GetRenderView()->GetWebView()->mainFrame();

    scoped_ptr<FetcherDelegate> delegate(new FetcherDelegate);
    scoped_ptr<ResourceFetcher> fetcher(ResourceFetcher::Create(url));
    fetcher->Start(frame, WebURLRequest::TargetIsMainFrame,
                   delegate->NewCallback());

    delegate->WaitForResponse();

    ASSERT_TRUE(delegate->completed());
    EXPECT_EQ(delegate->response().httpStatusCode(), 200);
    std::string text = delegate->data();
    EXPECT_TRUE(text.find("Basic html test.") != std::string::npos);
  }

  void ResourceFetcher404OnRenderer(const GURL& url) {
    WebFrame* frame = GetRenderView()->GetWebView()->mainFrame();

    scoped_ptr<FetcherDelegate> delegate(new FetcherDelegate);
    scoped_ptr<ResourceFetcher> fetcher(ResourceFetcher::Create(url));
    fetcher->Start(frame, WebURLRequest::TargetIsMainFrame,
                   delegate->NewCallback());

    delegate->WaitForResponse();

    ASSERT_TRUE(delegate->completed());
    EXPECT_EQ(delegate->response().httpStatusCode(), 404);
    EXPECT_TRUE(delegate->data().find("Not Found.") != std::string::npos);
  }

  void ResourceFetcherDidFailOnRenderer() {
    WebFrame* frame = GetRenderView()->GetWebView()->mainFrame();

    // Try to fetch a page on a site that doesn't exist.
    GURL url("http://localhost:1339/doesnotexist");
    scoped_ptr<FetcherDelegate> delegate(new FetcherDelegate);
    scoped_ptr<ResourceFetcher> fetcher(ResourceFetcher::Create(url));
    fetcher->Start(frame, WebURLRequest::TargetIsMainFrame,
                   delegate->NewCallback());

    delegate->WaitForResponse();

    // When we fail, we still call the Delegate callback but we pass in empty
    // values.
    EXPECT_TRUE(delegate->completed());
    EXPECT_TRUE(delegate->response().isNull());
    EXPECT_EQ(delegate->data(), std::string());
    EXPECT_FALSE(delegate->timed_out());
  }

  void ResourceFetcherTimeoutOnRenderer(const GURL& url) {
    WebFrame* frame = GetRenderView()->GetWebView()->mainFrame();

    scoped_ptr<FetcherDelegate> delegate(new FetcherDelegate);
    scoped_ptr<ResourceFetcher> fetcher(ResourceFetcher::Create(url));
    fetcher->Start(frame, WebURLRequest::TargetIsMainFrame,
                   delegate->NewCallback());
    fetcher->SetTimeout(base::TimeDelta());

    delegate->WaitForResponse();

    // When we timeout, we still call the Delegate callback but we pass in empty
    // values.
    EXPECT_TRUE(delegate->completed());
    EXPECT_TRUE(delegate->response().isNull());
    EXPECT_EQ(delegate->data(), std::string());
    EXPECT_FALSE(delegate->timed_out());
  }

  void ResourceFetcherDeletedInCallbackOnRenderer(const GURL& url) {
    WebFrame* frame = GetRenderView()->GetWebView()->mainFrame();

    scoped_ptr<EvilFetcherDelegate> delegate(new EvilFetcherDelegate);
    scoped_ptr<ResourceFetcher> fetcher(ResourceFetcher::Create(url));
    fetcher->Start(frame, WebURLRequest::TargetIsMainFrame,
                   delegate->NewCallback());
    fetcher->SetTimeout(base::TimeDelta());
    delegate->SetFetcher(fetcher.release());

    delegate->WaitForResponse();
    EXPECT_FALSE(delegate->timed_out());
  }

  void ResourceFetcherPost(const GURL& url) {
    const char* kBody = "Really nifty POST body!";

    WebFrame* frame = GetRenderView()->GetWebView()->mainFrame();

    scoped_ptr<FetcherDelegate> delegate(new FetcherDelegate);
    scoped_ptr<ResourceFetcher> fetcher(ResourceFetcher::Create(url));
    fetcher->SetMethod("POST");
    fetcher->SetBody(kBody);
    fetcher->Start(frame, WebURLRequest::TargetIsMainFrame,
                   delegate->NewCallback());

    delegate->WaitForResponse();
    ASSERT_TRUE(delegate->completed());
    EXPECT_EQ(delegate->response().httpStatusCode(), 200);
    EXPECT_EQ(kBody, delegate->data());
  }

  void ResourceFetcherSetHeader(const GURL& url) {
    const char* kHeader = "Rather boring header.";

    WebFrame* frame = GetRenderView()->GetWebView()->mainFrame();

    scoped_ptr<FetcherDelegate> delegate(new FetcherDelegate);
    scoped_ptr<ResourceFetcher> fetcher(ResourceFetcher::Create(url));
    fetcher->SetHeader("header", kHeader);
    fetcher->Start(frame, WebURLRequest::TargetIsMainFrame,
                   delegate->NewCallback());

    delegate->WaitForResponse();
    ASSERT_TRUE(delegate->completed());
    EXPECT_EQ(delegate->response().httpStatusCode(), 200);
    EXPECT_EQ(kHeader, delegate->data());
  }
};

#if defined(OS_ANDROID)
// Disable (http://crbug.com/248796).
#define MAYBE_ResourceFetcher404 DISABLED_ResourceFetcher404
#define MAYBE_ResourceFetcherDeletedInCallback \
  DISABLED_ResourceFetcherDeletedInCallback
#define MAYBE_ResourceFetcherTimeout DISABLED_ResourceFetcherTimeout
#define MAYBE_ResourceFetcherDownload DISABLED_ResourceFetcherDownload
// Disable (http://crbug.com/341142).
#define MAYBE_ResourceFetcherPost DISABLED_ResourceFetcherPost
#define MAYBE_ResourceFetcherSetHeader DISABLED_ResourceFetcherSetHeader
#else
#define MAYBE_ResourceFetcher404 ResourceFetcher404
#define MAYBE_ResourceFetcherDeletedInCallback ResourceFetcherDeletedInCallback
#define MAYBE_ResourceFetcherTimeout ResourceFetcherTimeout
#define MAYBE_ResourceFetcherDownload ResourceFetcherDownload
#define MAYBE_ResourceFetcherPost ResourceFetcherPost
#define MAYBE_ResourceFetcherSetHeader ResourceFetcherSetHeader
#endif

// Test a fetch from the test server.
// If this flakes, use http://crbug.com/51622.
IN_PROC_BROWSER_TEST_F(ResourceFetcherTests, MAYBE_ResourceFetcherDownload) {
  // Need to spin up the renderer.
  NavigateToURL(shell(), GURL(url::kAboutBlankURL));

  ASSERT_TRUE(test_server()->Start());
  GURL url(test_server()->GetURL("files/simple_page.html"));

  PostTaskToInProcessRendererAndWait(
        base::Bind(&ResourceFetcherTests::ResourceFetcherDownloadOnRenderer,
                   base::Unretained(this), url));
}

IN_PROC_BROWSER_TEST_F(ResourceFetcherTests, MAYBE_ResourceFetcher404) {
  // Need to spin up the renderer.
  NavigateToURL(shell(), GURL(url::kAboutBlankURL));

  // Test 404 response.
  ASSERT_TRUE(test_server()->Start());
  GURL url = test_server()->GetURL("files/thisfiledoesntexist.html");

  PostTaskToInProcessRendererAndWait(
        base::Bind(&ResourceFetcherTests::ResourceFetcher404OnRenderer,
                   base::Unretained(this), url));
}

// If this flakes, use http://crbug.com/51622.
IN_PROC_BROWSER_TEST_F(ResourceFetcherTests, ResourceFetcherDidFail) {
  // Need to spin up the renderer.
  NavigateToURL(shell(), GURL(url::kAboutBlankURL));

  PostTaskToInProcessRendererAndWait(
        base::Bind(&ResourceFetcherTests::ResourceFetcherDidFailOnRenderer,
                   base::Unretained(this)));
}

IN_PROC_BROWSER_TEST_F(ResourceFetcherTests, MAYBE_ResourceFetcherTimeout) {
  // Need to spin up the renderer.
  NavigateToURL(shell(), GURL(url::kAboutBlankURL));

  // Grab a page that takes at least 1 sec to respond, but set the fetcher to
  // timeout in 0 sec.
  ASSERT_TRUE(test_server()->Start());
  GURL url(test_server()->GetURL("slow?1"));

  PostTaskToInProcessRendererAndWait(
        base::Bind(&ResourceFetcherTests::ResourceFetcherTimeoutOnRenderer,
                   base::Unretained(this), url));
}

IN_PROC_BROWSER_TEST_F(ResourceFetcherTests,
                       MAYBE_ResourceFetcherDeletedInCallback) {
  // Need to spin up the renderer.
  NavigateToURL(shell(), GURL(url::kAboutBlankURL));

  // Grab a page that takes at least 1 sec to respond, but set the fetcher to
  // timeout in 0 sec.
  ASSERT_TRUE(test_server()->Start());
  GURL url(test_server()->GetURL("slow?1"));

  PostTaskToInProcessRendererAndWait(
        base::Bind(
            &ResourceFetcherTests::ResourceFetcherDeletedInCallbackOnRenderer,
            base::Unretained(this), url));
}



// Test that ResourceFetchers can handle POSTs.
IN_PROC_BROWSER_TEST_F(ResourceFetcherTests, MAYBE_ResourceFetcherPost) {
  // Need to spin up the renderer.
  NavigateToURL(shell(), GURL(url::kAboutBlankURL));

  // Grab a page that echos the POST body.
  ASSERT_TRUE(test_server()->Start());
  GURL url(test_server()->GetURL("echo"));

  PostTaskToInProcessRendererAndWait(
        base::Bind(
            &ResourceFetcherTests::ResourceFetcherPost,
            base::Unretained(this), url));
}

// Test that ResourceFetchers can set headers.
IN_PROC_BROWSER_TEST_F(ResourceFetcherTests, MAYBE_ResourceFetcherSetHeader) {
  // Need to spin up the renderer.
  NavigateToURL(shell(), GURL(url::kAboutBlankURL));

  // Grab a page that echos the POST body.
  ASSERT_TRUE(test_server()->Start());
  GURL url(test_server()->GetURL("echoheader?header"));

  PostTaskToInProcessRendererAndWait(
        base::Bind(
            &ResourceFetcherTests::ResourceFetcherSetHeader,
            base::Unretained(this), url));
}

}  // namespace content
