// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/browser/abstract_profile_sync_service_test.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "components/browser_sync/browser/test_http_bridge_factory.h"
#include "components/browser_sync/browser/test_profile_sync_service.h"
#include "components/sync_driver/glue/sync_backend_host_core.h"
#include "components/sync_driver/sync_api_component_factory_mock.h"
#include "google_apis/gaia/gaia_constants.h"
#include "sync/internal_api/public/test/sync_manager_factory_for_profile_sync_test.h"
#include "sync/internal_api/public/test/test_internal_components_factory.h"
#include "sync/internal_api/public/test/test_user_share.h"
#include "sync/protocol/sync.pb.h"

using syncer::ModelType;
using testing::_;
using testing::Return;

namespace {

class SyncBackendHostForProfileSyncTest
    : public browser_sync::SyncBackendHostImpl {
 public:
  SyncBackendHostForProfileSyncTest(
      const base::FilePath& temp_dir,
      sync_driver::SyncClient* sync_client,
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
      invalidation::InvalidationService* invalidator,
      const base::WeakPtr<sync_driver::SyncPrefs>& sync_prefs,
      const base::Closure& callback);
  ~SyncBackendHostForProfileSyncTest() override;

  void RequestConfigureSyncer(
      syncer::ConfigureReason reason,
      syncer::ModelTypeSet to_download,
      syncer::ModelTypeSet to_purge,
      syncer::ModelTypeSet to_journal,
      syncer::ModelTypeSet to_unapply,
      syncer::ModelTypeSet to_ignore,
      const syncer::ModelSafeRoutingInfo& routing_info,
      const base::Callback<void(syncer::ModelTypeSet, syncer::ModelTypeSet)>&
          ready_task,
      const base::Closure& retry_callback) override;

 protected:
  void InitCore(
      std::unique_ptr<browser_sync::DoInitializeOptions> options) override;

 private:
  // Invoked at the start of HandleSyncManagerInitializationOnFrontendLoop.
  // Allows extra initialization work to be performed before the backend comes
  // up.
  base::Closure callback_;

  DISALLOW_COPY_AND_ASSIGN(SyncBackendHostForProfileSyncTest);
};

SyncBackendHostForProfileSyncTest::SyncBackendHostForProfileSyncTest(
    const base::FilePath& temp_dir,
    sync_driver::SyncClient* sync_client,
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
    invalidation::InvalidationService* invalidator,
    const base::WeakPtr<sync_driver::SyncPrefs>& sync_prefs,
    const base::Closure& callback)
    : browser_sync::SyncBackendHostImpl(
          "dummy_debug_name",
          sync_client,
          ui_thread,
          invalidator,
          sync_prefs,
          temp_dir.Append(base::FilePath(FILE_PATH_LITERAL("test")))),
      callback_(callback) {}

SyncBackendHostForProfileSyncTest::~SyncBackendHostForProfileSyncTest() {}

void SyncBackendHostForProfileSyncTest::InitCore(
    std::unique_ptr<browser_sync::DoInitializeOptions> options) {
  options->http_bridge_factory =
      std::unique_ptr<syncer::HttpPostProviderFactory>(
          new browser_sync::TestHttpBridgeFactory());
  options->sync_manager_factory.reset(
      new syncer::SyncManagerFactoryForProfileSyncTest(callback_));
  options->credentials.email = "testuser@gmail.com";
  options->credentials.sync_token = "token";
  options->credentials.scope_set.insert(GaiaConstants::kChromeSyncOAuth2Scope);
  options->restored_key_for_bootstrapping.clear();

  // It'd be nice if we avoided creating the InternalComponentsFactory in the
  // first place, but SyncBackendHost will have created one by now so we must
  // free it. Grab the switches to pass on first.
  syncer::InternalComponentsFactory::Switches factory_switches =
      options->internal_components_factory->GetSwitches();
  options->internal_components_factory.reset(
      new syncer::TestInternalComponentsFactory(
          factory_switches,
          syncer::InternalComponentsFactory::STORAGE_IN_MEMORY, nullptr));

  browser_sync::SyncBackendHostImpl::InitCore(std::move(options));
}

void SyncBackendHostForProfileSyncTest::RequestConfigureSyncer(
    syncer::ConfigureReason reason,
    syncer::ModelTypeSet to_download,
    syncer::ModelTypeSet to_purge,
    syncer::ModelTypeSet to_journal,
    syncer::ModelTypeSet to_unapply,
    syncer::ModelTypeSet to_ignore,
    const syncer::ModelSafeRoutingInfo& routing_info,
    const base::Callback<void(syncer::ModelTypeSet, syncer::ModelTypeSet)>&
        ready_task,
    const base::Closure& retry_callback) {
  syncer::ModelTypeSet failed_configuration_types;

  // The first parameter there should be the set of enabled types.  That's not
  // something we have access to from this strange test harness.  We'll just
  // send back the list of newly configured types instead and hope it doesn't
  // break anything.
  // Posted to avoid re-entrancy issues.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&SyncBackendHostForProfileSyncTest::
                     FinishConfigureDataTypesOnFrontendLoop,
                 base::Unretained(this),
                 syncer::Difference(to_download, failed_configuration_types),
                 syncer::Difference(to_download, failed_configuration_types),
                 failed_configuration_types, ready_task));
}

// Helper function for return-type-upcasting of the callback.
sync_driver::SyncService* GetSyncService(
    base::Callback<TestProfileSyncService*(void)> get_sync_service_callback) {
  return get_sync_service_callback.Run();
}

}  // namespace

/* static */
syncer::ImmutableChangeRecordList
ProfileSyncServiceTestHelper::MakeSingletonChangeRecordList(
    int64_t node_id,
    syncer::ChangeRecord::Action action) {
  syncer::ChangeRecord record;
  record.action = action;
  record.id = node_id;
  syncer::ChangeRecordList records(1, record);
  return syncer::ImmutableChangeRecordList(&records);
}

/* static */
syncer::ImmutableChangeRecordList
ProfileSyncServiceTestHelper::MakeSingletonDeletionChangeRecordList(
    int64_t node_id,
    const sync_pb::EntitySpecifics& specifics) {
  syncer::ChangeRecord record;
  record.action = syncer::ChangeRecord::ACTION_DELETE;
  record.id = node_id;
  record.specifics = specifics;
  syncer::ChangeRecordList records(1, record);
  return syncer::ImmutableChangeRecordList(&records);
}

AbstractProfileSyncServiceTest::AbstractProfileSyncServiceTest()
    : data_type_thread_("Extra thread") {
  CHECK(temp_dir_.CreateUniqueTempDir());
}

AbstractProfileSyncServiceTest::~AbstractProfileSyncServiceTest() {
  sync_service_->Shutdown();
}

bool AbstractProfileSyncServiceTest::CreateRoot(ModelType model_type) {
  return syncer::TestUserShare::CreateRoot(model_type,
                                           sync_service_->GetUserShare());
}

void AbstractProfileSyncServiceTest::CreateSyncService(
    std::unique_ptr<sync_driver::SyncClient> sync_client,
    const base::Closure& initialization_success_callback) {
  DCHECK(sync_client);
  ProfileSyncService::InitParams init_params =
      profile_sync_service_bundle_.CreateBasicInitParams(
          ProfileSyncService::AUTO_START, std::move(sync_client));
  sync_service_ =
      base::WrapUnique(new TestProfileSyncService(std::move(init_params)));

  SyncApiComponentFactoryMock* components =
      profile_sync_service_bundle_.component_factory();
  EXPECT_CALL(*components, CreateSyncBackendHost(_, _, _, _))
      .WillOnce(Return(new SyncBackendHostForProfileSyncTest(
          temp_dir_.path(), sync_service_->GetSyncClient(),
          base::ThreadTaskRunnerHandle::Get(),
          profile_sync_service_bundle_.fake_invalidation_service(),
          sync_service_->sync_prefs()->AsWeakPtr(),
          initialization_success_callback)));

  sync_service_->SetFirstSetupComplete();
}

base::Callback<sync_driver::SyncService*(void)>
AbstractProfileSyncServiceTest::GetSyncServiceCallback() {
  return base::Bind(GetSyncService,
                    base::Bind(&AbstractProfileSyncServiceTest::sync_service,
                               base::Unretained(this)));
}

CreateRootHelper::CreateRootHelper(AbstractProfileSyncServiceTest* test,
                                   ModelType model_type)
    : callback_(base::Bind(&CreateRootHelper::CreateRootCallback,
                           base::Unretained(this))),
      test_(test),
      model_type_(model_type),
      success_(false) {
}

CreateRootHelper::~CreateRootHelper() {
}

const base::Closure& CreateRootHelper::callback() const {
  return callback_;
}

bool CreateRootHelper::success() {
  return success_;
}

void CreateRootHelper::CreateRootCallback() {
  success_ = test_->CreateRoot(model_type_);
}
