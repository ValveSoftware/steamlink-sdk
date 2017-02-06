// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/content/content_serialized_navigation_builder.h"

#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/page_state.h"
#include "content/public/common/referrer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sessions {

namespace {
// Create a NavigationEntry from the test_data constants in
// serialized_navigation_entry_test_helper.h.
std::unique_ptr<content::NavigationEntry> MakeNavigationEntryForTest() {
  std::unique_ptr<content::NavigationEntry> navigation_entry(
      content::NavigationEntry::Create());
  navigation_entry->SetReferrer(content::Referrer(
      test_data::kReferrerURL,
      static_cast<blink::WebReferrerPolicy>(test_data::kReferrerPolicy)));
  navigation_entry->SetVirtualURL(test_data::kVirtualURL);
  navigation_entry->SetTitle(test_data::kTitle);
  navigation_entry->SetPageState(
      content::PageState::CreateFromEncodedData(test_data::kEncodedPageState));
  navigation_entry->SetTransitionType(test_data::kTransitionType);
  navigation_entry->SetHasPostData(test_data::kHasPostData);
  navigation_entry->SetPostID(test_data::kPostID);
  navigation_entry->SetOriginalRequestURL(test_data::kOriginalRequestURL);
  navigation_entry->SetIsOverridingUserAgent(test_data::kIsOverridingUserAgent);
  navigation_entry->SetTimestamp(test_data::kTimestamp);
  navigation_entry->SetExtraData(kSearchTermsKey,
                                 test_data::kSearchTerms);
  navigation_entry->GetFavicon().valid = true;
  navigation_entry->GetFavicon().url = test_data::kFaviconURL;
  navigation_entry->SetHttpStatusCode(test_data::kHttpStatusCode);
  std::vector<GURL> redirect_chain;
  redirect_chain.push_back(test_data::kRedirectURL0);
  redirect_chain.push_back(test_data::kRedirectURL1);
  redirect_chain.push_back(test_data::kVirtualURL);
  navigation_entry->SetRedirectChain(redirect_chain);
  return navigation_entry;
}

}  // namespace


// Create a SerializedNavigationEntry from a NavigationEntry.  All its fields
// should match the NavigationEntry's.
TEST(ContentSerializedNavigationBuilderTest, FromNavigationEntry) {
  const std::unique_ptr<content::NavigationEntry> navigation_entry(
      MakeNavigationEntryForTest());

  const SerializedNavigationEntry& navigation =
      ContentSerializedNavigationBuilder::FromNavigationEntry(
          test_data::kIndex, *navigation_entry);

  EXPECT_EQ(test_data::kIndex, navigation.index());

  EXPECT_EQ(navigation_entry->GetUniqueID(), navigation.unique_id());
  EXPECT_EQ(test_data::kReferrerURL, navigation.referrer_url());
  EXPECT_EQ(test_data::kReferrerPolicy, navigation.referrer_policy());
  EXPECT_EQ(test_data::kVirtualURL, navigation.virtual_url());
  EXPECT_EQ(test_data::kTitle, navigation.title());
  EXPECT_EQ(test_data::kEncodedPageState, navigation.encoded_page_state());
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      navigation.transition_type(), test_data::kTransitionType));
  EXPECT_EQ(test_data::kHasPostData, navigation.has_post_data());
  EXPECT_EQ(test_data::kPostID, navigation.post_id());
  EXPECT_EQ(test_data::kOriginalRequestURL, navigation.original_request_url());
  EXPECT_EQ(test_data::kIsOverridingUserAgent,
            navigation.is_overriding_user_agent());
  EXPECT_EQ(test_data::kTimestamp, navigation.timestamp());
  EXPECT_EQ(test_data::kFaviconURL, navigation.favicon_url());
  EXPECT_EQ(test_data::kHttpStatusCode, navigation.http_status_code());
  ASSERT_EQ(3U, navigation.redirect_chain().size());
  EXPECT_EQ(test_data::kRedirectURL0, navigation.redirect_chain()[0]);
  EXPECT_EQ(test_data::kRedirectURL1, navigation.redirect_chain()[1]);
  EXPECT_EQ(test_data::kVirtualURL, navigation.redirect_chain()[2]);
}

// Create a NavigationEntry, then create another one by converting to
// a SerializedNavigationEntry and back.  The new one should match the old one
// except for fields that aren't preserved, which should be set to
// expected values.
TEST(ContentSerializedNavigationBuilderTest, ToNavigationEntry) {
  const std::unique_ptr<content::NavigationEntry> old_navigation_entry(
      MakeNavigationEntryForTest());

  const SerializedNavigationEntry& navigation =
      ContentSerializedNavigationBuilder::FromNavigationEntry(
          test_data::kIndex, *old_navigation_entry);

  const std::unique_ptr<content::NavigationEntry> new_navigation_entry(
      ContentSerializedNavigationBuilder::ToNavigationEntry(
          &navigation, test_data::kPageID, NULL));

  EXPECT_EQ(test_data::kReferrerURL, new_navigation_entry->GetReferrer().url);
  EXPECT_EQ(test_data::kReferrerPolicy,
            new_navigation_entry->GetReferrer().policy);
  EXPECT_EQ(test_data::kVirtualURL, new_navigation_entry->GetVirtualURL());
  EXPECT_EQ(test_data::kTitle, new_navigation_entry->GetTitle());
  EXPECT_EQ(test_data::kEncodedPageState,
            new_navigation_entry->GetPageState().ToEncodedData());
  EXPECT_EQ(test_data::kPageID, new_navigation_entry->GetPageID());
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      new_navigation_entry->GetTransitionType(), ui::PAGE_TRANSITION_RELOAD));
  EXPECT_EQ(test_data::kHasPostData, new_navigation_entry->GetHasPostData());
  EXPECT_EQ(test_data::kPostID, new_navigation_entry->GetPostID());
  EXPECT_EQ(test_data::kOriginalRequestURL,
            new_navigation_entry->GetOriginalRequestURL());
  EXPECT_EQ(test_data::kIsOverridingUserAgent,
            new_navigation_entry->GetIsOverridingUserAgent());
  base::string16 search_terms;
  new_navigation_entry->GetExtraData(kSearchTermsKey, &search_terms);
  EXPECT_EQ(test_data::kSearchTerms, search_terms);
  EXPECT_EQ(test_data::kHttpStatusCode,
            new_navigation_entry->GetHttpStatusCode());
  ASSERT_EQ(3U, new_navigation_entry->GetRedirectChain().size());
  EXPECT_EQ(test_data::kRedirectURL0,
            new_navigation_entry->GetRedirectChain()[0]);
  EXPECT_EQ(test_data::kRedirectURL1,
            new_navigation_entry->GetRedirectChain()[1]);
  EXPECT_EQ(test_data::kVirtualURL,
            new_navigation_entry->GetRedirectChain()[2]);
}

}  // namespace sessions
