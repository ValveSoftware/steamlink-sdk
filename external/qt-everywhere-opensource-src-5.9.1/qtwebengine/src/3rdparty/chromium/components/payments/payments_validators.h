// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_PAYMENTS_VALIDATORS_H_
#define COMPONENTS_PAYMENTS_PAYMENTS_VALIDATORS_H_

#include <string>

#include "components/payments/payment_request.mojom.h"

namespace payments {

class PaymentsValidators {
 public:
  // The most common identifiers are three-letter alphabetic codes as
  // defined by [ISO4217] (for example, "USD" for US Dollars).  |system| is
  // a URL that indicates the currency system that the currency identifier
  // belongs to.  By default, the value is urn:iso:std:iso:4217 indicating
  // that currency is defined by [[ISO4217]], however any string of at most
  // 2048 characters is considered valid in other currencySystem.  Returns
  // false if currency |code| is too long (greater than 2048).
  static bool isValidCurrencyCodeFormat(const std::string& code,
                                        const std::string& system,
                                        std::string* optionalErrorMessage);

  // Returns true if |amount| is a valid currency code as defined in ISO 20022
  // CurrencyAnd30Amount.
  static bool isValidAmountFormat(const std::string& amount,
                                  std::string* optionalErrorMessage);

  // Returns true if |code| is a valid ISO 3166 country code.
  static bool isValidCountryCodeFormat(const std::string& code,
                                       std::string* optionalErrorMessage);

  // Returns true if |code| is a valid ISO 639 language code.
  static bool isValidLanguageCodeFormat(const std::string& code,
                                        std::string* optionalErrorMessage);

  // Returns true if |code| is a valid ISO 15924 script code.
  static bool isValidScriptCodeFormat(const std::string& code,
                                      std::string* optionalErrorMessage);

  // Returns true if the payment address is valid:
  //  - Has a valid region code
  //  - Has a valid language code, if any.
  //  - Has a valid script code, if any.
  // A script code should be present only if language code is present.
  static bool isValidShippingAddress(const mojom::PaymentAddressPtr&,
                                     std::string* optionalErrorMessage);

  // Returns false if |error| is too long (greater than 2048).
  static bool isValidErrorMsgFormat(const std::string& code,
                                    std::string* optionalErrorMessage);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_PAYMENTS_VALIDATORS_H_
