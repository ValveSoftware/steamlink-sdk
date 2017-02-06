// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_FEATURES_H_
#define COMPONENTS_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_FEATURES_H_

#include "base/feature_list.h"
#include "components/subresource_filter/core/common/activation_state.h"

namespace subresource_filter {

// The master toggle to enable/disable the Safe Browsing Subresource Filter.
extern const base::Feature kSafeBrowsingSubresourceFilter;

// Name/values of the variation parameter controlling maximum activation state.
extern const char kActivationStateParameterName[];
extern const char kActivationStateDryRun[];
extern const char kActivationStateEnabled[];
extern const char kActivationStateDisabled[];

// Returns the maximum degree to which subresource filtering should be activated
// on any RenderFrame. This will be ActivationState::DISABLED unless the feature
// is enabled and variation parameters prescribe a higher activation state.
ActivationState GetMaximumActivationState();

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_FEATURES_H_
