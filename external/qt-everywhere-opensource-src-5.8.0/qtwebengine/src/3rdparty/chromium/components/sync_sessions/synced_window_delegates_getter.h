// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_SYNCED_WINDOW_DELEGATES_GETTER_H_
#define COMPONENTS_SYNC_SESSIONS_SYNCED_WINDOW_DELEGATES_GETTER_H_

#include <set>

#include "base/macros.h"
#include "components/sessions/core/session_id.h"

namespace browser_sync {

class SyncedWindowDelegate;

// An interface for accessing SyncedWindowDelegates. Subclasses define
// how this is done on different platforms.
class SyncedWindowDelegatesGetter {
 public:
  SyncedWindowDelegatesGetter();
  virtual ~SyncedWindowDelegatesGetter();

  // Returns all SyncedWindowDelegate instances.
  virtual std::set<const SyncedWindowDelegate*> GetSyncedWindowDelegates() = 0;

  // Find a SyncedWindowDelegate given its window's id.
  virtual const SyncedWindowDelegate* FindById(SessionID::id_type id) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncedWindowDelegatesGetter);
};

}  // namespace browser_sync

#endif  // COMPONENTS_SYNC_SESSIONS_SYNCED_WINDOW_DELEGATES_GETTER_H_
