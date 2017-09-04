// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_SYNC_SESSIONS_METRICS_H_
#define COMPONENTS_SYNC_SESSIONS_SYNC_SESSIONS_METRICS_H_

#include <vector>

namespace base {
class Time;
}  // namespace base

namespace sync_sessions {

class SessionsSyncManager;
struct SyncedSession;

class SyncSessionsMetrics {
 public:
  // Records via an UMA histogram the age of the youngest foreign tab the given
  // manager is aware of. No attempt is made to aquire a more recent version of
  // world. If anything goes wrong, such as the manager being null, no foreign
  // session present, or a foreign tab with a timestamp in the future then no
  // metric is emitted.
  static void RecordYoungestForeignTabAgeOnNTP(
      SessionsSyncManager* sessions_sync_manager);

 private:
  friend class SyncSessionsMetricsTest;

  // Returns the highest timestamp for any tab in the given sessions. Note that
  // sessions should be sorted by timestamp, and session timestamps should
  // always be greater or equal to children window/tab timestamps. No navigation
  // timestamps are checked.
  static base::Time MaxTabTimestamp(
      const std::vector<const SyncedSession*>& sessions);
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_SYNC_SESSIONS_METRICS_H_
