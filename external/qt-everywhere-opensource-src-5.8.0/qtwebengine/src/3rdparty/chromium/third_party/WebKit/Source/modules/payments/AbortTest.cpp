// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for PaymentRequest::abort().

#include "bindings/core/v8/V8BindingForTesting.h"
#include "modules/payments/PaymentRequest.h"
#include "modules/payments/PaymentTestHelper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

// If request.abort() is called without calling request.show() first, then
// abort() should reject with exception.
TEST(AbortTest, CannotAbortBeforeShow)
{
    V8TestingScope scope;
    PaymentRequestMockFunctionScope funcs(scope.getScriptState());
    makePaymentRequestOriginSecure(scope.document());
    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), scope.getExceptionState());

    request->abort(scope.getScriptState()).then(funcs.expectNoCall(), funcs.expectCall());
}

// If request.abort() is called again before the previous abort() resolved, then
// the second abort() should reject with exception.
TEST(AbortTest, CannotAbortTwiceConcurrently)
{
    V8TestingScope scope;
    PaymentRequestMockFunctionScope funcs(scope.getScriptState());
    makePaymentRequestOriginSecure(scope.document());
    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), scope.getExceptionState());
    request->show(scope.getScriptState());
    request->abort(scope.getScriptState());

    request->abort(scope.getScriptState()).then(funcs.expectNoCall(), funcs.expectCall());
}

// If request.abort() is called after calling request.show(), then abort()
// should not reject with exception.
TEST(AbortTest, CanAbortAfterShow)
{
    V8TestingScope scope;
    PaymentRequestMockFunctionScope funcs(scope.getScriptState());
    makePaymentRequestOriginSecure(scope.document());
    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), scope.getExceptionState());
    request->show(scope.getScriptState());

    request->abort(scope.getScriptState()).then(funcs.expectNoCall(), funcs.expectNoCall());
}

// If the browser is unable to abort the payment, then the request.abort()
// promise should be rejected.
TEST(AbortTest, FailedAbortShouldRejectAbortPromise)
{
    V8TestingScope scope;
    PaymentRequestMockFunctionScope funcs(scope.getScriptState());
    makePaymentRequestOriginSecure(scope.document());
    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), scope.getExceptionState());
    request->show(scope.getScriptState());

    request->abort(scope.getScriptState()).then(funcs.expectNoCall(), funcs.expectCall());

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnAbort(false);
}

// After the browser is unable to abort the payment once, the second abort()
// call should not be rejected, as it's not a duplicate request anymore.
TEST(AbortTest, CanAbortAgainAfterFirstAbortRejected)
{
    V8TestingScope scope;
    PaymentRequestMockFunctionScope funcs(scope.getScriptState());
    makePaymentRequestOriginSecure(scope.document());
    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), scope.getExceptionState());
    request->show(scope.getScriptState());
    request->abort(scope.getScriptState());
    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnAbort(false);

    request->abort(scope.getScriptState()).then(funcs.expectNoCall(), funcs.expectNoCall());
}

// If the browser successfully aborts the payment, then the request.show()
// promise should be rejected, and request.abort() promise should be resolved.
TEST(AbortTest, SuccessfulAbortShouldRejectShowPromiseAndResolveAbortPromise)
{
    V8TestingScope scope;
    PaymentRequestMockFunctionScope funcs(scope.getScriptState());
    makePaymentRequestOriginSecure(scope.document());
    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), scope.getExceptionState());

    request->show(scope.getScriptState()).then(funcs.expectNoCall(), funcs.expectCall());
    request->abort(scope.getScriptState()).then(funcs.expectCall(), funcs.expectNoCall());

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnAbort(true);
}

} // namespace
} // namespace blink
