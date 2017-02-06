// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/run_loop.h"
#include "components/web_restrictions/browser/mock_web_restrictions_client.h"
#include "components/web_restrictions/browser/web_restrictions_client.h"
#include "components/web_restrictions/browser/web_restrictions_resource_throttle.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/net_errors.h"
#include "net/url_request/redirect_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_restrictions {

namespace {

class TestResourceController : public content::ResourceController {
 public:
  TestResourceController(const base::Closure& quit_closure)
      : resume_called_(false),
        cancel_with_error_called_(false),
        error_code_(0),
        quit_closure_(quit_closure) {}

  void Cancel() override {}
  void CancelAndIgnore() override {}
  void CancelWithError(int error_code) override {
    cancel_with_error_called_ = true;
    error_code_ = error_code;
    quit_closure_.Run();
  }
  void Resume() override {
    resume_called_ = true;
    quit_closure_.Run();
  }

  bool CancelWithErrorCalled() const { return cancel_with_error_called_; }

  int GetErrorCode() const { return error_code_; }

  bool ResumeCalled() const { return resume_called_; }

 private:
  bool resume_called_;
  bool cancel_with_error_called_;
  int error_code_;
  base::Closure quit_closure_;
};

}  // namespace

class WebRestrictionsResourceThrottleTest : public testing::Test {
 protected:
  WebRestrictionsResourceThrottleTest()
      : throttle_(&provider_, GURL("http://example.com"), true),
        controller_(run_loop_.QuitClosure()) {
    throttle_.set_controller_for_testing(&controller_);
  }

  void StartProvider() {
    provider_.SetAuthority("Good");
    bool defer;
    throttle_.WillStartRequest(&defer);
    run_loop_.Run();
  }

  // Mock the Java WebRestrictionsClient. The real version
  // would need a content provider to do anything.
  web_restrictions::MockWebRestrictionsClient mock_;
  content::TestBrowserThreadBundle thread_bundle_;
  WebRestrictionsClient provider_;
  WebRestrictionsResourceThrottle throttle_;
  base::RunLoop run_loop_;
  TestResourceController controller_;
};

TEST_F(WebRestrictionsResourceThrottleTest, WillStartRequest_NoAuthority) {
  WebRestrictionsResourceThrottle throttle(&provider_,
                                           GURL("http://example.com"), true);
  bool defer;
  throttle.WillStartRequest(&defer);
  // If there is no authority the request won't be deferred.
  EXPECT_FALSE(defer);
}

TEST_F(WebRestrictionsResourceThrottleTest, WillStartRequest_DeferredAllow) {
  // Test deferring with a resource provider, and that the correct results
  // are received.
  provider_.SetAuthority("Good");
  bool defer;
  throttle_.WillStartRequest(&defer);
  EXPECT_TRUE(defer);
  run_loop_.Run();
  EXPECT_TRUE(controller_.ResumeCalled());
  EXPECT_FALSE(controller_.CancelWithErrorCalled());
}

TEST_F(WebRestrictionsResourceThrottleTest, WillStartRequest_DeferredForbid) {
  provider_.SetAuthority("Bad");
  bool defer;
  throttle_.WillStartRequest(&defer);
  EXPECT_TRUE(defer);
  run_loop_.Run();
  EXPECT_FALSE(controller_.ResumeCalled());
  EXPECT_TRUE(controller_.CancelWithErrorCalled());
  EXPECT_EQ(net::ERR_BLOCKED_BY_ADMINISTRATOR, controller_.GetErrorCode());
}

TEST_F(WebRestrictionsResourceThrottleTest, WillStartRequest_Subresource) {
  // Only the main frame should be deferred.
  // Initialization of the controller is asynchronous, and this will only work
  // correctly if the provider is initialized. Run a main frame through this
  // first to ensure that everything is initialized.
  StartProvider();
  // Now the real test.
  WebRestrictionsResourceThrottle throttle(
      &provider_, GURL("http://example.com/sub"), false);
  base::RunLoop test_run_loop;
  TestResourceController test_controller(test_run_loop.QuitClosure());
  throttle.set_controller_for_testing(&test_controller);
  bool defer;
  throttle.WillStartRequest(&defer);
  EXPECT_FALSE(defer);
}

TEST_F(WebRestrictionsResourceThrottleTest, WillRedirectRequest_KnownUrl) {
  // Set up a cached url.
  StartProvider();
  // Using the same URL should not be deferred
  net::RedirectInfo redirect;
  redirect.new_url = GURL("http://example.com");
  bool defer;
  throttle_.WillRedirectRequest(redirect, &defer);
  EXPECT_FALSE(defer);
}

TEST_F(WebRestrictionsResourceThrottleTest, WillRedirectRequest_NewUrl) {
  // Set up a cached url.
  StartProvider();
  // Using a different URL should be deferred
  net::RedirectInfo redirect;
  redirect.new_url = GURL("http://example.com/2");
  base::RunLoop test_run_loop;
  TestResourceController test_controller(test_run_loop.QuitClosure());
  throttle_.set_controller_for_testing(&test_controller);
  bool defer;
  throttle_.WillRedirectRequest(redirect, &defer);
  EXPECT_TRUE(defer);
  // If we don't wait for the callback it may happen after the exit, which
  // results in accesses the redirect_url after the stack frame is freed.
  test_run_loop.Run();
}

}  // namespace web_restrictions
