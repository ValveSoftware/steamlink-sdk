// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentsValidators.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/WTFString.h"
#include <ostream> // NOLINT

namespace blink {
namespace {

struct TestCase {
    TestCase(const char* input, bool expectedValid)
        : input(input)
        , expectedValid(expectedValid)
    {
    }
    ~TestCase() {}

    const char* input;
    bool expectedValid;
};

std::ostream& operator<<(std::ostream& out, const TestCase& testCase)
{
    out << "'" << testCase.input << "' is expected to be " << (testCase.expectedValid ? "valid" : "invalid");
    return out;
}

class PaymentsCurrencyValidatorTest : public testing::TestWithParam<TestCase> {
};

TEST_P(PaymentsCurrencyValidatorTest, IsValidCurrencyCodeFormat)
{
    String errorMessage;
    EXPECT_EQ(GetParam().expectedValid, PaymentsValidators::isValidCurrencyCodeFormat(GetParam().input, &errorMessage)) << errorMessage;
    EXPECT_EQ(GetParam().expectedValid, errorMessage.isEmpty()) << errorMessage;

    EXPECT_EQ(GetParam().expectedValid, PaymentsValidators::isValidCurrencyCodeFormat(GetParam().input, nullptr));
}

INSTANTIATE_TEST_CASE_P(CurrencyCodes,
    PaymentsCurrencyValidatorTest,
    testing::Values(
        TestCase("USD", true),
        // Invalid currency code formats
        TestCase("US1", false),
        TestCase("US", false),
        TestCase("USDO", false),
        TestCase("usd", false),
        TestCase("", false)));

class PaymentsAmountValidatorTest : public testing::TestWithParam<TestCase> {
};

TEST_P(PaymentsAmountValidatorTest, IsValidAmountFormat)
{
    String errorMessage;
    EXPECT_EQ(GetParam().expectedValid, PaymentsValidators::isValidAmountFormat(GetParam().input, &errorMessage)) << errorMessage;
    EXPECT_EQ(GetParam().expectedValid, errorMessage.isEmpty()) << errorMessage;

    EXPECT_EQ(GetParam().expectedValid, PaymentsValidators::isValidAmountFormat(GetParam().input, nullptr));
}

INSTANTIATE_TEST_CASE_P(Amounts,
    PaymentsAmountValidatorTest,
    testing::Values(
        TestCase("0", true),
        TestCase("-0", true),
        TestCase("1", true),
        TestCase("10", true),
        TestCase("-3", true),
        TestCase("10.99", true),
        TestCase("-3.00", true),
        TestCase("01234567890123456789.0123456789", true),
        TestCase("01234567890123456789012345678.9", true),
        TestCase("012345678901234567890123456789", true),
        TestCase("-01234567890123456789.0123456789", true),
        TestCase("-01234567890123456789012345678.9", true),
        TestCase("-012345678901234567890123456789", true),
        // Invalid amount formats
        TestCase("", false),
        TestCase("-", false),
        TestCase("notdigits", false),
        TestCase("ALSONOTDIGITS", false),
        TestCase("10.", false),
        TestCase(".99", false),
        TestCase("-10.", false),
        TestCase("-.99", false),
        TestCase("10-", false),
        TestCase("1-0", false),
        TestCase("1.0.0", false),
        TestCase("1/3", false)));

class PaymentsRegionValidatorTest : public testing::TestWithParam<TestCase> {
};

TEST_P(PaymentsRegionValidatorTest, IsValidCountryCodeFormat)
{
    String errorMessage;
    EXPECT_EQ(GetParam().expectedValid, PaymentsValidators::isValidCountryCodeFormat(GetParam().input, &errorMessage)) << errorMessage;
    EXPECT_EQ(GetParam().expectedValid, errorMessage.isEmpty()) << errorMessage;

    EXPECT_EQ(GetParam().expectedValid, PaymentsValidators::isValidCountryCodeFormat(GetParam().input, nullptr));
}

INSTANTIATE_TEST_CASE_P(CountryCodes,
    PaymentsRegionValidatorTest,
    testing::Values(
        TestCase("US", true),
        // Invalid country code formats
        TestCase("U1", false),
        TestCase("U", false),
        TestCase("us", false),
        TestCase("USA", false),
        TestCase("", false)));

class PaymentsLanguageValidatorTest : public testing::TestWithParam<TestCase> {
};

TEST_P(PaymentsLanguageValidatorTest, IsValidLanguageCodeFormat)
{
    String errorMessage;
    EXPECT_EQ(GetParam().expectedValid, PaymentsValidators::isValidLanguageCodeFormat(GetParam().input, &errorMessage)) << errorMessage;
    EXPECT_EQ(GetParam().expectedValid, errorMessage.isEmpty()) << errorMessage;

    EXPECT_EQ(GetParam().expectedValid, PaymentsValidators::isValidLanguageCodeFormat(GetParam().input, nullptr));
}

INSTANTIATE_TEST_CASE_P(LanguageCodes,
    PaymentsLanguageValidatorTest,
    testing::Values(
        TestCase("", true),
        TestCase("en", true),
        TestCase("eng", true),
        // Invalid language code formats
        TestCase("e1", false),
        TestCase("en1", false),
        TestCase("e", false),
        TestCase("engl", false),
        TestCase("EN", false)));

class PaymentsScriptValidatorTest : public testing::TestWithParam<TestCase> {
};

TEST_P(PaymentsScriptValidatorTest, IsValidScriptCodeFormat)
{
    String errorMessage;
    EXPECT_EQ(GetParam().expectedValid, PaymentsValidators::isValidScriptCodeFormat(GetParam().input, &errorMessage)) << errorMessage;
    EXPECT_EQ(GetParam().expectedValid, errorMessage.isEmpty()) << errorMessage;

    EXPECT_EQ(GetParam().expectedValid, PaymentsValidators::isValidScriptCodeFormat(GetParam().input, nullptr));
}

INSTANTIATE_TEST_CASE_P(ScriptCodes,
    PaymentsScriptValidatorTest,
    testing::Values(
        TestCase("", true),
        TestCase("Latn", true),
        // Invalid script code formats
        TestCase("Lat1", false),
        TestCase("1lat", false),
        TestCase("Latin", false),
        TestCase("Lat", false),
        TestCase("latn", false),
        TestCase("LATN", false)));

struct ShippingAddressTestCase {
    ShippingAddressTestCase(const char* countryCode, const char* languageCode, const char* scriptCode, bool expectedValid)
        : countryCode(countryCode)
        , languageCode(languageCode)
        , scriptCode(scriptCode)
        , expectedValid(expectedValid)
    {
    }
    ~ShippingAddressTestCase() {}

    const char* countryCode;
    const char* languageCode;
    const char* scriptCode;
    bool expectedValid;
};

class PaymentsShippingAddressValidatorTest : public testing::TestWithParam<ShippingAddressTestCase> {
};

TEST_P(PaymentsShippingAddressValidatorTest, IsValidShippingAddress)
{
    mojom::blink::PaymentAddressPtr address = mojom::blink::PaymentAddress::New();
    address->country = GetParam().countryCode;
    address->language_code = GetParam().languageCode;
    address->script_code = GetParam().scriptCode;

    String errorMessage;
    EXPECT_EQ(GetParam().expectedValid, PaymentsValidators::isValidShippingAddress(address, &errorMessage)) << errorMessage;
    EXPECT_EQ(GetParam().expectedValid, errorMessage.isEmpty()) << errorMessage;

    EXPECT_EQ(GetParam().expectedValid, PaymentsValidators::isValidShippingAddress(address, nullptr));
}

INSTANTIATE_TEST_CASE_P(ShippingAddresses,
    PaymentsShippingAddressValidatorTest,
    testing::Values(
        ShippingAddressTestCase("US", "en", "Latn", true),
        ShippingAddressTestCase("US", "en", "", true),
        ShippingAddressTestCase("US", "", "", true),
        // Invalid shipping addresses
        ShippingAddressTestCase("", "", "", false),
        ShippingAddressTestCase("InvalidCountryCode", "", "", false),
        ShippingAddressTestCase("US", "InvalidLanguageCode", "", false),
        ShippingAddressTestCase("US", "en", "InvalidScriptCode", false),
        ShippingAddressTestCase("US", "", "Latn", false)));

} // namespace
} // namespace blink
