// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains constants specific to the Autofill component.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_CONSTANTS_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_CONSTANTS_H_

#include <stddef.h>         // For size_t

namespace autofill {

// Help URL for the Autofill dialog.
extern const char kHelpURL[];

// The origin of an AutofillDataModel created or modified in the settings page.
extern const char kSettingsOrigin[];

// The number of fields required by Autofill to execute its heuristic and
// crowdsourcing prediction routines. Ideally we would execute those routines no
// matter how many fields are in the forms; however, finding the label for each
// field is a costly operation and we can't spare the cycles if it's not
// necessary.
const size_t kRequiredFieldsForPredictionRoutines = 3;

// The minimum number of fields required to upload a form to the Autofill
// servers.
const size_t kRequiredFieldsForUpload = 3;

// The minimum number of fields in a form that contains only password fields to
// upload the form to and request predictions from the Autofill servers.
const size_t kRequiredFieldsForFormsWithOnlyPasswordFields = 2;

// Options bitmask values for AutofillHostMsg_ShowPasswordSuggestions IPC
enum ShowPasswordSuggestionsOptions {
  SHOW_ALL = 1 << 0 /* show all credentials, not just ones matching username */,
  IS_PASSWORD_FIELD = 1 << 1 /* input field is a password field */
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_CONSTANTS_H_
