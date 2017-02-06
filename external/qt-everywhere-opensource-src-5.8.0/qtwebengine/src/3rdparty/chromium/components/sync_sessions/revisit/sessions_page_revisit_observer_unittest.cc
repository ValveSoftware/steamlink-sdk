// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/revisit/sessions_page_revisit_observer.h"

#include <memory>
#include <string>

#include "base/memory/ptr_util.h"
#include "base/test/histogram_tester.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "components/sessions/core/session_types.h"
#include "components/sync_sessions/revisit/page_visit_observer.h"
#include "components/sync_sessions/synced_session.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using sessions::SessionTab;
using sessions::SessionWindow;
using sync_driver::SyncedSession;

namespace sync_sessions {

namespace {

static const std::string kExampleUrl = "http://www.example.com";
static const std::string kDifferentUrl = "http://www.different.com";

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
        base::WrapUnique(new TestForeignSessionsProvider(sessions, true)));
    CheckAndExpect(&observer, url, current_match, offset_match);
  }
};

TEST_F(SessionsPageRevisitObserverTest, RunMatchersNoSessions) {
  std::vector<const SyncedSession*> sessions;
  SessionsPageRevisitObserver observer(
      base::WrapUnique(new TestForeignSessionsProvider(sessions, true)));
  CheckAndExpect(&observer, GURL(kExampleUrl), false, false);
}

TEST_F(SessionsPageRevisitObserverTest, RunMatchersNoWindows) {
  std::unique_ptr<SyncedSession> session(new SyncedSession());
  CheckAndExpect(session.get(), GURL(kExampleUrl), false, false);
}

TEST_F(SessionsPageRevisitObserverTest, RunMatchersNoTabs) {
  std::unique_ptr<SyncedSession> session(new SyncedSession());
  session->windows[0] = new SessionWindow();
  CheckAndExpect(session.get(), GURL(kExampleUrl), false, false);
}

TEST_F(SessionsPageRevisitObserverTest, RunMatchersNoEntries) {
  std::unique_ptr<SessionWindow> window(new SessionWindow());
  window->tabs.push_back(new SessionTab());
  std::unique_ptr<SyncedSession> session(new SyncedSession());
  session->windows[0] = window.release();
  CheckAndExpect(session.get(), GURL(kExampleUrl), false, false);
}

TEST_F(SessionsPageRevisitObserverTest, RunMatchersSingle) {
  std::unique_ptr<SessionTab> tab(new SessionTab());
  tab->navigations.push_back(
      sessions::SerializedNavigationEntryTestHelper::CreateNavigation(
          kExampleUrl, ""));
  tab->current_navigation_index = 0;
  std::unique_ptr<SessionWindow> window(new SessionWindow());
  window->tabs.push_back(tab.release());
  std::unique_ptr<SyncedSession> session(new SyncedSession());
  session->windows[0] = window.release();
  CheckAndExpect(session.get(), GURL(kExampleUrl), true, false);
}

TEST_F(SessionsPageRevisitObserverTest, RunMatchersFalseProvider) {
  std::unique_ptr<SessionTab> tab(new SessionTab());
  tab->navigations.push_back(
      sessions::SerializedNavigationEntryTestHelper::CreateNavigation(
          kExampleUrl, ""));
  tab->navigations.push_back(
      sessions::SerializedNavigationEntryTestHelper::CreateNavigation(
          kExampleUrl, ""));
  tab->current_navigation_index = 1;
  std::unique_ptr<SessionWindow> window(new SessionWindow());
  window->tabs.push_back(tab.release());
  std::unique_ptr<SyncedSession> session(new SyncedSession());
  session->windows[0] = window.release();

  // The provider returns false when asked for foreign sessions, even though
  // it has has a valid tab.
  std::vector<const SyncedSession*> sessions;
  sessions.push_back(session.get());
  SessionsPageRevisitObserver observer(
      base::WrapUnique(new TestForeignSessionsProvider(sessions, false)));
  CheckAndExpect(&observer, GURL(kExampleUrl), false, false);
}

TEST_F(SessionsPageRevisitObserverTest, RunMatchersMany) {
  std::unique_ptr<SessionTab> tab1(new SessionTab());
  tab1->navigations.push_back(
      sessions::SerializedNavigationEntryTestHelper::CreateNavigation(
          kExampleUrl, ""));
  tab1->current_navigation_index = 0;

  std::unique_ptr<SessionTab> tab2(new SessionTab());
  tab2->navigations.push_back(
      sessions::SerializedNavigationEntryTestHelper::CreateNavigation(
          kDifferentUrl, ""));
  tab2->current_navigation_index = 0;

  std::unique_ptr<SessionTab> tab3(new SessionTab());
  tab3->navigations.push_back(
      sessions::SerializedNavigationEntryTestHelper::CreateNavigation(
          kDifferentUrl, ""));
  tab3->current_navigation_index = 0;

  std::unique_ptr<SessionTab> tab4(new SessionTab());
  tab4->navigations.push_back(
      sessions::SerializedNavigationEntryTestHelper::CreateNavigation(
          kExampleUrl, ""));
  tab4->navigations.push_back(
      sessions::SerializedNavigationEntryTestHelper::CreateNavigation(
          kDifferentUrl, ""));
  tab4->current_navigation_index = 1;

  std::unique_ptr<SessionWindow> window1(new SessionWindow());
  window1->tabs.push_back(tab1.release());
  std::unique_ptr<SessionWindow> window2(new SessionWindow());
  window2->tabs.push_back(tab2.release());
  std::unique_ptr<SessionWindow> window3(new SessionWindow());
  window3->tabs.push_back(tab3.release());
  window3->tabs.push_back(tab4.release());

  std::unique_ptr<SyncedSession> session1(new SyncedSession());
  session1->windows[1] = window1.release();
  std::unique_ptr<SyncedSession> session2(new SyncedSession());
  session2->windows[2] = window2.release();
  session2->windows[3] = window3.release();

  std::vector<const SyncedSession*> sessions;
  sessions.push_back(session1.get());
  sessions.push_back(session2.get());
  SessionsPageRevisitObserver observer(
      base::WrapUnique(new TestForeignSessionsProvider(sessions, true)));

  base::HistogramTester histogram_tester;
  CheckAndExpect(&observer, GURL(kExampleUrl), true, true);
}

}  // namespace sync_sessions
