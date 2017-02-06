// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/browser/test_profile_sync_service.h"

#include <utility>

syncer::TestIdFactory* TestProfileSyncService::id_factory() {
  return &id_factory_;
}

syncer::WeakHandle<syncer::JsEventHandler>
TestProfileSyncService::GetJsEventHandler() {
  return syncer::WeakHandle<syncer::JsEventHandler>();
}

TestProfileSyncService::TestProfileSyncService(
    ProfileSyncService::InitParams init_params)
    : ProfileSyncService(std::move(init_params)) {}

TestProfileSyncService::~TestProfileSyncService() {}

void TestProfileSyncService::OnConfigureDone(
    const sync_driver::DataTypeManager::ConfigureResult& result) {
  ProfileSyncService::OnConfigureDone(result);
  base::MessageLoop::current()->QuitWhenIdle();
}

syncer::UserShare* TestProfileSyncService::GetUserShare() const {
  return backend_->GetUserShare();
}
