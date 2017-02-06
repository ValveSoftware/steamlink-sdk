// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/test/histogram_tester.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/password_manager/sync/browser/password_manager_setting_migrator_service.h"
#include "components/pref_registry/testing_pref_service_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/sync_driver/fake_sync_service.h"
#include "components/syncable_prefs/pref_model_associator_client.h"
#include "components/syncable_prefs/pref_service_mock_factory.h"
#include "components/syncable_prefs/pref_service_syncable.h"
#include "sync/api/fake_sync_change_processor.h"
#include "sync/api/sync_error_factory.h"
#include "sync/api/sync_error_factory_mock.h"
#include "sync/internal_api/public/attachments/attachment_service_proxy_for_test.h"
#include "sync/protocol/sync.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kFieldTrialName[] = "PasswordManagerSettingsMigration";
const char kEnabledGroupName[] = "Enable";
const char kDisabledGroupName[] = "Disable";

const char kInitialValuesHistogramName[] =
    "PasswordManager.SettingsReconciliation.InitialValues";

const char kInitialAndFinalValuesHistogramName[] =
    "PasswordManager.SettingsReconciliation.InitialAndFinalValues";

enum BooleanPrefState {
  OFF,
  ON,
  EMPTY,  // datatype bucket is empty
};

// Enum used for histogram tracking of the initial values for the legacy and new
// preferences.
enum PasswordManagerPreferencesInitialValues {
  N0L0,
  N0L1,
  N1L0,
  N1L1,
  NUM_INITIAL_VALUES,
};

// Enum used for histogram tracking of the combined initial values and final
// values for the legacy and new preferences.
enum PasswordManagerPreferencesInitialAndFinalValues {
  I00F00,
  I00F01,
  I00F10,
  I00F11,
  I01F00,
  I01F01,
  I01F10,
  I01F11,
  I10F00,
  I10F01,
  I10F10,
  I10F11,
  I11F00,
  I11F01,
  I11F10,
  I11F11,
  NUM_INITIAL_AND_FINAL_VALUES,
};

syncer::SyncData CreatePrefSyncData(const std::string& name, bool value) {
  base::FundamentalValue bool_value(value);
  std::string serialized;
  base::JSONWriter::Write(bool_value, &serialized);
  sync_pb::EntitySpecifics specifics;
  sync_pb::PreferenceSpecifics* pref = nullptr;
  if (name == password_manager::prefs::kPasswordManagerSavingEnabled)
    pref = specifics.mutable_preference();
  else if (name == password_manager::prefs::kCredentialsEnableService)
    pref = specifics.mutable_priority_preference()->mutable_preference();
  else
    NOTREACHED() << "Wrong preference name: " << name;
  pref->set_name(name);
  pref->set_value(serialized);
  return syncer::SyncData::CreateRemoteData(
      1, specifics, base::Time(), syncer::AttachmentIdList(),
      syncer::AttachmentServiceProxyForTest::Create());
}

// Emulates start of the syncing for the specific sync type. If |name| is
// kPasswordManagerSavingEnabled preference, then it's PREFERENCE data type.
// If |name| is kCredentialsEnableService  pref, then it's PRIORITY_PREFERENCE
// data type.
void StartSyncingPref(syncable_prefs::PrefServiceSyncable* prefs,
                      const std::string& name,
                      BooleanPrefState pref_state_in_sync) {
  syncer::SyncDataList sync_data_list;
  if (pref_state_in_sync == EMPTY) {
    sync_data_list = syncer::SyncDataList();
  } else {
    sync_data_list.push_back(
        CreatePrefSyncData(name, pref_state_in_sync == ON));
  }

  syncer::ModelType type = syncer::UNSPECIFIED;
  if (name == password_manager::prefs::kPasswordManagerSavingEnabled)
    type = syncer::PREFERENCES;
  else if (name == password_manager::prefs::kCredentialsEnableService)
    type = syncer::PRIORITY_PREFERENCES;
  ASSERT_NE(syncer::UNSPECIFIED, type) << "Wrong preference name: " << name;
  syncer::SyncableService* sync = prefs->GetSyncableService(type);
  sync->MergeDataAndStartSyncing(type, sync_data_list,
                                 std::unique_ptr<syncer::SyncChangeProcessor>(
                                     new syncer::FakeSyncChangeProcessor),
                                 std::unique_ptr<syncer::SyncErrorFactory>(
                                     new syncer::SyncErrorFactoryMock));
}

class SyncServiceMock : public sync_driver::FakeSyncService {
 public:
  bool IsFirstSetupComplete() const override { return true; }

  bool CanSyncStart() const override { return can_sync_start_; }

  void SetCanSyncStart(bool can_sync_start) {
    can_sync_start_ = can_sync_start;
  }

 private:
  bool can_sync_start_ = true;
};

class TestPrefModelAssociatorClient
    : public syncable_prefs::PrefModelAssociatorClient {
 public:
  TestPrefModelAssociatorClient() {}
  ~TestPrefModelAssociatorClient() override {}

  // PrefModelAssociatorClient implementation.
  bool IsMergeableListPreference(const std::string& pref_name) const override {
    return false;
  }

  bool IsMergeableDictionaryPreference(
      const std::string& pref_name) const override {
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestPrefModelAssociatorClient);
};

}  // namespace

namespace password_manager {

class PasswordManagerSettingMigratorServiceTest : public testing::Test {
 protected:
  PasswordManagerSettingMigratorServiceTest() {}
  ~PasswordManagerSettingMigratorServiceTest() override {}

  void SetUp() override {
    SetupPreferenceMigrationEnvironment();
    EnforcePasswordManagerSettingMigrationExperiment(kEnabledGroupName);
  }

  void SetupLocalPrefState(const std::string& name, BooleanPrefState state) {
    if (state == ON)
      prefs()->SetBoolean(name, true);
    else if (state == OFF)
      prefs()->SetBoolean(name, false);
    else if (state == EMPTY)
      ASSERT_TRUE(prefs()->FindPreference(name)->IsDefaultValue());
  }

  syncable_prefs::PrefServiceSyncable* prefs() { return pref_service_.get(); }

  void SetupPreferenceMigrationEnvironment() {
    syncable_prefs::PrefServiceMockFactory factory;
    factory.SetPrefModelAssociatorClient(&client_);
    scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry(
        new user_prefs::PrefRegistrySyncable);
    password_manager::PasswordManager::RegisterProfilePrefs(
        pref_registry.get());
    std::unique_ptr<syncable_prefs::PrefServiceSyncable> pref_service_syncable =
        factory.CreateSyncable(pref_registry.get());
    migration_service_.reset(
        new PasswordManagerSettingMigratorService(pref_service_syncable.get()));
    pref_service_.reset(pref_service_syncable.release());
  }

  void ExpectValuesForBothPrefValues(bool new_pref_value, bool old_pref_value) {
    EXPECT_EQ(new_pref_value,
              prefs()->GetBoolean(prefs::kCredentialsEnableService));
    EXPECT_EQ(old_pref_value,
              prefs()->GetBoolean(prefs::kPasswordManagerSavingEnabled));
  }

  SyncServiceMock* profile_sync_service() { return &sync_service_; }

  void NotifyProfileAdded() {
    migration_service_->InitializeMigration(&sync_service_);
  }

  void EnforcePasswordManagerSettingMigrationExperiment(const char* name) {
    // The existing instance of FieldTrialList should be deleted before creating
    // new one, so reset() is called in order to do so.
    field_trial_list_.reset();
    field_trial_list_.reset(new base::FieldTrialList(nullptr));
    base::FieldTrialList::CreateFieldTrial(kFieldTrialName, name);
  }

 private:
  std::unique_ptr<base::FieldTrialList> field_trial_list_;
  TestPrefModelAssociatorClient client_;
  SyncServiceMock sync_service_;
  std::unique_ptr<syncable_prefs::PrefServiceSyncable> pref_service_;
  std::unique_ptr<PasswordManagerSettingMigratorService> migration_service_;

  DISALLOW_COPY_AND_ASSIGN(PasswordManagerSettingMigratorServiceTest);
};

TEST_F(PasswordManagerSettingMigratorServiceTest, TestMigrationOnLocalChanges) {
  const struct {
    const char* group;
    const char* pref_name;
    bool pref_value;
    bool expected_new_pref_value;
    bool expected_old_pref_value;
  } kTestingTable[] = {
      {kEnabledGroupName, prefs::kPasswordManagerSavingEnabled, true, true,
       true},
      {kEnabledGroupName, prefs::kPasswordManagerSavingEnabled, false, false,
       false},
      {kEnabledGroupName, prefs::kCredentialsEnableService, true, true, true},
      {kEnabledGroupName, prefs::kCredentialsEnableService, false, false,
       false},
      {kDisabledGroupName, prefs::kPasswordManagerSavingEnabled, false, true,
       false},
      {kDisabledGroupName, prefs::kCredentialsEnableService, false, false,
       true}};

  for (const auto& test_case : kTestingTable) {
    SetupPreferenceMigrationEnvironment();
    EnforcePasswordManagerSettingMigrationExperiment(test_case.group);
    prefs()->SetBoolean(prefs::kCredentialsEnableService,
                        !test_case.pref_value);
    prefs()->SetBoolean(prefs::kPasswordManagerSavingEnabled,
                        !test_case.pref_value);
    NotifyProfileAdded();
    base::HistogramTester tester;
    prefs()->SetBoolean(test_case.pref_name, test_case.pref_value);
    ExpectValuesForBothPrefValues(test_case.expected_new_pref_value,
                                  test_case.expected_old_pref_value);
    EXPECT_THAT(tester.GetAllSamples(kInitialValuesHistogramName),
                testing::IsEmpty());
  }
}

TEST_F(PasswordManagerSettingMigratorServiceTest,
       ReconcileWhenWhenBothPrefsTypesArrivesFromSync) {
  const struct {
    BooleanPrefState new_pref_local_value;
    BooleanPrefState old_pref_local_value;
    BooleanPrefState new_pref_sync_value;
    BooleanPrefState old_pref_sync_value;
    bool result_value;
    PasswordManagerPreferencesInitialValues histogram_initial_value;
    PasswordManagerPreferencesInitialAndFinalValues histogram_initial_and_final;
  } kTestingTable[] = {
#if defined(OS_ANDROID) || defined(OS_IOS)
    {ON, OFF, ON, EMPTY, false, N1L0, I10F00},
    {ON, OFF, OFF, EMPTY, false, N1L0, I10F00},
    {ON, OFF, EMPTY, EMPTY, false, N1L0, I10F00},
    {ON, ON, ON, EMPTY, true, N1L1, I11F11},
    {ON, ON, OFF, EMPTY, false, N1L1, I11F00},
    {OFF, OFF, ON, EMPTY, true, N0L0, I00F11},
    {OFF, OFF, OFF, EMPTY, false, N0L0, I00F00},
    {OFF, ON, ON, EMPTY, true, N0L1, I01F11},
    {OFF, ON, OFF, EMPTY, false, N0L1, I01F00},
    {OFF, ON, EMPTY, EMPTY, false, N0L1, I01F00},
#else
    {EMPTY, EMPTY, EMPTY, EMPTY, true, N1L1, I11F11},
    {EMPTY, EMPTY, EMPTY, OFF, false, N1L1, I11F00},
    {EMPTY, EMPTY, EMPTY, ON, true, N1L1, I11F11},
    {EMPTY, EMPTY, OFF, EMPTY, false, N1L1, I11F00},
    {EMPTY, EMPTY, ON, EMPTY, true, N1L1, I11F11},
    {OFF, OFF, EMPTY, EMPTY, false, N0L0, I00F00},
    {OFF, OFF, OFF, OFF, false, N0L0, I00F00},
    {OFF, OFF, OFF, ON, true, N0L0, I00F11},
    {OFF, OFF, ON, OFF, true, N0L0, I00F11},
    {OFF, ON, OFF, ON, false, N0L1, I01F00},
    {OFF, ON, ON, OFF, false, N0L1, I01F00},
    {OFF, ON, ON, ON, true, N0L1, I01F11},
    {ON, OFF, EMPTY, EMPTY, false, N1L0, I10F00},
    {ON, OFF, OFF, ON, false, N1L0, I10F00},
    {ON, OFF, ON, OFF, false, N1L0, I10F00},
    {ON, OFF, ON, ON, true, N1L0, I10F11},
    {ON, ON, EMPTY, OFF, false, N1L1, I11F00},
    {ON, ON, EMPTY, ON, true, N1L1, I11F11},
    {ON, ON, OFF, EMPTY, false, N1L1, I11F00},
    {ON, ON, OFF, OFF, false, N1L1, I11F00},
    {ON, ON, OFF, ON, false, N1L1, I11F00},
    {ON, ON, ON, EMPTY, true, N1L1, I11F11},
    {ON, ON, ON, OFF, false, N1L1, I11F00},
    {ON, ON, ON, ON, true, N1L1, I11F11},
#endif
  };

  for (const auto& test_case : kTestingTable) {
    SetupPreferenceMigrationEnvironment();
    EnforcePasswordManagerSettingMigrationExperiment(kEnabledGroupName);
    SCOPED_TRACE(testing::Message("Local data = ")
                 << test_case.new_pref_local_value << " "
                 << test_case.old_pref_local_value);
    SCOPED_TRACE(testing::Message("Sync data = ")
                 << test_case.new_pref_sync_value << " "
                 << test_case.old_pref_sync_value);
    SetupLocalPrefState(prefs::kPasswordManagerSavingEnabled,
                        test_case.old_pref_local_value);
    SetupLocalPrefState(prefs::kCredentialsEnableService,
                        test_case.new_pref_local_value);
    base::HistogramTester tester;
    NotifyProfileAdded();
    StartSyncingPref(prefs(), prefs::kCredentialsEnableService,
                     test_case.new_pref_sync_value);
#if !defined(OS_ANDROID) && !defined(OS_IOS)
    StartSyncingPref(prefs(), prefs::kPasswordManagerSavingEnabled,
                     test_case.old_pref_sync_value);
#endif
    ExpectValuesForBothPrefValues(test_case.result_value,
                                  test_case.result_value);
    EXPECT_THAT(tester.GetAllSamples(kInitialValuesHistogramName),
                testing::ElementsAre(
                    base::Bucket(test_case.histogram_initial_value, 1)));
    EXPECT_THAT(tester.GetAllSamples(kInitialAndFinalValuesHistogramName),
                testing::ElementsAre(
                    base::Bucket(test_case.histogram_initial_and_final, 1)));
  }
}

TEST_F(PasswordManagerSettingMigratorServiceTest,
       DoNotReconcileWhenWhenBothPrefsTypesArrivesFromSync) {
  const struct {
    BooleanPrefState new_pref_local_value;
    BooleanPrefState old_pref_local_value;
    BooleanPrefState new_pref_sync_value;
    BooleanPrefState old_pref_sync_value;
    bool result_new_pref_value;
    bool result_old_pref_value;
    PasswordManagerPreferencesInitialValues histogram_initial_value;
  } kTestingTable[] = {
#if defined(OS_ANDROID) || defined(OS_IOS)
    {ON, OFF, ON, EMPTY, true, false, N1L0},
    {ON, OFF, OFF, EMPTY, false, false, N1L0},
    {ON, OFF, EMPTY, EMPTY, true, false, N1L0},
    {ON, ON, ON, EMPTY, true, true, N1L1},
    {ON, ON, OFF, EMPTY, false, true, N1L1},
    {OFF, OFF, ON, EMPTY, true, false, N0L0},
    {OFF, OFF, OFF, EMPTY, false, false, N0L0},
    {OFF, ON, ON, EMPTY, true, true, N0L1},
    {OFF, ON, OFF, EMPTY, false, true, N0L1},
    {OFF, ON, EMPTY, EMPTY, false, true, N0L1},
#else
    {OFF, OFF, OFF, ON, false, true, N0L0},
    {OFF, OFF, ON, OFF, true, false, N0L0},
    {OFF, OFF, ON, ON, true, true, N0L0},
    {OFF, ON, EMPTY, OFF, false, false, N0L1},
    {OFF, ON, EMPTY, ON, false, true, N0L1},
    {OFF, ON, OFF, EMPTY, false, true, N0L1},
    {OFF, ON, OFF, OFF, false, false, N0L1},
    {OFF, ON, OFF, ON, false, true, N0L1},
    {OFF, ON, ON, EMPTY, true, true, N0L1},
    {OFF, ON, ON, OFF, true, false, N0L1},
    {OFF, ON, ON, ON, true, true, N0L1},
    {ON, OFF, OFF, ON, false, true, N1L0},
    {ON, OFF, ON, OFF, true, false, N1L0},
    {ON, OFF, ON, ON, true, true, N1L0},
    {ON, ON, EMPTY, OFF, true, false, N1L1},
    {ON, ON, EMPTY, ON, true, true, N1L1},
    {ON, ON, OFF, EMPTY, false, true, N1L1},
    {ON, ON, OFF, OFF, false, false, N1L1},
    {ON, ON, OFF, ON, false, true, N1L1},
    {ON, ON, ON, EMPTY, true, true, N1L1},
    {ON, ON, ON, OFF, true, false, N1L1},
    {ON, ON, ON, ON, true, true, N1L1},
#endif
  };

  for (const auto& test_case : kTestingTable) {
    SetupPreferenceMigrationEnvironment();
    EnforcePasswordManagerSettingMigrationExperiment(kDisabledGroupName);
    SCOPED_TRACE(testing::Message("Local data = ")
                 << test_case.new_pref_local_value << " "
                 << test_case.old_pref_local_value);
    SCOPED_TRACE(testing::Message("Sync data = ")
                 << test_case.new_pref_sync_value << " "
                 << test_case.old_pref_sync_value);
    SetupLocalPrefState(prefs::kPasswordManagerSavingEnabled,
                        test_case.old_pref_local_value);
    SetupLocalPrefState(prefs::kCredentialsEnableService,
                        test_case.new_pref_local_value);
    base::HistogramTester tester;
    NotifyProfileAdded();
    StartSyncingPref(prefs(), prefs::kCredentialsEnableService,
                     test_case.new_pref_sync_value);
#if !defined(OS_ANDROID) && !defined(OS_IOS)
    StartSyncingPref(prefs(), prefs::kPasswordManagerSavingEnabled,
                     test_case.old_pref_sync_value);
#endif
    ExpectValuesForBothPrefValues(test_case.result_new_pref_value,
                                  test_case.result_old_pref_value);
    EXPECT_THAT(tester.GetAllSamples(kInitialValuesHistogramName),
                testing::ElementsAre(
                    base::Bucket(test_case.histogram_initial_value, 1)));
    EXPECT_THAT(tester.GetAllSamples(kInitialAndFinalValuesHistogramName),
                testing::IsEmpty());
  }
}

TEST_F(PasswordManagerSettingMigratorServiceTest,
       ReconcileWhenSyncIsNotExpectedPasswordManagerEnabledOff) {
  prefs()->SetBoolean(prefs::kPasswordManagerSavingEnabled, false);
  profile_sync_service()->SetCanSyncStart(false);
  base::HistogramTester tester;
  NotifyProfileAdded();
  ExpectValuesForBothPrefValues(false, false);
  EXPECT_THAT(tester.GetAllSamples(kInitialAndFinalValuesHistogramName),
              testing::ElementsAre(base::Bucket(I10F00, 1)));
}

TEST_F(PasswordManagerSettingMigratorServiceTest,
       ReconcileWhenSyncIsNotExpectedPasswordManagerEnabledOn) {
  prefs()->SetBoolean(prefs::kPasswordManagerSavingEnabled, true);
  ASSERT_EQ(prefs()->GetBoolean(prefs::kCredentialsEnableService), true);
  profile_sync_service()->SetCanSyncStart(false);
  base::HistogramTester tester;
  NotifyProfileAdded();
  ExpectValuesForBothPrefValues(true, true);
  EXPECT_THAT(tester.GetAllSamples(kInitialAndFinalValuesHistogramName),
              testing::ElementsAre(base::Bucket(I11F11, 1)));
}

TEST_F(PasswordManagerSettingMigratorServiceTest,
       ReconcileWhenSyncIsNotExpectedDefaultValuesForPrefs) {
  ASSERT_EQ(prefs()->GetBoolean(prefs::kCredentialsEnableService), true);
  profile_sync_service()->SetCanSyncStart(false);
  base::HistogramTester tester;
  NotifyProfileAdded();
  ExpectValuesForBothPrefValues(true, true);
  EXPECT_THAT(tester.GetAllSamples(kInitialAndFinalValuesHistogramName),
              testing::ElementsAre(base::Bucket(I11F11, 1)));
}

}  // namespace password_manager
