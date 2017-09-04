// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/experience_sampling_private/experience_sampling_private_api.h"

#include "base/metrics/field_trial.h"
#include "chrome/common/extensions/api/experience_sampling_private.h"

namespace sampling = extensions::api::experience_sampling_private;

namespace extensions {

bool ExperienceSamplingPrivateGetBrowserInfoFunction::RunAsync() {
  std::string field_trials;
  sampling::BrowserInfo info;

  base::FieldTrialList::StatesToString(&field_trials);
  info.variations = field_trials;

  SetResult(info.ToValue());
  SendResponse(true /* success */);
  return true;
}

}  // namespace extensions
