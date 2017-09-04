// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/browser/subresource_filter_features.h"

#include <string>

#include "base/metrics/field_trial.h"
#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

TEST(SubresourceFilterFeaturesTest, ActivationState) {
  const struct {
    bool feature_enabled;
    const char* activation_state_param;
    ActivationState expected_activation_state;
  } kTestCases[] = {{false, "", ActivationState::DISABLED},
                    {false, "disabled", ActivationState::DISABLED},
                    {false, "dryrun", ActivationState::DISABLED},
                    {false, "enabled", ActivationState::DISABLED},
                    {false, "%$ garbage !%", ActivationState::DISABLED},
                    {true, "", ActivationState::DISABLED},
                    {true, "disable", ActivationState::DISABLED},
                    {true, "Disable", ActivationState::DISABLED},
                    {true, "disabled", ActivationState::DISABLED},
                    {true, "%$ garbage !%", ActivationState::DISABLED},
                    {true, kActivationStateDryRun, ActivationState::DRYRUN},
                    {true, kActivationStateEnabled, ActivationState::ENABLED},
                    {true, "Enabled", ActivationState::ENABLED}};

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message("Enabled = ") << test_case.feature_enabled);
    SCOPED_TRACE(::testing::Message("ActivationStateParam = \"")
                 << test_case.activation_state_param << "\"");

    base::FieldTrialList field_trial_list(nullptr /* entropy_provider */);
    testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
        test_case.feature_enabled ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
                                  : base::FeatureList::OVERRIDE_USE_DEFAULT,
        test_case.activation_state_param, kActivationScopeNoSites);

    EXPECT_EQ(test_case.expected_activation_state, GetMaximumActivationState());
    EXPECT_EQ(ActivationScope::NO_SITES, GetCurrentActivationScope());
  }
}

TEST(SubresourceFilterFeaturesTest, ActivationScope) {
  const struct {
    bool feature_enabled;
    const char* activation_scope_param;
    ActivationScope expected_activation_scope;
  } kTestCases[] = {
      {false, "", ActivationScope::NO_SITES},
      {false, "no_sites", ActivationScope::NO_SITES},
      {false, "allsites", ActivationScope::NO_SITES},
      {false, "enabled", ActivationScope::NO_SITES},
      {false, "%$ garbage !%", ActivationScope::NO_SITES},
      {true, "", ActivationScope::NO_SITES},
      {true, "nosites", ActivationScope::NO_SITES},
      {true, "No_sites", ActivationScope::NO_SITES},
      {true, "no_sites", ActivationScope::NO_SITES},
      {true, "%$ garbage !%", ActivationScope::NO_SITES},
      {true, kActivationScopeAllSites, ActivationScope::ALL_SITES},
      {true, kActivationScopeActivationList, ActivationScope::ACTIVATION_LIST}};

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message("Enabled = ") << test_case.feature_enabled);
    SCOPED_TRACE(::testing::Message("ActivationScopeParam = \"")
                 << test_case.activation_scope_param << "\"");

    base::FieldTrialList field_trial_list(nullptr /* entropy_provider */);
    testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
        test_case.feature_enabled ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
                                  : base::FeatureList::OVERRIDE_USE_DEFAULT,
        kActivationStateDisabled, test_case.activation_scope_param);

    EXPECT_EQ(ActivationState::DISABLED, GetMaximumActivationState());
    EXPECT_EQ(test_case.expected_activation_scope, GetCurrentActivationScope());
  }
}

TEST(SubresourceFilterFeaturesTest, ActivationStateAndScope) {
  const struct {
    bool feature_enabled;
    const char* activation_state_param;
    ActivationState expected_activation_state;
    const char* activation_scope_param;
    ActivationScope expected_activation_scope;
  } kTestCases[] = {
      {false, kActivationStateDisabled, ActivationState::DISABLED,
       kActivationScopeNoSites, ActivationScope::NO_SITES},
      {true, kActivationStateDisabled, ActivationState::DISABLED,
       kActivationScopeNoSites, ActivationScope::NO_SITES},
      {true, kActivationStateDisabled, ActivationState::DISABLED,
       kActivationScopeAllSites, ActivationScope::ALL_SITES},
      {true, kActivationStateDisabled, ActivationState::DISABLED,
       kActivationScopeActivationList, ActivationScope::ACTIVATION_LIST},
      {true, kActivationStateDisabled, ActivationState::DISABLED,
       kActivationScopeAllSites, ActivationScope::ALL_SITES},
      {true, kActivationStateDryRun, ActivationState::DRYRUN,
       kActivationScopeNoSites, ActivationScope::NO_SITES},
      {true, kActivationStateDryRun, ActivationState::DRYRUN,
       kActivationScopeAllSites, ActivationScope::ALL_SITES},
      {true, kActivationStateDryRun, ActivationState::DRYRUN,
       kActivationScopeActivationList, ActivationScope::ACTIVATION_LIST},
      {true, kActivationStateDryRun, ActivationState::DRYRUN,
       kActivationScopeAllSites, ActivationScope::ALL_SITES},
      {true, kActivationStateEnabled, ActivationState::ENABLED,
       kActivationScopeNoSites, ActivationScope::NO_SITES},
      {true, kActivationStateEnabled, ActivationState::ENABLED,
       kActivationScopeAllSites, ActivationScope::ALL_SITES},
      {true, kActivationStateEnabled, ActivationState::ENABLED,
       kActivationScopeActivationList, ActivationScope::ACTIVATION_LIST},
      {true, kActivationStateEnabled, ActivationState::ENABLED,
       kActivationScopeAllSites, ActivationScope::ALL_SITES},
      {false, kActivationStateEnabled, ActivationState::DISABLED,
       kActivationScopeAllSites, ActivationScope::NO_SITES}};

  for (const auto& test_case : kTestCases) {
    base::FieldTrialList field_trial_list(nullptr /* entropy_provider */);
    testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
        test_case.feature_enabled ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
                                  : base::FeatureList::OVERRIDE_USE_DEFAULT,
        test_case.activation_state_param, test_case.activation_scope_param);

    EXPECT_EQ(test_case.expected_activation_scope, GetCurrentActivationScope());
    EXPECT_EQ(test_case.expected_activation_state, GetMaximumActivationState());
  }
}

TEST(SubresourceFilterFeaturesTest, ActivationList) {
  const std::string activation_soc_eng(
      kActivationListSocialEngineeringAdsInterstitial);
  const std::string activation_phishing(kActivationListPhishingInterstitial);
  const std::string socEngPhising = activation_soc_eng + activation_phishing;
  const std::string socEngCommaPhising =
      activation_soc_eng + "," + activation_phishing;
  const std::string phishingCommaSocEng =
      activation_phishing + "," + activation_soc_eng;
  const std::string socEngCommaPhisingCommaGarbage =
      socEngCommaPhising + "," + "Garbage";
  const struct {
    bool feature_enabled;
    const char* activation_list_param;
    ActivationList expected_activation_list;
  } kTestCases[] = {
      {false, "", ActivationList::NONE},
      {false, "social eng ads intertitial", ActivationList::NONE},
      {false, "phishing,interstital", ActivationList::NONE},
      {false, "%$ garbage !%", ActivationList::NONE},
      {true, "", ActivationList::NONE},
      {true, "social eng ads intertitial", ActivationList::NONE},
      {true, "phishing interstital", ActivationList::NONE},
      {true, "%$ garbage !%", ActivationList::NONE},
      {true, kActivationListSocialEngineeringAdsInterstitial,
       ActivationList::SOCIAL_ENG_ADS_INTERSTITIAL},
      {true, kActivationListPhishingInterstitial,
       ActivationList::PHISHING_INTERSTITIAL},
      {true, socEngPhising.c_str(), ActivationList::NONE},
      {true, socEngCommaPhising.c_str(), ActivationList::PHISHING_INTERSTITIAL},
      {true, phishingCommaSocEng.c_str(),
       ActivationList::PHISHING_INTERSTITIAL},
      {true, socEngCommaPhisingCommaGarbage.c_str(),
       ActivationList::PHISHING_INTERSTITIAL},
      {true, "List1, List2", ActivationList::NONE}};

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message("Enabled = ") << test_case.feature_enabled);
    SCOPED_TRACE(::testing::Message("ActivationListParam = \"")
                 << test_case.activation_list_param << "\"");

    base::FieldTrialList field_trial_list(nullptr /* entropy_provider */);
    testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
        test_case.feature_enabled ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
                                  : base::FeatureList::OVERRIDE_USE_DEFAULT,
        kActivationStateDisabled, kActivationScopeNoSites,
        test_case.activation_list_param);

    EXPECT_EQ(test_case.expected_activation_list, GetCurrentActivationList());
  }
}

}  // namespace subresource_filter
