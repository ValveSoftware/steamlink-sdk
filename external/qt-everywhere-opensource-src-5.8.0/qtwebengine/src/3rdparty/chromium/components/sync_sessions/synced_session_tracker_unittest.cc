// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/synced_session_tracker.h"

#include <string>
#include <vector>

#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "components/sessions/core/session_types.h"
#include "components/sync_sessions/fake_sync_sessions_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

namespace {

const char kValidUrl[] = "http://www.example.com";
const char kInvalidUrl[] = "invalid.url";

}  // namespace

class SyncedSessionTrackerTest : public testing::Test {
 public:
  SyncedSessionTrackerTest() : tracker_(&sessions_client_) {}
  ~SyncedSessionTrackerTest() override {}

  SyncedSessionTracker* GetTracker() { return &tracker_; }

 private:
  sync_sessions::FakeSyncSessionsClient sessions_client_;
  SyncedSessionTracker tracker_;
};

TEST_F(SyncedSessionTrackerTest, GetSession) {
  sync_driver::SyncedSession* session1 = GetTracker()->GetSession("tag");
  sync_driver::SyncedSession* session2 = GetTracker()->GetSession("tag2");
  ASSERT_EQ(session1, GetTracker()->GetSession("tag"));
  ASSERT_NE(session1, session2);
  // Should clean up memory on its own.
}

TEST_F(SyncedSessionTrackerTest, GetTabUnmapped) {
  sessions::SessionTab* tab = GetTracker()->GetTab("tag", 0, 0);
  ASSERT_EQ(tab, GetTracker()->GetTab("tag", 0, 0));
  // Should clean up memory on its own.
}

TEST_F(SyncedSessionTrackerTest, PutWindowInSession) {
  GetTracker()->PutWindowInSession("tag", 0);
  sync_driver::SyncedSession* session = GetTracker()->GetSession("tag");
  ASSERT_EQ(1U, session->windows.size());
  // Should clean up memory on its own.
}

TEST_F(SyncedSessionTrackerTest, PutTabInWindow) {
  GetTracker()->PutWindowInSession("tag", 10);
  GetTracker()->PutTabInWindow("tag", 10, 15,
                               0);  // win id 10, tab id 15, tab ind 0.
  sync_driver::SyncedSession* session = GetTracker()->GetSession("tag");
  ASSERT_EQ(1U, session->windows.size());
  ASSERT_EQ(1U, session->windows[10]->tabs.size());
  ASSERT_EQ(GetTracker()->GetTab("tag", 15, 1), session->windows[10]->tabs[0]);
  // Should clean up memory on its own.
}

TEST_F(SyncedSessionTrackerTest, LookupAllForeignSessions) {
  std::vector<const sync_driver::SyncedSession*> sessions;
  ASSERT_FALSE(GetTracker()->LookupAllForeignSessions(
      &sessions, SyncedSessionTracker::PRESENTABLE));
  GetTracker()->GetSession("tag1");
  GetTracker()->PutWindowInSession("tag1", 0);
  GetTracker()->PutTabInWindow("tag1", 0, 15, 0);
  sessions::SessionTab* tab = GetTracker()->GetTab("tag1", 15, 1);
  ASSERT_TRUE(tab);
  tab->navigations.push_back(
      sessions::SerializedNavigationEntryTestHelper::CreateNavigation(kValidUrl,
                                                                      "title"));
  GetTracker()->GetSession("tag2");
  GetTracker()->GetSession("tag3");
  GetTracker()->PutWindowInSession("tag3", 0);
  GetTracker()->PutTabInWindow("tag3", 0, 15, 0);
  tab = GetTracker()->GetTab("tag3", 15, 1);
  ASSERT_TRUE(tab);
  tab->navigations.push_back(
      sessions::SerializedNavigationEntryTestHelper::CreateNavigation(
          kInvalidUrl, "title"));
  ASSERT_TRUE(GetTracker()->LookupAllForeignSessions(
      &sessions, SyncedSessionTracker::PRESENTABLE));
  // Only the session with a valid window and tab gets returned.
  ASSERT_EQ(1U, sessions.size());
  ASSERT_EQ("tag1", sessions[0]->session_tag);

  ASSERT_TRUE(GetTracker()->LookupAllForeignSessions(
      &sessions, SyncedSessionTracker::RAW));
  ASSERT_EQ(3U, sessions.size());
}

TEST_F(SyncedSessionTrackerTest, LookupSessionWindows) {
  std::vector<const sessions::SessionWindow*> windows;
  ASSERT_FALSE(GetTracker()->LookupSessionWindows("tag1", &windows));
  GetTracker()->GetSession("tag1");
  GetTracker()->PutWindowInSession("tag1", 0);
  GetTracker()->PutWindowInSession("tag1", 2);
  GetTracker()->GetSession("tag2");
  GetTracker()->PutWindowInSession("tag2", 0);
  GetTracker()->PutWindowInSession("tag2", 2);
  ASSERT_TRUE(GetTracker()->LookupSessionWindows("tag1", &windows));
  ASSERT_EQ(2U, windows.size());  // Only windows from tag1 session.
  ASSERT_NE((sessions::SessionWindow*)nullptr, windows[0]);
  ASSERT_NE((sessions::SessionWindow*)nullptr, windows[1]);
  ASSERT_NE(windows[1], windows[0]);
}

TEST_F(SyncedSessionTrackerTest, LookupSessionTab) {
  const sessions::SessionTab* tab;
  ASSERT_FALSE(GetTracker()->LookupSessionTab("tag1", 5, &tab));
  GetTracker()->GetSession("tag1");
  GetTracker()->PutWindowInSession("tag1", 0);
  GetTracker()->PutTabInWindow("tag1", 0, 5, 0);
  ASSERT_TRUE(GetTracker()->LookupSessionTab("tag1", 5, &tab));
  ASSERT_NE((sessions::SessionTab*)nullptr, tab);
}

TEST_F(SyncedSessionTrackerTest, Complex) {
  const std::string tag1 = "tag";
  const std::string tag2 = "tag2";
  const std::string tag3 = "tag3";
  std::vector<sessions::SessionTab *> tabs1, tabs2;
  sessions::SessionTab* temp_tab;
  ASSERT_TRUE(GetTracker()->Empty());
  ASSERT_EQ(0U, GetTracker()->num_synced_sessions());
  ASSERT_EQ(0U, GetTracker()->num_synced_tabs(tag1));
  tabs1.push_back(GetTracker()->GetTab(tag1, 0, 0));
  tabs1.push_back(GetTracker()->GetTab(tag1, 1, 1));
  tabs1.push_back(GetTracker()->GetTab(tag1, 2, 2));
  ASSERT_EQ(3U, GetTracker()->num_synced_tabs(tag1));
  ASSERT_EQ(1U, GetTracker()->num_synced_sessions());
  temp_tab = GetTracker()->GetTab(tag1, 0, 0);  // Already created.
  ASSERT_EQ(3U, GetTracker()->num_synced_tabs(tag1));
  ASSERT_EQ(1U, GetTracker()->num_synced_sessions());
  ASSERT_EQ(tabs1[0], temp_tab);
  tabs2.push_back(GetTracker()->GetTab(tag2, 0, 0));
  ASSERT_EQ(1U, GetTracker()->num_synced_tabs(tag2));
  ASSERT_EQ(2U, GetTracker()->num_synced_sessions());
  ASSERT_FALSE(GetTracker()->DeleteSession(tag3));

  sync_driver::SyncedSession* session = GetTracker()->GetSession(tag1);
  sync_driver::SyncedSession* session2 = GetTracker()->GetSession(tag2);
  sync_driver::SyncedSession* session3 = GetTracker()->GetSession(tag3);
  session3->device_type = sync_driver::SyncedSession::TYPE_OTHER;
  ASSERT_EQ(3U, GetTracker()->num_synced_sessions());

  ASSERT_TRUE(session);
  ASSERT_TRUE(session2);
  ASSERT_TRUE(session3);
  ASSERT_NE(session, session2);
  ASSERT_NE(session2, session3);
  ASSERT_TRUE(GetTracker()->DeleteSession(tag3));
  ASSERT_EQ(2U, GetTracker()->num_synced_sessions());

  GetTracker()->PutWindowInSession(tag1, 0);           // Create a window.
  GetTracker()->PutTabInWindow(tag1, 0, 2, 0);         // No longer unmapped.
  ASSERT_EQ(3U, GetTracker()->num_synced_tabs(tag1));  // Has not changed.

  const sessions::SessionTab* tab_ptr;
  ASSERT_TRUE(GetTracker()->LookupSessionTab(tag1, 0, &tab_ptr));
  ASSERT_EQ(tab_ptr, tabs1[0]);
  ASSERT_TRUE(GetTracker()->LookupSessionTab(tag1, 2, &tab_ptr));
  ASSERT_EQ(tab_ptr, tabs1[2]);
  ASSERT_FALSE(GetTracker()->LookupSessionTab(tag1, 3, &tab_ptr));
  ASSERT_FALSE(tab_ptr);

  std::vector<const sessions::SessionWindow*> windows;
  ASSERT_TRUE(GetTracker()->LookupSessionWindows(tag1, &windows));
  ASSERT_EQ(1U, windows.size());
  ASSERT_TRUE(GetTracker()->LookupSessionWindows(tag2, &windows));
  ASSERT_EQ(0U, windows.size());

  // The sessions don't have valid tabs, lookup should not succeed.
  std::vector<const sync_driver::SyncedSession*> sessions;
  ASSERT_FALSE(GetTracker()->LookupAllForeignSessions(
      &sessions, SyncedSessionTracker::PRESENTABLE));
  ASSERT_TRUE(GetTracker()->LookupAllForeignSessions(
      &sessions, SyncedSessionTracker::RAW));
  ASSERT_EQ(2U, sessions.size());

  GetTracker()->Clear();
  ASSERT_EQ(0U, GetTracker()->num_synced_tabs(tag1));
  ASSERT_EQ(0U, GetTracker()->num_synced_tabs(tag2));
  ASSERT_EQ(0U, GetTracker()->num_synced_sessions());
}

TEST_F(SyncedSessionTrackerTest, ManyGetTabs) {
  ASSERT_TRUE(GetTracker()->Empty());
  const int kMaxSessions = 10;
  const int kMaxTabs = 1000;
  const int kMaxAttempts = 10000;
  for (int j = 0; j < kMaxSessions; ++j) {
    std::string tag = base::StringPrintf("tag%d", j);
    for (int i = 0; i < kMaxAttempts; ++i) {
      // More attempts than tabs means we'll sometimes get the same tabs,
      // sometimes have to allocate new tabs.
      int rand_tab_num = base::RandInt(0, kMaxTabs);
      sessions::SessionTab* tab =
          GetTracker()->GetTab(tag, rand_tab_num, rand_tab_num + 1);
      ASSERT_TRUE(tab);
    }
  }
}

TEST_F(SyncedSessionTrackerTest, LookupTabNodeIds) {
  std::set<int> result;
  std::string tag1 = "session1";
  std::string tag2 = "session2";
  std::string tag3 = "session3";

  GetTracker()->GetTab(tag1, 1, 1);
  GetTracker()->GetTab(tag1, 2, 2);
  GetTracker()->LookupTabNodeIds(tag1, &result);
  EXPECT_EQ(2U, result.size());
  EXPECT_FALSE(result.end() == result.find(1));
  EXPECT_FALSE(result.end() == result.find(2));
  GetTracker()->LookupTabNodeIds(tag2, &result);
  EXPECT_TRUE(result.empty());

  GetTracker()->PutWindowInSession(tag1, 0);
  GetTracker()->PutTabInWindow(tag1, 0, 3, 0);
  GetTracker()->LookupTabNodeIds(tag1, &result);
  EXPECT_EQ(2U, result.size());

  GetTracker()->GetTab(tag1, 3, 3);
  GetTracker()->LookupTabNodeIds(tag1, &result);
  EXPECT_EQ(3U, result.size());
  EXPECT_FALSE(result.end() == result.find(3));

  GetTracker()->GetTab(tag2, 1, 21);
  GetTracker()->GetTab(tag2, 2, 22);
  GetTracker()->LookupTabNodeIds(tag2, &result);
  EXPECT_EQ(2U, result.size());
  EXPECT_FALSE(result.end() == result.find(21));
  EXPECT_FALSE(result.end() == result.find(22));
  GetTracker()->LookupTabNodeIds(tag1, &result);
  EXPECT_EQ(3U, result.size());
  EXPECT_FALSE(result.end() == result.find(1));
  EXPECT_FALSE(result.end() == result.find(2));

  GetTracker()->LookupTabNodeIds(tag3, &result);
  EXPECT_TRUE(result.empty());
  GetTracker()->PutWindowInSession(tag3, 1);
  GetTracker()->PutTabInWindow(tag3, 1, 5, 0);
  GetTracker()->LookupTabNodeIds(tag3, &result);
  EXPECT_TRUE(result.empty());
  EXPECT_FALSE(GetTracker()->DeleteSession(tag3));
  GetTracker()->LookupTabNodeIds(tag3, &result);
  EXPECT_TRUE(result.empty());

  EXPECT_FALSE(GetTracker()->DeleteSession(tag1));
  GetTracker()->LookupTabNodeIds(tag1, &result);
  EXPECT_TRUE(result.empty());
  GetTracker()->LookupTabNodeIds(tag2, &result);
  EXPECT_EQ(2U, result.size());
  EXPECT_FALSE(result.end() == result.find(21));
  EXPECT_FALSE(result.end() == result.find(22));

  GetTracker()->GetTab(tag2, 1, 21);
  GetTracker()->GetTab(tag2, 2, 23);
  GetTracker()->LookupTabNodeIds(tag2, &result);
  EXPECT_EQ(3U, result.size());
  EXPECT_FALSE(result.end() == result.find(21));
  EXPECT_FALSE(result.end() == result.find(22));
  EXPECT_FALSE(result.end() == result.find(23));

  EXPECT_FALSE(GetTracker()->DeleteSession(tag2));
  GetTracker()->LookupTabNodeIds(tag2, &result);
  EXPECT_TRUE(result.empty());
}

TEST_F(SyncedSessionTrackerTest, SessionTracking) {
  ASSERT_TRUE(GetTracker()->Empty());
  std::string tag1 = "tag1";
  std::string tag2 = "tag2";

  // Create some session information that is stale.
  sync_driver::SyncedSession* session1 = GetTracker()->GetSession(tag1);
  GetTracker()->PutWindowInSession(tag1, 0);
  GetTracker()->PutTabInWindow(tag1, 0, 0, 0);
  GetTracker()->PutTabInWindow(tag1, 0, 1, 1);
  GetTracker()->GetTab(tag1, 2, 3U)->window_id.set_id(0);  // Will be unmapped.
  GetTracker()->GetTab(tag1, 3, 4U)->window_id.set_id(0);  // Will be unmapped.
  GetTracker()->PutWindowInSession(tag1, 1);
  GetTracker()->PutTabInWindow(tag1, 1, 4, 0);
  GetTracker()->PutTabInWindow(tag1, 1, 5, 1);
  ASSERT_EQ(2U, session1->windows.size());
  ASSERT_EQ(2U, session1->windows[0]->tabs.size());
  ASSERT_EQ(2U, session1->windows[1]->tabs.size());
  ASSERT_EQ(6U, GetTracker()->num_synced_tabs(tag1));

  // Create a session that should not be affected.
  sync_driver::SyncedSession* session2 = GetTracker()->GetSession(tag2);
  GetTracker()->PutWindowInSession(tag2, 2);
  GetTracker()->PutTabInWindow(tag2, 2, 1, 0);
  ASSERT_EQ(1U, session2->windows.size());
  ASSERT_EQ(1U, session2->windows[2]->tabs.size());
  ASSERT_EQ(1U, GetTracker()->num_synced_tabs(tag2));

  // Reset tracking and get the current windows/tabs.
  // We simulate moving a tab from one window to another, then closing the
  // first window (including its one remaining tab), and opening a new tab
  // on the remaining window.

  // New tab, arrived before meta node so unmapped.
  GetTracker()->GetTab(tag1, 6, 7U);
  GetTracker()->ResetSessionTracking(tag1);
  GetTracker()->PutWindowInSession(tag1, 0);
  GetTracker()->PutTabInWindow(tag1, 0, 0, 0);
  // Tab 1 is closed.
  GetTracker()->PutTabInWindow(tag1, 0, 2, 1);  // No longer unmapped.
  // Tab 3 was unmapped and does not get used.
  GetTracker()->PutTabInWindow(tag1, 0, 4, 2);  // Moved from window 1.
  // Window 1 was closed, along with tab 5.
  GetTracker()->PutTabInWindow(tag1, 0, 6, 3);  // No longer unmapped.
  // Session 2 should not be affected.
  GetTracker()->CleanupSession(tag1);

  // Verify that only those parts of the session not owned have been removed.
  ASSERT_EQ(1U, session1->windows.size());
  ASSERT_EQ(4U, session1->windows[0]->tabs.size());
  ASSERT_EQ(1U, session2->windows.size());
  ASSERT_EQ(1U, session2->windows[2]->tabs.size());
  ASSERT_EQ(2U, GetTracker()->num_synced_sessions());
  ASSERT_EQ(4U, GetTracker()->num_synced_tabs(tag1));
  ASSERT_EQ(1U, GetTracker()->num_synced_tabs(tag2));

  // All memory should be properly deallocated by destructor for the
  // SyncedSessionTracker.
}

TEST_F(SyncedSessionTrackerTest, DeleteForeignTab) {
  std::string session_tag = "session_tag";
  int tab_id_1 = 1;
  int tab_id_2 = 2;
  int tab_node_id_3 = 3;
  int tab_node_id_4 = 4;
  std::set<int> result;

  GetTracker()->GetTab(session_tag, tab_id_1, tab_node_id_3);
  GetTracker()->GetTab(session_tag, tab_id_1, tab_node_id_4);
  GetTracker()->GetTab(session_tag, tab_id_2, tab_node_id_3);
  GetTracker()->GetTab(session_tag, tab_id_2, tab_node_id_4);

  GetTracker()->LookupTabNodeIds(session_tag, &result);
  EXPECT_EQ(2U, result.size());
  EXPECT_TRUE(result.find(tab_node_id_3) != result.end());
  EXPECT_TRUE(result.find(tab_node_id_4) != result.end());

  GetTracker()->DeleteForeignTab(session_tag, tab_node_id_3);
  GetTracker()->LookupTabNodeIds(session_tag, &result);
  EXPECT_EQ(1U, result.size());
  EXPECT_TRUE(result.find(tab_node_id_4) != result.end());

  GetTracker()->DeleteForeignTab(session_tag, tab_node_id_4);
  GetTracker()->LookupTabNodeIds(session_tag, &result);
  EXPECT_TRUE(result.empty());
}

}  // namespace browser_sync
