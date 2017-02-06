// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/sync_sessions_metrics.h"

#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/time/time.h"
#include "components/sessions/core/session_types.h"
#include "components/sync_sessions/sessions_sync_manager.h"
#include "components/sync_sessions/synced_session.h"

namespace sync_sessions {

// static
void SyncSessionsMetrics::RecordYoungestForeignTabAgeOnNTP(
    browser_sync::SessionsSyncManager* sessions_sync_manager) {
  if (sessions_sync_manager != NULL) {
    std::vector<const sync_driver::SyncedSession*> foreign_sessions;
    sessions_sync_manager->GetAllForeignSessions(&foreign_sessions);
    base::Time best(MaxTabTimestamp(foreign_sessions));
    base::Time now(base::Time::Now());
    // Don't emit metrics if the foreign tab is timestamped in the future. While
    // the timestamp is set on a different machine, and we may lose some
    // fraction of metrics to clock skew, we don't want the potential to have
    // bad machines with clocks many hours off causing lots of seemingly 0
    // second entries.
    if (base::Time::UnixEpoch() < best && best <= now) {
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Sync.YoungestForeignTabAgeOnNTP", (now - best).InSeconds(), 1,
          base::TimeDelta::FromDays(14).InSeconds(), 100);
    }
  }
}

// static
base::Time SyncSessionsMetrics::MaxTabTimestamp(
    const std::vector<const sync_driver::SyncedSession*>& sessions) {
  // While Sessions are ordered by recency, windows and tabs are not. Because
  // the timestamp of sessions are updated when windows/tabs are removed, we
  // only need to search until all the remaining sessions are older than the
  // most recent tab we've found so far.
  base::Time best(base::Time::UnixEpoch());
  for (const sync_driver::SyncedSession* session : sessions) {
    if (session->modified_time < best) {
      break;
    }
    for (const std::pair<const SessionID::id_type, sessions::SessionWindow*>&
             key_value : session->windows) {
      for (const sessions::SessionTab* tab : key_value.second->tabs) {
        best = std::max(best, tab->timestamp);
      }
    }
  }
  return best;
}

}  // namespace sync_sessions
