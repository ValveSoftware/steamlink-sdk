// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"

#include <map>
#include <memory>

#include "base/metrics/field_trial.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/variations/variations_associated_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {
namespace testing {

namespace {
const char kTestFieldTrialName[] = "FieldTrialNameShouldNotMatter";
const char kTestExperimentGroupName[] = "GroupNameShouldNotMatter";
}  // namespace

ScopedSubresourceFilterFeatureToggle::ScopedSubresourceFilterFeatureToggle(
    base::FeatureList::OverrideState feature_state,
    const std::string& maximum_activation_state) {
  base::FeatureList::ClearInstanceForTesting();
  variations::testing::ClearAllVariationParams();

  std::map<std::string, std::string> variation_params;
  variation_params[kActivationStateParameterName] = maximum_activation_state;
  EXPECT_TRUE(variations::AssociateVariationParams(
      kTestFieldTrialName, kTestExperimentGroupName, variation_params));

  base::FieldTrial* field_trial = base::FieldTrialList::CreateFieldTrial(
      kTestFieldTrialName, kTestExperimentGroupName);

  std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
  feature_list->RegisterFieldTrialOverride(kSafeBrowsingSubresourceFilter.name,
                                           feature_state, field_trial);
  base::FeatureList::SetInstance(std::move(feature_list));
}

ScopedSubresourceFilterFeatureToggle::~ScopedSubresourceFilterFeatureToggle() {
  variations::testing::ClearAllVariationParams();
  base::FeatureList::ClearInstanceForTesting();
}

}  // namespace testing
}  // namespace subresource_filter
