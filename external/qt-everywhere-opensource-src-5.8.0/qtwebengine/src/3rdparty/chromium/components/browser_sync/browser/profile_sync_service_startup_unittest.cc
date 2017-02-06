// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/browser_sync/browser/profile_sync_test_util.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "components/sync_driver/data_type_manager.h"
#include "components/sync_driver/data_type_manager_mock.h"
#include "components/sync_driver/fake_data_type_controller.h"
#include "components/sync_driver/glue/sync_backend_host_mock.h"
#include "components/sync_driver/pref_names.h"
#include "components/sync_driver/sync_api_component_factory_mock.h"
#include "components/sync_driver/sync_prefs.h"
#include "components/sync_driver/sync_service_observer.h"
#include "components/syncable_prefs/pref_service_syncable.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using browser_sync::SyncBackendHostMock;
using sync_driver::DataTypeManager;
using sync_driver::DataTypeManagerMock;
using testing::_;
using testing::AnyNumber;
using testing::DoAll;
using testing::Mock;
using testing::Return;

namespace {

const char kGaiaId[] = "12345";
const char kEmail[] = "test_user@gmail.com";
const char kDummyPassword[] = "";

class SyncServiceObserverMock : public sync_driver::SyncServiceObserver {
 public:
  SyncServiceObserverMock();
  virtual ~SyncServiceObserverMock();

  MOCK_METHOD0(OnStateChanged, void());
};

SyncServiceObserverMock::SyncServiceObserverMock() {}

SyncServiceObserverMock::~SyncServiceObserverMock() {}

}  // namespace

ACTION_P(InvokeOnConfigureStart, sync_service) {
  sync_service->OnConfigureStart();
}

ACTION_P3(InvokeOnConfigureDone, sync_service, error_callback, result) {
  DataTypeManager::ConfigureResult configure_result =
      static_cast<DataTypeManager::ConfigureResult>(result);
  if (result.status == sync_driver::DataTypeManager::ABORTED)
    error_callback.Run(&configure_result);
  sync_service->OnConfigureDone(configure_result);
}

class ProfileSyncServiceStartupTest : public testing::Test {
 public:
  ProfileSyncServiceStartupTest() {
    profile_sync_service_bundle_.auth_service()
        ->set_auto_post_fetch_response_on_message_loop(true);
  }

  ~ProfileSyncServiceStartupTest() override {
    sync_service_->RemoveObserver(&observer_);
    sync_service_->Shutdown();
  }

  void CreateSyncService(
      ProfileSyncService::StartBehavior start_behavior) {
    component_factory_ = profile_sync_service_bundle_.component_factory();
    browser_sync::ProfileSyncServiceBundle::SyncClientBuilder builder(
        &profile_sync_service_bundle_);
    ProfileSyncService::InitParams init_params =
        profile_sync_service_bundle_.CreateBasicInitParams(start_behavior,
                                                           builder.Build());

    sync_service_.reset(new ProfileSyncService(std::move(init_params)));
    sync_service_->RegisterDataTypeController(
        new sync_driver::FakeDataTypeController(syncer::BOOKMARKS));
    sync_service_->AddObserver(&observer_);
  }

  void IssueTestTokens(const std::string& account_id) {
    profile_sync_service_bundle_.auth_service()->UpdateCredentials(
        account_id, "oauth2_login_token");
  }

  void SetError(DataTypeManager::ConfigureResult* result) {
    sync_driver::DataTypeStatusTable::TypeErrorMap errors;
    errors[syncer::BOOKMARKS] =
        syncer::SyncError(FROM_HERE,
                          syncer::SyncError::UNRECOVERABLE_ERROR,
                          "Error",
                          syncer::BOOKMARKS);
    result->data_type_status_table.UpdateFailedDataTypes(errors);
  }

 protected:
  std::string SimulateTestUserSignin(ProfileSyncService* sync_service) {
    std::string account_id =
        profile_sync_service_bundle_.account_tracker()->SeedAccountInfo(kGaiaId,
                                                                        kEmail);
    pref_service()->SetString(prefs::kGoogleServicesAccountId, account_id);
#if !defined(OS_CHROMEOS)
    profile_sync_service_bundle_.signin_manager()->SignIn(kGaiaId, kEmail,
                                                          kDummyPassword);
#else
    profile_sync_service_bundle_.signin_manager()->SetAuthenticatedAccountInfo(
        kGaiaId, kEmail);
    if (sync_service)
      sync_service->GoogleSigninSucceeded(account_id, kEmail, kDummyPassword);
#endif
    return account_id;
  }

  DataTypeManagerMock* SetUpDataTypeManager() {
    DataTypeManagerMock* data_type_manager = new DataTypeManagerMock();
    EXPECT_CALL(*component_factory_, CreateDataTypeManager(_, _, _, _, _))
        .WillOnce(Return(data_type_manager));
    return data_type_manager;
  }

  browser_sync::SyncBackendHostMock* SetUpSyncBackendHost() {
    browser_sync::SyncBackendHostMock* sync_backend_host =
        new browser_sync::SyncBackendHostMock();
    EXPECT_CALL(*component_factory_, CreateSyncBackendHost(_, _, _, _))
        .WillOnce(Return(sync_backend_host));
    return sync_backend_host;
  }

  PrefService* pref_service() {
    return profile_sync_service_bundle_.pref_service();
  }

  base::MessageLoop message_loop_;
  browser_sync::ProfileSyncServiceBundle profile_sync_service_bundle_;
  std::unique_ptr<ProfileSyncService> sync_service_;
  SyncServiceObserverMock observer_;
  sync_driver::DataTypeStatusTable data_type_status_table_;
  SyncApiComponentFactoryMock* component_factory_ = nullptr;
};

class ProfileSyncServiceStartupCrosTest : public ProfileSyncServiceStartupTest {
 public:
  ProfileSyncServiceStartupCrosTest() {
    CreateSyncService(ProfileSyncService::AUTO_START);
    SimulateTestUserSignin(nullptr);
    EXPECT_TRUE(
        profile_sync_service_bundle_.signin_manager()->IsAuthenticated());
  }
};

TEST_F(ProfileSyncServiceStartupTest, StartFirstTime) {
  // We've never completed startup.
  pref_service()->ClearPref(sync_driver::prefs::kSyncFirstSetupComplete);
  CreateSyncService(ProfileSyncService::MANUAL_START);
  SetUpSyncBackendHost();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  EXPECT_CALL(*data_type_manager, Configure(_, _)).Times(0);

  // Should not actually start, rather just clean things up and wait
  // to be enabled.
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  sync_service_->Initialize();

  // Preferences should be back to defaults.
  EXPECT_EQ(0,
            pref_service()->GetInt64(sync_driver::prefs::kSyncLastSyncedTime));
  EXPECT_FALSE(
      pref_service()->GetBoolean(sync_driver::prefs::kSyncFirstSetupComplete));
  Mock::VerifyAndClearExpectations(data_type_manager);

  // Then start things up.
  EXPECT_CALL(*data_type_manager, Configure(_, _)).Times(1);
  EXPECT_CALL(*data_type_manager, state()).
      WillOnce(Return(DataTypeManager::CONFIGURED)).
      WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*data_type_manager, Stop()).Times(1);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());

  auto sync_blocker = sync_service_->GetSetupInProgressHandle();

  // Simulate successful signin as test_user.
  std::string account_id = SimulateTestUserSignin(sync_service_.get());
  // Create some tokens in the token service.
  IssueTestTokens(account_id);

  // Simulate the UI telling sync it has finished setting up.
  sync_blocker.reset();
  sync_service_->SetFirstSetupComplete();
  EXPECT_TRUE(sync_service_->IsSyncActive());
}

// TODO(pavely): Reenable test once android is switched to oauth2.
TEST_F(ProfileSyncServiceStartupTest, DISABLED_StartNoCredentials) {
  // We've never completed startup.
  pref_service()->ClearPref(sync_driver::prefs::kSyncFirstSetupComplete);
  CreateSyncService(ProfileSyncService::MANUAL_START);

  // Should not actually start, rather just clean things up and wait
  // to be enabled.
  EXPECT_CALL(*component_factory_, CreateDataTypeManager(_, _, _, _, _))
      .Times(0);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  sync_service_->Initialize();

  // Preferences should be back to defaults.
  EXPECT_EQ(0,
            pref_service()->GetInt64(sync_driver::prefs::kSyncLastSyncedTime));
  EXPECT_FALSE(
      pref_service()->GetBoolean(sync_driver::prefs::kSyncFirstSetupComplete));

  // Then start things up.
  auto sync_blocker = sync_service_->GetSetupInProgressHandle();

  // Simulate successful signin as test_user.
  std::string account_id = SimulateTestUserSignin(sync_service_.get());

  profile_sync_service_bundle_.auth_service()->LoadCredentials(account_id);

  sync_blocker.reset();
  // ProfileSyncService should try to start by requesting access token.
  // This request should fail as login token was not issued.
  EXPECT_FALSE(sync_service_->IsSyncActive());
  EXPECT_EQ(GoogleServiceAuthError::USER_NOT_SIGNED_UP,
            sync_service_->GetAuthError().state());
}

// TODO(pavely): Reenable test once android is switched to oauth2.
TEST_F(ProfileSyncServiceStartupTest, DISABLED_StartInvalidCredentials) {
  CreateSyncService(ProfileSyncService::MANUAL_START);
  std::string account_id = SimulateTestUserSignin(sync_service_.get());
  SyncBackendHostMock* mock_sbh = SetUpSyncBackendHost();

  // Tell the backend to stall while downloading control types (simulating an
  // auth error).
  mock_sbh->set_fail_initial_download(true);

  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  EXPECT_CALL(*data_type_manager, Configure(_, _)).Times(0);

  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  sync_service_->Initialize();
  EXPECT_FALSE(sync_service_->IsSyncActive());
  Mock::VerifyAndClearExpectations(data_type_manager);

  // Update the credentials, unstalling the backend.
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  EXPECT_CALL(*data_type_manager, state()).
      WillRepeatedly(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*data_type_manager, Stop()).Times(1);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  auto sync_blocker = sync_service_->GetSetupInProgressHandle();

  // Simulate successful signin.
  SimulateTestUserSignin(sync_service_.get());

  sync_blocker.reset();

  // Verify we successfully finish startup and configuration.
  EXPECT_TRUE(sync_service_->IsSyncActive());
}

TEST_F(ProfileSyncServiceStartupCrosTest, StartCrosNoCredentials) {
  EXPECT_CALL(*component_factory_, CreateDataTypeManager(_, _, _, _, _))
      .Times(0);
  EXPECT_CALL(*component_factory_, CreateSyncBackendHost(_, _, _, _)).Times(0);
  pref_service()->ClearPref(sync_driver::prefs::kSyncFirstSetupComplete);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());

  sync_service_->Initialize();
  // Sync should not start because there are no tokens yet.
  EXPECT_FALSE(sync_service_->IsSyncActive());
  sync_service_->SetFirstSetupComplete();

  // Sync should not start because there are still no tokens.
  EXPECT_FALSE(sync_service_->IsSyncActive());
}

TEST_F(ProfileSyncServiceStartupCrosTest, StartFirstTime) {
  SetUpSyncBackendHost();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  pref_service()->ClearPref(sync_driver::prefs::kSyncFirstSetupComplete);
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  EXPECT_CALL(*data_type_manager, state()).
      WillRepeatedly(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*data_type_manager, Stop());
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());

  IssueTestTokens(
      profile_sync_service_bundle_.account_tracker()->PickAccountIdForAccount(
          "12345", kEmail));
  sync_service_->Initialize();
  EXPECT_TRUE(sync_service_->IsSyncActive());
}

TEST_F(ProfileSyncServiceStartupTest, StartNormal) {
  // Pre load the tokens
  CreateSyncService(ProfileSyncService::MANUAL_START);
  std::string account_id = SimulateTestUserSignin(sync_service_.get());
  sync_service_->SetFirstSetupComplete();
  SetUpSyncBackendHost();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  EXPECT_CALL(*data_type_manager, state()).
      WillRepeatedly(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*data_type_manager, Stop()).Times(1);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());

  IssueTestTokens(account_id);

  sync_service_->Initialize();
}

// Test that we can recover from a case where a bug in the code resulted in
// OnUserChoseDatatypes not being properly called and datatype preferences
// therefore being left unset.
TEST_F(ProfileSyncServiceStartupTest, StartRecoverDatatypePrefs) {
  // Clear the datatype preference fields (simulating bug 154940).
  pref_service()->ClearPref(sync_driver::prefs::kSyncKeepEverythingSynced);
  syncer::ModelTypeSet user_types = syncer::UserTypes();
  for (syncer::ModelTypeSet::Iterator iter = user_types.First();
       iter.Good(); iter.Inc()) {
    pref_service()->ClearPref(
        sync_driver::SyncPrefs::GetPrefNameForDataType(iter.Get()));
  }

  // Pre load the tokens
  CreateSyncService(ProfileSyncService::MANUAL_START);
  std::string account_id = SimulateTestUserSignin(sync_service_.get());
  sync_service_->SetFirstSetupComplete();
  SetUpSyncBackendHost();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  EXPECT_CALL(*data_type_manager, state()).
      WillRepeatedly(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*data_type_manager, Stop()).Times(1);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());

  IssueTestTokens(account_id);
  sync_service_->Initialize();

  EXPECT_TRUE(pref_service()->GetBoolean(
      sync_driver::prefs::kSyncKeepEverythingSynced));
}

// Verify that the recovery of datatype preferences doesn't overwrite a valid
// case where only bookmarks are enabled.
TEST_F(ProfileSyncServiceStartupTest, StartDontRecoverDatatypePrefs) {
  // Explicitly set Keep Everything Synced to false and have only bookmarks
  // enabled.
  pref_service()->SetBoolean(sync_driver::prefs::kSyncKeepEverythingSynced,
                             false);

  // Pre load the tokens
  CreateSyncService(ProfileSyncService::MANUAL_START);
  std::string account_id = SimulateTestUserSignin(sync_service_.get());
  sync_service_->SetFirstSetupComplete();
  SetUpSyncBackendHost();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  EXPECT_CALL(*data_type_manager, state()).
      WillRepeatedly(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*data_type_manager, Stop()).Times(1);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  IssueTestTokens(account_id);
  sync_service_->Initialize();

  EXPECT_FALSE(pref_service()->GetBoolean(
      sync_driver::prefs::kSyncKeepEverythingSynced));
}

TEST_F(ProfileSyncServiceStartupTest, ManagedStartup) {
  // Service should not be started by Initialize() since it's managed.
  pref_service()->SetString(prefs::kGoogleServicesAccountId, kEmail);
  CreateSyncService(ProfileSyncService::MANUAL_START);

  // Disable sync through policy.
  pref_service()->SetBoolean(sync_driver::prefs::kSyncManaged, true);
  EXPECT_CALL(*component_factory_, CreateDataTypeManager(_, _, _, _, _))
      .Times(0);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());

  sync_service_->Initialize();
}

TEST_F(ProfileSyncServiceStartupTest, SwitchManaged) {
  CreateSyncService(ProfileSyncService::MANUAL_START);
  std::string account_id = SimulateTestUserSignin(sync_service_.get());
  sync_service_->SetFirstSetupComplete();
  SetUpSyncBackendHost();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  EXPECT_CALL(*data_type_manager, state())
      .WillRepeatedly(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  IssueTestTokens(account_id);
  sync_service_->Initialize();
  EXPECT_TRUE(sync_service_->IsBackendInitialized());
  EXPECT_TRUE(sync_service_->IsSyncActive());

  // The service should stop when switching to managed mode.
  Mock::VerifyAndClearExpectations(data_type_manager);
  EXPECT_CALL(*data_type_manager, state()).
      WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*data_type_manager, Stop()).Times(1);
  pref_service()->SetBoolean(sync_driver::prefs::kSyncManaged, true);
  EXPECT_FALSE(sync_service_->IsBackendInitialized());
  // Note that PSS no longer references |data_type_manager| after stopping.

  // When switching back to unmanaged, the state should change but sync should
  // not start automatically because IsFirstSetupComplete() will be false.
  // A new DataTypeManager should not be created.
  Mock::VerifyAndClearExpectations(data_type_manager);
  EXPECT_CALL(*component_factory_, CreateDataTypeManager(_, _, _, _, _))
      .Times(0);
  pref_service()->ClearPref(sync_driver::prefs::kSyncManaged);
  EXPECT_FALSE(sync_service_->IsBackendInitialized());
  EXPECT_FALSE(sync_service_->IsSyncActive());
}

TEST_F(ProfileSyncServiceStartupTest, StartFailure) {
  CreateSyncService(ProfileSyncService::MANUAL_START);
  std::string account_id = SimulateTestUserSignin(sync_service_.get());
  sync_service_->SetFirstSetupComplete();
  SetUpSyncBackendHost();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  DataTypeManager::ConfigureStatus status = DataTypeManager::ABORTED;
  DataTypeManager::ConfigureResult result(
      status,
      syncer::ModelTypeSet());
  EXPECT_CALL(*data_type_manager, Configure(_, _))
      .WillRepeatedly(
          DoAll(InvokeOnConfigureStart(sync_service_.get()),
                InvokeOnConfigureDone(
                    sync_service_.get(),
                    base::Bind(&ProfileSyncServiceStartupTest::SetError,
                               base::Unretained(this)),
                    result)));
  EXPECT_CALL(*data_type_manager, state()).
      WillOnce(Return(DataTypeManager::STOPPED));
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  IssueTestTokens(account_id);
  sync_service_->Initialize();
  EXPECT_TRUE(sync_service_->HasUnrecoverableError());
}

TEST_F(ProfileSyncServiceStartupTest, StartDownloadFailed) {
  // Pre load the tokens
  CreateSyncService(ProfileSyncService::MANUAL_START);
  std::string account_id = SimulateTestUserSignin(sync_service_.get());
  SyncBackendHostMock* mock_sbh = SetUpSyncBackendHost();
  mock_sbh->set_fail_initial_download(true);

  pref_service()->ClearPref(sync_driver::prefs::kSyncFirstSetupComplete);

  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  sync_service_->Initialize();

  auto sync_blocker = sync_service_->GetSetupInProgressHandle();
  IssueTestTokens(account_id);
  sync_blocker.reset();
  EXPECT_FALSE(sync_service_->IsSyncActive());
}
