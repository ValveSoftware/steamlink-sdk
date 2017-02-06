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
        test_case.activation_state_param);

    EXPECT_EQ(test_case.expected_activation_state, GetMaximumActivationState());
  }
}

}  // namespace subresource_filter
