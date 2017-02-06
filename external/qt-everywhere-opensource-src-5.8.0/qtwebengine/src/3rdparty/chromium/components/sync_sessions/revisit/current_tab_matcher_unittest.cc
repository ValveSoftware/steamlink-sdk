// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/revisit/current_tab_matcher.h"

#include <memory>
#include <string>

#include "base/test/histogram_tester.h"
#include "base/time/time.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "components/sessions/core/session_types.h"
#include "components/sync_sessions/revisit/page_equality.h"
#include "components/sync_sessions/revisit/page_visit_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using sessions::SessionTab;

namespace sync_sessions {

namespace {

static const std::string kExampleUrl = "http://www.example.com";
static const std::string kDifferentUrl = "http://www.different.com";

sessions::SerializedNavigationEntry Entry(const std::string& url) {
  return sessions::SerializedNavigationEntryTestHelper::CreateNavigation(url,
                                                                         "");
}

std::unique_ptr<SessionTab> Tab(const int index, const base::Time timestamp) {
  std::unique_ptr<SessionTab> tab(new SessionTab());
  tab->current_navigation_index = index;
  tab->timestamp = timestamp;
  return tab;
}

void VerifyMatch(CurrentTabMatcher* matcher) {
  base::HistogramTester histogram_tester;
  matcher->Emit(PageVisitObserver::kTransitionPage);
  histogram_tester.ExpectUniqueSample("Sync.PageRevisitTabMatchTransition",
                                      PageVisitObserver::kTransitionPage, 1);
  histogram_tester.ExpectTotalCount("Sync.PageRevisitTabMatchAge", 1);
}

void VerifyMiss(CurrentTabMatcher* matcher) {
  base::HistogramTester histogram_tester;
  matcher->Emit(PageVisitObserver::kTransitionPage);
  histogram_tester.ExpectUniqueSample("Sync.PageRevisitTabMissTransition",
                                      PageVisitObserver::kTransitionPage, 1);
}

}  // namespace

TEST(CurrentTabMatcherTest, NoCheck) {
  CurrentTabMatcher matcher((PageEquality(GURL(kExampleUrl))));
  VerifyMiss(&matcher);
}

TEST(CurrentTabMatcherTest, EmptyTab) {
  std::unique_ptr<SessionTab> tab = Tab(0, base::Time::Now());
  CurrentTabMatcher matcher((PageEquality(GURL(kExampleUrl))));
  matcher.Check(tab.get());
  VerifyMiss(&matcher);
}

TEST(CurrentTabMatcherTest, SameUrl) {
  std::unique_ptr<SessionTab> tab = Tab(0, base::Time::Now());
  tab->navigations.push_back(Entry(kExampleUrl));

  CurrentTabMatcher matcher((PageEquality(GURL(kExampleUrl))));
  matcher.Check(tab.get());
  VerifyMatch(&matcher);
}

TEST(CurrentTabMatcherTest, DifferentUrl) {
  std::unique_ptr<SessionTab> tab = Tab(0, base::Time::Now());
  tab->navigations.push_back(Entry(kDifferentUrl));

  CurrentTabMatcher matcher((PageEquality(GURL(kExampleUrl))));
  matcher.Check(tab.get());
  VerifyMiss(&matcher);
}

TEST(CurrentTabMatcherTest, DifferentIndex) {
  std::unique_ptr<SessionTab> tab = Tab(0, base::Time::Now());
  tab->navigations.push_back(Entry(kDifferentUrl));
  tab->navigations.push_back(Entry(kExampleUrl));

  CurrentTabMatcher matcher((PageEquality(GURL(kExampleUrl))));
  matcher.Check(tab.get());
  VerifyMiss(&matcher);
}

TEST(CurrentTabMatcherTest, Timestamp) {
  std::unique_ptr<SessionTab> tab1 = Tab(0, base::Time::UnixEpoch());
  tab1->navigations.push_back(Entry(kExampleUrl));

  std::unique_ptr<SessionTab> tab2 = Tab(0, base::Time::Now());
  tab2->navigations.push_back(Entry(kExampleUrl));

  CurrentTabMatcher matcher1((PageEquality(GURL(kExampleUrl))));
  matcher1.Check(tab1.get());
  matcher1.Check(tab2.get());
  ASSERT_EQ(tab2.get(), matcher1.most_recent_match_);

  // Now repeat the same test but check the tabs in the opposite order.
  CurrentTabMatcher matcher2((PageEquality(GURL(kExampleUrl))));
  matcher2.Check(tab2.get());
  matcher2.Check(tab1.get());
  ASSERT_EQ(tab2.get(), matcher2.most_recent_match_);
}

}  // namespace sync_sessions
