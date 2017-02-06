// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_SYNC_BROWSER_ABSTRACT_PROFILE_SYNC_SERVICE_TEST_H_
#define COMPONENTS_BROWSER_SYNC_BROWSER_ABSTRACT_PROFILE_SYNC_SERVICE_TEST_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/test/test_message_loop.h"
#include "components/browser_sync/browser/profile_sync_test_util.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/change_record.h"
#include "testing/gtest/include/gtest/gtest.h"

class TestProfileSyncService;

namespace syncer {
struct UserShare;
}  //  namespace syncer

class ProfileSyncServiceTestHelper {
 public:
  static syncer::ImmutableChangeRecordList MakeSingletonChangeRecordList(
      int64_t node_id,
      syncer::ChangeRecord::Action action);

  // Deletions must provide an EntitySpecifics for the deleted data.
  static syncer::ImmutableChangeRecordList
  MakeSingletonDeletionChangeRecordList(
      int64_t node_id,
      const sync_pb::EntitySpecifics& specifics);

 private:
  DISALLOW_COPY_AND_ASSIGN(ProfileSyncServiceTestHelper);
};

class AbstractProfileSyncServiceTest : public testing::Test {
 public:
  AbstractProfileSyncServiceTest();
  ~AbstractProfileSyncServiceTest() override;

  bool CreateRoot(syncer::ModelType model_type);

 protected:
  // Creates a TestProfileSyncService instance based on
  // |profile_sync_service_bundle_|, with start behavior
  // browser_sync::AUTO_START. Passes |callback| down to
  // SyncManagerForProfileSyncTest to be used by NotifyInitializationSuccess.
  // |sync_client| is passed to the service. The created service is stored in
  // |sync_service_|.
  void CreateSyncService(std::unique_ptr<sync_driver::SyncClient> sync_client,
                         const base::Closure& initialization_success_callback);

  base::Thread* data_type_thread() { return &data_type_thread_; }

  TestProfileSyncService* sync_service() { return sync_service_.get(); }

  // Returns the callback for the FakeSyncClient builder. It is not possible to
  // just Bind() sync_service(), because of Callback not understanding the
  // inheritance of its template arguments.
  base::Callback<sync_driver::SyncService*(void)> GetSyncServiceCallback();

  browser_sync::ProfileSyncServiceBundle* profile_sync_service_bundle() {
    return &profile_sync_service_bundle_;
  }

 private:
  // Use |data_type_thread_| for code disallowed on the UI thread.
  base::Thread data_type_thread_;

  base::TestMessageLoop message_loop_;
  browser_sync::ProfileSyncServiceBundle profile_sync_service_bundle_;
  std::unique_ptr<TestProfileSyncService> sync_service_;

  base::ScopedTempDir temp_dir_;  // To pass to the backend host.

  DISALLOW_COPY_AND_ASSIGN(AbstractProfileSyncServiceTest);
};

class CreateRootHelper {
 public:
  CreateRootHelper(AbstractProfileSyncServiceTest* test,
                   syncer::ModelType model_type);
  virtual ~CreateRootHelper();

  const base::Closure& callback() const;
  bool success();

 private:
  void CreateRootCallback();

  base::Closure callback_;
  AbstractProfileSyncServiceTest* test_;
  syncer::ModelType model_type_;
  bool success_;

  DISALLOW_COPY_AND_ASSIGN(CreateRootHelper);
};

#endif  // COMPONENTS_BROWSER_SYNC_BROWSER_ABSTRACT_PROFILE_SYNC_SERVICE_TEST_H_
