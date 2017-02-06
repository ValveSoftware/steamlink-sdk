// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_bubble_experiment.h"

#include <ostream>

#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_driver/fake_sync_service.h"
#include "components/variations/variations_associated_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_bubble_experiment {

namespace {

const char kSmartLockNoBrandingGroupName[] = "NoSmartLockBranding";

enum class CustomPassphraseState { NONE, SET };

enum class SavePromptFirstRunExperience { NONE, PRESENT };

enum class UserType { SMARTLOCK, NOT_SMARTLOCK };

struct IsSmartLockBrandingEnabledTestcase {
  CustomPassphraseState passphrase_state;
  syncer::ModelType type;
  SmartLockBranding expected_branding;
  UserType expected_user_type;
};

std::ostream& operator<<(std::ostream& os,
                         const IsSmartLockBrandingEnabledTestcase& testcase) {
  os << (testcase.passphrase_state == CustomPassphraseState::SET ? "{SET, "
                                                                 : "{NONE, ");
  os << (testcase.type == syncer::PASSWORDS ? "syncer::PASSWORDS, "
                                            : "not syncer::PASSWORDS, ");
  switch (testcase.expected_branding) {
    case SmartLockBranding::NONE:
      os << "NONE, ";
      break;
    case SmartLockBranding::FULL:
      os << "FULL, ";
      break;
    case SmartLockBranding::SAVE_PROMPT_ONLY:
      os << "SAVE_PROMPT_ONLY, ";
      break;
  }
  os << (testcase.expected_user_type == UserType::SMARTLOCK ? "SMARTLOCK}"
                                                            : "NOT_SMARTLOCK}");
  return os;
}

struct ShouldShowSavePromptFirstRunExperienceTestcase {
  CustomPassphraseState passphrase_state;
  syncer::ModelType type;
  bool pref_value;
  SavePromptFirstRunExperience first_run_experience;
};

class TestSyncService : public sync_driver::FakeSyncService {
 public:
  // FakeSyncService overrides.
  bool IsSyncAllowed() const override { return is_sync_allowed_; }

  bool IsFirstSetupComplete() const override {
    return is_first_setup_complete_;
  }

  bool IsSyncActive() const override {
    return is_sync_allowed_ && is_first_setup_complete_;
  }

  syncer::ModelTypeSet GetActiveDataTypes() const override { return type_set_; }

  syncer::ModelTypeSet GetPreferredDataTypes() const override {
    return type_set_;
  }

  bool IsUsingSecondaryPassphrase() const override {
    return is_using_secondary_passphrase_;
  }

  void set_is_using_secondary_passphrase(bool is_using_secondary_passphrase) {
    is_using_secondary_passphrase_ = is_using_secondary_passphrase;
  }

  void set_active_data_types(syncer::ModelTypeSet type_set) {
    type_set_ = type_set;
  }

  void set_sync_allowed(bool sync_allowed) { is_sync_allowed_ = sync_allowed; }

  void set_first_setup_complete(bool setup_complete) {
    is_first_setup_complete_ = setup_complete;
  }

  void ClearActiveDataTypes() { type_set_.Clear(); }

  bool CanSyncStart() const override { return true; }

 private:
  syncer::ModelTypeSet type_set_;
  bool is_using_secondary_passphrase_ = false;
  bool is_sync_allowed_ = true;
  bool is_first_setup_complete_ = true;
};

}  // namespace

class PasswordManagerPasswordBubbleExperimentTest : public testing::Test {
 public:
  PasswordManagerPasswordBubbleExperimentTest() : field_trial_list_(nullptr) {
    RegisterPrefs(pref_service_.registry());
  }

  ~PasswordManagerPasswordBubbleExperimentTest() override {
    variations::testing::ClearAllVariationIDs();
    variations::testing::ClearAllVariationParams();
  }

  PrefService* prefs() { return &pref_service_; }

  void EnforceExperimentGroup(const char* group_name) {
    ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(kBrandingExperimentName,
                                                       group_name));
  }

  TestSyncService* sync_service() { return &fake_sync_service_; }

  void TestIsSmartLockBrandingEnabledTestcase(
      const IsSmartLockBrandingEnabledTestcase& test_case) {
    SetupFakeSyncServiceForTestCase(test_case.type, test_case.passphrase_state);
    EXPECT_EQ(test_case.expected_branding,
              GetSmartLockBrandingState(sync_service()));
    EXPECT_EQ(test_case.expected_user_type == UserType::SMARTLOCK,
              IsSmartLockUser(sync_service()));
  }

  void TestShouldShowSavePromptFirstRunExperienceTestcase(
      const ShouldShowSavePromptFirstRunExperienceTestcase& test_case) {
    SetupFakeSyncServiceForTestCase(test_case.type, test_case.passphrase_state);
    prefs()->SetBoolean(
        password_manager::prefs::kWasSavePrompFirstRunExperienceShown,
        test_case.pref_value);
    bool should_show_first_run_experience =
        ShouldShowSavePromptFirstRunExperience(sync_service(), prefs());
    if (test_case.first_run_experience ==
        SavePromptFirstRunExperience::PRESENT) {
      EXPECT_FALSE(should_show_first_run_experience);
    } else {
      EXPECT_FALSE(should_show_first_run_experience);
    }
  }

 private:
  void SetupFakeSyncServiceForTestCase(syncer::ModelType type,
                                       CustomPassphraseState passphrase_state) {
    syncer::ModelTypeSet active_types;
    active_types.Put(type);
    sync_service()->ClearActiveDataTypes();
    sync_service()->set_active_data_types(active_types);
    sync_service()->set_is_using_secondary_passphrase(
        passphrase_state == CustomPassphraseState::SET);
  }

  TestSyncService fake_sync_service_;
  base::FieldTrialList field_trial_list_;
  TestingPrefServiceSimple pref_service_;
};

TEST_F(PasswordManagerPasswordBubbleExperimentTest,
       IsSmartLockBrandingEnabledTestNoBranding) {
  const IsSmartLockBrandingEnabledTestcase kTestData[] = {
      {CustomPassphraseState::SET, syncer::PASSWORDS, SmartLockBranding::NONE,
       UserType::NOT_SMARTLOCK},
      {CustomPassphraseState::SET, syncer::BOOKMARKS, SmartLockBranding::NONE,
       UserType::NOT_SMARTLOCK},
      {CustomPassphraseState::NONE, syncer::PASSWORDS, SmartLockBranding::NONE,
       UserType::SMARTLOCK},
      {CustomPassphraseState::NONE, syncer::BOOKMARKS, SmartLockBranding::NONE,
       UserType::NOT_SMARTLOCK},
  };

  EnforceExperimentGroup(kSmartLockNoBrandingGroupName);
  for (const auto& test_case : kTestData) {
    SCOPED_TRACE(testing::Message("test_case = ") << test_case);
    TestIsSmartLockBrandingEnabledTestcase(test_case);
  }
}

TEST_F(PasswordManagerPasswordBubbleExperimentTest,
       IsSmartLockBrandingEnabledTest_FULL) {
  const IsSmartLockBrandingEnabledTestcase kTestData[] = {
      {CustomPassphraseState::SET, syncer::PASSWORDS, SmartLockBranding::NONE,
       UserType::NOT_SMARTLOCK},
      {CustomPassphraseState::SET, syncer::BOOKMARKS, SmartLockBranding::NONE,
       UserType::NOT_SMARTLOCK},
      {CustomPassphraseState::NONE, syncer::PASSWORDS, SmartLockBranding::FULL,
       UserType::SMARTLOCK},
      {CustomPassphraseState::NONE, syncer::BOOKMARKS, SmartLockBranding::NONE,
       UserType::NOT_SMARTLOCK},
  };

  EnforceExperimentGroup(kSmartLockBrandingGroupName);
  for (const auto& test_case : kTestData) {
    SCOPED_TRACE(testing::Message("test_case = ") << test_case);
    TestIsSmartLockBrandingEnabledTestcase(test_case);
  }
}

TEST_F(PasswordManagerPasswordBubbleExperimentTest,
       IsSmartLockBrandingEnabledTest_SAVE_PROMPT_ONLY) {
  const IsSmartLockBrandingEnabledTestcase kTestData[] = {
      {CustomPassphraseState::SET, syncer::PASSWORDS, SmartLockBranding::NONE,
       UserType::NOT_SMARTLOCK},
      {CustomPassphraseState::SET, syncer::BOOKMARKS, SmartLockBranding::NONE,
       UserType::NOT_SMARTLOCK},
      {CustomPassphraseState::NONE, syncer::PASSWORDS,
       SmartLockBranding::SAVE_PROMPT_ONLY, UserType::SMARTLOCK},
      {CustomPassphraseState::NONE, syncer::BOOKMARKS, SmartLockBranding::NONE,
       UserType::NOT_SMARTLOCK},
  };

  EnforceExperimentGroup(kSmartLockBrandingSavePromptOnlyGroupName);
  for (const auto& test_case : kTestData) {
    SCOPED_TRACE(testing::Message("test_case = ") << test_case);
    TestIsSmartLockBrandingEnabledTestcase(test_case);
  }
}

TEST_F(PasswordManagerPasswordBubbleExperimentTest,
       ShoulShowSavePrompBrandingGroup) {
  const struct ShouldShowSavePromptFirstRunExperienceTestcase kTestData[] = {
      {CustomPassphraseState::SET, syncer::PASSWORDS, true,
       SavePromptFirstRunExperience::NONE},
      {CustomPassphraseState::SET, syncer::PASSWORDS, false,
       SavePromptFirstRunExperience::NONE},
      {CustomPassphraseState::SET, syncer::BOOKMARKS, true,
       SavePromptFirstRunExperience::NONE},
      {CustomPassphraseState::SET, syncer::BOOKMARKS, false,
       SavePromptFirstRunExperience::NONE},
      {CustomPassphraseState::NONE, syncer::PASSWORDS, true,
       SavePromptFirstRunExperience::NONE},
      {CustomPassphraseState::NONE, syncer::PASSWORDS, false,
       SavePromptFirstRunExperience::NONE},
      {CustomPassphraseState::NONE, syncer::BOOKMARKS, true,
       SavePromptFirstRunExperience::NONE},
      {CustomPassphraseState::NONE, syncer::BOOKMARKS, false,
       SavePromptFirstRunExperience::NONE},
  };

  EnforceExperimentGroup(kSmartLockBrandingGroupName);
  for (const auto& test_case : kTestData) {
    TestShouldShowSavePromptFirstRunExperienceTestcase(test_case);
  }
}

TEST_F(PasswordManagerPasswordBubbleExperimentTest,
       ShoulShowSavePrompNoBrandingGroup) {
  const struct ShouldShowSavePromptFirstRunExperienceTestcase kTestData[] = {
      {CustomPassphraseState::SET, syncer::PASSWORDS, true,
       SavePromptFirstRunExperience::NONE},
      {CustomPassphraseState::SET, syncer::PASSWORDS, false,
       SavePromptFirstRunExperience::NONE},
      {CustomPassphraseState::SET, syncer::BOOKMARKS, true,
       SavePromptFirstRunExperience::NONE},
      {CustomPassphraseState::SET, syncer::BOOKMARKS, false,
       SavePromptFirstRunExperience::NONE},
      {CustomPassphraseState::NONE, syncer::PASSWORDS, true,
       SavePromptFirstRunExperience::NONE},
      {CustomPassphraseState::NONE, syncer::PASSWORDS, false,
       SavePromptFirstRunExperience::NONE},
      {CustomPassphraseState::NONE, syncer::BOOKMARKS, true,
       SavePromptFirstRunExperience::NONE},
      {CustomPassphraseState::NONE, syncer::BOOKMARKS, false,
       SavePromptFirstRunExperience::NONE},
  };

  EnforceExperimentGroup(kSmartLockNoBrandingGroupName);
  for (const auto& test_case : kTestData) {
    TestShouldShowSavePromptFirstRunExperienceTestcase(test_case);
  }
}

TEST_F(PasswordManagerPasswordBubbleExperimentTest,
       RecordFirstRunExperienceWasShownTest) {
  const struct {
    bool initial_pref_value;
    bool result_pref_value;
  } kTestData[] = {
      {false, true}, {true, true},
  };
  for (const auto& test_case : kTestData) {
    // Record Save prompt first run experience.
    prefs()->SetBoolean(
        password_manager::prefs::kWasSavePrompFirstRunExperienceShown,
        test_case.initial_pref_value);
    RecordSavePromptFirstRunExperienceWasShown(prefs());
    EXPECT_EQ(
        test_case.result_pref_value,
        prefs()->GetBoolean(
            password_manager::prefs::kWasSavePrompFirstRunExperienceShown));
    // Record Auto sign-in first run experience.
    prefs()->SetBoolean(
        password_manager::prefs::kWasAutoSignInFirstRunExperienceShown,
        test_case.initial_pref_value);
    EXPECT_EQ(!test_case.initial_pref_value,
              ShouldShowAutoSignInPromptFirstRunExperience(prefs()));
        RecordAutoSignInPromptFirstRunExperienceWasShown(prefs());
    EXPECT_EQ(
        test_case.result_pref_value,
        prefs()->GetBoolean(
            password_manager::prefs::kWasAutoSignInFirstRunExperienceShown));
    EXPECT_EQ(!test_case.result_pref_value,
              ShouldShowAutoSignInPromptFirstRunExperience(prefs()));
  }
}

TEST_F(PasswordManagerPasswordBubbleExperimentTest,
       ShouldShowChromeSignInPasswordPromo) {
  // By default the promo is off.
  EXPECT_FALSE(ShouldShowChromeSignInPasswordPromo(prefs(), nullptr));
  const struct {
    bool was_already_clicked;
    bool is_sync_allowed;
    bool is_first_setup_complete;
    int current_shown_count;
    int experiment_threshold;
    bool result;
  } kTestData[] = {
      {false, true, false, 0, 5, true},   {false, true, false, 5, 5, false},
      {true, true, false, 0, 5, false},   {true, true, false, 10, 5, false},
      {false, false, false, 0, 5, false}, {false, true, true, 0, 5, false},
  };
  const char kFakeGroup[] = "FakeGroup";
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      kChromeSignInPasswordPromoExperimentName, kFakeGroup));
  for (const auto& test_case : kTestData) {
    SCOPED_TRACE(testing::Message("#test_case = ") << (&test_case - kTestData));
    prefs()->SetBoolean(password_manager::prefs::kWasSignInPasswordPromoClicked,
                        test_case.was_already_clicked);
    prefs()->SetInteger(
        password_manager::prefs::kNumberSignInPasswordPromoShown,
        test_case.current_shown_count);
    sync_service()->set_sync_allowed(test_case.is_sync_allowed);
    sync_service()->set_first_setup_complete(test_case.is_first_setup_complete);
    variations::AssociateVariationParams(
        kChromeSignInPasswordPromoExperimentName, kFakeGroup,
        {{kChromeSignInPasswordPromoThresholdParam,
          base::IntToString(test_case.experiment_threshold)}});

    EXPECT_EQ(test_case.result,
              ShouldShowChromeSignInPasswordPromo(prefs(), sync_service()));
  }
}

}  // namespace password_bubble_experiment
