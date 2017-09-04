// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for PaymentRequest::complete().

#include "bindings/core/v8/V8BindingForTesting.h"
#include "modules/payments/PaymentRequest.h"
#include "modules/payments/PaymentTestHelper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

TEST(CompleteTest, CannotCallCompleteTwice) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.getScriptState());
  makePaymentRequestOriginSecure(scope.document());
  PaymentRequest* request = PaymentRequest::create(
      scope.getScriptState(), buildPaymentMethodDataForTest(),
      buildPaymentDetailsForTest(), scope.getExceptionState());
  EXPECT_FALSE(scope.getExceptionState().hadException());
  request->show(scope.getScriptState());
  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(buildPaymentResponseForTest());
  request->complete(scope.getScriptState(), PaymentCompleter::Fail);

  request->complete(scope.getScriptState(), PaymentCompleter::Success)
      .then(funcs.expectNoCall(), funcs.expectCall());
}

TEST(CompleteTest, RejectCompletePromiseOnError) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.getScriptState());
  makePaymentRequestOriginSecure(scope.document());
  PaymentRequest* request = PaymentRequest::create(
      scope.getScriptState(), buildPaymentMethodDataForTest(),
      buildPaymentDetailsForTest(), scope.getExceptionState());
  EXPECT_FALSE(scope.getExceptionState().hadException());
  request->show(scope.getScriptState());
  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(buildPaymentResponseForTest());

  String errorMessage;
  request->complete(scope.getScriptState(), PaymentCompleter::Success)
      .then(funcs.expectNoCall(), funcs.expectCall(&errorMessage));

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)->OnError(
      payments::mojom::blink::PaymentErrorReason::UNKNOWN);

  v8::MicrotasksScope::PerformCheckpoint(scope.getScriptState()->isolate());
  EXPECT_EQ("UnknownError: Request failed", errorMessage);
}

// If user cancels the transaction during processing, the complete() promise
// should be rejected.
TEST(CompleteTest, RejectCompletePromiseAfterError) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.getScriptState());
  makePaymentRequestOriginSecure(scope.document());
  PaymentRequest* request = PaymentRequest::create(
      scope.getScriptState(), buildPaymentMethodDataForTest(),
      buildPaymentDetailsForTest(), scope.getExceptionState());
  EXPECT_FALSE(scope.getExceptionState().hadException());
  request->show(scope.getScriptState());
  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(buildPaymentResponseForTest());
  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)->OnError(
      payments::mojom::blink::PaymentErrorReason::USER_CANCEL);

  request->complete(scope.getScriptState(), PaymentCompleter::Success)
      .then(funcs.expectNoCall(), funcs.expectCall());
}

TEST(CompleteTest, ResolvePromiseOnComplete) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.getScriptState());
  makePaymentRequestOriginSecure(scope.document());
  PaymentRequest* request = PaymentRequest::create(
      scope.getScriptState(), buildPaymentMethodDataForTest(),
      buildPaymentDetailsForTest(), scope.getExceptionState());
  EXPECT_FALSE(scope.getExceptionState().hadException());
  request->show(scope.getScriptState());
  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(buildPaymentResponseForTest());

  request->complete(scope.getScriptState(), PaymentCompleter::Success)
      .then(funcs.expectCall(), funcs.expectNoCall());

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnComplete();
}

TEST(CompleteTest, RejectCompletePromiseOnUpdateDetailsFailure) {
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

  String errorMessage;
  request->complete(scope.getScriptState(), PaymentCompleter::Success)
      .then(funcs.expectNoCall(), funcs.expectCall(&errorMessage));

  request->onUpdatePaymentDetailsFailure("oops");

  v8::MicrotasksScope::PerformCheckpoint(scope.getScriptState()->isolate());
  EXPECT_EQ("AbortError: oops", errorMessage);
}

TEST(CompleteTest, RejectCompletePromiseAfterTimeout) {
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
  request->onCompleteTimeoutForTesting();

  String errorMessage;
  request->complete(scope.getScriptState(), PaymentCompleter::Success)
      .then(funcs.expectNoCall(), funcs.expectCall(&errorMessage));

  v8::MicrotasksScope::PerformCheckpoint(scope.getScriptState()->isolate());
  EXPECT_EQ(
      "InvalidStateError: Timed out after 60 seconds, complete() called too "
      "late",
      errorMessage);
}

}  // namespace
}  // namespace blink
