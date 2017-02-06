// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_data_type_controller.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/autofill/core/browser/webdata/autocomplete_syncable_service.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/sync_driver/data_type_controller_mock.h"
#include "components/sync_driver/fake_sync_client.h"
#include "components/webdata_services/web_data_service_test_util.h"
#include "sync/api/sync_error.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::AutofillWebDataService;
using autofill::AutofillWebDataBackend;

namespace browser_sync {

namespace {

using testing::_;
using testing::Return;

class NoOpAutofillBackend : public AutofillWebDataBackend {
 public:
  NoOpAutofillBackend() {}
  ~NoOpAutofillBackend() override {}
  WebDatabase* GetDatabase() override { return NULL; }
  void AddObserver(
      autofill::AutofillWebDataServiceObserverOnDBThread* observer) override {}
  void RemoveObserver(
      autofill::AutofillWebDataServiceObserverOnDBThread* observer) override {}
  void RemoveExpiredFormElements() override {}
  void NotifyOfMultipleAutofillChanges() override {}
  void NotifyThatSyncHasStarted(syncer::ModelType /* model_type */) override {}
};

// Fake WebDataService implementation that stubs out the database loading.
class FakeWebDataService : public AutofillWebDataService {
 public:
  FakeWebDataService(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& db_task_runner)
      : AutofillWebDataService(ui_task_runner, db_task_runner),
        is_database_loaded_(false),
        db_task_runner_(db_task_runner),
        db_loaded_callback_(base::Callback<void(void)>()) {}

  // Mark the database as loaded and send out the appropriate notification.
  void LoadDatabase() {
    StartSyncableService();
    is_database_loaded_ = true;

    if (!db_loaded_callback_.is_null()) {
      db_loaded_callback_.Run();
      // Clear the callback here or the WDS and DTC will have refs to each other
      // and create a memory leak.
      db_loaded_callback_ = base::Callback<void(void)>();
    }
  }

  bool IsDatabaseLoaded() override { return is_database_loaded_; }

  void RegisterDBLoadedCallback(
      const base::Callback<void(void)>& callback) override {
    db_loaded_callback_ = callback;
  }

  void StartSyncableService() {
    // The |autofill_profile_syncable_service_| must be constructed on the DB
    // thread.
    base::RunLoop run_loop;
    db_task_runner_->PostTaskAndReply(
        FROM_HERE, base::Bind(&FakeWebDataService::CreateSyncableService,
                              base::Unretained(this)),
        run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  ~FakeWebDataService() override {}

  void CreateSyncableService() {
    ASSERT_TRUE(db_task_runner_->BelongsToCurrentThread());
    // These services are deleted in DestroySyncableService().
    autofill::AutocompleteSyncableService::CreateForWebDataServiceAndBackend(
        this, &autofill_backend_);
  }

  bool is_database_loaded_;
  NoOpAutofillBackend autofill_backend_;
  const scoped_refptr<base::SingleThreadTaskRunner> db_task_runner_;
  base::Callback<void(void)> db_loaded_callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeWebDataService);
};

class SyncAutofillDataTypeControllerTest : public testing::Test {
 public:
  SyncAutofillDataTypeControllerTest()
      : db_thread_("DB_Thread"),
        last_start_result_(sync_driver::DataTypeController::OK),
        weak_ptr_factory_(this) {}
  ~SyncAutofillDataTypeControllerTest() override {}

  void SetUp() override {
    db_thread_.Start();
    web_data_service_ = new FakeWebDataService(
        base::ThreadTaskRunnerHandle::Get(), db_thread_.task_runner());
    autofill_dtc_ = new AutofillDataTypeController(
        base::ThreadTaskRunnerHandle::Get(), db_thread_.task_runner(),
        base::Bind(&base::DoNothing), &sync_client_, web_data_service_);
  }

  void TearDown() override {
    web_data_service_->ShutdownOnUIThread();

    // Make sure WebDataService is shutdown properly on DB thread before we
    // destroy it.
    base::RunLoop run_loop;
    ASSERT_TRUE(db_thread_.task_runner()->PostTaskAndReply(
        FROM_HERE, base::Bind(&base::DoNothing), run_loop.QuitClosure()));
    run_loop.Run();
  }

  // Passed to AutofillDTC::Start().
  void OnStartFinished(sync_driver::DataTypeController::ConfigureResult result,
                       const syncer::SyncMergeResult& local_merge_result,
                       const syncer::SyncMergeResult& syncer_merge_result) {
    last_start_result_ = result;
    last_start_error_ = local_merge_result.error();
  }

  void OnLoadFinished(syncer::ModelType type, syncer::SyncError error) {
    EXPECT_FALSE(error.IsSet());
    EXPECT_EQ(type, syncer::AUTOFILL);
  }

  void BlockForDBThread() {
    base::RunLoop run_loop;
    ASSERT_TRUE(db_thread_.task_runner()->PostTaskAndReply(
        FROM_HERE, base::Bind(&base::DoNothing), run_loop.QuitClosure()));
    run_loop.Run();
  }

 protected:
  base::MessageLoop message_loop_;
  base::Thread db_thread_;
  sync_driver::FakeSyncClient sync_client_;
  scoped_refptr<AutofillDataTypeController> autofill_dtc_;
  scoped_refptr<FakeWebDataService> web_data_service_;

  // Stores arguments of most recent call of OnStartFinished().
  sync_driver::DataTypeController::ConfigureResult last_start_result_;
  syncer::SyncError last_start_error_;
  base::WeakPtrFactory<SyncAutofillDataTypeControllerTest> weak_ptr_factory_;
};

// Load the WDS's database, then start the Autofill DTC.  It should
// immediately try to start association and fail (due to missing DB
// thread).
TEST_F(SyncAutofillDataTypeControllerTest, StartWDSReady) {
  web_data_service_->LoadDatabase();
  autofill_dtc_->LoadModels(
      base::Bind(&SyncAutofillDataTypeControllerTest::OnLoadFinished,
                 weak_ptr_factory_.GetWeakPtr()));

  autofill_dtc_->StartAssociating(
      base::Bind(&SyncAutofillDataTypeControllerTest::OnStartFinished,
                 weak_ptr_factory_.GetWeakPtr()));
  BlockForDBThread();

  EXPECT_EQ(sync_driver::DataTypeController::ASSOCIATION_FAILED,
            last_start_result_);
  EXPECT_TRUE(last_start_error_.IsSet());
  EXPECT_EQ(sync_driver::DataTypeController::DISABLED, autofill_dtc_->state());
}

// Start the autofill DTC without the WDS's database loaded, then
// start the DB.  The Autofill DTC should be in the MODEL_STARTING
// state until the database in loaded, when it should try to start
// association and fail (due to missing DB thread).
TEST_F(SyncAutofillDataTypeControllerTest, StartWDSNotReady) {
  autofill_dtc_->LoadModels(
      base::Bind(&SyncAutofillDataTypeControllerTest::OnLoadFinished,
                 weak_ptr_factory_.GetWeakPtr()));

  EXPECT_EQ(sync_driver::DataTypeController::OK, last_start_result_);
  EXPECT_FALSE(last_start_error_.IsSet());
  EXPECT_EQ(sync_driver::DataTypeController::MODEL_STARTING,
            autofill_dtc_->state());

  web_data_service_->LoadDatabase();

  autofill_dtc_->StartAssociating(
      base::Bind(&SyncAutofillDataTypeControllerTest::OnStartFinished,
                 weak_ptr_factory_.GetWeakPtr()));
  BlockForDBThread();

  EXPECT_EQ(sync_driver::DataTypeController::ASSOCIATION_FAILED,
            last_start_result_);
  EXPECT_TRUE(last_start_error_.IsSet());

  EXPECT_EQ(sync_driver::DataTypeController::DISABLED, autofill_dtc_->state());
}

}  // namespace

}  // namespace browser_sync
