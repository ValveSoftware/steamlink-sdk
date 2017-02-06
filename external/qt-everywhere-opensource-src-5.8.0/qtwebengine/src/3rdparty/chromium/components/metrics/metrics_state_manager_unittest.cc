// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_state_manager.h"

#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "components/metrics/client_info.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_switches.h"
#include "components/metrics/test_enabled_state_provider.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/caching_permuted_entropy_provider.h"
#include "components/variations/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

class MetricsStateManagerTest : public testing::Test {
 public:
  MetricsStateManagerTest()
      : enabled_state_provider_(new TestEnabledStateProvider(false, false)) {
    MetricsService::RegisterPrefs(prefs_.registry());
  }

  std::unique_ptr<MetricsStateManager> CreateStateManager() {
    return MetricsStateManager::Create(
        &prefs_, enabled_state_provider_.get(),
        base::Bind(&MetricsStateManagerTest::MockStoreClientInfoBackup,
                   base::Unretained(this)),
        base::Bind(&MetricsStateManagerTest::LoadFakeClientInfoBackup,
                   base::Unretained(this)));
  }

  // Sets metrics reporting as enabled for testing.
  void EnableMetricsReporting() {
    enabled_state_provider_->set_consent(true);
    enabled_state_provider_->set_enabled(true);
  }

 protected:
  TestingPrefServiceSimple prefs_;

  // Last ClientInfo stored by the MetricsStateManager via
  // MockStoreClientInfoBackup.
  std::unique_ptr<ClientInfo> stored_client_info_backup_;

  // If set, will be returned via LoadFakeClientInfoBackup if requested by the
  // MetricsStateManager.
  std::unique_ptr<ClientInfo> fake_client_info_backup_;

 private:
  // Stores the |client_info| in |stored_client_info_backup_| for verification
  // by the tests later.
  void MockStoreClientInfoBackup(const ClientInfo& client_info) {
    stored_client_info_backup_.reset(new ClientInfo);
    stored_client_info_backup_->client_id = client_info.client_id;
    stored_client_info_backup_->installation_date =
        client_info.installation_date;
    stored_client_info_backup_->reporting_enabled_date =
        client_info.reporting_enabled_date;

    // Respect the contract that storing an empty client_id voids the existing
    // backup (required for the last section of the ForceClientIdCreation test
    // below).
    if (client_info.client_id.empty())
      fake_client_info_backup_.reset();
  }

  // Hands out a copy of |fake_client_info_backup_| if it is set.
  std::unique_ptr<ClientInfo> LoadFakeClientInfoBackup() {
    if (!fake_client_info_backup_)
      return std::unique_ptr<ClientInfo>();

    std::unique_ptr<ClientInfo> backup_copy(new ClientInfo);
    backup_copy->client_id = fake_client_info_backup_->client_id;
    backup_copy->installation_date =
        fake_client_info_backup_->installation_date;
    backup_copy->reporting_enabled_date =
        fake_client_info_backup_->reporting_enabled_date;
    return backup_copy;
  }

  std::unique_ptr<TestEnabledStateProvider> enabled_state_provider_;

  DISALLOW_COPY_AND_ASSIGN(MetricsStateManagerTest);
};

// Ensure the ClientId is formatted as expected.
TEST_F(MetricsStateManagerTest, ClientIdCorrectlyFormatted) {
  std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());
  state_manager->ForceClientIdCreation();

  const std::string client_id = state_manager->client_id();
  EXPECT_EQ(36U, client_id.length());

  for (size_t i = 0; i < client_id.length(); ++i) {
    char current = client_id[i];
    if (i == 8 || i == 13 || i == 18 || i == 23)
      EXPECT_EQ('-', current);
    else
      EXPECT_TRUE(isxdigit(current));
  }
}

TEST_F(MetricsStateManagerTest, EntropySourceUsed_Low) {
  std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());
  state_manager->CreateDefaultEntropyProvider();
  EXPECT_EQ(MetricsStateManager::ENTROPY_SOURCE_LOW,
            state_manager->entropy_source_returned());
}

TEST_F(MetricsStateManagerTest, EntropySourceUsed_High) {
  EnableMetricsReporting();
  std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());
  state_manager->CreateDefaultEntropyProvider();
  EXPECT_EQ(MetricsStateManager::ENTROPY_SOURCE_HIGH,
            state_manager->entropy_source_returned());
}

TEST_F(MetricsStateManagerTest, LowEntropySource0NotReset) {
  std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());

  // Get the low entropy source once, to initialize it.
  state_manager->GetLowEntropySource();

  // Now, set it to 0 and ensure it doesn't get reset.
  state_manager->low_entropy_source_ = 0;
  EXPECT_EQ(0, state_manager->GetLowEntropySource());
  // Call it another time, just to make sure.
  EXPECT_EQ(0, state_manager->GetLowEntropySource());
}

TEST_F(MetricsStateManagerTest,
       PermutedEntropyCacheClearedWhenLowEntropyReset) {
  const PrefService::Preference* low_entropy_pref =
      prefs_.FindPreference(prefs::kMetricsLowEntropySource);
  const char* kCachePrefName =
      variations::prefs::kVariationsPermutedEntropyCache;
  int low_entropy_value = -1;

  // First, generate an initial low entropy source value.
  {
    EXPECT_TRUE(low_entropy_pref->IsDefaultValue());

    std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());
    state_manager->GetLowEntropySource();

    EXPECT_FALSE(low_entropy_pref->IsDefaultValue());
    EXPECT_TRUE(low_entropy_pref->GetValue()->GetAsInteger(&low_entropy_value));
  }

  // Now, set a dummy value in the permuted entropy cache pref and verify that
  // another call to GetLowEntropySource() doesn't clobber it when
  // --reset-variation-state wasn't specified.
  {
    prefs_.SetString(kCachePrefName, "test");

    std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());
    state_manager->GetLowEntropySource();

    EXPECT_EQ("test", prefs_.GetString(kCachePrefName));
    EXPECT_EQ(low_entropy_value,
              prefs_.GetInteger(prefs::kMetricsLowEntropySource));
  }

  // Verify that the cache does get reset if --reset-variations-state is passed.
  {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kResetVariationState);

    std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());
    state_manager->GetLowEntropySource();

    EXPECT_TRUE(prefs_.GetString(kCachePrefName).empty());
  }
}

// Check that setting the kMetricsResetIds pref to true causes the client id to
// be reset. We do not check that the low entropy source is reset because we
// cannot ensure that metrics state manager won't generate the same id again.
TEST_F(MetricsStateManagerTest, ResetMetricsIDs) {
  // Set an initial client id in prefs. It should not be possible for the
  // metrics state manager to generate this id randomly.
  const std::string kInitialClientId = "initial client id";
  prefs_.SetString(prefs::kMetricsClientID, kInitialClientId);

  // Make sure the initial client id isn't reset by the metrics state manager.
  {
    std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());
    state_manager->ForceClientIdCreation();
    EXPECT_EQ(kInitialClientId, state_manager->client_id());
  }

  // Set the reset pref to cause the IDs to be reset.
  prefs_.SetBoolean(prefs::kMetricsResetIds, true);

  // Cause the actual reset to happen.
  {
    std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());
    state_manager->ForceClientIdCreation();
    EXPECT_NE(kInitialClientId, state_manager->client_id());

    state_manager->GetLowEntropySource();

    EXPECT_FALSE(prefs_.GetBoolean(prefs::kMetricsResetIds));
  }

  EXPECT_NE(kInitialClientId, prefs_.GetString(prefs::kMetricsClientID));
}

TEST_F(MetricsStateManagerTest, ForceClientIdCreation) {
  const int64_t kFakeInstallationDate = 12345;
  prefs_.SetInt64(prefs::kInstallDate, kFakeInstallationDate);

  const int64_t test_begin_time = base::Time::Now().ToTimeT();

  // Holds ClientInfo from previous scoped test for extra checks.
  std::unique_ptr<ClientInfo> previous_client_info;

  {
    std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());

    // client_id shouldn't be auto-generated if metrics reporting is not
    // enabled.
    EXPECT_EQ(std::string(), state_manager->client_id());
    EXPECT_EQ(0, prefs_.GetInt64(prefs::kMetricsReportingEnabledTimestamp));

    // Confirm that the initial ForceClientIdCreation call creates the client id
    // and backs it up via MockStoreClientInfoBackup.
    EXPECT_FALSE(stored_client_info_backup_);
    state_manager->ForceClientIdCreation();
    EXPECT_NE(std::string(), state_manager->client_id());
    EXPECT_GE(prefs_.GetInt64(prefs::kMetricsReportingEnabledTimestamp),
              test_begin_time);

    ASSERT_TRUE(stored_client_info_backup_);
    EXPECT_EQ(state_manager->client_id(),
              stored_client_info_backup_->client_id);
    EXPECT_EQ(kFakeInstallationDate,
              stored_client_info_backup_->installation_date);
    EXPECT_EQ(prefs_.GetInt64(prefs::kMetricsReportingEnabledTimestamp),
              stored_client_info_backup_->reporting_enabled_date);

    previous_client_info = std::move(stored_client_info_backup_);
  }

  EnableMetricsReporting();

  {
    EXPECT_FALSE(stored_client_info_backup_);

    std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());

    // client_id should be auto-obtained from the constructor when metrics
    // reporting is enabled.
    EXPECT_EQ(previous_client_info->client_id, state_manager->client_id());

    // The backup should also be refreshed when the client id re-initialized.
    ASSERT_TRUE(stored_client_info_backup_);
    EXPECT_EQ(previous_client_info->client_id,
              stored_client_info_backup_->client_id);
    EXPECT_EQ(kFakeInstallationDate,
              stored_client_info_backup_->installation_date);
    EXPECT_EQ(previous_client_info->reporting_enabled_date,
              stored_client_info_backup_->reporting_enabled_date);

    // Re-forcing client id creation shouldn't cause another backup and
    // shouldn't affect the existing client id.
    stored_client_info_backup_.reset();
    state_manager->ForceClientIdCreation();
    EXPECT_FALSE(stored_client_info_backup_);
    EXPECT_EQ(previous_client_info->client_id, state_manager->client_id());
  }

  const int64_t kBackupInstallationDate = 1111;
  const int64_t kBackupReportingEnabledDate = 2222;
  const char kBackupClientId[] = "AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE";
  fake_client_info_backup_.reset(new ClientInfo);
  fake_client_info_backup_->client_id = kBackupClientId;
  fake_client_info_backup_->installation_date = kBackupInstallationDate;
  fake_client_info_backup_->reporting_enabled_date =
      kBackupReportingEnabledDate;

  {
    // The existence of a backup should result in the same behaviour as
    // before if we already have a client id.

    EXPECT_FALSE(stored_client_info_backup_);

    std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());
    EXPECT_EQ(previous_client_info->client_id, state_manager->client_id());

    // The backup should also be refreshed when the client id re-initialized.
    ASSERT_TRUE(stored_client_info_backup_);
    EXPECT_EQ(previous_client_info->client_id,
              stored_client_info_backup_->client_id);
    EXPECT_EQ(kFakeInstallationDate,
              stored_client_info_backup_->installation_date);
    EXPECT_EQ(previous_client_info->reporting_enabled_date,
              stored_client_info_backup_->reporting_enabled_date);
    stored_client_info_backup_.reset();
  }

  prefs_.ClearPref(prefs::kMetricsClientID);
  prefs_.ClearPref(prefs::kMetricsReportingEnabledTimestamp);

  {
    // The backup should kick in if the client id has gone missing. It should
    // replace remaining and missing dates as well.

    EXPECT_FALSE(stored_client_info_backup_);

    std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());
    EXPECT_EQ(kBackupClientId, state_manager->client_id());
    EXPECT_EQ(kBackupInstallationDate, prefs_.GetInt64(prefs::kInstallDate));
    EXPECT_EQ(kBackupReportingEnabledDate,
              prefs_.GetInt64(prefs::kMetricsReportingEnabledTimestamp));

    EXPECT_TRUE(stored_client_info_backup_);
    stored_client_info_backup_.reset();
  }

  const char kNoDashesBackupClientId[] = "AAAAAAAABBBBCCCCDDDDEEEEEEEEEEEE";
  fake_client_info_backup_.reset(new ClientInfo);
  fake_client_info_backup_->client_id = kNoDashesBackupClientId;

  prefs_.ClearPref(prefs::kInstallDate);
  prefs_.ClearPref(prefs::kMetricsClientID);
  prefs_.ClearPref(prefs::kMetricsReportingEnabledTimestamp);

  {
    // When running the backup from old-style client ids, dashes should be
    // re-added. And missing dates in backup should be replaced by Time::Now().

    EXPECT_FALSE(stored_client_info_backup_);

    std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());
    EXPECT_EQ(kBackupClientId, state_manager->client_id());
    EXPECT_GE(prefs_.GetInt64(prefs::kInstallDate), test_begin_time);
    EXPECT_GE(prefs_.GetInt64(prefs::kMetricsReportingEnabledTimestamp),
              test_begin_time);

    EXPECT_TRUE(stored_client_info_backup_);
    previous_client_info = std::move(stored_client_info_backup_);
  }

  prefs_.SetBoolean(prefs::kMetricsResetIds, true);

  {
    // Upon request to reset metrics ids, the existing backup should not be
    // restored.

    EXPECT_FALSE(stored_client_info_backup_);

    std::unique_ptr<MetricsStateManager> state_manager(CreateStateManager());

    // A brand new client id should have been generated.
    EXPECT_NE(std::string(), state_manager->client_id());
    EXPECT_NE(previous_client_info->client_id, state_manager->client_id());

    // The installation date should not have been affected.
    EXPECT_EQ(previous_client_info->installation_date,
              prefs_.GetInt64(prefs::kInstallDate));

    // The metrics-reporting-enabled date will be reset to Now().
    EXPECT_GE(prefs_.GetInt64(prefs::kMetricsReportingEnabledTimestamp),
              previous_client_info->reporting_enabled_date);

    stored_client_info_backup_.reset();
  }
}

}  // namespace metrics
