// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/revisit/sessions_page_revisit_observer.h"

#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/test/histogram_tester.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "components/sessions/core/session_types.h"
#include "components/sync_sessions/synced_session.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using sessions::SessionTab;
using sessions::SessionWindow;

namespace sync_sessions {

namespace {

static const char kExampleUrl[] = "http://www.example.com";
static const char kDifferentUrl[] = "http://www.different.com";

class TestForeignSessionsProvider : public ForeignSessionsProvider {
 public:
  TestForeignSessionsProvider(const std::vector<const SyncedSession*>& sessions,
                              bool return_value)
      : sessions_(sessions), return_value_(return_value) {}
  ~TestForeignSessionsProvider() override{};

  bool GetAllForeignSessions(
      std::vector<const SyncedSession*>* sessions) override {
    sessions->clear();
    *sessions = sessions_;
    return return_value_;
  }

 private:
  const std::vector<const SyncedSession*>& sessions_;
  const bool return_value_;
};

}  // namespace

class SessionsPageRevisitObserverTest : public ::testing::Test {
 protected:
  void CheckAndExpect(SessionsPageRevisitObserver* observer,
                      const GURL& url,
                      const bool current_match,
                      const bool offset_match) {
    base::HistogramTester histogram_tester;
    observer->CheckForRevisit(url, PageVisitObserver::kTransitionPage);

    histogram_tester.ExpectTotalCount("Sync.PageRevisitTabMatchTransition",
                                      current_match ? 1 : 0);
    histogram_tester.ExpectTotalCount("Sync.PageRevisitTabMissTransition",
                                      current_match ? 0 : 1);
    histogram_tester.ExpectTotalCount(
        "Sync.PageRevisitNavigationMatchTransition", offset_match ? 1 : 0);
    histogram_tester.ExpectTotalCount(
        "Sync.PageRevisitNavigationMissTransition", offset_match ? 0 : 1);
    histogram_tester.ExpectTotalCount("Sync.PageRevisitSessionDuration", 1);
  }

  void CheckAndExpect(const SyncedSession* session,
                      const GURL& url,
                      const bool current_match,
                      const bool offset_match) {
    std::vector<const SyncedSession*> sessions;
    sessions.push_back(session);
    SessionsPageRevisitObserver observer(
        base::MakeUnique<TestForeignSessionsProvider>(sessions, true));
    CheckAndExpect(&observer, url, current_match, offset_match);
  }
};

TEST_F(SessionsPageRevisitObserverTest, RunMatchersNoSessions) {
  std::vector<const SyncedSession*> sessions;
  SessionsPageRevisitObserver observer(
      base::MakeUnique<TestForeignSessionsProvider>(sessions, true));
  CheckAndExpect(&observer, GURL(kExampleUrl), false, false);
}

TEST_F(SessionsPageRevisitObserverTest, RunMatchersNoWindows) {
  std::unique_ptr<SyncedSession> session = base::MakeUnique<SyncedSession>();
  CheckAndExpect(session.get(), GURL(kExampleUrl), false, false);
}

TEST_F(SessionsPageRevisitObserverTest, RunMatchersNoTabs) {
  std::unique_ptr<SyncedSession> session = base::MakeUnique<SyncedSession>();
  session->windows[0] = base::MakeUnique<SessionWindow>();
  CheckAndExpect(session.get(), GURL(kExampleUrl), false, false);
}

TEST_F(SessionsPageRevisitObserverTest, RunMatchersNoEntries) {
  std::unique_ptr<SessionWindow> window = base::MakeUnique<SessionWindow>();
  window->tabs.push_back(base::MakeUnique<SessionTab>());
  std::unique_ptr<SyncedSession> session = base::MakeUnique<SyncedSession>();
  session->windows[0] = std::move(window);
  CheckAndExpect(session.get(), GURL(kExampleUrl), false, false);
}

TEST_F(SessionsPageRevisitObserverTest, RunMatchersSingle) {
  std::unique_ptr<SessionTab> tab = base::MakeUnique<SessionTab>();
  tab->navigations.push_back(
      sessions::SerializedNavigationEntryTestHelper::CreateNavigation(
          kExampleUrl, ""));
  tab->current_navigation_index = 0;
  std::unique_ptr<SessionWindow> window = base::MakeUnique<SessionWindow>();
  window->tabs.push_back(std::move(tab));
  std::unique_ptr<SyncedSession> session = base::MakeUnique<SyncedSession>();
  session->windows[0] = std::move(window);
  CheckAndExpect(session.get(), GURL(kExampleUrl), true, false);
}

TEST_F(SessionsPageRevisitObserverTest, RunMatchersFalseProvider) {
  std::unique_ptr<SessionTab> tab = base::MakeUnique<SessionTab>();
  tab->navigations.push_back(
      sessions::SerializedNavigationEntryTestHelper::CreateNavigation(
          kExampleUrl, ""));
  tab->navigations.push_back(
      sessions::SerializedNavigationEntryTestHelper::CreateNavigation(
          kExampleUrl, ""));
  tab->current_navigation_index = 1;
  std::unique_ptr<SessionWindow> window = base::MakeUnique<SessionWindow>();
  window->tabs.push_back(std::move(tab));
  std::unique_ptr<SyncedSession> session = base::MakeUnique<SyncedSession>();
  session->windows[0] = std::move(window);

  // The provider returns false when asked for foreign sessions, even though
  // it has has a valid tab.
  std::vector<const SyncedSession*> sessions;
  sessions.push_back(session.get());
  SessionsPageRevisitObserver observer(
      base::MakeUnique<TestForeignSessionsProvider>(sessions, false));
  CheckAndExpect(&observer, GURL(kExampleUrl), false, false);
}

TEST_F(SessionsPageRevisitObserverTest, RunMatchersMany) {
  std::unique_ptr<SessionTab> tab1 = base::MakeUnique<SessionTab>();
  tab1->navigations.push_back(
      sessions::SerializedNavigationEntryTestHelper::CreateNavigation(
          kExampleUrl, ""));
  tab1->current_navigation_index = 0;

  std::unique_ptr<SessionTab> tab2 = base::MakeUnique<SessionTab>();
  tab2->navigations.push_back(
      sessions::SerializedNavigationEntryTestHelper::CreateNavigation(
          kDifferentUrl, ""));
  tab2->current_navigation_index = 0;

  std::unique_ptr<SessionTab> tab3 = base::MakeUnique<SessionTab>();
  tab3->navigations.push_back(
      sessions::SerializedNavigationEntryTestHelper::CreateNavigation(
          kDifferentUrl, ""));
  tab3->current_navigation_index = 0;

  std::unique_ptr<SessionTab> tab4 = base::MakeUnique<SessionTab>();
  tab4->navigations.push_back(
      sessions::SerializedNavigationEntryTestHelper::CreateNavigation(
          kExampleUrl, ""));
  tab4->navigations.push_back(
      sessions::SerializedNavigationEntryTestHelper::CreateNavigation(
          kDifferentUrl, ""));
  tab4->current_navigation_index = 1;

  std::unique_ptr<SessionWindow> window1 = base::MakeUnique<SessionWindow>();
  window1->tabs.push_back(std::move(tab1));
  std::unique_ptr<SessionWindow> window2 = base::MakeUnique<SessionWindow>();
  window2->tabs.push_back(std::move(tab2));
  std::unique_ptr<SessionWindow> window3 = base::MakeUnique<SessionWindow>();
  window3->tabs.push_back(std::move(tab3));
  window3->tabs.push_back(std::move(tab4));

  std::unique_ptr<SyncedSession> session1 = base::MakeUnique<SyncedSession>();
  session1->windows[1] = std::move(window1);
  std::unique_ptr<SyncedSession> session2 = base::MakeUnique<SyncedSession>();
  session2->windows[2] = std::move(window2);
  session2->windows[3] = std::move(window3);

  std::vector<const SyncedSession*> sessions;
  sessions.push_back(session1.get());
  sessions.push_back(session2.get());
  SessionsPageRevisitObserver observer(
      base::MakeUnique<TestForeignSessionsProvider>(sessions, true));

  base::HistogramTester histogram_tester;
  CheckAndExpect(&observer, GURL(kExampleUrl), true, true);
}

}  // namespace sync_sessions
