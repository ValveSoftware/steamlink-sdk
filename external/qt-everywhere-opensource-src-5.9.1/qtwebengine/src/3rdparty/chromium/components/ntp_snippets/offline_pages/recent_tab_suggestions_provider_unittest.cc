// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/offline_pages/recent_tab_suggestions_provider.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_factory.h"
#include "components/ntp_snippets/content_suggestions_provider.h"
#include "components/ntp_snippets/mock_content_suggestions_provider_observer.h"
#include "components/ntp_snippets/offline_pages/offline_pages_test_utils.h"
#include "components/offline_pages/client_namespace_constants.h"
#include "components/offline_pages/offline_page_item.h"
#include "components/offline_pages/stub_offline_page_model.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ntp_snippets::test::CaptureDismissedSuggestions;
using ntp_snippets::test::FakeOfflinePageModel;
using offline_pages::ClientId;
using offline_pages::MultipleOfflinePageItemCallback;
using offline_pages::OfflinePageItem;
using offline_pages::StubOfflinePageModel;
using testing::_;
using testing::IsEmpty;
using testing::Mock;
using testing::Property;
using testing::SizeIs;

namespace ntp_snippets {

namespace {

OfflinePageItem CreateDummyRecentTab(int id) {
  return test::CreateDummyOfflinePageItem(id, offline_pages::kLastNNamespace);
}

std::vector<OfflinePageItem> CreateDummyRecentTabs(
    const std::vector<int>& ids) {
  std::vector<OfflinePageItem> result;
  for (int id : ids) {
    result.push_back(CreateDummyRecentTab(id));
  }
  return result;
}

OfflinePageItem CreateDummyRecentTab(int id, base::Time time) {
  OfflinePageItem item = CreateDummyRecentTab(id);
  item.last_access_time = time;
  return item;
}

}  // namespace

class RecentTabSuggestionsProviderTest : public testing::Test {
 public:
  RecentTabSuggestionsProviderTest()
      : pref_service_(new TestingPrefServiceSimple()) {
    RecentTabSuggestionsProvider::RegisterProfilePrefs(
        pref_service()->registry());

    provider_.reset(new RecentTabSuggestionsProvider(
        &observer_, &category_factory_, &model_, pref_service()));
  }

  Category recent_tabs_category() {
    return category_factory_.FromKnownCategory(KnownCategories::RECENT_TABS);
  }

  ContentSuggestion::ID GetDummySuggestionId(int id) {
    return ContentSuggestion::ID(recent_tabs_category(), base::IntToString(id));
  }

  void FireOfflinePageModelChanged(const std::vector<OfflinePageItem>& items) {
    *(model_.mutable_items()) = items;
    provider_->OfflinePageModelChanged(&model_);
  }

  void FireOfflinePageDeleted(const OfflinePageItem& item) {
    provider_->OfflinePageDeleted(item.offline_id, item.client_id);
  }

  std::set<std::string> ReadDismissedIDsFromPrefs() {
    return provider_->ReadDismissedIDsFromPrefs();
  }

  RecentTabSuggestionsProvider* provider() { return provider_.get(); }
  FakeOfflinePageModel* model() { return &model_; }
  MockContentSuggestionsProviderObserver* observer() { return &observer_; }
  TestingPrefServiceSimple* pref_service() { return pref_service_.get(); }

 private:
  FakeOfflinePageModel model_;
  MockContentSuggestionsProviderObserver observer_;
  CategoryFactory category_factory_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  // Last so that the dependencies are deleted after the provider.
  std::unique_ptr<RecentTabSuggestionsProvider> provider_;

  DISALLOW_COPY_AND_ASSIGN(RecentTabSuggestionsProviderTest);
};

TEST_F(RecentTabSuggestionsProviderTest, ShouldConvertToSuggestions) {
  EXPECT_CALL(
      *observer(),
      OnNewSuggestions(_, recent_tabs_category(),
                       UnorderedElementsAre(
                           Property(&ContentSuggestion::url,
                                    GURL("http://dummy.com/1")),
                           Property(&ContentSuggestion::url,
                                    GURL("http://dummy.com/2")),
                           Property(&ContentSuggestion::url,
                                    GURL("http://dummy.com/3")))));
  FireOfflinePageModelChanged(CreateDummyRecentTabs({1, 2, 3}));
}

TEST_F(RecentTabSuggestionsProviderTest, ShouldSortByMostRecentlyVisited) {
  base::Time now = base::Time::Now();
  base::Time yesterday = now - base::TimeDelta::FromDays(1);
  base::Time tomorrow = now + base::TimeDelta::FromDays(1);
  std::vector<OfflinePageItem> offline_pages = {
      CreateDummyRecentTab(1, now), CreateDummyRecentTab(2, yesterday),
      CreateDummyRecentTab(3, tomorrow)};

  EXPECT_CALL(
      *observer(),
      OnNewSuggestions(
          _, recent_tabs_category(),
          ElementsAre(Property(&ContentSuggestion::url,
                               GURL("http://dummy.com/3")),
                      Property(&ContentSuggestion::url,
                               GURL("http://dummy.com/1")),
                      Property(&ContentSuggestion::url,
                               GURL("http://dummy.com/2")))));
  FireOfflinePageModelChanged(offline_pages);
}

TEST_F(RecentTabSuggestionsProviderTest, ShouldDeliverCorrectCategoryInfo) {
  EXPECT_FALSE(
      provider()->GetCategoryInfo(recent_tabs_category()).has_more_action());
  EXPECT_FALSE(
      provider()->GetCategoryInfo(recent_tabs_category()).has_reload_action());
  EXPECT_FALSE(provider()
                   ->GetCategoryInfo(recent_tabs_category())
                   .has_view_all_action());
}

TEST_F(RecentTabSuggestionsProviderTest, ShouldDismiss) {
  FireOfflinePageModelChanged(CreateDummyRecentTabs({1, 2, 3, 4}));

  // Dismiss 2 and 3.
  EXPECT_CALL(*observer(), OnNewSuggestions(_, _, _)).Times(0);
  provider()->DismissSuggestion(GetDummySuggestionId(2));
  provider()->DismissSuggestion(GetDummySuggestionId(3));
  Mock::VerifyAndClearExpectations(observer());

  // They should disappear from the reported suggestions.
  EXPECT_CALL(
      *observer(),
      OnNewSuggestions(_, recent_tabs_category(),
                       UnorderedElementsAre(
                           Property(&ContentSuggestion::url,
                                    GURL("http://dummy.com/1")),
                           Property(&ContentSuggestion::url,
                                    GURL("http://dummy.com/4")))));

  FireOfflinePageModelChanged(model()->items());
  Mock::VerifyAndClearExpectations(observer());

  // And appear in the dismissed suggestions.
  std::vector<ContentSuggestion> dismissed_suggestions;
  provider()->GetDismissedSuggestionsForDebugging(
      recent_tabs_category(),
      base::Bind(&CaptureDismissedSuggestions, &dismissed_suggestions));
  EXPECT_THAT(
      dismissed_suggestions,
      UnorderedElementsAre(Property(&ContentSuggestion::url,
                                    GURL("http://dummy.com/2")),
                           Property(&ContentSuggestion::url,
                                    GURL("http://dummy.com/3"))));

  // Clear dismissed suggestions.
  provider()->ClearDismissedSuggestionsForDebugging(recent_tabs_category());

  // They should be gone from the dismissed suggestions.
  dismissed_suggestions.clear();
  provider()->GetDismissedSuggestionsForDebugging(
      recent_tabs_category(),
      base::Bind(&CaptureDismissedSuggestions, &dismissed_suggestions));
  EXPECT_THAT(dismissed_suggestions, IsEmpty());

  // And appear in the reported suggestions for the category again.
  EXPECT_CALL(*observer(),
              OnNewSuggestions(_, recent_tabs_category(), SizeIs(4)));
  FireOfflinePageModelChanged(model()->items());
  Mock::VerifyAndClearExpectations(observer());
}

TEST_F(RecentTabSuggestionsProviderTest,
       ShouldInvalidateWhenOfflinePageDeleted) {
  std::vector<OfflinePageItem> offline_pages = CreateDummyRecentTabs({1, 2, 3});
  FireOfflinePageModelChanged(offline_pages);

  // Invalidation of suggestion 2 should be forwarded.
  EXPECT_CALL(*observer(), OnSuggestionInvalidated(_, GetDummySuggestionId(2)));
  FireOfflinePageDeleted(offline_pages[1]);
}

TEST_F(RecentTabSuggestionsProviderTest, ShouldClearDismissedOnInvalidate) {
  std::vector<OfflinePageItem> offline_pages = CreateDummyRecentTabs({1, 2, 3});
  FireOfflinePageModelChanged(offline_pages);
  EXPECT_THAT(ReadDismissedIDsFromPrefs(), IsEmpty());

  provider()->DismissSuggestion(GetDummySuggestionId(2));
  EXPECT_THAT(ReadDismissedIDsFromPrefs(), SizeIs(1));

  FireOfflinePageDeleted(offline_pages[1]);
  EXPECT_THAT(ReadDismissedIDsFromPrefs(), IsEmpty());
}

TEST_F(RecentTabSuggestionsProviderTest, ShouldClearDismissedOnFetch) {
  FireOfflinePageModelChanged(CreateDummyRecentTabs({1, 2, 3}));

  provider()->DismissSuggestion(GetDummySuggestionId(2));
  provider()->DismissSuggestion(GetDummySuggestionId(3));
  EXPECT_THAT(ReadDismissedIDsFromPrefs(), SizeIs(2));

  FireOfflinePageModelChanged(CreateDummyRecentTabs({2}));
  EXPECT_THAT(ReadDismissedIDsFromPrefs(), SizeIs(1));

  FireOfflinePageModelChanged(std::vector<OfflinePageItem>());
  EXPECT_THAT(ReadDismissedIDsFromPrefs(), IsEmpty());
}

}  // namespace ntp_snippets
