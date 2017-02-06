// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/navigation_interception/intercept_navigation_throttle.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "components/navigation_interception/navigation_params.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::NavigationThrottle;
using testing::_;
using testing::Eq;
using testing::Ne;
using testing::Property;
using testing::Return;

namespace navigation_interception {

namespace {

const char kTestUrl[] = "http://www.test.com/";

// The MS C++ compiler complains about not being able to resolve which url()
// method (const or non-const) to use if we use the Property matcher to check
// the return value of the NavigationParams::url() method.
// It is possible to suppress the error by specifying the types directly but
// that results in very ugly syntax, which is why these custom matchers are
// used instead.
MATCHER(NavigationParamsUrlIsTest, "") {
  return arg.url() == GURL(kTestUrl);
}

}  // namespace

// MockInterceptCallbackReceiver ----------------------------------------------

class MockInterceptCallbackReceiver {
 public:
  MOCK_METHOD2(ShouldIgnoreNavigation,
               bool(content::WebContents* source,
                    const NavigationParams& navigation_params));
};

// InterceptNavigationThrottleTest ------------------------------------

class InterceptNavigationThrottleTest
    : public content::RenderViewHostTestHarness {
 public:
  InterceptNavigationThrottleTest()
      : mock_callback_receiver_(new MockInterceptCallbackReceiver()) {}

  NavigationThrottle::ThrottleCheckResult
  SimulateWillStart(const GURL& url, const GURL& sanitized_url, bool is_post) {
    std::unique_ptr<content::NavigationHandle> test_handle =
        content::NavigationHandle::CreateNavigationHandleForTesting(url,
                                                                    main_rfh());
    test_handle->RegisterThrottleForTesting(
        base::WrapUnique(new InterceptNavigationThrottle(
            test_handle.get(),
            base::Bind(&MockInterceptCallbackReceiver::ShouldIgnoreNavigation,
                       base::Unretained(mock_callback_receiver_.get())),
            true)));
    return test_handle->CallWillStartRequestForTesting(
        is_post, content::Referrer(), false, ui::PAGE_TRANSITION_LINK, false);
  }

  NavigationThrottle::ThrottleCheckResult Simulate302() {
    std::unique_ptr<content::NavigationHandle> test_handle =
        content::NavigationHandle::CreateNavigationHandleForTesting(
            GURL(kTestUrl), main_rfh());
    test_handle->RegisterThrottleForTesting(
        base::WrapUnique(new InterceptNavigationThrottle(
            test_handle.get(),
            base::Bind(&MockInterceptCallbackReceiver::ShouldIgnoreNavigation,
                       base::Unretained(mock_callback_receiver_.get())),
            true)));
    test_handle->CallWillStartRequestForTesting(
        true, content::Referrer(), false, ui::PAGE_TRANSITION_LINK, false);
    return test_handle->CallWillRedirectRequestForTesting(GURL(kTestUrl), false,
                                                          GURL(), false);
  }

  std::unique_ptr<MockInterceptCallbackReceiver> mock_callback_receiver_;
};

TEST_F(InterceptNavigationThrottleTest,
       RequestDeferredAndResumedIfNavigationNotIgnored) {
  ON_CALL(*mock_callback_receiver_, ShouldIgnoreNavigation(_, _))
      .WillByDefault(Return(false));
  EXPECT_CALL(
      *mock_callback_receiver_,
      ShouldIgnoreNavigation(web_contents(), NavigationParamsUrlIsTest()));
  NavigationThrottle::ThrottleCheckResult result =
      SimulateWillStart(GURL(kTestUrl), GURL(kTestUrl), false);

  EXPECT_EQ(NavigationThrottle::PROCEED, result);
}

TEST_F(InterceptNavigationThrottleTest,
       RequestDeferredAndCancelledIfNavigationIgnored) {
  ON_CALL(*mock_callback_receiver_, ShouldIgnoreNavigation(_, _))
      .WillByDefault(Return(true));
  EXPECT_CALL(
      *mock_callback_receiver_,
      ShouldIgnoreNavigation(web_contents(), NavigationParamsUrlIsTest()));
  NavigationThrottle::ThrottleCheckResult result =
      SimulateWillStart(GURL(kTestUrl), GURL(kTestUrl), false);

  EXPECT_EQ(NavigationThrottle::CANCEL_AND_IGNORE, result);
}

TEST_F(InterceptNavigationThrottleTest, CallbackIsPostFalseForGet) {
  EXPECT_CALL(*mock_callback_receiver_,
              ShouldIgnoreNavigation(
                  _, AllOf(NavigationParamsUrlIsTest(),
                           Property(&NavigationParams::is_post, Eq(false)))))
      .WillOnce(Return(false));

  NavigationThrottle::ThrottleCheckResult result =
      SimulateWillStart(GURL(kTestUrl), GURL(kTestUrl), false);

  EXPECT_EQ(NavigationThrottle::PROCEED, result);
}

TEST_F(InterceptNavigationThrottleTest, CallbackIsPostTrueForPost) {
  EXPECT_CALL(*mock_callback_receiver_,
              ShouldIgnoreNavigation(
                  _, AllOf(NavigationParamsUrlIsTest(),
                           Property(&NavigationParams::is_post, Eq(true)))))
      .WillOnce(Return(false));
  NavigationThrottle::ThrottleCheckResult result =
      SimulateWillStart(GURL(kTestUrl), GURL(kTestUrl), true);

  EXPECT_EQ(NavigationThrottle::PROCEED, result);
}

TEST_F(InterceptNavigationThrottleTest,
       CallbackIsPostFalseForPostConvertedToGetBy302) {
  EXPECT_CALL(*mock_callback_receiver_,
              ShouldIgnoreNavigation(
                  _, AllOf(NavigationParamsUrlIsTest(),
                           Property(&NavigationParams::is_post, Eq(true)))))
      .WillOnce(Return(false));
  EXPECT_CALL(*mock_callback_receiver_,
              ShouldIgnoreNavigation(
                  _, AllOf(NavigationParamsUrlIsTest(),
                           Property(&NavigationParams::is_post, Eq(false)))))
      .WillOnce(Return(false));
  NavigationThrottle::ThrottleCheckResult result = Simulate302();

  EXPECT_EQ(NavigationThrottle::PROCEED, result);
}

}  // namespace navigation_interception
