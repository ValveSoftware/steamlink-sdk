// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_resource_dispatcher_host_delegate.h"
#include "ipc/ipc_security_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/url_request/url_request.h"

namespace content {

namespace {

// A ResourceDispatchHostDelegate that uses ResourceThrottles to pause a
// targeted request temporarily, to run a chunk of test code.
class TestResourceDispatcherHostDelegate
    : public ShellResourceDispatcherHostDelegate {
 public:
  using RequestDeferredHook = base::Callback<void(const base::Closure& resume)>;
  TestResourceDispatcherHostDelegate() : throttle_created_(false) {}

  void RequestBeginning(net::URLRequest* request,
                        ResourceContext* resource_context,
                        AppCacheService* appcache_service,
                        ResourceType resource_type,
                        ScopedVector<ResourceThrottle>* throttles) override {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    ShellResourceDispatcherHostDelegate::RequestBeginning(
        request, resource_context, appcache_service, resource_type, throttles);

    // If this is a request for the tracked URL, add a throttle to track it.
    if (request->url() == tracked_url_) {
      // Expect only a single request for the tracked url.
      ASSERT_FALSE(throttle_created_);
      throttle_created_ = true;

      throttles->push_back(
          new CallbackRunningResourceThrottle(request, this, run_on_start_));
    }
  }

  // Starts tracking a URL.  The request for previously tracked URL, if any,
  // must have been made and deleted before calling this function.
  void SetTrackedURL(const GURL& tracked_url,
                     const RequestDeferredHook& run_on_start) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    // Should not currently be tracking any URL.
    ASSERT_FALSE(run_loop_);

    // Create a RunLoop that will be stopped once the request for the tracked
    // URL has been destroyed, to allow tracking the URL while also waiting for
    // other events.
    run_loop_.reset(new base::RunLoop());

    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&TestResourceDispatcherHostDelegate::SetTrackedURLOnIOThread,
                   base::Unretained(this), tracked_url, run_on_start,
                   run_loop_->QuitClosure()));
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
  // A ResourceThrottle which defers the request at WillStartRequest time until
  // a test-supplied callback completes. Notifies |tracker| when the request is
  // destroyed.
  class CallbackRunningResourceThrottle : public ResourceThrottle {
   public:
    CallbackRunningResourceThrottle(net::URLRequest* request,
                                    TestResourceDispatcherHostDelegate* tracker,
                                    const RequestDeferredHook& run_on_start)
        : resumed_(false),
          request_(request),
          tracker_(tracker),
          run_on_start_(run_on_start),
          weak_factory_(this) {}

    void WillStartRequest(bool* defer) override {
      *defer = true;
      base::Closure resume_request_on_io_thread = base::Bind(
          base::IgnoreResult(&BrowserThread::PostTask), BrowserThread::IO,
          FROM_HERE, base::Bind(&CallbackRunningResourceThrottle::Resume,
                                weak_factory_.GetWeakPtr()));
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(run_on_start_, resume_request_on_io_thread));
    }

    ~CallbackRunningResourceThrottle() override {
      // If the request is deleted without being cancelled, its status will
      // indicate it succeeded, so have to check if the request is still pending
      // as well. If the request never even started, the throttle will never
      // resume it. Check this condition as well to allow for early
      // cancellation.
      tracker_->OnTrackedRequestDestroyed(!request_->is_pending() &&
                                          request_->status().is_success() &&
                                          resumed_);
    }

    // ResourceThrottle implementation:
    const char* GetNameForLogging() const override {
      return "CallbackRunningResourceThrottle";
    }

   private:
    void Resume() {
      resumed_ = true;
      controller()->Resume();
    }

    bool resumed_;
    net::URLRequest* request_;
    TestResourceDispatcherHostDelegate* tracker_;
    RequestDeferredHook run_on_start_;
    base::WeakPtrFactory<CallbackRunningResourceThrottle> weak_factory_;

    DISALLOW_COPY_AND_ASSIGN(CallbackRunningResourceThrottle);
  };

  void SetTrackedURLOnIOThread(const GURL& tracked_url,
                               const RequestDeferredHook& run_on_start,
                               const base::Closure& run_loop_quit_closure) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    throttle_created_ = false;
    tracked_url_ = tracked_url;
    run_on_start_ = run_on_start;
    run_loop_quit_closure_ = run_loop_quit_closure;
  }

  void OnTrackedRequestDestroyed(bool completed) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    tracked_request_completed_ = completed;
    tracked_url_ = GURL();
    run_on_start_ = RequestDeferredHook();

    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            run_loop_quit_closure_);
  }

  // These live on the IO thread.
  GURL tracked_url_;
  bool throttle_created_;
  base::Closure run_loop_quit_closure_;
  RequestDeferredHook run_on_start_;

  // This lives on the UI thread.
  std::unique_ptr<base::RunLoop> run_loop_;

  // Set on the IO thread while |run_loop_| is non-nullptr, read on the UI
  // thread after deleting run_loop_.
  bool tracked_request_completed_;

  DISALLOW_COPY_AND_ASSIGN(TestResourceDispatcherHostDelegate);
};

class CrossSiteResourceHandlerTest : public ContentBrowserTest {
 public:
  CrossSiteResourceHandlerTest() : old_delegate_(nullptr) {}

  // ContentBrowserTest implementation:
  void SetUpOnMainThread() override {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(
            &CrossSiteResourceHandlerTest::InjectResourceDispatcherHostDelegate,
            base::Unretained(this)));
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    content::SetupCrossSiteRedirector(embedded_test_server());
  }

  void TearDownOnMainThread() override {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&CrossSiteResourceHandlerTest::
                       RestoreResourceDispatcherHostDelegate,
                   base::Unretained(this)));
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolateAllSitesForTesting(command_line);
  }

  void InjectResourceDispatcherHostDelegate() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    old_delegate_ = ResourceDispatcherHostImpl::Get()->delegate();
    ResourceDispatcherHostImpl::Get()->SetDelegate(&tracking_delegate_);
  }

  void RestoreResourceDispatcherHostDelegate() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    ResourceDispatcherHostImpl::Get()->SetDelegate(old_delegate_);
    old_delegate_ = nullptr;
  }

  TestResourceDispatcherHostDelegate& tracking_delegate() {
    return tracking_delegate_;
  }

 private:
  TestResourceDispatcherHostDelegate tracking_delegate_;
  ResourceDispatcherHostDelegate* old_delegate_;
};

void SimulateMaliciousFrameDetachOnUIThread(int render_process_id,
                                            int frame_routing_id,
                                            const base::Closure& done_cb) {
  RenderFrameHostImpl* rfh =
      RenderFrameHostImpl::FromID(render_process_id, frame_routing_id);
  CHECK(rfh);

  // Inject a frame detach message. An attacker-controlled renderer could do
  // this without also cancelling the pending navigation (as blink would, if you
  // removed the iframe from the document via js).
  rfh->OnMessageReceived(FrameHostMsg_Detach(frame_routing_id));
  done_cb.Run();
}

}  // namespace

// Regression test for https://crbug.com/538784 -- ensures that one can't
// sidestep CrossSiteResourceHandler by detaching a frame mid-request.
IN_PROC_BROWSER_TEST_F(CrossSiteResourceHandlerTest,
                       NoDeliveryToDetachedFrame) {
  GURL attacker_page = embedded_test_server()->GetURL(
      "evil.com", "/cross_site_iframe_factory.html?evil(evil)");
  EXPECT_TRUE(NavigateToURL(shell(), attacker_page));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  RenderFrameHost* child_frame = root->child_at(0)->current_frame_host();

  // Attacker initiates a navigation to a cross-site document. Under --site-per-
  // process, these bytes must not be sent to the attacker process.
  GURL target_resource =
      embedded_test_server()->GetURL("a.com", "/title1.html");

  // We add a testing hook to simulate the attacker-controlled process sending
  // FrameHostMsg_Detach before the http response arrives. At the time this test
  // was written, the resource request had a lifetime separate from the RFH,
  tracking_delegate().SetTrackedURL(
      target_resource, base::Bind(&SimulateMaliciousFrameDetachOnUIThread,
                                  child_frame->GetProcess()->GetID(),
                                  child_frame->GetRoutingID()));
  EXPECT_TRUE(ExecuteScript(
      shell()->web_contents()->GetMainFrame(),
      base::StringPrintf("document.getElementById('child-0').src='%s'",
                         target_resource.spec().c_str())));

  // Wait for the scenario to play out. If this returns false, it means the
  // request did not succeed, which is good in this case.
  EXPECT_FALSE(tracking_delegate().WaitForTrackedURLAndGetCompleted())
      << "Request should have been cancelled before reaching the renderer.";
}

}  // namespace content
