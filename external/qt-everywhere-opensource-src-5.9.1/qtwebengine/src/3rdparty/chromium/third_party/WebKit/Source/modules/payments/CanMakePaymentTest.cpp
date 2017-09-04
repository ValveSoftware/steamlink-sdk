// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for PaymentRequest::canMakePayment().

#include "bindings/core/v8/V8BindingForTesting.h"
#include "modules/payments/PaymentRequest.h"
#include "modules/payments/PaymentTestHelper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

using payments::mojom::blink::CanMakePaymentQueryResult;
using payments::mojom::blink::PaymentErrorReason;
using payments::mojom::blink::PaymentRequestClient;

TEST(CanMakePaymentTest, RejectPromiseOnUserCancel) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.getScriptState());
  makePaymentRequestOriginSecure(scope.document());
  PaymentRequest* request = PaymentRequest::create(
      scope.getScriptState(), buildPaymentMethodDataForTest(),
      buildPaymentDetailsForTest(), scope.getExceptionState());

  request->canMakePayment(scope.getScriptState())
      .then(funcs.expectNoCall(), funcs.expectCall());

  static_cast<PaymentRequestClient*>(request)->OnError(
      PaymentErrorReason::USER_CANCEL);
}

TEST(CanMakePaymentTest, RejectPromiseOnUnknownError) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.getScriptState());
  makePaymentRequestOriginSecure(scope.document());
  PaymentRequest* request = PaymentRequest::create(
      scope.getScriptState(), buildPaymentMethodDataForTest(),
      buildPaymentDetailsForTest(), scope.getExceptionState());

  request->canMakePayment(scope.getScriptState())
      .then(funcs.expectNoCall(), funcs.expectCall());

  static_cast<PaymentRequestClient*>(request)->OnError(
      PaymentErrorReason::UNKNOWN);
}

TEST(CanMakePaymentTest, RejectDuplicateRequest) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.getScriptState());
  makePaymentRequestOriginSecure(scope.document());
  PaymentRequest* request = PaymentRequest::create(
      scope.getScriptState(), buildPaymentMethodDataForTest(),
      buildPaymentDetailsForTest(), scope.getExceptionState());
  request->canMakePayment(scope.getScriptState());

  request->canMakePayment(scope.getScriptState())
      .then(funcs.expectNoCall(), funcs.expectCall());
}

TEST(CanMakePaymentTest, RejectQueryQuotaExceeded) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.getScriptState());
  makePaymentRequestOriginSecure(scope.document());
  PaymentRequest* request = PaymentRequest::create(
      scope.getScriptState(), buildPaymentMethodDataForTest(),
      buildPaymentDetailsForTest(), scope.getExceptionState());

  request->canMakePayment(scope.getScriptState())
      .then(funcs.expectNoCall(), funcs.expectCall());

  static_cast<PaymentRequestClient*>(request)->OnCanMakePayment(
      CanMakePaymentQueryResult::QUERY_QUOTA_EXCEEDED);
}

TEST(CanMakePaymentTest, ReturnCannotMakeCanMakePayment) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.getScriptState());
  makePaymentRequestOriginSecure(scope.document());
  PaymentRequest* request = PaymentRequest::create(
      scope.getScriptState(), buildPaymentMethodDataForTest(),
      buildPaymentDetailsForTest(), scope.getExceptionState());
  String captor;
  request->canMakePayment(scope.getScriptState())
      .then(funcs.expectCall(&captor), funcs.expectNoCall());

  static_cast<PaymentRequestClient*>(request)->OnCanMakePayment(
      CanMakePaymentQueryResult::CANNOT_MAKE_PAYMENT);

  v8::MicrotasksScope::PerformCheckpoint(scope.getScriptState()->isolate());
  EXPECT_EQ("false", captor);
}

TEST(CanMakePaymentTest, ReturnCanMakePayment) {
  V8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.getScriptState());
  makePaymentRequestOriginSecure(scope.document());
  PaymentRequest* request = PaymentRequest::create(
      scope.getScriptState(), buildPaymentMethodDataForTest(),
      buildPaymentDetailsForTest(), scope.getExceptionState());
  String captor;
  request->canMakePayment(scope.getScriptState())
      .then(funcs.expectCall(&captor), funcs.expectNoCall());

  static_cast<PaymentRequestClient*>(request)->OnCanMakePayment(
      CanMakePaymentQueryResult::CAN_MAKE_PAYMENT);

  v8::MicrotasksScope::PerformCheckpoint(scope.getScriptState()->isolate());
  EXPECT_EQ("true", captor);
}

}  // namespace
}  // namespace blink
