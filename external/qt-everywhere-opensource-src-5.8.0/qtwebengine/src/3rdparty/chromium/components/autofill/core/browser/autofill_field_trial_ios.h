// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_FIELD_TRIAL_IOS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_FIELD_TRIAL_IOS_H_

#include "base/macros.h"

namespace autofill {

// This class manages the iOS Autofill field trials.
class AutofillFieldTrialIOS {
 public:
  // Returns whether the user is in a full-form Autofill field trial. Full-form
  // Autofill fills all fields of the form at once, similar to the desktop and
  // Clank Autofill implementations. Previous iOS implementation requires user
  // to select an Autofill value for each field individually, automatically
  // advancing focus to the next field after each selection.
  static bool IsFullFormAutofillEnabled();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(AutofillFieldTrialIOS);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_FIELD_TRIAL_IOS_H_
