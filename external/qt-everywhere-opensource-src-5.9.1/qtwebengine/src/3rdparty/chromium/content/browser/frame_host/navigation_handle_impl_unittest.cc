// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/common/request_context_type.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_web_contents.h"

namespace content {

// Test version of a NavigationThrottle. It will always return the current
// NavigationThrottle::ThrottleCheckResult |result_|, It also monitors the
// number of times WillStartRequest, WillRedirectRequest, and
// WillProcessResponse were called.
class TestNavigationThrottle : public NavigationThrottle {
 public:
  TestNavigationThrottle(NavigationHandle* handle,
                         NavigationThrottle::ThrottleCheckResult result)
      : NavigationThrottle(handle),
        result_(result),
        will_start_calls_(0),
        will_redirect_calls_(0),
        will_process_response_calls_(0) {}

  ~TestNavigationThrottle() override {}

  NavigationThrottle::ThrottleCheckResult WillStartRequest() override {
    ++will_start_calls_;
    return result_;
  }

  NavigationThrottle::ThrottleCheckResult WillRedirectRequest() override {
    ++will_redirect_calls_;
    return result_;
  }

  NavigationThrottle::ThrottleCheckResult WillProcessResponse() override {
    ++will_process_response_calls_;
    return result_;
  }

  int will_start_calls() const { return will_start_calls_; }
  int will_redirect_calls() const { return will_redirect_calls_; }
  int will_process_response_calls() const {
    return will_process_response_calls_;
  }

 private:
  // The result returned by the TestNavigationThrottle.
  NavigationThrottle::ThrottleCheckResult result_;

  // The number of times each handler was called.
  int will_start_calls_;
  int will_redirect_calls_;
  int will_process_response_calls_;
};

class NavigationHandleImplTest : public RenderViewHostImplTestHarness {
 public:
  NavigationHandleImplTest()
      : was_callback_called_(false),
        callback_result_(NavigationThrottle::DEFER) {}

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    test_handle_ = NavigationHandleImpl::Create(
        GURL(), main_test_rfh()->frame_tree_node(), true, false, false,
        base::TimeTicks::Now(), 0, false);
    EXPECT_EQ(REQUEST_CONTEXT_TYPE_UNSPECIFIED,
              test_handle_->request_context_type_);
    contents()->GetMainFrame()->InitializeRenderFrameIfNeeded();
  }

  void TearDown() override {
    // Release the |test_handle_| before destroying the WebContents, to match
    // the WebContentsObserverSanityChecker expectations.
    test_handle_.reset();
    RenderViewHostImplTestHarness::TearDown();
  }

  bool IsDeferringStart() {
    return test_handle_->state() == NavigationHandleImpl::DEFERRING_START;
  }

  bool IsDeferringRedirect() {
    return test_handle_->state() == NavigationHandleImpl::DEFERRING_REDIRECT;
  }

  bool IsDeferringResponse() {
    return test_handle_->state() == NavigationHandleImpl::DEFERRING_RESPONSE;
  }

  bool IsCanceling() {
    return test_handle_->state() == NavigationHandleImpl::CANCELING;
  }

  // Helper function to call WillStartRequest on |handle|. If this function
  // returns DEFER, |callback_result_| will be set to the actual result of
  // the throttle checks when they are finished.
  void SimulateWillStartRequest() {
    was_callback_called_ = false;
    callback_result_ = NavigationThrottle::DEFER;

    // It's safe to use base::Unretained since the NavigationHandle is owned by
    // the NavigationHandleImplTest.
    test_handle_->WillStartRequest(
        "GET", nullptr, Referrer(), false, ui::PAGE_TRANSITION_LINK, false,
        REQUEST_CONTEXT_TYPE_LOCATION,
        base::Bind(&NavigationHandleImplTest::UpdateThrottleCheckResult,
                   base::Unretained(this)));
  }

  // Helper function to call WillRedirectRequest on |handle|. If this function
  // returns DEFER, |callback_result_| will be set to the actual result of the
  // throttle checks when they are finished.
  // TODO(clamy): this should also simulate that WillStartRequest was called if
  // it has not been called before.
  void SimulateWillRedirectRequest() {
    was_callback_called_ = false;
    callback_result_ = NavigationThrottle::DEFER;

    // It's safe to use base::Unretained since the NavigationHandle is owned by
    // the NavigationHandleImplTest.
    test_handle_->WillRedirectRequest(
        GURL(), "GET", GURL(), false, scoped_refptr<net::HttpResponseHeaders>(),
        net::HttpResponseInfo::CONNECTION_INFO_HTTP1_1,
        base::Bind(&NavigationHandleImplTest::UpdateThrottleCheckResult,
                   base::Unretained(this)));
  }

  // Helper function to call WillProcessResponse on |handle|. If this function
  // returns DEFER, |callback_result_| will be set to the actual result of the
  // throttle checks when they are finished.
  // TODO(clamy): this should also simulate that WillStartRequest was called if
  // it has not been called before.
  void SimulateWillProcessResponse() {
    was_callback_called_ = false;
    callback_result_ = NavigationThrottle::DEFER;

    // It's safe to use base::Unretained since the NavigationHandle is owned
    // by the NavigationHandleImplTest. The ConnectionInfo is different from
    // that sent to WillRedirectRequest to verify that it's correctly plumbed
    // in both cases.
    test_handle_->WillProcessResponse(
        main_test_rfh(), scoped_refptr<net::HttpResponseHeaders>(),
        net::HttpResponseInfo::CONNECTION_INFO_QUIC_35, SSLStatus(),
        GlobalRequestID(), false, false, false, base::Closure(),
        base::Bind(&NavigationHandleImplTest::UpdateThrottleCheckResult,
                   base::Unretained(this)));
  }

  // Returns the handle used in tests.
  NavigationHandleImpl* test_handle() const { return test_handle_.get(); }

  // Whether the callback was called.
  bool was_callback_called() const { return was_callback_called_; }

  // Returns the callback_result.
  NavigationThrottle::ThrottleCheckResult callback_result() const {
    return callback_result_;
  }

  // Creates, register and returns a TestNavigationThrottle that will return
  // |result| on checks.
  TestNavigationThrottle* CreateTestNavigationThrottle(
      NavigationThrottle::ThrottleCheckResult result) {
    TestNavigationThrottle* test_throttle =
        new TestNavigationThrottle(test_handle(), result);
    test_handle()->RegisterThrottleForTesting(
        std::unique_ptr<TestNavigationThrottle>(test_throttle));
    return test_throttle;
  }

 private:
  // The callback provided to NavigationHandleImpl::WillStartRequest and
  // NavigationHandleImpl::WillRedirectRequest during the tests.
  void UpdateThrottleCheckResult(
      NavigationThrottle::ThrottleCheckResult result) {
    callback_result_ = result;
    was_callback_called_ = true;
  }

  std::unique_ptr<NavigationHandleImpl> test_handle_;
  bool was_callback_called_;
  NavigationThrottle::ThrottleCheckResult callback_result_;
};

// Checks that the request_context_type is properly set.
// Note: can be extended to cover more internal members.
TEST_F(NavigationHandleImplTest, SimpleDataChecks) {
  SimulateWillStartRequest();
  EXPECT_EQ(REQUEST_CONTEXT_TYPE_LOCATION,
            test_handle()->GetRequestContextType());
  EXPECT_EQ(net::HttpResponseInfo::CONNECTION_INFO_UNKNOWN,
            test_handle()->GetConnectionInfo());

  test_handle()->Resume();
  SimulateWillRedirectRequest();
  EXPECT_EQ(REQUEST_CONTEXT_TYPE_LOCATION,
            test_handle()->GetRequestContextType());
  EXPECT_EQ(net::HttpResponseInfo::CONNECTION_INFO_HTTP1_1,
            test_handle()->GetConnectionInfo());

  test_handle()->Resume();
  SimulateWillProcessResponse();
  EXPECT_EQ(REQUEST_CONTEXT_TYPE_LOCATION,
            test_handle()->GetRequestContextType());
  EXPECT_EQ(net::HttpResponseInfo::CONNECTION_INFO_QUIC_35,
            test_handle()->GetConnectionInfo());
}

TEST_F(NavigationHandleImplTest, SimpleDataCheckNoRedirect) {
  SimulateWillStartRequest();
  EXPECT_EQ(net::HttpResponseInfo::CONNECTION_INFO_UNKNOWN,
            test_handle()->GetConnectionInfo());

  test_handle()->Resume();
  SimulateWillProcessResponse();
  EXPECT_EQ(net::HttpResponseInfo::CONNECTION_INFO_QUIC_35,
            test_handle()->GetConnectionInfo());
}

// Checks that a deferred navigation can be properly resumed.
TEST_F(NavigationHandleImplTest, ResumeDeferred) {
  TestNavigationThrottle* test_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::DEFER);
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_FALSE(IsDeferringResponse());
  EXPECT_EQ(0, test_throttle->will_start_calls());
  EXPECT_EQ(0, test_throttle->will_redirect_calls());
  EXPECT_EQ(0, test_throttle->will_process_response_calls());

  // Simulate WillStartRequest. The request should be deferred. The callback
  // should not have been called.
  SimulateWillStartRequest();
  EXPECT_TRUE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_FALSE(IsDeferringResponse());
  EXPECT_FALSE(was_callback_called());
  EXPECT_EQ(1, test_throttle->will_start_calls());
  EXPECT_EQ(0, test_throttle->will_redirect_calls());
  EXPECT_EQ(0, test_throttle->will_process_response_calls());

  // Resume the request. It should no longer be deferred and the callback
  // should have been called.
  test_handle()->Resume();
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_FALSE(IsDeferringResponse());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(NavigationThrottle::PROCEED, callback_result());
  EXPECT_EQ(1, test_throttle->will_start_calls());
  EXPECT_EQ(0, test_throttle->will_redirect_calls());
  EXPECT_EQ(0, test_throttle->will_process_response_calls());

  // Simulate WillRedirectRequest. The request should be deferred. The callback
  // should not have been called.
  SimulateWillRedirectRequest();
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_TRUE(IsDeferringRedirect());
  EXPECT_FALSE(IsDeferringResponse());
  EXPECT_FALSE(was_callback_called());
  EXPECT_EQ(1, test_throttle->will_start_calls());
  EXPECT_EQ(1, test_throttle->will_redirect_calls());
  EXPECT_EQ(0, test_throttle->will_process_response_calls());

  // Resume the request. It should no longer be deferred and the callback
  // should have been called.
  test_handle()->Resume();
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_FALSE(IsDeferringResponse());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(NavigationThrottle::PROCEED, callback_result());
  EXPECT_EQ(1, test_throttle->will_start_calls());
  EXPECT_EQ(1, test_throttle->will_redirect_calls());
  EXPECT_EQ(0, test_throttle->will_process_response_calls());

  // Simulate WillProcessResponse. It will be deferred. The callback should not
  // have been called.
  SimulateWillProcessResponse();
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_TRUE(IsDeferringResponse());
  EXPECT_FALSE(was_callback_called());
  EXPECT_EQ(1, test_throttle->will_start_calls());
  EXPECT_EQ(1, test_throttle->will_redirect_calls());
  EXPECT_EQ(1, test_throttle->will_process_response_calls());

  // Resume the request. It should no longer be deferred and the callback should
  // have been called.
  test_handle()->Resume();
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_FALSE(IsDeferringResponse());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(NavigationThrottle::PROCEED, callback_result());
  EXPECT_EQ(1, test_throttle->will_start_calls());
  EXPECT_EQ(1, test_throttle->will_redirect_calls());
  EXPECT_EQ(1, test_throttle->will_process_response_calls());
  EXPECT_TRUE(test_handle()->GetRenderFrameHost());
}

// Checks that a navigation deferred during WillStartRequest can be properly
// cancelled.
TEST_F(NavigationHandleImplTest, CancelDeferredWillStart) {
  TestNavigationThrottle* test_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::DEFER);
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_EQ(0, test_throttle->will_start_calls());
  EXPECT_EQ(0, test_throttle->will_redirect_calls());
  EXPECT_EQ(0, test_throttle->will_process_response_calls());

  // Simulate WillStartRequest. The request should be deferred. The callback
  // should not have been called.
  SimulateWillStartRequest();
  EXPECT_TRUE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_FALSE(was_callback_called());
  EXPECT_EQ(1, test_throttle->will_start_calls());
  EXPECT_EQ(0, test_throttle->will_redirect_calls());
  EXPECT_EQ(0, test_throttle->will_process_response_calls());

  // Cancel the request. The callback should have been called.
  test_handle()->CancelDeferredNavigation(
      NavigationThrottle::CANCEL_AND_IGNORE);
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_TRUE(IsCanceling());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(NavigationThrottle::CANCEL_AND_IGNORE, callback_result());
  EXPECT_EQ(1, test_throttle->will_start_calls());
  EXPECT_EQ(0, test_throttle->will_redirect_calls());
  EXPECT_EQ(0, test_throttle->will_process_response_calls());
}

// Checks that a navigation deferred during WillRedirectRequest can be properly
// cancelled.
TEST_F(NavigationHandleImplTest, CancelDeferredWillRedirect) {
  TestNavigationThrottle* test_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::DEFER);
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_EQ(0, test_throttle->will_start_calls());
  EXPECT_EQ(0, test_throttle->will_redirect_calls());
  EXPECT_EQ(0, test_throttle->will_process_response_calls());

  // Simulate WillRedirectRequest. The request should be deferred. The callback
  // should not have been called.
  SimulateWillRedirectRequest();
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_TRUE(IsDeferringRedirect());
  EXPECT_FALSE(was_callback_called());
  EXPECT_EQ(0, test_throttle->will_start_calls());
  EXPECT_EQ(1, test_throttle->will_redirect_calls());
  EXPECT_EQ(0, test_throttle->will_process_response_calls());

  // Cancel the request. The callback should have been called.
  test_handle()->CancelDeferredNavigation(
      NavigationThrottle::CANCEL_AND_IGNORE);
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_TRUE(IsCanceling());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(NavigationThrottle::CANCEL_AND_IGNORE, callback_result());
  EXPECT_EQ(0, test_throttle->will_start_calls());
  EXPECT_EQ(1, test_throttle->will_redirect_calls());
  EXPECT_EQ(0, test_throttle->will_process_response_calls());
}

// Checks that a navigation deferred can be canceled and not ignored.
TEST_F(NavigationHandleImplTest, CancelDeferredNoIgnore) {
  TestNavigationThrottle* test_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::DEFER);
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_EQ(0, test_throttle->will_start_calls());
  EXPECT_EQ(0, test_throttle->will_redirect_calls());
  EXPECT_EQ(0, test_throttle->will_process_response_calls());

  // Simulate WillRedirectRequest. The request should be deferred. The callback
  // should not have been called.
  SimulateWillStartRequest();
  EXPECT_TRUE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_FALSE(was_callback_called());
  EXPECT_EQ(1, test_throttle->will_start_calls());
  EXPECT_EQ(0, test_throttle->will_redirect_calls());
  EXPECT_EQ(0, test_throttle->will_process_response_calls());

  // Cancel the request. The callback should have been called with CANCEL, and
  // not CANCEL_AND_IGNORE.
  test_handle()->CancelDeferredNavigation(NavigationThrottle::CANCEL);
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_TRUE(IsCanceling());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(NavigationThrottle::CANCEL, callback_result());
  EXPECT_EQ(1, test_throttle->will_start_calls());
  EXPECT_EQ(0, test_throttle->will_redirect_calls());
  EXPECT_EQ(0, test_throttle->will_process_response_calls());
}

// Checks that a NavigationThrottle asking to defer followed by a
// NavigationThrottle asking to proceed behave correctly.
TEST_F(NavigationHandleImplTest, DeferThenProceed) {
  TestNavigationThrottle* defer_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::DEFER);
  TestNavigationThrottle* proceed_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::PROCEED);
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_EQ(0, defer_throttle->will_start_calls());
  EXPECT_EQ(0, defer_throttle->will_redirect_calls());
  EXPECT_EQ(0, defer_throttle->will_process_response_calls());
  EXPECT_EQ(0, proceed_throttle->will_start_calls());
  EXPECT_EQ(0, proceed_throttle->will_redirect_calls());
  EXPECT_EQ(0, proceed_throttle->will_process_response_calls());

  // Simulate WillStartRequest. The request should be deferred. The callback
  // should not have been called. The second throttle should not have been
  // notified.
  SimulateWillStartRequest();
  EXPECT_TRUE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_FALSE(was_callback_called());
  EXPECT_EQ(1, defer_throttle->will_start_calls());
  EXPECT_EQ(0, defer_throttle->will_redirect_calls());
  EXPECT_EQ(0, defer_throttle->will_process_response_calls());
  EXPECT_EQ(0, proceed_throttle->will_start_calls());
  EXPECT_EQ(0, proceed_throttle->will_redirect_calls());

  // Resume the request. It should no longer be deferred and the callback
  // should have been called. The second throttle should have been notified.
  test_handle()->Resume();
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(NavigationThrottle::PROCEED, callback_result());
  EXPECT_EQ(1, defer_throttle->will_start_calls());
  EXPECT_EQ(0, defer_throttle->will_redirect_calls());
  EXPECT_EQ(0, defer_throttle->will_process_response_calls());
  EXPECT_EQ(1, proceed_throttle->will_start_calls());
  EXPECT_EQ(0, proceed_throttle->will_redirect_calls());

  // Simulate WillRedirectRequest. The request should be deferred. The callback
  // should not have been called. The second throttle should not have been
  // notified.
  SimulateWillRedirectRequest();
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_TRUE(IsDeferringRedirect());
  EXPECT_FALSE(was_callback_called());
  EXPECT_EQ(1, defer_throttle->will_start_calls());
  EXPECT_EQ(1, defer_throttle->will_redirect_calls());
  EXPECT_EQ(0, defer_throttle->will_process_response_calls());
  EXPECT_EQ(1, proceed_throttle->will_start_calls());
  EXPECT_EQ(0, proceed_throttle->will_redirect_calls());

  // Resume the request. It should no longer be deferred and the callback
  // should have been called. The second throttle should have been notified.
  test_handle()->Resume();
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(NavigationThrottle::PROCEED, callback_result());
  EXPECT_EQ(1, defer_throttle->will_start_calls());
  EXPECT_EQ(1, defer_throttle->will_redirect_calls());
  EXPECT_EQ(0, defer_throttle->will_process_response_calls());
  EXPECT_EQ(1, proceed_throttle->will_start_calls());
  EXPECT_EQ(1, proceed_throttle->will_redirect_calls());
}

// Checks that a NavigationThrottle asking to defer followed by a
// NavigationThrottle asking to cancel behave correctly in WillStartRequest.
TEST_F(NavigationHandleImplTest, DeferThenCancelWillStartRequest) {
  TestNavigationThrottle* defer_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::DEFER);
  TestNavigationThrottle* cancel_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::CANCEL_AND_IGNORE);
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_EQ(0, defer_throttle->will_start_calls());
  EXPECT_EQ(0, defer_throttle->will_redirect_calls());
  EXPECT_EQ(0, defer_throttle->will_process_response_calls());
  EXPECT_EQ(0, cancel_throttle->will_start_calls());
  EXPECT_EQ(0, cancel_throttle->will_redirect_calls());

  // Simulate WillStartRequest. The request should be deferred. The callback
  // should not have been called. The second throttle should not have been
  // notified.
  SimulateWillStartRequest();
  EXPECT_TRUE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_FALSE(was_callback_called());
  EXPECT_EQ(1, defer_throttle->will_start_calls());
  EXPECT_EQ(0, defer_throttle->will_redirect_calls());
  EXPECT_EQ(0, defer_throttle->will_process_response_calls());
  EXPECT_EQ(0, cancel_throttle->will_start_calls());
  EXPECT_EQ(0, cancel_throttle->will_redirect_calls());

  // Resume the request. The callback should have been called. The second
  // throttle should have been notified.
  test_handle()->Resume();
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_TRUE(IsCanceling());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(NavigationThrottle::CANCEL_AND_IGNORE, callback_result());
  EXPECT_EQ(1, defer_throttle->will_start_calls());
  EXPECT_EQ(0, defer_throttle->will_redirect_calls());
  EXPECT_EQ(0, defer_throttle->will_process_response_calls());
  EXPECT_EQ(1, cancel_throttle->will_start_calls());
  EXPECT_EQ(0, cancel_throttle->will_redirect_calls());
}

// Checks that a NavigationThrottle asking to defer followed by a
// NavigationThrottle asking to cancel behave correctly in WillRedirectRequest.
TEST_F(NavigationHandleImplTest, DeferThenCancelWillRedirectRequest) {
  TestNavigationThrottle* defer_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::DEFER);
  TestNavigationThrottle* cancel_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::CANCEL_AND_IGNORE);
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_EQ(0, defer_throttle->will_start_calls());
  EXPECT_EQ(0, defer_throttle->will_redirect_calls());
  EXPECT_EQ(0, defer_throttle->will_process_response_calls());
  EXPECT_EQ(0, cancel_throttle->will_start_calls());
  EXPECT_EQ(0, cancel_throttle->will_redirect_calls());

  // Simulate WillRedirectRequest. The request should be deferred. The callback
  // should not have been called. The second throttle should not have been
  // notified.
  SimulateWillRedirectRequest();
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_TRUE(IsDeferringRedirect());
  EXPECT_FALSE(was_callback_called());
  EXPECT_EQ(0, defer_throttle->will_start_calls());
  EXPECT_EQ(1, defer_throttle->will_redirect_calls());
  EXPECT_EQ(0, defer_throttle->will_process_response_calls());
  EXPECT_EQ(0, cancel_throttle->will_start_calls());
  EXPECT_EQ(0, cancel_throttle->will_redirect_calls());

  // Resume the request. The callback should have been called. The second
  // throttle should have been notified.
  test_handle()->Resume();
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_TRUE(IsCanceling());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(NavigationThrottle::CANCEL_AND_IGNORE, callback_result());
  EXPECT_EQ(0, defer_throttle->will_start_calls());
  EXPECT_EQ(1, defer_throttle->will_redirect_calls());
  EXPECT_EQ(0, defer_throttle->will_process_response_calls());
  EXPECT_EQ(0, cancel_throttle->will_start_calls());
  EXPECT_EQ(1, cancel_throttle->will_redirect_calls());
}

// Checks that a NavigationThrottle asking to cancel followed by a
// NavigationThrottle asking to proceed behave correctly in WillStartRequest.
// The navigation will be canceled directly, and the second throttle will not
// be called.
TEST_F(NavigationHandleImplTest, CancelThenProceedWillStartRequest) {
  TestNavigationThrottle* cancel_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::CANCEL_AND_IGNORE);
  TestNavigationThrottle* proceed_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::PROCEED);
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_EQ(0, cancel_throttle->will_start_calls());
  EXPECT_EQ(0, cancel_throttle->will_redirect_calls());
  EXPECT_EQ(0, cancel_throttle->will_process_response_calls());
  EXPECT_EQ(0, proceed_throttle->will_start_calls());
  EXPECT_EQ(0, proceed_throttle->will_redirect_calls());

  // Simulate WillStartRequest. The request should not be deferred. The
  // callback should not have been called. The second throttle should not have
  // been notified.
  SimulateWillStartRequest();
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(NavigationThrottle::CANCEL_AND_IGNORE, callback_result());
  EXPECT_EQ(1, cancel_throttle->will_start_calls());
  EXPECT_EQ(0, cancel_throttle->will_redirect_calls());
  EXPECT_EQ(0, cancel_throttle->will_process_response_calls());
  EXPECT_EQ(0, proceed_throttle->will_start_calls());
  EXPECT_EQ(0, proceed_throttle->will_redirect_calls());
}

// Checks that a NavigationThrottle asking to cancel followed by a
// NavigationThrottle asking to proceed behave correctly in WillRedirectRequest.
// The navigation will be canceled directly, and the second throttle will not
// be called.
TEST_F(NavigationHandleImplTest, CancelThenProceedWillRedirectRequest) {
  TestNavigationThrottle* cancel_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::CANCEL_AND_IGNORE);
  TestNavigationThrottle* proceed_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::PROCEED);
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_EQ(0, cancel_throttle->will_start_calls());
  EXPECT_EQ(0, cancel_throttle->will_redirect_calls());
  EXPECT_EQ(0, cancel_throttle->will_process_response_calls());
  EXPECT_EQ(0, proceed_throttle->will_start_calls());
  EXPECT_EQ(0, proceed_throttle->will_redirect_calls());

  // Simulate WillRedirectRequest. The request should not be deferred. The
  // callback should not have been called. The second throttle should not have
  // been notified.
  SimulateWillRedirectRequest();
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(NavigationThrottle::CANCEL_AND_IGNORE, callback_result());
  EXPECT_EQ(0, cancel_throttle->will_start_calls());
  EXPECT_EQ(1, cancel_throttle->will_redirect_calls());
  EXPECT_EQ(0, cancel_throttle->will_process_response_calls());
  EXPECT_EQ(0, proceed_throttle->will_start_calls());
  EXPECT_EQ(0, proceed_throttle->will_redirect_calls());
}

// Checks that a NavigationThrottle asking to proceed followed by a
// NavigationThrottle asking to cancel behave correctly in WillProcessResponse.
// Both throttles will be called, and the request will be cancelled.
TEST_F(NavigationHandleImplTest, ProceedThenCancelWillProcessResponse) {
  TestNavigationThrottle* proceed_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::PROCEED);
  TestNavigationThrottle* cancel_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::CANCEL_AND_IGNORE);
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_EQ(0, cancel_throttle->will_start_calls());
  EXPECT_EQ(0, cancel_throttle->will_redirect_calls());
  EXPECT_EQ(0, cancel_throttle->will_process_response_calls());
  EXPECT_EQ(0, proceed_throttle->will_start_calls());
  EXPECT_EQ(0, proceed_throttle->will_redirect_calls());
  EXPECT_EQ(0, proceed_throttle->will_process_response_calls());

  // Simulate WillRedirectRequest. The request should not be deferred. The
  // callback should have been called.
  SimulateWillProcessResponse();
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(NavigationThrottle::CANCEL_AND_IGNORE, callback_result());
  EXPECT_EQ(0, cancel_throttle->will_start_calls());
  EXPECT_EQ(0, cancel_throttle->will_redirect_calls());
  EXPECT_EQ(1, cancel_throttle->will_process_response_calls());
  EXPECT_EQ(0, proceed_throttle->will_start_calls());
  EXPECT_EQ(0, proceed_throttle->will_redirect_calls());
  EXPECT_EQ(1, proceed_throttle->will_process_response_calls());
}

// Checks that a NavigationThrottle asking to cancel followed by a
// NavigationThrottle asking to proceed behave correctly in WillProcessResponse.
// The navigation will be canceled directly, and the second throttle will not
// be called.
TEST_F(NavigationHandleImplTest, CancelThenProceedWillProcessResponse) {
  TestNavigationThrottle* cancel_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::CANCEL_AND_IGNORE);
  TestNavigationThrottle* proceed_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::PROCEED);
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_EQ(0, cancel_throttle->will_start_calls());
  EXPECT_EQ(0, cancel_throttle->will_redirect_calls());
  EXPECT_EQ(0, cancel_throttle->will_process_response_calls());
  EXPECT_EQ(0, proceed_throttle->will_start_calls());
  EXPECT_EQ(0, proceed_throttle->will_redirect_calls());

  // Simulate WillRedirectRequest. The request should not be deferred. The
  // callback should have been called. The second throttle should not have
  // been notified.
  SimulateWillProcessResponse();
  EXPECT_FALSE(IsDeferringStart());
  EXPECT_FALSE(IsDeferringRedirect());
  EXPECT_TRUE(was_callback_called());
  EXPECT_EQ(NavigationThrottle::CANCEL_AND_IGNORE, callback_result());
  EXPECT_EQ(0, cancel_throttle->will_start_calls());
  EXPECT_EQ(0, cancel_throttle->will_redirect_calls());
  EXPECT_EQ(1, cancel_throttle->will_process_response_calls());
  EXPECT_EQ(0, proceed_throttle->will_start_calls());
  EXPECT_EQ(0, proceed_throttle->will_redirect_calls());
  EXPECT_EQ(0, proceed_throttle->will_process_response_calls());
}

}  // namespace content
