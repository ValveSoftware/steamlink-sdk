// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_field_trial_ios.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "components/autofill/core/common/autofill_switches.h"

namespace autofill {

// The full-form Autofill field trial name.
const char kFullFormFieldTrialName[] = "FullFormAutofill";

// static
bool AutofillFieldTrialIOS::IsFullFormAutofillEnabled() {
  // Query the field trial state first to ensure that UMA reports the correct
  // group.
  std::string field_trial_state =
      base::FieldTrialList::FindFullName(kFullFormFieldTrialName);

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableFullFormAutofillIOS))
    return false;
  if (command_line->HasSwitch(switches::kEnableFullFormAutofillIOS))
    return true;

  return !field_trial_state.empty() && field_trial_state != "Disabled";
}

}  // namespace autofill
