// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_VALIDATION_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_VALIDATION_H_

#include "base/strings/string16.h"

namespace base {
class Time;
}  // namespace base

namespace autofill {

// Returns true if |year| and |month| describe a date later than |now|.
// |year| must have 4 digits.
bool IsValidCreditCardExpirationDate(int year,
                                     int month,
                                     const base::Time& now);

// Returns true if |text| looks like a valid credit card number.
// Uses the Luhn formula to validate the number.
bool IsValidCreditCardNumber(const base::string16& text);

// Returns true if |text| looks like a valid credit card security code.
bool IsValidCreditCardSecurityCode(const base::string16& text);

// Returns true if |code| looks like a valid credit card security code
// for the type of credit card designated by |number|.
bool IsValidCreditCardSecurityCode(const base::string16& code,
                                   const base::string16& number);

// Returns true if |text| looks like a valid e-mail address.
bool IsValidEmailAddress(const base::string16& text);

// Returns true if |text| is a valid US state name or abbreviation.  It is
// case insensitive.  Valid for US states only.
bool IsValidState(const base::string16& text);

// Returns true if |text| looks like a valid zip code.
// Valid for US zip codes only.
bool IsValidZip(const base::string16& text);

// Returns true if |text| looks like an SSN, with or without separators.
bool IsSSN(const base::string16& text);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_VALIDATION_H_
