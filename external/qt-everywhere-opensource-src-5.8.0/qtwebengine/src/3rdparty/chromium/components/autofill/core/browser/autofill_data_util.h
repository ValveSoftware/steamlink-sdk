// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DATA_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DATA_UTIL_H_

#include "base/strings/string16.h"
#include "components/autofill/core/browser/autofill_profile.h"

namespace autofill {
namespace data_util {

struct NameParts {
  base::string16 given;
  base::string16 middle;
  base::string16 family;
};

// TODO(crbug.com/586510): Investigate the use of app_locale to do better name
// splitting.
// Returns the different name parts (given, middle and family names) of the full
// |name| passed as a parameter.
NameParts SplitName(const base::string16& name);

// Returns true iff |full_name| is a concatenation of some combination of the
// first/middle/last (incl. middle initial) in |profile|.
bool ProfileMatchesFullName(const base::string16 full_name,
                            const autofill::AutofillProfile& profile);

}  // namespace data_util
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DATA_UTIL_H_
