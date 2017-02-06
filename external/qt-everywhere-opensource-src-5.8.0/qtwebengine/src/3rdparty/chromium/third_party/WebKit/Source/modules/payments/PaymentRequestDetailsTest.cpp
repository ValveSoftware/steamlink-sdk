// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentRequest.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "modules/payments/PaymentDetails.h"
#include "modules/payments/PaymentTestHelper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <ostream> // NOLINT

namespace blink {
namespace {

class DetailsTestCase {
public:
    DetailsTestCase(PaymentTestDetailToChange detail, PaymentTestDataToChange data, PaymentTestModificationType modType, const char* valueToUse, bool expectException = false, ExceptionCode expectedExceptionCode = 0)
        : m_detail(detail)
        , m_data(data)
        , m_modType(modType)
        , m_valueToUse(valueToUse)
        , m_expectException(expectException)
        , m_expectedExceptionCode(expectedExceptionCode)
    {
    }

    ~DetailsTestCase() {}

    PaymentDetails buildDetails() const
    {
        return buildPaymentDetailsForTest(m_detail, m_data, m_modType, m_valueToUse);
    }

    bool expectException() const
    {
        return m_expectException;
    }

    ExceptionCode getExpectedExceptionCode() const
    {
        return m_expectedExceptionCode;
    }

private:
    friend std::ostream& operator<<(std::ostream&, DetailsTestCase);
    PaymentTestDetailToChange m_detail;
    PaymentTestDataToChange m_data;
    PaymentTestModificationType m_modType;
    const char* m_valueToUse;
    bool m_expectException;
    ExceptionCode m_expectedExceptionCode;
};

std::ostream& operator<<(std::ostream& out, DetailsTestCase testCase)
{
    if (testCase.m_expectException)
        out << "Expecting an exception when ";
    else
        out << "Not expecting an exception when ";

    switch (testCase.m_detail) {
    case PaymentTestDetailTotal:
        out << "total ";
        break;
    case PaymentTestDetailItem:
        out << "displayItem ";
        break;
    case PaymentTestDetailShippingOption:
        out << "shippingOption ";
        break;
    case PaymentTestDetailModifierTotal:
        out << "modifiers.total ";
        break;
    case PaymentTestDetailModifierItem:
        out << "modifiers.displayItem ";
        break;
    case PaymentTestDetailNone:
        NOTREACHED();
        break;
    }

    switch (testCase.m_data) {
    case PaymentTestDataId:
        out << "id ";
        break;
    case PaymentTestDataLabel:
        out << "label ";
        break;
    case PaymentTestDataAmount:
        out << "amount ";
        break;
    case PaymentTestDataCurrencyCode:
        out << "currency ";
        break;
    case PaymentTestDataValue:
        out << "value ";
        break;
    case PaymentTestDataNone:
        NOTREACHED();
        break;
    }

    switch (testCase.m_modType) {
    case PaymentTestOverwriteValue:
        out << "is overwritten by ";
        out << testCase.m_valueToUse;
        break;
    case PaymentTestRemoveKey:
        out << "is removed";
        break;
    }

    return out;
}

class PaymentRequestDetailsTest : public testing::TestWithParam<DetailsTestCase> {};

TEST_P(PaymentRequestDetailsTest, ValidatesDetails)
{
    V8TestingScope scope;
    scope.document().setSecurityOrigin(SecurityOrigin::create(KURL(KURL(), "https://www.example.com/")));
    PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), GetParam().buildDetails(), scope.getExceptionState());

    EXPECT_EQ(GetParam().expectException(), scope.getExceptionState().hadException());
    if (GetParam().expectException())
        EXPECT_EQ(GetParam().getExpectedExceptionCode(), scope.getExceptionState().code());
}

INSTANTIATE_TEST_CASE_P(MissingData,
    PaymentRequestDetailsTest,
    testing::Values(
        DetailsTestCase(PaymentTestDetailTotal, PaymentTestDataAmount, PaymentTestRemoveKey, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailTotal, PaymentTestDataValue, PaymentTestRemoveKey, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailTotal, PaymentTestDataCurrencyCode, PaymentTestRemoveKey, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailTotal, PaymentTestDataLabel, PaymentTestRemoveKey, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataAmount, PaymentTestRemoveKey, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestRemoveKey, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataCurrencyCode, PaymentTestRemoveKey, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataLabel, PaymentTestRemoveKey, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataAmount, PaymentTestRemoveKey, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestRemoveKey, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataCurrencyCode, PaymentTestRemoveKey, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataId, PaymentTestRemoveKey, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataLabel, PaymentTestRemoveKey, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierTotal, PaymentTestDataAmount, PaymentTestRemoveKey, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierTotal, PaymentTestDataValue, PaymentTestRemoveKey, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierTotal, PaymentTestDataCurrencyCode, PaymentTestRemoveKey, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierTotal, PaymentTestDataLabel, PaymentTestRemoveKey, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierItem, PaymentTestDataAmount, PaymentTestRemoveKey, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierItem, PaymentTestDataValue, PaymentTestRemoveKey, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierItem, PaymentTestDataCurrencyCode, PaymentTestRemoveKey, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierItem, PaymentTestDataLabel, PaymentTestRemoveKey, "", true, V8TypeError)));

INSTANTIATE_TEST_CASE_P(EmptyData,
    PaymentRequestDetailsTest,
    testing::Values(
        DetailsTestCase(PaymentTestDetailTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailTotal, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailTotal, PaymentTestDataLabel, PaymentTestOverwriteValue, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataLabel, PaymentTestOverwriteValue, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataId, PaymentTestOverwriteValue, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataLabel, PaymentTestOverwriteValue, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierTotal, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierTotal, PaymentTestDataLabel, PaymentTestOverwriteValue, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierItem, PaymentTestDataValue, PaymentTestOverwriteValue, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierItem, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierItem, PaymentTestDataLabel, PaymentTestOverwriteValue, "", true, V8TypeError)));

INSTANTIATE_TEST_CASE_P(ValidCurrencyCodeFormat,
    PaymentRequestDetailsTest,
    testing::Values(
        DetailsTestCase(PaymentTestDetailTotal, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "USD"),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "USD"),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "USD"),
        DetailsTestCase(PaymentTestDetailModifierTotal, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "USD"),
        DetailsTestCase(PaymentTestDetailModifierItem, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "USD")));

INSTANTIATE_TEST_CASE_P(InvalidCurrencyCodeFormat,
    PaymentRequestDetailsTest,
    testing::Values(
        DetailsTestCase(PaymentTestDetailTotal, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "US1", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailTotal, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "US", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailTotal, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "USDO", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailTotal, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "usd", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailTotal, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "US1", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "US", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "USDO", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "usd", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "US1", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "US", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "USDO", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "usd", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierTotal, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "US1", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierTotal, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "US", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierTotal, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "USDO", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierTotal, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "usd", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierTotal, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierItem, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "US1", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierItem, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "US", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierItem, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "USDO", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierItem, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "usd", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierItem, PaymentTestDataCurrencyCode, PaymentTestOverwriteValue, "", true, V8TypeError)));

INSTANTIATE_TEST_CASE_P(ValidValueFormat,
    PaymentRequestDetailsTest,
    testing::Values(
        DetailsTestCase(PaymentTestDetailTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "0"),
        DetailsTestCase(PaymentTestDetailTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "1"),
        DetailsTestCase(PaymentTestDetailTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "10"),
        DetailsTestCase(PaymentTestDetailTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "10.99"),
        DetailsTestCase(PaymentTestDetailTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "01234567890123456789.0123456789"),
        DetailsTestCase(PaymentTestDetailTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "01234567890123456789012345678.9"),
        DetailsTestCase(PaymentTestDetailTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "012345678901234567890123456789"),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "0"),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "-0"),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "1"),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "10"),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "-3"),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "10.99"),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "-3.00"),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "01234567890123456789.0123456789"),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "01234567890123456789012345678.9"),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "012345678901234567890123456789"),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "-01234567890123456789.0123456789"),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "-01234567890123456789012345678.9"),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "-012345678901234567890123456789"),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "0"),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "-0"),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "1"),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "10"),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "-3"),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "10.99"),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "-3.00"),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "01234567890123456789.0123456789"),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "01234567890123456789012345678.9"),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "012345678901234567890123456789"),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "-01234567890123456789.0123456789"),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "-01234567890123456789012345678.9"),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "-012345678901234567890123456789")));

INSTANTIATE_TEST_CASE_P(ValidValueFormatForModifier,
    PaymentRequestDetailsTest,
    testing::Values(
        DetailsTestCase(PaymentTestDetailModifierTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "0"),
        DetailsTestCase(PaymentTestDetailModifierTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "1"),
        DetailsTestCase(PaymentTestDetailModifierTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "10"),
        DetailsTestCase(PaymentTestDetailModifierTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "10.99"),
        DetailsTestCase(PaymentTestDetailModifierTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "01234567890123456789.0123456789"),
        DetailsTestCase(PaymentTestDetailModifierTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "01234567890123456789012345678.9"),
        DetailsTestCase(PaymentTestDetailModifierTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "012345678901234567890123456789"),
        DetailsTestCase(PaymentTestDetailModifierItem, PaymentTestDataValue, PaymentTestOverwriteValue, "0"),
        DetailsTestCase(PaymentTestDetailModifierItem, PaymentTestDataValue, PaymentTestOverwriteValue, "-0"),
        DetailsTestCase(PaymentTestDetailModifierItem, PaymentTestDataValue, PaymentTestOverwriteValue, "1"),
        DetailsTestCase(PaymentTestDetailModifierItem, PaymentTestDataValue, PaymentTestOverwriteValue, "10"),
        DetailsTestCase(PaymentTestDetailModifierItem, PaymentTestDataValue, PaymentTestOverwriteValue, "-3"),
        DetailsTestCase(PaymentTestDetailModifierItem, PaymentTestDataValue, PaymentTestOverwriteValue, "10.99"),
        DetailsTestCase(PaymentTestDetailModifierItem, PaymentTestDataValue, PaymentTestOverwriteValue, "-3.00"),
        DetailsTestCase(PaymentTestDetailModifierItem, PaymentTestDataValue, PaymentTestOverwriteValue, "01234567890123456789.0123456789"),
        DetailsTestCase(PaymentTestDetailModifierItem, PaymentTestDataValue, PaymentTestOverwriteValue, "01234567890123456789012345678.9"),
        DetailsTestCase(PaymentTestDetailModifierItem, PaymentTestDataValue, PaymentTestOverwriteValue, "012345678901234567890123456789"),
        DetailsTestCase(PaymentTestDetailModifierItem, PaymentTestDataValue, PaymentTestOverwriteValue, "-01234567890123456789.0123456789"),
        DetailsTestCase(PaymentTestDetailModifierItem, PaymentTestDataValue, PaymentTestOverwriteValue, "-01234567890123456789012345678.9"),
        DetailsTestCase(PaymentTestDetailModifierItem, PaymentTestDataValue, PaymentTestOverwriteValue, "-012345678901234567890123456789")));

INSTANTIATE_TEST_CASE_P(InvalidValueFormat,
    PaymentRequestDetailsTest,
    testing::Values(
        DetailsTestCase(PaymentTestDetailTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "-0", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "-3", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "-3.00", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "-", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "notdigits", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "ALSONOTDIGITS", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "10.", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailTotal, PaymentTestDataValue, PaymentTestOverwriteValue, ".99", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "-10.", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "10-", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "1-0", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "1.0.0", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "1/3", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "-01234567890123456789.0123456789", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "-01234567890123456789012345678.9", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "-012345678901234567890123456789", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "-", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "notdigits", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "ALSONOTDIGITS", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "10.", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, ".99", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "-10.", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "10-", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "1-0", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "1.0.0", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailItem, PaymentTestDataValue, PaymentTestOverwriteValue, "1/3", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "-", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "notdigits", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "ALSONOTDIGITS", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "10.", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, ".99", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "-10.", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "10-", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "1-0", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "1.0.0", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailShippingOption, PaymentTestDataValue, PaymentTestOverwriteValue, "1/3", true, V8TypeError)));

INSTANTIATE_TEST_CASE_P(InvalidValueFormatForModifier,
    PaymentRequestDetailsTest,
    testing::Values(
        DetailsTestCase(PaymentTestDetailModifierTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "-0", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "-3", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "-3.00", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "-", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "notdigits", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "ALSONOTDIGITS", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "10.", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierTotal, PaymentTestDataValue, PaymentTestOverwriteValue, ".99", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "-10.", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "10-", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "1-0", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "1.0.0", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "1/3", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "-01234567890123456789.0123456789", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "-01234567890123456789012345678.9", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierTotal, PaymentTestDataValue, PaymentTestOverwriteValue, "-012345678901234567890123456789", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierItem, PaymentTestDataValue, PaymentTestOverwriteValue, "", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierItem, PaymentTestDataValue, PaymentTestOverwriteValue, "-", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierItem, PaymentTestDataValue, PaymentTestOverwriteValue, "notdigits", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierItem, PaymentTestDataValue, PaymentTestOverwriteValue, "ALSONOTDIGITS", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierItem, PaymentTestDataValue, PaymentTestOverwriteValue, "10.", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierItem, PaymentTestDataValue, PaymentTestOverwriteValue, ".99", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierItem, PaymentTestDataValue, PaymentTestOverwriteValue, "-10.", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierItem, PaymentTestDataValue, PaymentTestOverwriteValue, "10-", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierItem, PaymentTestDataValue, PaymentTestOverwriteValue, "1-0", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierItem, PaymentTestDataValue, PaymentTestOverwriteValue, "1.0.0", true, V8TypeError),
        DetailsTestCase(PaymentTestDetailModifierItem, PaymentTestDataValue, PaymentTestOverwriteValue, "1/3", true, V8TypeError)));

} // namespace
} // namespace blink
