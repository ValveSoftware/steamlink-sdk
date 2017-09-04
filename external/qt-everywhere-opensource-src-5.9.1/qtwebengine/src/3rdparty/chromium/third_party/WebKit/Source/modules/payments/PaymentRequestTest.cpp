// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentRequest.h"

#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "modules/payments/PaymentTestHelper.h"
#include "platform/heap/HeapAllocator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

TEST(PaymentRequestTest, SecureContextRequired) {
  V8TestingScope scope;
  scope.document().setSecurityOrigin(
      SecurityOrigin::create(KURL(KURL(), "http://www.example.com/")));

  PaymentRequest::create(
      scope.getScriptState(), buildPaymentMethodDataForTest(),
      buildPaymentDetailsForTest(), scope.getExceptionState());

  EXPECT_TRUE(scope.getExceptionState().hadException());
  EXPECT_EQ(SecurityError, scope.getExceptionState().code());
}

TEST(PaymentRequestTest, NoExceptionWithValidData) {
  V8TestingScope scope;
  makePaymentRequestOriginSecure(scope.document());
  PaymentRequest::create(
      scope.getScriptState(), buildPaymentMethodDataForTest(),
      buildPaymentDetailsForTest(), scope.getExceptionState());

  EXPECT_FALSE(scope.getExceptionState().hadException());
}

TEST(PaymentRequestTest, SupportedMethodListRequired) {
  V8TestingScope scope;
  makePaymentRequestOriginSecure(scope.document());
  PaymentRequest::create(
      scope.getScriptState(), HeapVector<PaymentMethodData>(),
      buildPaymentDetailsForTest(), scope.getExceptionState());

  EXPECT_TRUE(scope.getExceptionState().hadException());
  EXPECT_EQ(V8TypeError, scope.getExceptionState().code());
}

TEST(PaymentRequestTest, TotalRequired) {
  V8TestingScope scope;
  makePaymentRequestOriginSecure(scope.document());
  PaymentRequest::create(scope.getScriptState(),
                         buildPaymentMethodDataForTest(), PaymentDetails(),
                         scope.getExceptionState());

  EXPECT_TRUE(scope.getExceptionState().hadException());
  EXPECT_EQ(V8TypeError, scope.getExceptionState().code());
}

TEST(PaymentRequestTest, ErrorMsgMustBeEmptyInConstrctor) {
  V8TestingScope scope;
  makePaymentRequestOriginSecure(scope.document());
  PaymentRequest::create(
      scope.getScriptState(), buildPaymentMethodDataForTest(),
      buildPaymentDetailsErrorMsgForTest("This is an error message."),
      scope.getExceptionState());

  EXPECT_TRUE(scope.getExceptionState().hadException());
  EXPECT_EQ(V8TypeError, scope.getExceptionState().code());
}

TEST(PaymentRequestTest, NullShippingOptionWhenNoOptionsAvailable) {
  V8TestingScope scope;
  makePaymentRequestOriginSecure(scope.document());
  PaymentDetails details;
  details.setTotal(buildPaymentItemForTest());
  PaymentOptions options;
  options.setRequestShipping(true);

  PaymentRequest* request = PaymentRequest::create(
      scope.getScriptState(), buildPaymentMethodDataForTest(), details, options,
      scope.getExceptionState());

  EXPECT_TRUE(request->shippingOption().isNull());
}

TEST(PaymentRequestTest, NullShippingOptionWhenMultipleOptionsAvailable) {
  V8TestingScope scope;
  makePaymentRequestOriginSecure(scope.document());
  PaymentDetails details;
  details.setTotal(buildPaymentItemForTest());
  details.setShippingOptions(
      HeapVector<PaymentShippingOption>(2, buildShippingOptionForTest()));
  PaymentOptions options;
  options.setRequestShipping(true);

  PaymentRequest* request = PaymentRequest::create(
      scope.getScriptState(), buildPaymentMethodDataForTest(), details, options,
      scope.getExceptionState());

  EXPECT_TRUE(request->shippingOption().isNull());
}

TEST(PaymentRequestTest, DontSelectSingleAvailableShippingOptionByDefault) {
  V8TestingScope scope;
  makePaymentRequestOriginSecure(scope.document());
  PaymentDetails details;
  details.setTotal(buildPaymentItemForTest());
  details.setShippingOptions(HeapVector<PaymentShippingOption>(
      1, buildShippingOptionForTest(PaymentTestDataId,
                                    PaymentTestOverwriteValue, "standard")));

  PaymentRequest* request = PaymentRequest::create(
      scope.getScriptState(), buildPaymentMethodDataForTest(), details,
      scope.getExceptionState());

  EXPECT_TRUE(request->shippingOption().isNull());
}

TEST(PaymentRequestTest,
     DontSelectSingleAvailableShippingOptionWhenShippingNotRequested) {
  V8TestingScope scope;
  makePaymentRequestOriginSecure(scope.document());
  PaymentDetails details;
  details.setTotal(buildPaymentItemForTest());
  details.setShippingOptions(
      HeapVector<PaymentShippingOption>(1, buildShippingOptionForTest()));
  PaymentOptions options;
  options.setRequestShipping(false);

  PaymentRequest* request = PaymentRequest::create(
      scope.getScriptState(), buildPaymentMethodDataForTest(), details, options,
      scope.getExceptionState());

  EXPECT_TRUE(request->shippingOption().isNull());
}

TEST(PaymentRequestTest,
     DontSelectSingleUnselectedShippingOptionWhenShippingRequested) {
  V8TestingScope scope;
  makePaymentRequestOriginSecure(scope.document());
  PaymentDetails details;
  details.setTotal(buildPaymentItemForTest());
  details.setShippingOptions(
      HeapVector<PaymentShippingOption>(1, buildShippingOptionForTest()));
  PaymentOptions options;
  options.setRequestShipping(true);

  PaymentRequest* request = PaymentRequest::create(
      scope.getScriptState(), buildPaymentMethodDataForTest(), details, options,
      scope.getExceptionState());

  EXPECT_TRUE(request->shippingOption().isNull());
}

TEST(PaymentRequestTest,
     SelectSingleSelectedShippingOptionWhenShippingRequested) {
  V8TestingScope scope;
  makePaymentRequestOriginSecure(scope.document());
  PaymentDetails details;
  details.setTotal(buildPaymentItemForTest());
  HeapVector<PaymentShippingOption> shippingOptions(
      1, buildShippingOptionForTest(PaymentTestDataId,
                                    PaymentTestOverwriteValue, "standard"));
  shippingOptions[0].setSelected(true);
  details.setShippingOptions(shippingOptions);
  PaymentOptions options;
  options.setRequestShipping(true);

  PaymentRequest* request = PaymentRequest::create(
      scope.getScriptState(), buildPaymentMethodDataForTest(), details, options,
      scope.getExceptionState());

  EXPECT_EQ("standard", request->shippingOption());
}

TEST(PaymentRequestTest,
     SelectOnlySelectedShippingOptionWhenShippingRequested) {
  V8TestingScope scope;
  makePaymentRequestOriginSecure(scope.document());
  PaymentDetails details;
  details.setTotal(buildPaymentItemForTest());
  HeapVector<PaymentShippingOption> shippingOptions(2);
  shippingOptions[0] = buildShippingOptionForTest(
      PaymentTestDataId, PaymentTestOverwriteValue, "standard");
  shippingOptions[0].setSelected(true);
  shippingOptions[1] = buildShippingOptionForTest(
      PaymentTestDataId, PaymentTestOverwriteValue, "express");
  details.setShippingOptions(shippingOptions);
  PaymentOptions options;
  options.setRequestShipping(true);

  PaymentRequest* request = PaymentRequest::create(
      scope.getScriptState(), buildPaymentMethodDataForTest(), details, options,
      scope.getExceptionState());

  EXPECT_EQ("standard", request->shippingOption());
}

TEST(PaymentRequestTest,
     SelectLastSelectedShippingOptionWhenShippingRequested) {
  V8TestingScope scope;
  makePaymentRequestOriginSecure(scope.document());
  PaymentDetails details;
  details.setTotal(buildPaymentItemForTest());
  HeapVector<PaymentShippingOption> shippingOptions(2);
  shippingOptions[0] = buildShippingOptionForTest(
      PaymentTestDataId, PaymentTestOverwriteValue, "standard");
  shippingOptions[0].setSelected(true);
  shippingOptions[1] = buildShippingOptionForTest(
      PaymentTestDataId, PaymentTestOverwriteValue, "express");
  shippingOptions[1].setSelected(true);
  details.setShippingOptions(shippingOptions);
  PaymentOptions options;
  options.setRequestShipping(true);

  PaymentRequest* request = PaymentRequest::create(
      scope.getScriptState(), buildPaymentMethodDataForTest(), details, options,
      scope.getExceptionState());

  EXPECT_EQ("express", request->shippingOption());
}

TEST(PaymentRequestTest, NullShippingTypeWhenRequestShippingIsFalse) {
  V8TestingScope scope;
  makePaymentRequestOriginSecure(scope.document());
  PaymentDetails details;
  details.setTotal(buildPaymentItemForTest());
  PaymentOptions options;
  options.setRequestShipping(false);

  PaymentRequest* request = PaymentRequest::create(
      scope.getScriptState(), buildPaymentMethodDataForTest(), details, options,
      scope.getExceptionState());

  EXPECT_TRUE(request->shippingType().isNull());
}

TEST(PaymentRequestTest,
     DefaultShippingTypeWhenRequestShippingIsTrueWithNoSpecificType) {
  V8TestingScope scope;
  makePaymentRequestOriginSecure(scope.document());
  PaymentDetails details;
  details.setTotal(buildPaymentItemForTest());
  PaymentOptions options;
  options.setRequestShipping(true);

  PaymentRequest* request = PaymentRequest::create(
      scope.getScriptState(), buildPaymentMethodDataForTest(), details, options,
      scope.getExceptionState());

  EXPECT_EQ("shipping", request->shippingType());
}

TEST(PaymentRequestTest, DeliveryShippingTypeWhenShippingTypeIsDelivery) {
  V8TestingScope scope;
  makePaymentRequestOriginSecure(scope.document());
  PaymentDetails details;
  details.setTotal(buildPaymentItemForTest());
  PaymentOptions options;
  options.setRequestShipping(true);
  options.setShippingType("delivery");

  PaymentRequest* request = PaymentRequest::create(
      scope.getScriptState(), buildPaymentMethodDataForTest(), details, options,
      scope.getExceptionState());

  EXPECT_EQ("delivery", request->shippingType());
}

TEST(PaymentRequestTest, PickupShippingTypeWhenShippingTypeIsPickup) {
  V8TestingScope scope;
  makePaymentRequestOriginSecure(scope.document());
  PaymentDetails details;
  details.setTotal(buildPaymentItemForTest());
  PaymentOptions options;
  options.setRequestShipping(true);
  options.setShippingType("pickup");

  PaymentRequest* request = PaymentRequest::create(
      scope.getScriptState(), buildPaymentMethodDataForTest(), details, options,
      scope.getExceptionState());

  EXPECT_EQ("pickup", request->shippingType());
}

TEST(PaymentRequestTest, DefaultShippingTypeWhenShippingTypeIsInvalid) {
  V8TestingScope scope;
  makePaymentRequestOriginSecure(scope.document());
  PaymentDetails details;
  details.setTotal(buildPaymentItemForTest());
  PaymentOptions options;
  options.setRequestShipping(true);
  options.setShippingType("invalid");

  PaymentRequest* request = PaymentRequest::create(
      scope.getScriptState(), buildPaymentMethodDataForTest(), details, options,
      scope.getExceptionState());

  EXPECT_EQ("shipping", request->shippingType());
}

TEST(PaymentRequestTest, RejectShowPromiseOnInvalidShippingAddress) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.getScriptState());
  makePaymentRequestOriginSecure(scope.document());
  PaymentRequest* request = PaymentRequest::create(
      scope.getScriptState(), buildPaymentMethodDataForTest(),
      buildPaymentDetailsForTest(), scope.getExceptionState());
  EXPECT_FALSE(scope.getExceptionState().hadException());

  request->show(scope.getScriptState())
      .then(funcs.expectNoCall(), funcs.expectCall());

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnShippingAddressChange(payments::mojom::blink::PaymentAddress::New());
}

TEST(PaymentRequestTest, OnShippingOptionChange) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.getScriptState());
  makePaymentRequestOriginSecure(scope.document());
  PaymentRequest* request = PaymentRequest::create(
      scope.getScriptState(), buildPaymentMethodDataForTest(),
      buildPaymentDetailsForTest(), scope.getExceptionState());
  EXPECT_FALSE(scope.getExceptionState().hadException());

  request->show(scope.getScriptState())
      .then(funcs.expectNoCall(), funcs.expectNoCall());

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnShippingOptionChange("standardShipping");
}

TEST(PaymentRequestTest, CannotCallShowTwice) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.getScriptState());
  makePaymentRequestOriginSecure(scope.document());
  PaymentRequest* request = PaymentRequest::create(
      scope.getScriptState(), buildPaymentMethodDataForTest(),
      buildPaymentDetailsForTest(), scope.getExceptionState());
  EXPECT_FALSE(scope.getExceptionState().hadException());
  request->show(scope.getScriptState());

  request->show(scope.getScriptState())
      .then(funcs.expectNoCall(), funcs.expectCall());
}

TEST(PaymentRequestTest, CannotShowAfterAborted) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.getScriptState());
  makePaymentRequestOriginSecure(scope.document());
  PaymentRequest* request = PaymentRequest::create(
      scope.getScriptState(), buildPaymentMethodDataForTest(),
      buildPaymentDetailsForTest(), scope.getExceptionState());
  EXPECT_FALSE(scope.getExceptionState().hadException());
  request->show(scope.getScriptState());
  request->abort(scope.getScriptState());
  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)->OnAbort(
      true);

  request->show(scope.getScriptState())
      .then(funcs.expectNoCall(), funcs.expectCall());
}

TEST(PaymentRequestTest, RejectShowPromiseOnErrorPaymentMethodNotSupported) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.getScriptState());
  makePaymentRequestOriginSecure(scope.document());
  PaymentRequest* request = PaymentRequest::create(
      scope.getScriptState(), buildPaymentMethodDataForTest(),
      buildPaymentDetailsForTest(), scope.getExceptionState());
  EXPECT_FALSE(scope.getExceptionState().hadException());

  String errorMessage;
  request->show(scope.getScriptState())
      .then(funcs.expectNoCall(), funcs.expectCall(&errorMessage));

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)->OnError(
      payments::mojom::blink::PaymentErrorReason::NOT_SUPPORTED);

  v8::MicrotasksScope::PerformCheckpoint(scope.getScriptState()->isolate());
  EXPECT_EQ("NotSupportedError: The payment method is not supported",
            errorMessage);
}

TEST(PaymentRequestTest, RejectShowPromiseOnErrorCancelled) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.getScriptState());
  makePaymentRequestOriginSecure(scope.document());
  PaymentRequest* request = PaymentRequest::create(
      scope.getScriptState(), buildPaymentMethodDataForTest(),
      buildPaymentDetailsForTest(), scope.getExceptionState());
  EXPECT_FALSE(scope.getExceptionState().hadException());

  String errorMessage;
  request->show(scope.getScriptState())
      .then(funcs.expectNoCall(), funcs.expectCall(&errorMessage));

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)->OnError(
      payments::mojom::blink::PaymentErrorReason::USER_CANCEL);

  v8::MicrotasksScope::PerformCheckpoint(scope.getScriptState()->isolate());
  EXPECT_EQ("Request cancelled", errorMessage);
}

TEST(PaymentRequestTest, RejectShowPromiseOnUpdateDetailsFailure) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.getScriptState());
  makePaymentRequestOriginSecure(scope.document());
  PaymentRequest* request = PaymentRequest::create(
      scope.getScriptState(), buildPaymentMethodDataForTest(),
      buildPaymentDetailsForTest(), scope.getExceptionState());
  EXPECT_FALSE(scope.getExceptionState().hadException());

  String errorMessage;
  request->show(scope.getScriptState())
      .then(funcs.expectNoCall(), funcs.expectCall(&errorMessage));

  request->onUpdatePaymentDetailsFailure("oops");

  v8::MicrotasksScope::PerformCheckpoint(scope.getScriptState()->isolate());
  EXPECT_EQ("AbortError: oops", errorMessage);
}

TEST(PaymentRequestTest, IgnoreUpdatePaymentDetailsAfterShowPromiseResolved) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.getScriptState());
  makePaymentRequestOriginSecure(scope.document());
  PaymentRequest* request = PaymentRequest::create(
      scope.getScriptState(), buildPaymentMethodDataForTest(),
      buildPaymentDetailsForTest(), scope.getExceptionState());
  EXPECT_FALSE(scope.getExceptionState().hadException());
  request->show(scope.getScriptState())
      .then(funcs.expectCall(), funcs.expectNoCall());
  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(buildPaymentResponseForTest());

  request->onUpdatePaymentDetails(
      ScriptValue::from(scope.getScriptState(), "foo"));
}

TEST(PaymentRequestTest, RejectShowPromiseOnNonPaymentDetailsUpdate) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.getScriptState());
  makePaymentRequestOriginSecure(scope.document());
  PaymentRequest* request = PaymentRequest::create(
      scope.getScriptState(), buildPaymentMethodDataForTest(),
      buildPaymentDetailsForTest(), scope.getExceptionState());
  EXPECT_FALSE(scope.getExceptionState().hadException());

  request->show(scope.getScriptState())
      .then(funcs.expectNoCall(), funcs.expectCall());

  request->onUpdatePaymentDetails(
      ScriptValue::from(scope.getScriptState(), "NotPaymentDetails"));
}

TEST(PaymentRequestTest, RejectShowPromiseOnInvalidPaymentDetailsUpdate) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.getScriptState());
  makePaymentRequestOriginSecure(scope.document());
  PaymentRequest* request = PaymentRequest::create(
      scope.getScriptState(), buildPaymentMethodDataForTest(),
      buildPaymentDetailsForTest(), scope.getExceptionState());
  EXPECT_FALSE(scope.getExceptionState().hadException());

  request->show(scope.getScriptState())
      .then(funcs.expectNoCall(), funcs.expectCall());

  request->onUpdatePaymentDetails(ScriptValue::from(
      scope.getScriptState(), fromJSONString(scope.getScriptState()->isolate(),
                                             "{}", scope.getExceptionState())));
  EXPECT_FALSE(scope.getExceptionState().hadException());
}

TEST(PaymentRequestTest,
     ClearShippingOptionOnPaymentDetailsUpdateWithoutShippingOptions) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.getScriptState());
  makePaymentRequestOriginSecure(scope.document());
  PaymentDetails details;
  details.setTotal(buildPaymentItemForTest());
  PaymentOptions options;
  options.setRequestShipping(true);
  PaymentRequest* request = PaymentRequest::create(
      scope.getScriptState(), buildPaymentMethodDataForTest(), details, options,
      scope.getExceptionState());
  EXPECT_FALSE(scope.getExceptionState().hadException());
  EXPECT_TRUE(request->shippingOption().isNull());
  request->show(scope.getScriptState())
      .then(funcs.expectNoCall(), funcs.expectNoCall());
  String detailWithShippingOptions =
      "{\"total\": {\"label\": \"Total\", \"amount\": {\"currency\": \"USD\", "
      "\"value\": \"5.00\"}},"
      "\"shippingOptions\": [{\"id\": \"standardShippingOption\", \"label\": "
      "\"Standard shipping\", \"amount\": {\"currency\": \"USD\", \"value\": "
      "\"5.00\"}, \"selected\": true}]}";
  request->onUpdatePaymentDetails(ScriptValue::from(
      scope.getScriptState(),
      fromJSONString(scope.getScriptState()->isolate(),
                     detailWithShippingOptions, scope.getExceptionState())));
  EXPECT_FALSE(scope.getExceptionState().hadException());
  EXPECT_EQ("standardShippingOption", request->shippingOption());
  String detailWithoutShippingOptions =
      "{\"total\": {\"label\": \"Total\", \"amount\": {\"currency\": \"USD\", "
      "\"value\": \"5.00\"}}}";

  request->onUpdatePaymentDetails(ScriptValue::from(
      scope.getScriptState(),
      fromJSONString(scope.getScriptState()->isolate(),
                     detailWithoutShippingOptions, scope.getExceptionState())));

  EXPECT_FALSE(scope.getExceptionState().hadException());
  EXPECT_TRUE(request->shippingOption().isNull());
}

TEST(
    PaymentRequestTest,
    ClearShippingOptionOnPaymentDetailsUpdateWithMultipleUnselectedShippingOptions) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.getScriptState());
  makePaymentRequestOriginSecure(scope.document());
  PaymentOptions options;
  options.setRequestShipping(true);
  PaymentRequest* request = PaymentRequest::create(
      scope.getScriptState(), buildPaymentMethodDataForTest(),
      buildPaymentDetailsForTest(), options, scope.getExceptionState());
  EXPECT_FALSE(scope.getExceptionState().hadException());
  request->show(scope.getScriptState())
      .then(funcs.expectNoCall(), funcs.expectNoCall());
  String detail =
      "{\"total\": {\"label\": \"Total\", \"amount\": {\"currency\": \"USD\", "
      "\"value\": \"5.00\"}},"
      "\"shippingOptions\": [{\"id\": \"slow\", \"label\": \"Slow\", "
      "\"amount\": {\"currency\": \"USD\", \"value\": \"5.00\"}},"
      "{\"id\": \"fast\", \"label\": \"Fast\", \"amount\": {\"currency\": "
      "\"USD\", \"value\": \"50.00\"}}]}";

  request->onUpdatePaymentDetails(
      ScriptValue::from(scope.getScriptState(),
                        fromJSONString(scope.getScriptState()->isolate(),
                                       detail, scope.getExceptionState())));
  EXPECT_FALSE(scope.getExceptionState().hadException());

  EXPECT_TRUE(request->shippingOption().isNull());
}

TEST(PaymentRequestTest, UseTheSelectedShippingOptionFromPaymentDetailsUpdate) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.getScriptState());
  makePaymentRequestOriginSecure(scope.document());
  PaymentOptions options;
  options.setRequestShipping(true);
  PaymentRequest* request = PaymentRequest::create(
      scope.getScriptState(), buildPaymentMethodDataForTest(),
      buildPaymentDetailsForTest(), options, scope.getExceptionState());
  EXPECT_FALSE(scope.getExceptionState().hadException());
  request->show(scope.getScriptState())
      .then(funcs.expectNoCall(), funcs.expectNoCall());
  String detail =
      "{\"total\": {\"label\": \"Total\", \"amount\": {\"currency\": \"USD\", "
      "\"value\": \"5.00\"}},"
      "\"shippingOptions\": [{\"id\": \"slow\", \"label\": \"Slow\", "
      "\"amount\": {\"currency\": \"USD\", \"value\": \"5.00\"}},"
      "{\"id\": \"fast\", \"label\": \"Fast\", \"amount\": {\"currency\": "
      "\"USD\", \"value\": \"50.00\"}, \"selected\": true}]}";

  request->onUpdatePaymentDetails(
      ScriptValue::from(scope.getScriptState(),
                        fromJSONString(scope.getScriptState()->isolate(),
                                       detail, scope.getExceptionState())));
  EXPECT_FALSE(scope.getExceptionState().hadException());

  EXPECT_EQ("fast", request->shippingOption());
}

TEST(PaymentRequestTest, NoExceptionWithErrorMessageInUpdate) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.getScriptState());
  makePaymentRequestOriginSecure(scope.document());
  PaymentRequest* request = PaymentRequest::create(
      scope.getScriptState(), buildPaymentMethodDataForTest(),
      buildPaymentDetailsForTest(), scope.getExceptionState());
  EXPECT_FALSE(scope.getExceptionState().hadException());

  request->show(scope.getScriptState())
      .then(funcs.expectNoCall(), funcs.expectNoCall());
  String detailWithErrorMsg =
      "{\"total\": {\"label\": \"Total\", \"amount\": {\"currency\": \"USD\", "
      "\"value\": \"5.00\"}},"
      "\"error\": \"This is an error message.\"}";

  request->onUpdatePaymentDetails(ScriptValue::from(
      scope.getScriptState(),
      fromJSONString(scope.getScriptState()->isolate(), detailWithErrorMsg,
                     scope.getExceptionState())));
  EXPECT_FALSE(scope.getExceptionState().hadException());
}

TEST(PaymentRequestTest,
     ShouldResolveWithEmptyShippingOptionsIfIDsOfShippingOptionsAreDuplicated) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.getScriptState());
  makePaymentRequestOriginSecure(scope.document());
  PaymentDetails details;
  details.setTotal(buildPaymentItemForTest());
  HeapVector<PaymentShippingOption> shippingOptions(2);
  shippingOptions[0] = buildShippingOptionForTest(
      PaymentTestDataId, PaymentTestOverwriteValue, "standard");
  shippingOptions[0].setSelected(true);
  shippingOptions[1] = buildShippingOptionForTest(
      PaymentTestDataId, PaymentTestOverwriteValue, "standard");
  details.setShippingOptions(shippingOptions);
  PaymentOptions options;
  options.setRequestShipping(true);
  PaymentRequest* request = PaymentRequest::create(
      scope.getScriptState(), buildPaymentMethodDataForTest(), details, options,
      scope.getExceptionState());
  EXPECT_FALSE(scope.getExceptionState().hadException());
  EXPECT_TRUE(request->shippingOption().isNull());
  request->show(scope.getScriptState())
      .then(funcs.expectNoCall(), funcs.expectNoCall());
  String detailWithShippingOptions =
      "{\"total\": {\"label\": \"Total\", \"amount\": {\"currency\": \"USD\", "
      "\"value\": \"5.00\"}},"
      "\"shippingOptions\": [{\"id\": \"standardShippingOption\", \"label\": "
      "\"Standard shipping\", \"amount\": {\"currency\": \"USD\", \"value\": "
      "\"5.00\"}, \"selected\": true}, {\"id\": \"standardShippingOption\", "
      "\"label\": \"Standard shipping\", \"amount\": {\"currency\": \"USD\", "
      "\"value\": \"5.00\"}, \"selected\": true}]}";

  request->onUpdatePaymentDetails(ScriptValue::from(
      scope.getScriptState(),
      fromJSONString(scope.getScriptState()->isolate(),
                     detailWithShippingOptions, scope.getExceptionState())));

  EXPECT_FALSE(scope.getExceptionState().hadException());
  EXPECT_TRUE(request->shippingOption().isNull());
}

}  // namespace
}  // namespace blink
