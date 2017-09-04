// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentsValidators.h"

#include "bindings/core/v8/ScriptRegexp.h"
#include "platform/weborigin/KURL.h"
#include "wtf/text/StringImpl.h"

namespace blink {

// We limit the maximum length of string to 2048 bytes for security reasons.
static const int maxiumStringLength = 2048;

bool PaymentsValidators::isValidCurrencyCodeFormat(
    const String& code,
    const String& system,
    String* optionalErrorMessage) {
  if (system == "urn:iso:std:iso:4217") {
    if (ScriptRegexp("^[A-Z]{3}$", TextCaseSensitive).match(code) == 0)
      return true;

    if (optionalErrorMessage)
      *optionalErrorMessage = "'" + code +
                              "' is not a valid ISO 4217 currency code, should "
                              "be 3 upper case letters [A-Z]";

    return false;
  }

  if (!KURL(KURL(), system).isValid()) {
    if (optionalErrorMessage)
      *optionalErrorMessage = "The currency system is not a valid URL";

    return false;
  }

  if (code.length() <= maxiumStringLength)
    return true;

  if (optionalErrorMessage)
    *optionalErrorMessage =
        "The currency code should be at most 2048 characters long";

  return false;
}

bool PaymentsValidators::isValidAmountFormat(const String& amount,
                                             String* optionalErrorMessage) {
  if (ScriptRegexp("^-?[0-9]+(\\.[0-9]+)?$", TextCaseSensitive).match(amount) ==
      0)
    return true;

  if (optionalErrorMessage)
    *optionalErrorMessage = "'" + amount + "' is not a valid amount format";

  return false;
}

bool PaymentsValidators::isValidCountryCodeFormat(
    const String& code,
    String* optionalErrorMessage) {
  if (ScriptRegexp("^[A-Z]{2}$", TextCaseSensitive).match(code) == 0)
    return true;

  if (optionalErrorMessage)
    *optionalErrorMessage = "'" + code +
                            "' is not a valid CLDR country code, should be 2 "
                            "upper case letters [A-Z]";

  return false;
}

bool PaymentsValidators::isValidLanguageCodeFormat(
    const String& code,
    String* optionalErrorMessage) {
  if (ScriptRegexp("^([a-z]{2,3})?$", TextCaseSensitive).match(code) == 0)
    return true;

  if (optionalErrorMessage)
    *optionalErrorMessage = "'" + code +
                            "' is not a valid BCP-47 language code, should be "
                            "2-3 lower case letters [a-z]";

  return false;
}

bool PaymentsValidators::isValidScriptCodeFormat(const String& code,
                                                 String* optionalErrorMessage) {
  if (ScriptRegexp("^([A-Z][a-z]{3})?$", TextCaseSensitive).match(code) == 0)
    return true;

  if (optionalErrorMessage)
    *optionalErrorMessage = "'" + code +
                            "' is not a valid ISO 15924 script code, should be "
                            "an upper case letter [A-Z] followed by 3 lower "
                            "case letters [a-z]";

  return false;
}

bool PaymentsValidators::isValidShippingAddress(
    const payments::mojom::blink::PaymentAddressPtr& address,
    String* optionalErrorMessage) {
  if (!isValidCountryCodeFormat(address->country, optionalErrorMessage))
    return false;

  if (!isValidLanguageCodeFormat(address->language_code, optionalErrorMessage))
    return false;

  if (!isValidScriptCodeFormat(address->script_code, optionalErrorMessage))
    return false;

  if (address->language_code.isEmpty() && !address->script_code.isEmpty()) {
    if (optionalErrorMessage)
      *optionalErrorMessage =
          "If language code is empty, then script code should also be empty";

    return false;
  }

  return true;
}

bool PaymentsValidators::isValidErrorMsgFormat(const String& error,
                                               String* optionalErrorMessage) {
  if (error.length() <= maxiumStringLength)
    return true;

  if (optionalErrorMessage)
    *optionalErrorMessage =
        "Error message should be at most 2048 characters long";

  return false;
}

}  // namespace blink
