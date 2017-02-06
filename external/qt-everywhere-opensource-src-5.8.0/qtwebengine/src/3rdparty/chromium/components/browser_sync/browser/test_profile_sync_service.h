// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_SYNC_BROWSER_TEST_PROFILE_SYNC_SERVICE_H_
#define COMPONENTS_BROWSER_SYNC_BROWSER_TEST_PROFILE_SYNC_SERVICE_H_

#include "base/macros.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/sync_driver/data_type_manager.h"
#include "sync/internal_api/public/util/weak_handle.h"
#include "sync/js/js_event_handler.h"
#include "sync/test/engine/test_id_factory.h"

namespace sync_driver {
class SyncPrefs;
}  // namespace sync_driver

class TestProfileSyncService : public ProfileSyncService {
 public:
  explicit TestProfileSyncService(InitParams init_params);

  ~TestProfileSyncService() override;

  void OnConfigureDone(
      const sync_driver::DataTypeManager::ConfigureResult& result) override;

  // We implement our own version to avoid some DCHECKs.
  syncer::UserShare* GetUserShare() const override;

  syncer::TestIdFactory* id_factory();

  // Raise visibility to ease testing.
  using ProfileSyncService::NotifyObservers;

  sync_driver::SyncPrefs* sync_prefs() { return &sync_prefs_; }

 protected:
  // Return NULL handle to use in backend initialization to avoid receiving
  // js messages on UI loop when it's being destroyed, which are not deleted
  // and cause memory leak in test.
  syncer::WeakHandle<syncer::JsEventHandler> GetJsEventHandler() override;

 private:
  syncer::TestIdFactory id_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestProfileSyncService);
};

#endif  // COMPONENTS_BROWSER_SYNC_BROWSER_TEST_PROFILE_SYNC_SERVICE_H_
