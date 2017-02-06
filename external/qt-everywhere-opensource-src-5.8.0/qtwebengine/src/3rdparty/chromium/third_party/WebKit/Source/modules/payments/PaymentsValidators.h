// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaymentsValidators_h
#define PaymentsValidators_h

#include "modules/ModulesExport.h"
#include "public/platform/modules/payments/payment_request.mojom-blink.h"
#include "wtf/Allocator.h"
#include "wtf/text/WTFString.h"

namespace blink {

class MODULES_EXPORT PaymentsValidators final {
    STATIC_ONLY(PaymentsValidators);

public:
    // Returns true if |code| is a valid ISO 4217 currency code.
    static bool isValidCurrencyCodeFormat(const String& code, String* optionalErrorMessage);

    // Returns true if |amount| is a valid currency code as defined in ISO 20022 CurrencyAnd30Amount.
    static bool isValidAmountFormat(const String& amount, String* optionalErrorMessage);

    // Returns true if |code| is a valid ISO 3166 country code.
    static bool isValidCountryCodeFormat(const String& code, String* optionalErrorMessage);

    // Returns true if |code| is a valid ISO 639 language code.
    static bool isValidLanguageCodeFormat(const String& code, String* optionalErrorMessage);

    // Returns true if |code| is a valid ISO 15924 script code.
    static bool isValidScriptCodeFormat(const String& code, String* optionalErrorMessage);

    // Returns true if the payment address is valid:
    //  - Has a valid region code
    //  - Has a valid language code, if any.
    //  - Has a valid script code, if any.
    // A script code should be present only if language code is present.
    static bool isValidShippingAddress(const mojom::blink::PaymentAddressPtr&, String* optionalErrorMessage);
};

} // namespace blink

#endif // PaymentsValidators_h
