// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/browser/subresource_filter_features.h"

#include <string>

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/variations/variations_associated_data.h"

namespace subresource_filter {

const base::Feature kSafeBrowsingSubresourceFilter{
    "SubresourceFilter", base::FEATURE_DISABLED_BY_DEFAULT};

const char kActivationStateParameterName[] = "activation_state";
const char kActivationStateDryRun[] = "dryrun";
const char kActivationStateEnabled[] = "enabled";
const char kActivationStateDisabled[] = "disabled";

const char kActivationScopeParameterName[] = "activation_scope";
const char kActivationScopeAllSites[] = "all_sites";
const char kActivationScopeActivationList[] = "activation_list";
const char kActivationScopeNoSites[] = "no_sites";

const char kActivationListsParameterName[] = "activation_lists";
const char kActivationListSocialEngineeringAdsInterstitial[] =
    "social_engineering_ads_interstitial";
const char kActivationListPhishingInterstitial[] = "phishing_interstitial";

ActivationState GetMaximumActivationState() {
  std::string activation_state = variations::GetVariationParamValueByFeature(
      kSafeBrowsingSubresourceFilter, kActivationStateParameterName);
  if (base::LowerCaseEqualsASCII(activation_state, kActivationStateEnabled))
    return ActivationState::ENABLED;
  else if (base::LowerCaseEqualsASCII(activation_state, kActivationStateDryRun))
    return ActivationState::DRYRUN;
  return ActivationState::DISABLED;
}

ActivationScope GetCurrentActivationScope() {
  std::string activation_scope = variations::GetVariationParamValueByFeature(
      kSafeBrowsingSubresourceFilter, kActivationScopeParameterName);
  if (base::LowerCaseEqualsASCII(activation_scope, kActivationScopeAllSites))
    return ActivationScope::ALL_SITES;
  else if (base::LowerCaseEqualsASCII(activation_scope,
                                      kActivationScopeActivationList))
    return ActivationScope::ACTIVATION_LIST;
  return ActivationScope::NO_SITES;
}

ActivationList GetCurrentActivationList() {
  std::string activation_lists = variations::GetVariationParamValueByFeature(
      kSafeBrowsingSubresourceFilter, kActivationListsParameterName);
  ActivationList activation_list_type = ActivationList::NONE;
  for (const base::StringPiece& activation_list :
       base::SplitStringPiece(activation_lists, ",", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    if (base::LowerCaseEqualsASCII(activation_list,
                                   kActivationListPhishingInterstitial)) {
      return ActivationList::PHISHING_INTERSTITIAL;
    } else if (base::LowerCaseEqualsASCII(
                   activation_list,
                   kActivationListSocialEngineeringAdsInterstitial)) {
      activation_list_type = ActivationList::SOCIAL_ENG_ADS_INTERSTITIAL;
    }
  }
  return activation_list_type;
}

}  // namespace subresource_filter
