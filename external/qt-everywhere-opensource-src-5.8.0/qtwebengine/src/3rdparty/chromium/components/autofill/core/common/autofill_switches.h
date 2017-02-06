// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_SWITCHES_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_SWITCHES_H_

#include "build/build_config.h"

namespace autofill {
namespace switches {

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.
extern const char kDisableCreditCardScan[];
extern const char kDisableFullFormAutofillIOS[];
extern const char kDisableOfferStoreUnmaskedWalletCards[];
extern const char kDisableOfferUploadCreditCards[];
extern const char kDisablePasswordGeneration[];
extern const char kDisableSingleClickAutofill[];
extern const char kEnableCreditCardScan[];
extern const char kEnableFullFormAutofillIOS[];
extern const char kEnableOfferStoreUnmaskedWalletCards[];
extern const char kEnableOfferUploadCreditCards[];
extern const char kEnablePasswordGeneration[];
extern const char kEnableSingleClickAutofill[];
extern const char kEnableSuggestionsWithSubstringMatch[];
extern const char kIgnoreAutocompleteOffForAutofill[];
extern const char kLocalHeuristicsOnlyForPasswordGeneration[];
extern const char kShowAutofillTypePredictions[];
extern const char kWalletServiceUseSandbox[];

#if defined(OS_ANDROID)
extern const char kDisableAccessorySuggestionView[];
extern const char kEnableAccessorySuggestionView[];
#endif  // defined(OS_ANDROID)

}  // namespace switches
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_SWITCHES_H_
