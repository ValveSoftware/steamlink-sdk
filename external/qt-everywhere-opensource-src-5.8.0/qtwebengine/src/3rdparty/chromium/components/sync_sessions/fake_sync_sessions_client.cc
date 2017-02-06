// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/fake_sync_sessions_client.h"
#include "components/sync_sessions/local_session_event_router.h"

#include "url/gurl.h"

namespace sync_sessions {

FakeSyncSessionsClient::FakeSyncSessionsClient() {}
FakeSyncSessionsClient::~FakeSyncSessionsClient() {}

bookmarks::BookmarkModel* FakeSyncSessionsClient::GetBookmarkModel() {
  return nullptr;
}

favicon::FaviconService* FakeSyncSessionsClient::GetFaviconService() {
  return nullptr;
}

history::HistoryService* FakeSyncSessionsClient::GetHistoryService() {
  return nullptr;
}

bool FakeSyncSessionsClient::ShouldSyncURL(const GURL& url) const {
  return url.is_valid();
}

browser_sync::SyncedWindowDelegatesGetter*
FakeSyncSessionsClient::GetSyncedWindowDelegatesGetter() {
  return nullptr;
}

std::unique_ptr<browser_sync::LocalSessionEventRouter>
FakeSyncSessionsClient::GetLocalSessionEventRouter() {
  return nullptr;
}

}  // namespace sync_sessions
