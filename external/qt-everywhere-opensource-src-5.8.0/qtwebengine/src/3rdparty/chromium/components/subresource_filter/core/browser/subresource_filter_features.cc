// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/browser/subresource_filter_features.h"

#include <string>

#include "base/strings/string_util.h"
#include "components/variations/variations_associated_data.h"

namespace subresource_filter {

const base::Feature kSafeBrowsingSubresourceFilter{
    "SubresourceFilter", base::FEATURE_DISABLED_BY_DEFAULT};

const char kActivationStateParameterName[] = "activation_state";
const char kActivationStateDryRun[] = "dryrun";
const char kActivationStateEnabled[] = "enabled";
const char kActivationStateDisabled[] = "disabled";

ActivationState GetMaximumActivationState() {
  std::string activation_state = variations::GetVariationParamValueByFeature(
      kSafeBrowsingSubresourceFilter, kActivationStateParameterName);
  if (base::LowerCaseEqualsASCII(activation_state, kActivationStateEnabled))
    return ActivationState::ENABLED;
  else if (base::LowerCaseEqualsASCII(activation_state, kActivationStateDryRun))
    return ActivationState::DRYRUN;
  return ActivationState::DISABLED;
}

}  // namespace subresource_filter
