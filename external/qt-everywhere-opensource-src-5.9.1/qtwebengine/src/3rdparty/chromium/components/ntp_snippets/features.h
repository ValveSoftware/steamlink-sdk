// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_FEATURES_H_
#define COMPONENTS_NTP_SNIPPETS_FEATURES_H_

#include <string>

#include "base/feature_list.h"

namespace ntp_snippets {

// Features to turn individual providers/categories on/off.
extern const base::Feature kArticleSuggestionsFeature;
extern const base::Feature kBookmarkSuggestionsFeature;
extern const base::Feature kRecentOfflineTabSuggestionsFeature;
extern const base::Feature kPhysicalWebPageSuggestionsFeature;
extern const base::Feature kForeignSessionsSuggestionsFeature;;

// Feature to allow the 'save to offline' option to appear in the snippets
// context menu.
extern const base::Feature kSaveToOfflineFeature;

// Feature to allow offline badges to appear on snippets.
extern const base::Feature kOfflineBadgeFeature;

// Feature to allow dismissing sections.
extern const base::Feature kSectionDismissalFeature;

// Global toggle for the whole content suggestions feature. If this is set to
// false, all the per-provider features are ignored.
extern const base::Feature kContentSuggestionsFeature;

// Feature to allow UI as specified here: https://crbug.com/660837.
extern const base::Feature kIncreasedVisibility;

// Feature to enable the Fetch More action
extern const base::Feature kFetchMoreFeature;

// Returns a feature param as an int instead of a string.
int GetParamAsInt(const base::Feature& feature,
                  const std::string& param_name,
                  int default_value);

// Returns a feature param as a bool instead of a string.
// TODO(jkrcal): Use this function in other code in the ntp_snippets component.
bool GetParamAsBool(const base::Feature& feature,
                    const std::string& param_name,
                    bool default_value);

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_FEATURES_H_
