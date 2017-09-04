// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing_db/safe_browsing_prefs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class SafeBrowsingPrefsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    prefs_.registry()->RegisterBooleanPref(
        prefs::kSafeBrowsingExtendedReportingEnabled, false);
    prefs_.registry()->RegisterBooleanPref(
        prefs::kSafeBrowsingScoutReportingEnabled, false);
    prefs_.registry()->RegisterBooleanPref(
        prefs::kSafeBrowsingScoutGroupSelected, false);

    ResetExperiments(/*can_show_scout=*/false, /*only_show_scout=*/false);
  }

  void ResetPrefs(bool sber_reporting, bool scout_reporting, bool scout_group) {
    prefs_.SetBoolean(prefs::kSafeBrowsingExtendedReportingEnabled,
                      sber_reporting);
    prefs_.SetBoolean(prefs::kSafeBrowsingScoutReportingEnabled,
                      scout_reporting);
    prefs_.SetBoolean(prefs::kSafeBrowsingScoutGroupSelected, scout_group);
  }

  void ResetExperiments(bool can_show_scout, bool only_show_scout) {
    std::vector<std::string> enabled_features;
    std::vector<std::string> disabled_features;

    auto* target_vector =
        can_show_scout ? &enabled_features : &disabled_features;
    target_vector->push_back(kCanShowScoutOptIn.name);

    target_vector = only_show_scout ? &enabled_features : &disabled_features;
    target_vector->push_back(kOnlyShowScoutOptIn.name);

    feature_list_.reset(new base::test::ScopedFeatureList);
    feature_list_->InitFromCommandLine(
        base::JoinString(enabled_features, ","),
        base::JoinString(disabled_features, ","));
  }

  std::string GetActivePref() { return GetExtendedReportingPrefName(prefs_); }

  // Convenience method for explicitly setting up all combinations of prefs and
  // experiments.
  void TestGetPrefName(bool sber_reporting,
                       bool scout_reporting,
                       bool scout_group,
                       bool can_show_scout,
                       bool only_show_scout,
                       const std::string& expected_pref) {
    ResetPrefs(sber_reporting, scout_reporting, scout_group);
    ResetExperiments(can_show_scout, only_show_scout);
    EXPECT_EQ(expected_pref, GetActivePref())
        << "sber=" << sber_reporting << " scout=" << scout_reporting
        << " scout_group=" << scout_group
        << " can_show_scout=" << can_show_scout
        << " only_show_scout=" << only_show_scout;
  }

  void InitPrefs() { InitializeSafeBrowsingPrefs(&prefs_); }

  bool IsScoutGroupSelected() {
    return prefs_.GetBoolean(prefs::kSafeBrowsingScoutGroupSelected);
  }

  void ExpectPrefs(bool sber_reporting,
                   bool scout_reporting,
                   bool scout_group) {
    LOG(INFO) << "Pref values: sber=" << sber_reporting
              << " scout=" << scout_reporting << " scout_group=" << scout_group;
    EXPECT_EQ(sber_reporting,
              prefs_.GetBoolean(prefs::kSafeBrowsingExtendedReportingEnabled));
    EXPECT_EQ(scout_reporting,
              prefs_.GetBoolean(prefs::kSafeBrowsingScoutReportingEnabled));
    EXPECT_EQ(scout_group,
              prefs_.GetBoolean(prefs::kSafeBrowsingScoutGroupSelected));
  }

  void ExpectPrefsExist(bool sber_reporting,
                        bool scout_reporting,
                        bool scout_group) {
    LOG(INFO) << "Prefs exist: sber=" << sber_reporting
              << " scout=" << scout_reporting << " scout_group=" << scout_group;
    EXPECT_EQ(sber_reporting,
              prefs_.HasPrefPath(prefs::kSafeBrowsingExtendedReportingEnabled));
    EXPECT_EQ(scout_reporting,
              prefs_.HasPrefPath(prefs::kSafeBrowsingScoutReportingEnabled));
    EXPECT_EQ(scout_group,
              prefs_.HasPrefPath(prefs::kSafeBrowsingScoutGroupSelected));
  }
  TestingPrefServiceSimple prefs_;

 private:
  std::unique_ptr<base::test::ScopedFeatureList> feature_list_;
};

// This test ensures that we correctly select between SBER and Scout as the
// active preference in a number of common scenarios.
TEST_F(SafeBrowsingPrefsTest, GetExtendedReportingPrefName_Common) {
  const std::string& sber = prefs::kSafeBrowsingExtendedReportingEnabled;
  const std::string& scout = prefs::kSafeBrowsingScoutReportingEnabled;

  // By default (all prefs and experiment features disabled), SBER pref is used.
  TestGetPrefName(false, false, false, false, false, sber);

  // Changing any prefs (including ScoutGroupSelected) keeps SBER as the active
  // pref because the experiment remains in the Control group.
  TestGetPrefName(/*sber=*/true, false, false, false, false, sber);
  TestGetPrefName(false, /*scout=*/true, false, false, false, sber);
  TestGetPrefName(false, false, /*scout_group=*/true, false, false, sber);

  // Being in either experiment group with ScoutGroup selected makes Scout the
  // active pref.
  TestGetPrefName(false, false, /*scout_group=*/true, /*can_show_scout=*/true,
                  false, scout);
  TestGetPrefName(false, false, /*scout_group=*/true, false,
                  /*only_show_scout=*/true, scout);

  // When ScoutGroup is not selected then SBER remains the active pref,
  // regardless which experiment is enabled.
  TestGetPrefName(false, false, false, /*can_show_scout=*/true, false, sber);
  TestGetPrefName(false, false, false, false, /*only_show_scout=*/true, sber);
}

// Here we exhaustively check all combinations of pref and experiment states.
// This should help catch regressions.
TEST_F(SafeBrowsingPrefsTest, GetExtendedReportingPrefName_Exhaustive) {
  const std::string& sber = prefs::kSafeBrowsingExtendedReportingEnabled;
  const std::string& scout = prefs::kSafeBrowsingScoutReportingEnabled;
  TestGetPrefName(false, false, false, false, false, sber);
  TestGetPrefName(false, false, false, false, true, sber);
  TestGetPrefName(false, false, false, true, false, sber);
  TestGetPrefName(false, false, false, true, true, sber);
  TestGetPrefName(false, false, true, false, false, sber);
  TestGetPrefName(false, false, true, false, true, scout);
  TestGetPrefName(false, false, true, true, false, scout);
  TestGetPrefName(false, false, true, true, true, scout);
  TestGetPrefName(false, true, false, false, false, sber);
  TestGetPrefName(false, true, false, false, true, sber);
  TestGetPrefName(false, true, false, true, false, sber);
  TestGetPrefName(false, true, false, true, true, sber);
  TestGetPrefName(false, true, true, false, false, sber);
  TestGetPrefName(false, true, true, false, true, scout);
  TestGetPrefName(false, true, true, true, false, scout);
  TestGetPrefName(false, true, true, true, true, scout);
  TestGetPrefName(true, false, false, false, false, sber);
  TestGetPrefName(true, false, false, false, true, sber);
  TestGetPrefName(true, false, false, true, false, sber);
  TestGetPrefName(true, false, false, true, true, sber);
  TestGetPrefName(true, false, true, false, false, sber);
  TestGetPrefName(true, false, true, false, true, scout);
  TestGetPrefName(true, false, true, true, false, scout);
  TestGetPrefName(true, false, true, true, true, scout);
  TestGetPrefName(true, true, false, false, false, sber);
  TestGetPrefName(true, true, false, false, true, sber);
  TestGetPrefName(true, true, false, true, false, sber);
  TestGetPrefName(true, true, false, true, true, sber);
  TestGetPrefName(true, true, true, false, false, sber);
  TestGetPrefName(true, true, true, false, true, scout);
  TestGetPrefName(true, true, true, true, false, scout);
  TestGetPrefName(true, true, true, true, true, scout);
}

// Basic test that command-line flags can force the ScoutGroupSelected pref on
// or off.
TEST_F(SafeBrowsingPrefsTest, InitPrefs_ForceScoutGroupOnOff) {
  // By default ScoutGroupSelected is off.
  EXPECT_FALSE(IsScoutGroupSelected());

  // Command-line flag can force it on during initialization.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      kSwitchForceScoutGroup, "true");
  InitPrefs();
  EXPECT_TRUE(IsScoutGroupSelected());

  // ScoutGroup remains on if switches are cleared, but only if an experiment
  // is active (since being in the Control group automatically clears the
  // Scout prefs).
  base::CommandLine::StringVector empty;
  base::CommandLine::ForCurrentProcess()->InitFromArgv(empty);
  ResetExperiments(/*can_show_scout=*/true, /*only_show_scout=*/false);
  EXPECT_TRUE(IsScoutGroupSelected());

  // Nonsense values are ignored and ScoutGroup is unchanged.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      kSwitchForceScoutGroup, "foo");
  InitPrefs();
  EXPECT_TRUE(IsScoutGroupSelected());

  // ScoutGroup can also be forced off during initialization.
  base::CommandLine::ForCurrentProcess()->InitFromArgv(empty);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      kSwitchForceScoutGroup, "false");
  InitPrefs();
  EXPECT_FALSE(IsScoutGroupSelected());
}

// Test all combinations of prefs during initialization when neither experiment
// is on (ie: control group). In all cases the Scout prefs should be cleared,
// and the SBER pref may get switched.
TEST_F(SafeBrowsingPrefsTest, InitPrefs_Control) {
  // Turn both experiments off.
  ResetExperiments(/*can_show_scout=*/false, /*only_show_scout=*/false);

  // Default case (everything off) - no change on init.
  ResetPrefs(false, false, false);
  InitPrefs();
  ExpectPrefs(false, false, false);
  // SBER pref exists because it was set to false above.
  ExpectPrefsExist(true, false, false);

  // ScoutGroup on - SBER cleared since Scout opt-in was shown but Scout pref
  // was not chosen. Scout prefs cleared.
  ResetPrefs(false, false, true);
  InitPrefs();
  ExpectPrefs(false, false, false);
  ExpectPrefsExist(true, false, false);

  // ScoutReporting on without ScoutGroup - SBER turns on since user opted-in to
  // broader Scout reporting, we can continue collecting the SBER subset. Scout
  // prefs cleared.
  ResetPrefs(false, true, false);
  InitPrefs();
  ExpectPrefs(true, false, false);
  ExpectPrefsExist(true, false, false);

  // ScoutReporting and ScoutGroup on - SBER turns on since user opted-in to
  // broader Scout reporting, we can continue collecting the SBER subset. Scout
  // prefs cleared.
  ResetPrefs(false, true, true);
  InitPrefs();
  ExpectPrefs(true, false, false);
  ExpectPrefsExist(true, false, false);

  // SBER on - no change on init since ScoutGroup is off implying that user
  // never saw Scout opt-in text. Scout prefs remain cleared.
  ResetPrefs(true, false, false);
  InitPrefs();
  ExpectPrefs(true, false, false);
  ExpectPrefsExist(true, false, false);

  // SBER and ScoutGroup on - SBER cleared. User previously opted-in to SBER
  // and they saw Scout opt-in text (ie. ScoutGroup on), but chose not to opt-in
  // to Scout reporting. We want them to re-evaluate their choice of SBER since
  // the lack of Scout opt-in was a conscious choice. Scout cleared.
  ResetPrefs(true, false, true);
  InitPrefs();
  ExpectPrefs(false, false, false);
  ExpectPrefsExist(true, false, false);

  // SBER and ScoutReporting on. User has opted-in to broader level of reporting
  // so SBER stays on. Scout prefs cleared.
  ResetPrefs(true, true, false);
  InitPrefs();
  ExpectPrefs(true, false, false);
  ExpectPrefsExist(true, false, false);

  // Everything on. User has opted-in to broader level of reporting so SBER
  // stays on. Scout prefs cleared.
  ResetPrefs(true, true, true);
  InitPrefs();
  ExpectPrefs(true, false, false);
  ExpectPrefs(true, false, false);
}

// Tests a unique case where the Extended Reporting pref will be Cleared instead
// of set to False in order to mimic the state of the Scout reporting pref.
// This happens when a user is in the OnlyShowScoutOptIn experiment but never
// encounters a security issue so never sees the Scout opt-in. This user then
// returns to the Control group having never seen the Scout opt-in, so their
// Scout Reporting pref is un-set. We want to return their SBER pref to the
// unset state as well.
TEST_F(SafeBrowsingPrefsTest, InitPrefs_Control_SberPrefCleared) {
  // Turn both experiments off.
  ResetExperiments(/*can_show_scout=*/false, /*only_show_scout=*/false);

  // Set the user's old SBER pref to on to be explicit.
  prefs_.SetBoolean(prefs::kSafeBrowsingExtendedReportingEnabled, true);
  // User is in the OnlyShowScoutOptIn experiment so they go directly to the
  // Scout Group.
  prefs_.SetBoolean(prefs::kSafeBrowsingScoutGroupSelected, true);
  // But they never see a security popup or change the setting manually so the
  // Scout pref remains unset.
  prefs_.ClearPref(prefs::kSafeBrowsingScoutReportingEnabled);

  InitPrefs();

  // All pref values should be false and unset.
  ExpectPrefs(false, false, false);
  ExpectPrefsExist(false, false, false);
}

// Test all combinations of prefs during initialization when the CanShowScout
// experiment is on.
TEST_F(SafeBrowsingPrefsTest, InitPrefs_CanShowScout) {
  // Turn the CanShowScout experiment on.
  ResetExperiments(/*can_show_scout=*/true, /*only_show_scout=*/false);

  // Default case (everything off) - ScoutGroup turns on because SBER is off.
  ResetPrefs(false, false, false);
  InitPrefs();
  ExpectPrefs(false, false, true);

  // ScoutGroup on - no change on init since ScoutGroup is already on.
  ResetPrefs(false, false, true);
  InitPrefs();
  ExpectPrefs(false, false, true);

  // ScoutReporting on without ScoutGroup - ScoutGroup turns on because SBER is
  // off.
  ResetPrefs(false, true, false);
  InitPrefs();
  ExpectPrefs(false, true, true);

  // ScoutReporting and ScoutGroup on - no change on init since ScoutGroup is
  // already on.
  ResetPrefs(false, true, true);
  InitPrefs();
  ExpectPrefs(false, true, true);

  // SBER on - no change on init. Will wait for first security incident before
  // turning on ScoutGroup and displaying the Scout opt-in.
  ResetPrefs(true, false, false);
  InitPrefs();
  ExpectPrefs(true, false, false);

  // SBER and ScoutGroup on - no change on init since ScoutGroup is already on.
  ResetPrefs(true, false, true);
  InitPrefs();
  ExpectPrefs(true, false, true);

  // SBER and ScoutReporting on - no change on init because SBER is on.
  // ScoutGroup will turn on on next security incident.
  ResetPrefs(true, true, false);
  InitPrefs();
  ExpectPrefs(true, true, false);

  // Everything on - no change on init since ScoutGroup is already on.
  ResetPrefs(true, true, true);
  InitPrefs();
  ExpectPrefs(true, true, true);
}

// Test all combinations of prefs during initialization when the OnlyShowScout
// experiment is on.
TEST_F(SafeBrowsingPrefsTest, InitPrefs_OnlyShowScout) {
  // Turn the OnlyShowScout experiment on.
  ResetExperiments(/*can_show_scout=*/false, /*only_show_scout=*/true);

  // Default case (everything off) - ScoutGroup turns on.
  ResetPrefs(false, false, false);
  InitPrefs();
  ExpectPrefs(false, false, true);

  // ScoutGroup on - no change on init since ScoutGroup is already on.
  ResetPrefs(false, false, true);
  InitPrefs();
  ExpectPrefs(false, false, true);

  // ScoutReporting on without ScoutGroup - ScoutGroup turns on.
  ResetPrefs(false, true, false);
  InitPrefs();
  ExpectPrefs(false, true, true);

  // ScoutReporting and ScoutGroup on - no change on init since ScoutGroup is
  // already on.
  ResetPrefs(false, true, true);
  InitPrefs();
  ExpectPrefs(false, true, true);

  // SBER on - ScoutGroup turns on immediately, not waiting for first security
  // incident.
  ResetPrefs(true, false, false);
  InitPrefs();
  ExpectPrefs(true, false, true);

  // SBER and ScoutGroup on - no change on init since ScoutGroup is already on.
  ResetPrefs(true, false, true);
  InitPrefs();
  ExpectPrefs(true, false, true);

  // SBER and ScoutReporting on - ScoutGroup turns on immediately, not waiting
  // for first security incident.
  ResetPrefs(true, true, false);
  InitPrefs();
  ExpectPrefs(true, true, true);

  // Everything on - no change on init since ScoutGroup is already on.
  ResetPrefs(true, true, true);
  InitPrefs();
  ExpectPrefs(true, true, true);
}

TEST_F(SafeBrowsingPrefsTest, ChooseOptInText) {
  const int kSberResource = 100;
  const int kScoutResource = 500;
  // By default, SBER opt-in is used
  EXPECT_EQ(kSberResource,
            ChooseOptInTextResource(prefs_, kSberResource, kScoutResource));

  // Enabling Scout switches to the Scout opt-in text.
  ResetExperiments(/*can_show_scout=*/false, /*only_show_scout=*/true);
  ResetPrefs(/*sber=*/false, /*scout=*/false, /*scout_group=*/true);
  EXPECT_EQ(kScoutResource,
            ChooseOptInTextResource(prefs_, kSberResource, kScoutResource));
}

TEST_F(SafeBrowsingPrefsTest, GetSafeBrowsingExtendedReportingLevel) {
  // By Default, SBER is off
  EXPECT_EQ(SBER_LEVEL_OFF, GetExtendedReportingLevel(prefs_));

  // Opt-in to Legacy SBER gives Legacy reporting leve.
  ResetPrefs(/*sber=*/true, /*scout_reporting=*/false, /*scout_group=*/false);
  EXPECT_EQ(SBER_LEVEL_LEGACY, GetExtendedReportingLevel(prefs_));

  // The value of the Scout pref doesn't change the reporting level if the user
  // is outside of the Scout Group and/or no experiment is running.
  // No scout group.
  ResetPrefs(/*sber=*/true, /*scout_reporting=*/true, /*scout_group=*/false);
  EXPECT_EQ(SBER_LEVEL_LEGACY, GetExtendedReportingLevel(prefs_));
  // Scout group but no experiment.
  ResetPrefs(/*sber=*/true, /*scout_reporting=*/true, /*scout_group=*/true);
  EXPECT_EQ(SBER_LEVEL_LEGACY, GetExtendedReportingLevel(prefs_));

  // Remaining in the Scout Group and adding an experiment will switch to the
  // Scout pref to determine reporting level.
  ResetExperiments(/*can_show_scout=*/false, /*only_show_scout=*/true);
  // Both reporting prefs off, so reporting is off.
  ResetPrefs(/*sber=*/false, /*scout_reporting=*/false, /*scout_group=*/true);
  EXPECT_EQ(SBER_LEVEL_OFF, GetExtendedReportingLevel(prefs_));
  // Legacy pref on when we're using Scout - reporting remains off.
  ResetPrefs(/*sber=*/true, /*scout_reporting=*/false, /*scout_group=*/true);
  EXPECT_EQ(SBER_LEVEL_OFF, GetExtendedReportingLevel(prefs_));
  // Turning on Scout gives us Scout level reporting
  ResetPrefs(/*sber=*/false, /*scout_reporting=*/true, /*scout_group=*/true);
  EXPECT_EQ(SBER_LEVEL_SCOUT, GetExtendedReportingLevel(prefs_));
  // .. and the legacy pref doesn't affect this.
  ResetPrefs(/*sber=*/true, /*scout_reporting=*/true, /*scout_group=*/true);
  EXPECT_EQ(SBER_LEVEL_SCOUT, GetExtendedReportingLevel(prefs_));
}

}  // namespace safe_browsing
