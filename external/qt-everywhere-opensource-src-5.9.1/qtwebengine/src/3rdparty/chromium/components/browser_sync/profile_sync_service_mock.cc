// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/profile_sync_service_mock.h"

#include <utility>

namespace browser_sync {

ProfileSyncServiceMock::ProfileSyncServiceMock(InitParams init_params)
    : ProfileSyncService(std::move(init_params)) {
  ON_CALL(*this, IsSyncRequested()).WillByDefault(testing::Return(true));
}

ProfileSyncServiceMock::ProfileSyncServiceMock(InitParams* init_params)
    : ProfileSyncServiceMock(std::move(*init_params)) {}

ProfileSyncServiceMock::~ProfileSyncServiceMock() {}

}  // namespace browser_sync
