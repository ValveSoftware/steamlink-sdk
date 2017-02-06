// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_util.h"

#include <vector>

#include "components/crash/core/common/crash_keys.h"
#include "components/variations/active_field_trials.h"

namespace variations {

void SetVariationListCrashKeys() {
  std::vector<std::string> experiment_strings;
  GetFieldTrialActiveGroupIdsAsStrings(&experiment_strings);
  crash_keys::SetVariationsList(experiment_strings);
}

}  // namespace variations
