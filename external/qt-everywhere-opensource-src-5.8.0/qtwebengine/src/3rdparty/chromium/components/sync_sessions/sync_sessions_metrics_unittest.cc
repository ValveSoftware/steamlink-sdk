// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/sync_sessions_metrics.h"

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "base/test/histogram_tester.h"
#include "base/time/time.h"
#include "components/sessions/core/session_types.h"
#include "components/sync_sessions/fake_sync_sessions_client.h"
#include "components/sync_sessions/sessions_sync_manager.h"
#include "components/sync_sessions/synced_session.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_driver::SyncedSession;
using sessions::SessionWindow;
using sessions::SessionTab;
using base::Time;
using base::TimeDelta;

namespace sync_sessions {

namespace {

class FakeSessionsSyncManager : public browser_sync::SessionsSyncManager {
 public:
  FakeSessionsSyncManager(SyncSessionsClient* sessions_client,
                          std::vector<std::unique_ptr<SyncedSession>>* sessions)
      : browser_sync::SessionsSyncManager(sessions_client,
                                          nullptr,
                                          nullptr,
                                          nullptr,
                                          base::Closure(),
                                          base::Closure()),
        sessions_(sessions) {}

  bool GetAllForeignSessions(
      std::vector<const sync_driver::SyncedSession*>* sessions) override {
    for (auto& session : *sessions_) {
      sessions->push_back(session.get());
    }
    return true;
  }

 private:
  std::vector<std::unique_ptr<SyncedSession>>* sessions_;
};

Time SecondsFromEpoch(int seconds) {
  return Time::UnixEpoch() + TimeDelta::FromSeconds(seconds);
}

}  // namespace

class SyncSessionsMetricsTest : public ::testing::Test {
 protected:
  SyncSessionsMetricsTest() : fake_manager_(&fake_client_, &sessions_) {}

  // Sets the tab/window/session timestamps and creates anything needed. The new
  // calls in here are safe because the session/window objects are going to
  // delete all their children when their destructors are invoked.
  void PushTab(size_t tabIndex, int windowIndex, Time timestamp) {
    // First add sessions/windows as necessary.
    while (tabIndex >= sessions_.size()) {
      sessions_.push_back(base::WrapUnique(new SyncedSession()));
    }
    if (sessions_[tabIndex]->windows.find(windowIndex) ==
        sessions_[tabIndex]->windows.end()) {
      sessions_[tabIndex]->windows[windowIndex] = new SessionWindow();
    }

    sessions_[tabIndex]->modified_time =
        std::max(sessions_[tabIndex]->modified_time, timestamp);
    sessions_[tabIndex]->windows[windowIndex]->timestamp = std::max(
        sessions_[tabIndex]->windows[windowIndex]->timestamp, timestamp);
    sessions_[tabIndex]->windows[windowIndex]->tabs.push_back(new SessionTab());
    sessions_[tabIndex]->windows[windowIndex]->tabs.back()->timestamp =
        timestamp;
  }

  // Removes the last tab at the given indexes. The idexes provided should be
  // valid for existing data, this method does not check their validity. Windows
  // are not cleaned up/removed if they're left with 0 tabs.
  void PopTab(size_t tabIndex, int windowIndex, Time timestamp) {
    sessions_[tabIndex]->modified_time =
        std::max(sessions_[tabIndex]->modified_time, timestamp);
    sessions_[tabIndex]->windows[windowIndex]->timestamp = std::max(
        sessions_[tabIndex]->windows[windowIndex]->timestamp, timestamp);
    delete sessions_[tabIndex]->windows[windowIndex]->tabs.back();
    sessions_[tabIndex]->windows[windowIndex]->tabs.pop_back();
  }

  // Runs MaxTabTimestamp on the current sessions data.
  Time MaxTabTimestamp() {
    std::vector<const SyncedSession*> typed_sessions;
    for (auto& session : sessions_) {
      typed_sessions.push_back(session.get());
    }
    return SyncSessionsMetrics::MaxTabTimestamp(typed_sessions);
  }

  browser_sync::SessionsSyncManager* get_sessions_sync_manager() {
    return &fake_manager_;
  }

 private:
  std::vector<std::unique_ptr<SyncedSession>> sessions_;
  FakeSyncSessionsClient fake_client_;
  FakeSessionsSyncManager fake_manager_;
};

TEST_F(SyncSessionsMetricsTest, NoWindows) {
  ASSERT_EQ(Time::UnixEpoch(), MaxTabTimestamp());
}

TEST_F(SyncSessionsMetricsTest, NoTabs) {
  PushTab(0, 0, SecondsFromEpoch(1));
  PopTab(0, 0, SecondsFromEpoch(2));
  ASSERT_EQ(Time::UnixEpoch(), MaxTabTimestamp());
}

TEST_F(SyncSessionsMetricsTest, OneTab) {
  PushTab(0, 0, SecondsFromEpoch(1));
  ASSERT_EQ(SecondsFromEpoch(1), MaxTabTimestamp());
}

TEST_F(SyncSessionsMetricsTest, MultipleTabs) {
  PushTab(0, 0, SecondsFromEpoch(1));
  PushTab(0, 0, SecondsFromEpoch(3));
  PushTab(0, 0, SecondsFromEpoch(2));
  ASSERT_EQ(SecondsFromEpoch(3), MaxTabTimestamp());
}

TEST_F(SyncSessionsMetricsTest, MultipleWindows) {
  PushTab(0, 1, SecondsFromEpoch(1));
  PushTab(0, 2, SecondsFromEpoch(3));
  PushTab(0, 3, SecondsFromEpoch(2));
  ASSERT_EQ(SecondsFromEpoch(3), MaxTabTimestamp());
}

TEST_F(SyncSessionsMetricsTest, OrderedSessions) {
  PushTab(0, 0, SecondsFromEpoch(3));
  PushTab(1, 0, SecondsFromEpoch(2));
  PushTab(2, 0, SecondsFromEpoch(1));
  ASSERT_EQ(SecondsFromEpoch(3), MaxTabTimestamp());
}

TEST_F(SyncSessionsMetricsTest, NonOrderedSessions) {
  // While 2 is not the max, it should give up when it sees the tab at index 1.
  // This is breaking the assumptions of the logic we're testing, and thus we
  // expect to return an incorrect ansewr. What this test is really verifying
  // is that the logic that finds the most recent timestamp is is exiting early
  // instead of inefficiently examining all foreign tabs.
  PushTab(0, 0, SecondsFromEpoch(2));
  PushTab(1, 0, SecondsFromEpoch(1));
  PushTab(2, 0, SecondsFromEpoch(3));
  ASSERT_EQ(SecondsFromEpoch(2), MaxTabTimestamp());
}

TEST_F(SyncSessionsMetricsTest, OrderedSessionsWithDeletedTab) {
  // Tab/window with index 0 is going to be the most recent, but the recent tab
  // was removed. The algorithm should continue on and find the tab in index 1.
  PushTab(0, 0, SecondsFromEpoch(1));
  PushTab(0, 0, SecondsFromEpoch(4));
  PopTab(0, 0, SecondsFromEpoch(5));
  PushTab(1, 0, SecondsFromEpoch(3));
  PushTab(2, 0, SecondsFromEpoch(2));
  ASSERT_EQ(SecondsFromEpoch(3), MaxTabTimestamp());
}

TEST_F(SyncSessionsMetricsTest, SkipEmitNoManager) {
  base::HistogramTester histogram_tester;
  PushTab(0, 0, Time::Now() - TimeDelta::FromHours(1));
  SyncSessionsMetrics::RecordYoungestForeignTabAgeOnNTP(nullptr);
  histogram_tester.ExpectTotalCount("Sync.YoungestForeignTabAgeOnNTP", 0);
}

TEST_F(SyncSessionsMetricsTest, SkipEmitNoSessions) {
  base::HistogramTester histogram_tester;
  SyncSessionsMetrics::RecordYoungestForeignTabAgeOnNTP(
      get_sessions_sync_manager());
  histogram_tester.ExpectTotalCount("Sync.YoungestForeignTabAgeOnNTP", 0);
}

TEST_F(SyncSessionsMetricsTest, SkipEmitInvalidTimestamp) {
  base::HistogramTester histogram_tester;
  // Foreign session is far in the future, it should be ignored.
  PushTab(0, 0, Time::Now() + TimeDelta::FromHours(1));
  SyncSessionsMetrics::RecordYoungestForeignTabAgeOnNTP(
      get_sessions_sync_manager());
  histogram_tester.ExpectTotalCount("Sync.YoungestForeignTabAgeOnNTP", 0);
}

TEST_F(SyncSessionsMetricsTest, EmitNormalCase) {
  base::HistogramTester histogram_tester;
  PushTab(0, 0, Time::Now() - TimeDelta::FromHours(1));
  SyncSessionsMetrics::RecordYoungestForeignTabAgeOnNTP(
      get_sessions_sync_manager());
  histogram_tester.ExpectTotalCount("Sync.YoungestForeignTabAgeOnNTP", 1);
}

}  // namespace sync_sessions
