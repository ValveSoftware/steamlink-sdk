// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_SUBRESOURCE_FILTER_FEATURES_TEST_SUPPORT_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_SUBRESOURCE_FILTER_FEATURES_TEST_SUPPORT_H_

#include <string>

#include "base/feature_list.h"
#include "base/macros.h"

namespace subresource_filter {
namespace testing {

// Helper to override the state of the |kSafeBrowsingSubresourceFilter| feature
// and the maximum activation state during tests. Expects a pre-existing global
// base::FieldTrialList singleton.
class ScopedSubresourceFilterFeatureToggle {
 public:
  ScopedSubresourceFilterFeatureToggle(
      base::FeatureList::OverrideState feature_state,
      const std::string& maximum_activation_state);
  ~ScopedSubresourceFilterFeatureToggle();

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedSubresourceFilterFeatureToggle);
};

}  // namespace testing
}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_SUBRESOURCE_FILTER_FEATURES_TEST_SUPPORT_H_
