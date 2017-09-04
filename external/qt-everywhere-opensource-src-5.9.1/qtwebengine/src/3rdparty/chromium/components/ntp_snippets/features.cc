// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/features.h"

#include "base/strings/string_number_conversions.h"
#include "components/variations/variations_associated_data.h"

namespace ntp_snippets {

const base::Feature kArticleSuggestionsFeature{
    "NTPArticleSuggestions", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kBookmarkSuggestionsFeature{
    "NTPBookmarkSuggestions", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kRecentOfflineTabSuggestionsFeature{
    "NTPOfflinePageSuggestions", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSaveToOfflineFeature{
    "NTPSaveToOffline", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kOfflineBadgeFeature{
    "NTPOfflineBadge", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kIncreasedVisibility{
    "NTPSnippetsIncreasedVisibility", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kPhysicalWebPageSuggestionsFeature{
    "NTPPhysicalWebPageSuggestions", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kContentSuggestionsFeature{
    "NTPSnippets", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSectionDismissalFeature{
    "NTPSuggestionsSectionDismissal", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kForeignSessionsSuggestionsFeature{
    "NTPForeignSessionsSuggestions", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kFetchMoreFeature{"NTPSuggestionsFetchMore",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

int GetParamAsInt(const base::Feature& feature,
                  const std::string& param_name,
                  const int default_value) {
  std::string value_as_string =
      variations::GetVariationParamValueByFeature(feature, param_name);
  int value_as_int = 0;
  if (!base::StringToInt(value_as_string, &value_as_int)) {
    if (!value_as_string.empty()) {
      LOG(WARNING) << "Failed to parse variation param " << param_name
                   << " with string value " << value_as_string
                   << " under feature " << feature.name
                   << " into an int. Falling back to default value of "
                   << default_value;
    }
    value_as_int = default_value;
  }
  return value_as_int;
}


bool GetParamAsBool(const base::Feature& feature,
                    const std::string& param_name,
                    bool default_value) {
  std::string value_as_string =
      variations::GetVariationParamValueByFeature(feature, param_name);
  if (value_as_string == "true")
    return true;
  if (value_as_string == "false")
    return false;

  if (!value_as_string.empty()) {
    LOG(WARNING) << "Failed to parse variation param " << param_name
                 << " with string value " << value_as_string
                 << " under feature " << feature.name
                 << " into a bool. Falling back to default value of "
                 << default_value;
  }
  return default_value;
}

}  // namespace ntp_snippets
