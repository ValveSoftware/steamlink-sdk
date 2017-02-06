// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engine_data_type_controller.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/tracked_objects.h"
#include "components/search_engines/template_url_service.h"
#include "components/sync_driver/data_type_controller_mock.h"
#include "components/sync_driver/fake_generic_change_processor.h"
#include "components/sync_driver/fake_sync_client.h"
#include "components/sync_driver/sync_api_component_factory_mock.h"
#include "sync/api/fake_syncable_service.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::DoAll;
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::SetArgumentPointee;

namespace browser_sync {
namespace {

class SyncSearchEngineDataTypeControllerTest
    : public testing::Test,
      public sync_driver::FakeSyncClient {
 public:
  SyncSearchEngineDataTypeControllerTest()
      : sync_driver::FakeSyncClient(&profile_sync_factory_),
        template_url_service_(new TemplateURLService(nullptr, 0)) {
    // Disallow the TemplateURLService from loading until
    // PreloadTemplateURLService() is called .
    template_url_service_->set_disable_load(true);
  }

  // FakeSyncClient overrides.
  base::WeakPtr<syncer::SyncableService> GetSyncableServiceForType(
      syncer::ModelType type) override {
    return syncable_service_.AsWeakPtr();
  }

  void SetUp() override {
    search_engine_dtc_ = new SearchEngineDataTypeController(
        base::ThreadTaskRunnerHandle::Get(),
        base::Bind(&base::DoNothing), this,
        template_url_service_.get());
  }

  void TearDown() override {
    // Must be done before we pump the loop.
    syncable_service_.StopSyncing(syncer::SEARCH_ENGINES);
    search_engine_dtc_ = NULL;
  }

 protected:
  void PreloadTemplateURLService() {
    template_url_service_->set_disable_load(false);
    template_url_service_->Load();
  }

  void SetStartExpectations() {
    search_engine_dtc_->SetGenericChangeProcessorFactoryForTest(
        base::WrapUnique<sync_driver::GenericChangeProcessorFactory>(
            new sync_driver::FakeGenericChangeProcessorFactory(
                base::WrapUnique(new sync_driver::FakeGenericChangeProcessor(
                    syncer::SEARCH_ENGINES, this)))));
    EXPECT_CALL(model_load_callback_, Run(_, _));
  }

  void Start() {
    search_engine_dtc_->LoadModels(
        base::Bind(&sync_driver::ModelLoadCallbackMock::Run,
                   base::Unretained(&model_load_callback_)));
    search_engine_dtc_->StartAssociating(
        base::Bind(&sync_driver::StartCallbackMock::Run,
                   base::Unretained(&start_callback_)));
    base::RunLoop().RunUntilIdle();
  }

  base::MessageLoop message_loop_;
  std::unique_ptr<TemplateURLService> template_url_service_;
  scoped_refptr<SearchEngineDataTypeController> search_engine_dtc_;
  SyncApiComponentFactoryMock profile_sync_factory_;
  syncer::FakeSyncableService syncable_service_;
  sync_driver::StartCallbackMock start_callback_;
  sync_driver::ModelLoadCallbackMock model_load_callback_;
};

TEST_F(SyncSearchEngineDataTypeControllerTest, StartURLServiceReady) {
  SetStartExpectations();
  // We want to start ready.
  PreloadTemplateURLService();
  EXPECT_CALL(start_callback_, Run(sync_driver::DataTypeController::OK, _, _));

  EXPECT_EQ(sync_driver::DataTypeController::NOT_RUNNING,
            search_engine_dtc_->state());
  EXPECT_FALSE(syncable_service_.syncing());
  Start();
  EXPECT_EQ(sync_driver::DataTypeController::RUNNING,
            search_engine_dtc_->state());
  EXPECT_TRUE(syncable_service_.syncing());
}

TEST_F(SyncSearchEngineDataTypeControllerTest, StartURLServiceNotReady) {
  EXPECT_CALL(model_load_callback_, Run(_, _));
  EXPECT_FALSE(syncable_service_.syncing());
  search_engine_dtc_->LoadModels(
      base::Bind(&sync_driver::ModelLoadCallbackMock::Run,
                 base::Unretained(&model_load_callback_)));
  EXPECT_TRUE(search_engine_dtc_->GetSubscriptionForTesting());
  EXPECT_EQ(sync_driver::DataTypeController::MODEL_STARTING,
            search_engine_dtc_->state());
  EXPECT_FALSE(syncable_service_.syncing());

  // Send the notification that the TemplateURLService has started.
  PreloadTemplateURLService();
  EXPECT_EQ(NULL, search_engine_dtc_->GetSubscriptionForTesting());
  EXPECT_EQ(sync_driver::DataTypeController::MODEL_LOADED,
            search_engine_dtc_->state());

  // Wait until WebDB is loaded before we shut it down.
  base::RunLoop().RunUntilIdle();
}

TEST_F(SyncSearchEngineDataTypeControllerTest, StartAssociationFailed) {
  SetStartExpectations();
  PreloadTemplateURLService();
  EXPECT_CALL(start_callback_,
              Run(sync_driver::DataTypeController::ASSOCIATION_FAILED, _, _));
  syncable_service_.set_merge_data_and_start_syncing_error(
      syncer::SyncError(FROM_HERE,
                        syncer::SyncError::DATATYPE_ERROR,
                        "Error",
                        syncer::SEARCH_ENGINES));

  Start();
  EXPECT_EQ(sync_driver::DataTypeController::DISABLED,
            search_engine_dtc_->state());
  EXPECT_FALSE(syncable_service_.syncing());
  search_engine_dtc_->Stop();
  EXPECT_EQ(sync_driver::DataTypeController::NOT_RUNNING,
            search_engine_dtc_->state());
  EXPECT_FALSE(syncable_service_.syncing());
}

TEST_F(SyncSearchEngineDataTypeControllerTest, Stop) {
  SetStartExpectations();
  PreloadTemplateURLService();
  EXPECT_CALL(start_callback_, Run(sync_driver::DataTypeController::OK, _, _));

  EXPECT_EQ(sync_driver::DataTypeController::NOT_RUNNING,
            search_engine_dtc_->state());
  EXPECT_FALSE(syncable_service_.syncing());
  Start();
  EXPECT_EQ(sync_driver::DataTypeController::RUNNING,
            search_engine_dtc_->state());
  EXPECT_TRUE(syncable_service_.syncing());
  search_engine_dtc_->Stop();
  EXPECT_EQ(sync_driver::DataTypeController::NOT_RUNNING,
            search_engine_dtc_->state());
  EXPECT_FALSE(syncable_service_.syncing());
}

TEST_F(SyncSearchEngineDataTypeControllerTest, StopBeforeLoaded) {
  EXPECT_FALSE(syncable_service_.syncing());
  search_engine_dtc_->LoadModels(
      base::Bind(&sync_driver::ModelLoadCallbackMock::Run,
                 base::Unretained(&model_load_callback_)));
  EXPECT_TRUE(search_engine_dtc_->GetSubscriptionForTesting());
  EXPECT_EQ(sync_driver::DataTypeController::MODEL_STARTING,
            search_engine_dtc_->state());
  EXPECT_FALSE(syncable_service_.syncing());
  search_engine_dtc_->Stop();
  EXPECT_EQ(NULL, search_engine_dtc_->GetSubscriptionForTesting());
  EXPECT_EQ(sync_driver::DataTypeController::NOT_RUNNING,
            search_engine_dtc_->state());
  EXPECT_FALSE(syncable_service_.syncing());
}

}  // namespace
}  // namespace browser_sync
