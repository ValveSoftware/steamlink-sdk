// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DATA_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DATA_UTIL_H_

#include "base/strings/string16.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "grit/components_scaled_resources.h"

namespace autofill {
namespace data_util {

struct NameParts {
  base::string16 given;
  base::string16 middle;
  base::string16 family;
};

// Used to map Chrome card types to Payment Request API basic card payment spec
// types and icons. https://w3c.github.io/webpayments-methods-card/#method-id
struct PaymentRequestData {
  const char* card_type;
  const char* basic_card_payment_type;
  const int icon_resource_id;
};

// Returns true if |name| looks like a CJK name (or some kind of mish-mash of
// the three, at least).
bool IsCJKName(const base::string16& name);

// TODO(crbug.com/586510): Investigate the use of app_locale to do better name
// splitting.
// Returns the different name parts (given, middle and family names) of the full
// |name| passed as a parameter.
NameParts SplitName(const base::string16& name);

// Concatenates the name parts together in the correct order (based on script),
// and returns the result.
base::string16 JoinNameParts(const base::string16& given,
                             const base::string16& middle,
                             const base::string16& family);

// Returns true iff |full_name| is a concatenation of some combination of the
// first/middle/last (incl. middle initial) in |profile|.
bool ProfileMatchesFullName(const base::string16 full_name,
                            const autofill::AutofillProfile& profile);

// Returns the Payment Request API basic card payment spec data for the provided
// autofill credit card |type|.  Will set the type and the icon to "generic" for
// any unrecognized type.
const PaymentRequestData& GetPaymentRequestData(const std::string& type);

// Returns the autofill credit card type string for the provided Payment Request
// API basic card payment spec |type|.
const char* GetCardTypeForBasicCardPaymentType(const std::string& type);

}  // namespace data_util
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_DATA_UTIL_H_
