// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_FAKE_SYNC_SESSIONS_CLIENT_H_
#define COMPONENTS_SYNC_SESSIONS_FAKE_SYNC_SESSIONS_CLIENT_H_

#include "base/macros.h"
#include "components/sync_sessions/sync_sessions_client.h"

namespace sync_sessions {

// Fake implementation of a SyncSessionsClient for testing.
class FakeSyncSessionsClient : public SyncSessionsClient {
 public:
  FakeSyncSessionsClient();
  ~FakeSyncSessionsClient() override;

  // SyncSessionsClient:
  bookmarks::BookmarkModel* GetBookmarkModel() override;
  favicon::FaviconService* GetFaviconService() override;
  history::HistoryService* GetHistoryService() override;
  bool ShouldSyncURL(const GURL& url) const override;
  browser_sync::SyncedWindowDelegatesGetter* GetSyncedWindowDelegatesGetter()
      override;
  std::unique_ptr<browser_sync::LocalSessionEventRouter>
  GetLocalSessionEventRouter() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeSyncSessionsClient);
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_FAKE_SYNC_SESSIONS_CLIENT_H_
