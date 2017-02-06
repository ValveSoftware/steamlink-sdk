// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "components/autofill/core/common/autofill_switches.h"

namespace autofill {
namespace switches {

// Disables using device's camera to scan a new credit card when filling out a
// credit card form.
const char kDisableCreditCardScan[]         = "disable-credit-card-scan";

// Disables the experimental Full Form Autofill on iOS feature.
const char kDisableFullFormAutofillIOS[]    = "disable-full-form-autofill-ios";

// Force hiding the local save checkbox in the autofill dialog box for getting
// the full credit card number for a wallet card. The card will never be stored
// locally.
const char kDisableOfferStoreUnmaskedWalletCards[] =
    "disable-offer-store-unmasked-wallet-cards";

// Disables offering to upload credit cards.
const char kDisableOfferUploadCreditCards[] =
    "disable-offer-upload-credit-cards";

// Disables password generation when we detect that the user is going through
// account creation.
const char kDisablePasswordGeneration[]     = "disable-password-generation";

// The "disable" flag for kEnableSingleClickAutofill.
const char kDisableSingleClickAutofill[]    = "disable-single-click-autofill";

// Enables using device's camera to scan a new credit card when filling out a
// credit card form.
const char kEnableCreditCardScan[]          = "enable-credit-card-scan";

// Enables the experimental Full Form Autofill on iOS feature.
const char kEnableFullFormAutofillIOS[]     = "enable-full-form-autofill-ios";

// Force showing the local save checkbox in the autofill dialog box for getting
// the full credit card number for a wallet card.
const char kEnableOfferStoreUnmaskedWalletCards[] =
    "enable-offer-store-unmasked-wallet-cards";

// Enables offering to upload credit cards.
const char kEnableOfferUploadCreditCards[] = "enable-offer-upload-credit-cards";

// Enables password generation when we detect that the user is going through
// account creation.
const char kEnablePasswordGeneration[]      = "enable-password-generation";

// Enables/disables suggestions without typing anything (on first click).
const char kEnableSingleClickAutofill[]     = "enable-single-click-autofill";

// Enables suggestions with substring matching instead of prefix matching.
const char kEnableSuggestionsWithSubstringMatch[] =
    "enable-suggestions-with-substring-match";

// Ignores autocomplete="off" for Autofill data (profiles + credit cards).
const char kIgnoreAutocompleteOffForAutofill[] =
    "ignore-autocomplete-off-autofill";

// Removes the requirement that we recieved a ping from the autofill servers
// and that the user doesn't have the given form blacklisted. Used in testing.
const char kLocalHeuristicsOnlyForPasswordGeneration[] =
    "local-heuristics-only-for-password-generation";

// Annotates forms with Autofill field type predictions.
const char kShowAutofillTypePredictions[]   = "show-autofill-type-predictions";

// Use the sandbox Online Wallet service URL (for developer testing).
const char kWalletServiceUseSandbox[]       = "wallet-service-use-sandbox";

#if defined(OS_ANDROID)
// Disables showing suggestions in a keyboard accessory view.
const char kDisableAccessorySuggestionView[] =
    "disable-autofill-keyboard-accessory-view";

// Enables showing suggestions in a keyboard accessory view.
const char kEnableAccessorySuggestionView[] =
    "enable-autofill-keyboard-accessory-view";
#endif  // defined(OS_ANDROID)

}  // namespace switches
}  // namespace autofill
