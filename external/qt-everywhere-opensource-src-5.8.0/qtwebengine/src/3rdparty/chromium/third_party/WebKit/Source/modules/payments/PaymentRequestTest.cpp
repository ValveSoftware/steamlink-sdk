// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentRequest.h"

#include "bindings/core/v8/JSONValuesForV8.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "modules/payments/PaymentAddress.h"
#include "modules/payments/PaymentTestHelper.h"
#include "platform/heap/HeapAllocator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

TEST(PaymentRequestTest, SecureContextRequired)
{
    V8TestingScope scope;
    scope.document().setSecurityOrigin(SecurityOrigin::create(KURL(KURL(), "http://www.example.com/")));

    PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), scope.getExceptionState());

    EXPECT_TRUE(scope.getExceptionState().hadException());
    EXPECT_EQ(SecurityError, scope.getExceptionState().code());
}

TEST(PaymentRequestTest, NoExceptionWithValidData)
{
    V8TestingScope scope;
    makePaymentRequestOriginSecure(scope.document());
    PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), scope.getExceptionState());

    EXPECT_FALSE(scope.getExceptionState().hadException());
}


TEST(PaymentRequestTest, SupportedMethodListRequired)
{
    V8TestingScope scope;
    makePaymentRequestOriginSecure(scope.document());
    PaymentRequest::create(scope.getScriptState(), HeapVector<PaymentMethodData>(), buildPaymentDetailsForTest(), scope.getExceptionState());

    EXPECT_TRUE(scope.getExceptionState().hadException());
    EXPECT_EQ(V8TypeError, scope.getExceptionState().code());
}

TEST(PaymentRequestTest, TotalRequired)
{
    V8TestingScope scope;
    makePaymentRequestOriginSecure(scope.document());
    PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), PaymentDetails(), scope.getExceptionState());

    EXPECT_TRUE(scope.getExceptionState().hadException());
    EXPECT_EQ(V8TypeError, scope.getExceptionState().code());
}

TEST(PaymentRequestTest, NullShippingOptionWhenNoOptionsAvailable)
{
    V8TestingScope scope;
    makePaymentRequestOriginSecure(scope.document());
    PaymentDetails details;
    details.setTotal(buildPaymentItemForTest());
    PaymentOptions options;
    options.setRequestShipping(true);

    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), details, options, scope.getExceptionState());

    EXPECT_TRUE(request->shippingOption().isNull());
}

TEST(PaymentRequestTest, NullShippingOptionWhenMultipleOptionsAvailable)
{
    V8TestingScope scope;
    makePaymentRequestOriginSecure(scope.document());
    PaymentDetails details;
    details.setTotal(buildPaymentItemForTest());
    details.setShippingOptions(HeapVector<PaymentShippingOption>(2, buildShippingOptionForTest()));
    PaymentOptions options;
    options.setRequestShipping(true);

    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), details, options, scope.getExceptionState());

    EXPECT_TRUE(request->shippingOption().isNull());
}

TEST(PaymentRequestTest, DontSelectSingleAvailableShippingOptionByDefault)
{
    V8TestingScope scope;
    makePaymentRequestOriginSecure(scope.document());
    PaymentDetails details;
    details.setTotal(buildPaymentItemForTest());
    details.setShippingOptions(HeapVector<PaymentShippingOption>(1, buildShippingOptionForTest(PaymentTestDataId, PaymentTestOverwriteValue, "standard")));

    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), details, scope.getExceptionState());

    EXPECT_TRUE(request->shippingOption().isNull());
}

TEST(PaymentRequestTest, DontSelectSingleAvailableShippingOptionWhenShippingNotRequested)
{
    V8TestingScope scope;
    makePaymentRequestOriginSecure(scope.document());
    PaymentDetails details;
    details.setTotal(buildPaymentItemForTest());
    details.setShippingOptions(HeapVector<PaymentShippingOption>(1, buildShippingOptionForTest()));
    PaymentOptions options;
    options.setRequestShipping(false);

    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), details, options, scope.getExceptionState());

    EXPECT_TRUE(request->shippingOption().isNull());
}

TEST(PaymentRequestTest, DontSelectSingleUnselectedShippingOptionWhenShippingRequested)
{
    V8TestingScope scope;
    makePaymentRequestOriginSecure(scope.document());
    PaymentDetails details;
    details.setTotal(buildPaymentItemForTest());
    details.setShippingOptions(HeapVector<PaymentShippingOption>(1, buildShippingOptionForTest()));
    PaymentOptions options;
    options.setRequestShipping(true);

    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), details, options, scope.getExceptionState());

    EXPECT_TRUE(request->shippingOption().isNull());
}

TEST(PaymentRequestTest, SelectSingleSelectedShippingOptionWhenShippingRequested)
{
    V8TestingScope scope;
    makePaymentRequestOriginSecure(scope.document());
    PaymentDetails details;
    details.setTotal(buildPaymentItemForTest());
    HeapVector<PaymentShippingOption> shippingOptions(1, buildShippingOptionForTest(PaymentTestDataId, PaymentTestOverwriteValue, "standard"));
    shippingOptions[0].setSelected(true);
    details.setShippingOptions(shippingOptions);
    PaymentOptions options;
    options.setRequestShipping(true);

    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), details, options, scope.getExceptionState());

    EXPECT_EQ("standard", request->shippingOption());
}

TEST(PaymentRequestTest, SelectOnlySelectedShippingOptionWhenShippingRequested)
{
    V8TestingScope scope;
    makePaymentRequestOriginSecure(scope.document());
    PaymentDetails details;
    details.setTotal(buildPaymentItemForTest());
    HeapVector<PaymentShippingOption> shippingOptions(2);
    shippingOptions[0] = buildShippingOptionForTest(PaymentTestDataId, PaymentTestOverwriteValue, "standard");
    shippingOptions[0].setSelected(true);
    shippingOptions[1] = buildShippingOptionForTest(PaymentTestDataId, PaymentTestOverwriteValue, "express");
    details.setShippingOptions(shippingOptions);
    PaymentOptions options;
    options.setRequestShipping(true);

    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), details, options, scope.getExceptionState());

    EXPECT_EQ("standard", request->shippingOption());
}

TEST(PaymentRequestTest, SelectLastSelectedShippingOptionWhenShippingRequested)
{
    V8TestingScope scope;
    makePaymentRequestOriginSecure(scope.document());
    PaymentDetails details;
    details.setTotal(buildPaymentItemForTest());
    HeapVector<PaymentShippingOption> shippingOptions(2);
    shippingOptions[0] = buildShippingOptionForTest(PaymentTestDataId, PaymentTestOverwriteValue, "standard");
    shippingOptions[0].setSelected(true);
    shippingOptions[1] = buildShippingOptionForTest(PaymentTestDataId, PaymentTestOverwriteValue, "express");
    shippingOptions[1].setSelected(true);
    details.setShippingOptions(shippingOptions);
    PaymentOptions options;
    options.setRequestShipping(true);

    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), details, options, scope.getExceptionState());

    EXPECT_EQ("express", request->shippingOption());
}

TEST(PaymentRequestTest, RejectShowPromiseOnInvalidShippingAddress)
{
    V8TestingScope scope;
    PaymentRequestMockFunctionScope funcs(scope.getScriptState());
    makePaymentRequestOriginSecure(scope.document());
    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), scope.getExceptionState());
    EXPECT_FALSE(scope.getExceptionState().hadException());

    request->show(scope.getScriptState()).then(funcs.expectNoCall(), funcs.expectCall());

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnShippingAddressChange(mojom::blink::PaymentAddress::New());
}

TEST(PaymentRequestTest, OnShippingOptionChange)
{
    V8TestingScope scope;
    PaymentRequestMockFunctionScope funcs(scope.getScriptState());
    makePaymentRequestOriginSecure(scope.document());
    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), scope.getExceptionState());
    EXPECT_FALSE(scope.getExceptionState().hadException());

    request->show(scope.getScriptState()).then(funcs.expectNoCall(), funcs.expectNoCall());

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnShippingOptionChange("standardShipping");
}

TEST(PaymentRequestTest, CannotCallShowTwice)
{
    V8TestingScope scope;
    PaymentRequestMockFunctionScope funcs(scope.getScriptState());
    makePaymentRequestOriginSecure(scope.document());
    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), scope.getExceptionState());
    EXPECT_FALSE(scope.getExceptionState().hadException());
    request->show(scope.getScriptState());

    request->show(scope.getScriptState()).then(funcs.expectNoCall(), funcs.expectCall());
}

TEST(PaymentRequestTest, CannotCallCompleteTwice)
{
    V8TestingScope scope;
    PaymentRequestMockFunctionScope funcs(scope.getScriptState());
    makePaymentRequestOriginSecure(scope.document());
    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), scope.getExceptionState());
    EXPECT_FALSE(scope.getExceptionState().hadException());
    request->show(scope.getScriptState());
    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnPaymentResponse(buildPaymentResponseForTest());
    request->complete(scope.getScriptState(), Fail);

    request->complete(scope.getScriptState(), Success).then(funcs.expectNoCall(), funcs.expectCall());
}

TEST(PaymentRequestTest, RejectShowPromiseOnErrorPaymentMethodNotSupported)
{
    V8TestingScope scope;
    PaymentRequestMockFunctionScope funcs(scope.getScriptState());
    makePaymentRequestOriginSecure(scope.document());
    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), scope.getExceptionState());
    EXPECT_FALSE(scope.getExceptionState().hadException());

    String errorMessage;
    request->show(scope.getScriptState()).then(funcs.expectNoCall(), funcs.expectCall(&errorMessage));

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnError(mojom::blink::PaymentErrorReason::NOT_SUPPORTED);

    v8::MicrotasksScope::PerformCheckpoint(scope.getScriptState()->isolate());
    EXPECT_EQ("NotSupportedError: The payment method is not supported", errorMessage);
}

TEST(PaymentRequestTest, RejectShowPromiseOnErrorCancelled)
{
    V8TestingScope scope;
    PaymentRequestMockFunctionScope funcs(scope.getScriptState());
    makePaymentRequestOriginSecure(scope.document());
    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), scope.getExceptionState());
    EXPECT_FALSE(scope.getExceptionState().hadException());

    String errorMessage;
    request->show(scope.getScriptState()).then(funcs.expectNoCall(), funcs.expectCall(&errorMessage));

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnError(mojom::blink::PaymentErrorReason::USER_CANCEL);

    v8::MicrotasksScope::PerformCheckpoint(scope.getScriptState()->isolate());
    EXPECT_EQ("Request cancelled", errorMessage);
}

TEST(PaymentRequestTest, RejectCompletePromiseOnError)
{
    V8TestingScope scope;
    PaymentRequestMockFunctionScope funcs(scope.getScriptState());
    makePaymentRequestOriginSecure(scope.document());
    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), scope.getExceptionState());
    EXPECT_FALSE(scope.getExceptionState().hadException());
    request->show(scope.getScriptState());
    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnPaymentResponse(buildPaymentResponseForTest());

    String errorMessage;
    request->complete(scope.getScriptState(), Success).then(funcs.expectNoCall(), funcs.expectCall(&errorMessage));

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnError(mojom::blink::PaymentErrorReason::UNKNOWN);

    v8::MicrotasksScope::PerformCheckpoint(scope.getScriptState()->isolate());
    EXPECT_EQ("UnknownError: Request failed", errorMessage);
}

// If user cancels the transaction during processing, the complete() promise
// should be rejected.
TEST(PaymentRequestTest, RejectCompletePromiseAfterError)
{
    V8TestingScope scope;
    PaymentRequestMockFunctionScope funcs(scope.getScriptState());
    makePaymentRequestOriginSecure(scope.document());
    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), scope.getExceptionState());
    EXPECT_FALSE(scope.getExceptionState().hadException());
    request->show(scope.getScriptState());
    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnPaymentResponse(buildPaymentResponseForTest());
    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnError(mojom::blink::PaymentErrorReason::USER_CANCEL);

    request->complete(scope.getScriptState(), Success).then(funcs.expectNoCall(), funcs.expectCall());
}

TEST(PaymentRequestTest, ResolvePromiseOnComplete)
{
    V8TestingScope scope;
    PaymentRequestMockFunctionScope funcs(scope.getScriptState());
    makePaymentRequestOriginSecure(scope.document());
    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), scope.getExceptionState());
    EXPECT_FALSE(scope.getExceptionState().hadException());
    request->show(scope.getScriptState());
    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnPaymentResponse(buildPaymentResponseForTest());

    request->complete(scope.getScriptState(), Success).then(funcs.expectCall(), funcs.expectNoCall());

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnComplete();
}

TEST(PaymentRequestTest, RejectShowPromiseOnUpdateDetailsFailure)
{
    V8TestingScope scope;
    PaymentRequestMockFunctionScope funcs(scope.getScriptState());
    makePaymentRequestOriginSecure(scope.document());
    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), scope.getExceptionState());
    EXPECT_FALSE(scope.getExceptionState().hadException());

    request->show(scope.getScriptState()).then(funcs.expectNoCall(), funcs.expectCall());

    request->onUpdatePaymentDetailsFailure(ScriptValue::from(scope.getScriptState(), "oops"));
}

TEST(PaymentRequestTest, RejectCompletePromiseOnUpdateDetailsFailure)
{
    V8TestingScope scope;
    PaymentRequestMockFunctionScope funcs(scope.getScriptState());
    makePaymentRequestOriginSecure(scope.document());
    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), scope.getExceptionState());
    EXPECT_FALSE(scope.getExceptionState().hadException());
    request->show(scope.getScriptState()).then(funcs.expectCall(), funcs.expectNoCall());
    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnPaymentResponse(buildPaymentResponseForTest());

    request->complete(scope.getScriptState(), Success).then(funcs.expectNoCall(), funcs.expectCall());

    request->onUpdatePaymentDetailsFailure(ScriptValue::from(scope.getScriptState(), "oops"));
}

TEST(PaymentRequestTest, IgnoreUpdatePaymentDetailsAfterShowPromiseResolved)
{
    V8TestingScope scope;
    PaymentRequestMockFunctionScope funcs(scope.getScriptState());
    makePaymentRequestOriginSecure(scope.document());
    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), scope.getExceptionState());
    EXPECT_FALSE(scope.getExceptionState().hadException());
    request->show(scope.getScriptState()).then(funcs.expectCall(), funcs.expectNoCall());
    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnPaymentResponse(buildPaymentResponseForTest());

    request->onUpdatePaymentDetails(ScriptValue::from(scope.getScriptState(), "foo"));
}

TEST(PaymentRequestTest, RejectShowPromiseOnNonPaymentDetailsUpdate)
{
    V8TestingScope scope;
    PaymentRequestMockFunctionScope funcs(scope.getScriptState());
    makePaymentRequestOriginSecure(scope.document());
    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), scope.getExceptionState());
    EXPECT_FALSE(scope.getExceptionState().hadException());

    request->show(scope.getScriptState()).then(funcs.expectNoCall(), funcs.expectCall());

    request->onUpdatePaymentDetails(ScriptValue::from(scope.getScriptState(), "NotPaymentDetails"));
}

TEST(PaymentRequestTest, RejectShowPromiseOnInvalidPaymentDetailsUpdate)
{
    V8TestingScope scope;
    PaymentRequestMockFunctionScope funcs(scope.getScriptState());
    makePaymentRequestOriginSecure(scope.document());
    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), scope.getExceptionState());
    EXPECT_FALSE(scope.getExceptionState().hadException());

    request->show(scope.getScriptState()).then(funcs.expectNoCall(), funcs.expectCall());

    request->onUpdatePaymentDetails(ScriptValue::from(scope.getScriptState(), fromJSONString(scope.getScriptState(), "{}", scope.getExceptionState())));
    EXPECT_FALSE(scope.getExceptionState().hadException());
}

TEST(PaymentRequestTest, ClearShippingOptionOnPaymentDetailsUpdateWithoutShippingOptions)
{
    V8TestingScope scope;
    PaymentRequestMockFunctionScope funcs(scope.getScriptState());
    makePaymentRequestOriginSecure(scope.document());
    PaymentDetails details;
    details.setTotal(buildPaymentItemForTest());
    PaymentOptions options;
    options.setRequestShipping(true);
    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), details, options, scope.getExceptionState());
    EXPECT_FALSE(scope.getExceptionState().hadException());
    EXPECT_TRUE(request->shippingOption().isNull());
    request->show(scope.getScriptState()).then(funcs.expectNoCall(), funcs.expectNoCall());
    String detailWithShippingOptions = "{\"total\": {\"label\": \"Total\", \"amount\": {\"currency\": \"USD\", \"value\": \"5.00\"}},"
        "\"shippingOptions\": [{\"id\": \"standardShippingOption\", \"label\": \"Standard shipping\", \"amount\": {\"currency\": \"USD\", \"value\": \"5.00\"}, \"selected\": true}]}";
    request->onUpdatePaymentDetails(ScriptValue::from(scope.getScriptState(), fromJSONString(scope.getScriptState(), detailWithShippingOptions, scope.getExceptionState())));
    EXPECT_FALSE(scope.getExceptionState().hadException());
    EXPECT_EQ("standardShippingOption", request->shippingOption());
    String detailWithoutShippingOptions = "{\"total\": {\"label\": \"Total\", \"amount\": {\"currency\": \"USD\", \"value\": \"5.00\"}}}";

    request->onUpdatePaymentDetails(ScriptValue::from(scope.getScriptState(), fromJSONString(scope.getScriptState(), detailWithoutShippingOptions, scope.getExceptionState())));

    EXPECT_FALSE(scope.getExceptionState().hadException());
    EXPECT_TRUE(request->shippingOption().isNull());
}

TEST(PaymentRequestTest, ClearShippingOptionOnPaymentDetailsUpdateWithMultipleUnselectedShippingOptions)
{
    V8TestingScope scope;
    PaymentRequestMockFunctionScope funcs(scope.getScriptState());
    makePaymentRequestOriginSecure(scope.document());
    PaymentOptions options;
    options.setRequestShipping(true);
    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), options, scope.getExceptionState());
    EXPECT_FALSE(scope.getExceptionState().hadException());
    request->show(scope.getScriptState()).then(funcs.expectNoCall(), funcs.expectNoCall());
    String detail = "{\"total\": {\"label\": \"Total\", \"amount\": {\"currency\": \"USD\", \"value\": \"5.00\"}},"
        "\"shippingOptions\": [{\"id\": \"slow\", \"label\": \"Slow\", \"amount\": {\"currency\": \"USD\", \"value\": \"5.00\"}},"
        "{\"id\": \"fast\", \"label\": \"Fast\", \"amount\": {\"currency\": \"USD\", \"value\": \"50.00\"}}]}";

    request->onUpdatePaymentDetails(ScriptValue::from(scope.getScriptState(), fromJSONString(scope.getScriptState(), detail, scope.getExceptionState())));
    EXPECT_FALSE(scope.getExceptionState().hadException());

    EXPECT_TRUE(request->shippingOption().isNull());
}

TEST(PaymentRequestTest, UseTheSelectedShippingOptionFromPaymentDetailsUpdate)
{
    V8TestingScope scope;
    PaymentRequestMockFunctionScope funcs(scope.getScriptState());
    makePaymentRequestOriginSecure(scope.document());
    PaymentOptions options;
    options.setRequestShipping(true);
    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), options, scope.getExceptionState());
    EXPECT_FALSE(scope.getExceptionState().hadException());
    request->show(scope.getScriptState()).then(funcs.expectNoCall(), funcs.expectNoCall());
    String detail = "{\"total\": {\"label\": \"Total\", \"amount\": {\"currency\": \"USD\", \"value\": \"5.00\"}},"
        "\"shippingOptions\": [{\"id\": \"slow\", \"label\": \"Slow\", \"amount\": {\"currency\": \"USD\", \"value\": \"5.00\"}},"
        "{\"id\": \"fast\", \"label\": \"Fast\", \"amount\": {\"currency\": \"USD\", \"value\": \"50.00\"}, \"selected\": true}]}";

    request->onUpdatePaymentDetails(ScriptValue::from(scope.getScriptState(), fromJSONString(scope.getScriptState(), detail, scope.getExceptionState())));
    EXPECT_FALSE(scope.getExceptionState().hadException());

    EXPECT_EQ("fast", request->shippingOption());
}

} // namespace
} // namespace blink
