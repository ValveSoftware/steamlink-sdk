// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/content/content_serialized_navigation_driver.h"

#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "content/public/common/page_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebReferrerPolicy.h"
#include "ui/base/page_transition_types.h"

namespace sessions {

// Tests that PageState data is properly sanitized when post data is present.
TEST(ContentSerializedNavigationDriverTest, PickleSanitizationWithPostData) {
  ContentSerializedNavigationDriver* driver =
      ContentSerializedNavigationDriver::GetInstance();
  SerializedNavigationEntry navigation =
      SerializedNavigationEntryTestHelper::CreateNavigationForTest();
  ASSERT_TRUE(navigation.has_post_data());

  // When post data is present, the page state should be sanitized.
  std::string sanitized_page_state =
      driver->GetSanitizedPageStateForPickle(&navigation);
  EXPECT_EQ(std::string(), sanitized_page_state);
}

// Tests that PageState data is left unsanitized when post data is absent.
TEST(ContentSerializedNavigationDriverTest, PickleSanitizationNoPostData) {
  ContentSerializedNavigationDriver* driver =
      ContentSerializedNavigationDriver::GetInstance();
  SerializedNavigationEntry navigation =
      SerializedNavigationEntryTestHelper::CreateNavigationForTest();
  SerializedNavigationEntryTestHelper::SetHasPostData(false, &navigation);
  ASSERT_FALSE(navigation.has_post_data());

  std::string sanitized_page_state =
      driver->GetSanitizedPageStateForPickle(&navigation);
  EXPECT_EQ(test_data::kEncodedPageState, sanitized_page_state);
}

// Tests that the input data is left unsanitized when the referrer policy is
// Always.
TEST(ContentSerializedNavigationDriverTest, SanitizeWithReferrerPolicyAlways) {
  ContentSerializedNavigationDriver* driver =
      ContentSerializedNavigationDriver::GetInstance();
  SerializedNavigationEntry navigation =
      SerializedNavigationEntryTestHelper::CreateNavigationForTest();
  SerializedNavigationEntryTestHelper::SetReferrerPolicy(
      blink::WebReferrerPolicyAlways, &navigation);

  content::PageState page_state =
      content::PageState::CreateFromURL(test_data::kVirtualURL);
  SerializedNavigationEntryTestHelper::SetEncodedPageState(
      page_state.ToEncodedData(), &navigation);

  driver->Sanitize(&navigation);
  EXPECT_EQ(test_data::kIndex, navigation.index());
  EXPECT_EQ(test_data::kUniqueID, navigation.unique_id());
  EXPECT_EQ(test_data::kReferrerURL, navigation.referrer_url());
  EXPECT_EQ(blink::WebReferrerPolicyAlways, navigation.referrer_policy());
  EXPECT_EQ(test_data::kVirtualURL, navigation.virtual_url());
  EXPECT_EQ(test_data::kTitle, navigation.title());
  EXPECT_EQ(page_state.ToEncodedData(), navigation.encoded_page_state());
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      navigation.transition_type(), test_data::kTransitionType));
  EXPECT_EQ(test_data::kHasPostData, navigation.has_post_data());
  EXPECT_EQ(test_data::kPostID, navigation.post_id());
  EXPECT_EQ(test_data::kOriginalRequestURL, navigation.original_request_url());
  EXPECT_EQ(test_data::kIsOverridingUserAgent,
            navigation.is_overriding_user_agent());
  EXPECT_EQ(test_data::kTimestamp, navigation.timestamp());
  EXPECT_EQ(test_data::kSearchTerms, navigation.search_terms());
  EXPECT_EQ(test_data::kFaviconURL, navigation.favicon_url());
  EXPECT_EQ(test_data::kHttpStatusCode, navigation.http_status_code());
}

// Tests that the input data is properly sanitized when the referrer policy is
// Never.
TEST(ContentSerializedNavigationDriverTest, SanitizeWithReferrerPolicyNever) {
  ContentSerializedNavigationDriver* driver =
      ContentSerializedNavigationDriver::GetInstance();
  SerializedNavigationEntry navigation =
      SerializedNavigationEntryTestHelper::CreateNavigationForTest();
  SerializedNavigationEntryTestHelper::SetReferrerPolicy(
      blink::WebReferrerPolicyNever, &navigation);

  content::PageState page_state =
      content::PageState::CreateFromURL(test_data::kVirtualURL);
  SerializedNavigationEntryTestHelper::SetEncodedPageState(
      page_state.ToEncodedData(), &navigation);

  driver->Sanitize(&navigation);

  // Fields that should remain untouched.
  EXPECT_EQ(test_data::kIndex, navigation.index());
  EXPECT_EQ(test_data::kUniqueID, navigation.unique_id());
  EXPECT_EQ(test_data::kVirtualURL, navigation.virtual_url());
  EXPECT_EQ(test_data::kTitle, navigation.title());
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      navigation.transition_type(), test_data::kTransitionType));
  EXPECT_EQ(test_data::kHasPostData, navigation.has_post_data());
  EXPECT_EQ(test_data::kPostID, navigation.post_id());
  EXPECT_EQ(test_data::kOriginalRequestURL, navigation.original_request_url());
  EXPECT_EQ(test_data::kIsOverridingUserAgent,
            navigation.is_overriding_user_agent());
  EXPECT_EQ(test_data::kTimestamp, navigation.timestamp());
  EXPECT_EQ(test_data::kSearchTerms, navigation.search_terms());
  EXPECT_EQ(test_data::kFaviconURL, navigation.favicon_url());
  EXPECT_EQ(test_data::kHttpStatusCode, navigation.http_status_code());

  // Fields that were sanitized.
  EXPECT_EQ(GURL(), navigation.referrer_url());
  EXPECT_EQ(blink::WebReferrerPolicyDefault, navigation.referrer_policy());
  EXPECT_EQ(page_state.ToEncodedData(), navigation.encoded_page_state());
}

}  // namespace sessions
