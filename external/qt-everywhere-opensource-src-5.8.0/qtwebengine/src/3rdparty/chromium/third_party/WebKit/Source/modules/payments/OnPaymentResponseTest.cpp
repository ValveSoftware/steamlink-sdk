// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for PaymentRequest::OnPaymentResponse().

#include "bindings/core/v8/ScriptFunction.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "bindings/modules/v8/V8PaymentResponse.h"
#include "modules/payments/PaymentAddress.h"
#include "modules/payments/PaymentRequest.h"
#include "modules/payments/PaymentTestHelper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <utility>

namespace blink {
namespace {

// If the merchant requests shipping information, but the browser does not
// provide the shipping option, reject the show() promise.
TEST(OnPaymentResponseTest, RejectMissingShippingOption)
{
    V8TestingScope scope;
    PaymentRequestMockFunctionScope funcs(scope.getScriptState());
    makePaymentRequestOriginSecure(scope.document());
    PaymentOptions options;
    options.setRequestShipping(true);
    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), options, scope.getExceptionState());
    ASSERT_FALSE(scope.getExceptionState().hadException());
    mojom::blink::PaymentResponsePtr response = buildPaymentResponseForTest();
    response->shipping_address = mojom::blink::PaymentAddress::New();
    response->shipping_address->country = "US";
    response->shipping_address->language_code = "en";
    response->shipping_address->script_code = "Latn";

    request->show(scope.getScriptState()).then(funcs.expectNoCall(), funcs.expectCall());

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnPaymentResponse(std::move(response));
}

// If the merchant requests shipping information, but the browser does not
// provide a shipping address, reject the show() promise.
TEST(OnPaymentResponseTest, RejectMissingAddress)
{
    V8TestingScope scope;
    PaymentRequestMockFunctionScope funcs(scope.getScriptState());
    makePaymentRequestOriginSecure(scope.document());
    PaymentOptions options;
    options.setRequestShipping(true);
    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), options, scope.getExceptionState());
    ASSERT_FALSE(scope.getExceptionState().hadException());
    mojom::blink::PaymentResponsePtr response = buildPaymentResponseForTest();
    response->shipping_option = "standardShipping";

    request->show(scope.getScriptState()).then(funcs.expectNoCall(), funcs.expectCall());

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnPaymentResponse(std::move(response));
}

// If the merchant requests an email address, but the browser does not provide
// it, reject the show() promise.
TEST(PaymentRequestTest, RejectMissingEmail)
{
    V8TestingScope scope;
    PaymentRequestMockFunctionScope funcs(scope.getScriptState());
    makePaymentRequestOriginSecure(scope.document());
    PaymentOptions options;
    options.setRequestPayerEmail(true);
    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), options, scope.getExceptionState());
    EXPECT_FALSE(scope.getExceptionState().hadException());
    mojom::blink::PaymentResponsePtr response = mojom::blink::PaymentResponse::New();

    request->show(scope.getScriptState()).then(funcs.expectNoCall(), funcs.expectCall());

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnPaymentResponse(std::move(response));
}

// If the merchant requests a phone number, but the browser does not provide it,
// reject the show() promise.
TEST(PaymentRequestTest, RejectMissingPhone)
{
    V8TestingScope scope;
    PaymentRequestMockFunctionScope funcs(scope.getScriptState());
    makePaymentRequestOriginSecure(scope.document());
    PaymentOptions options;
    options.setRequestPayerPhone(true);
    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), options, scope.getExceptionState());
    EXPECT_FALSE(scope.getExceptionState().hadException());
    mojom::blink::PaymentResponsePtr response = mojom::blink::PaymentResponse::New();

    request->show(scope.getScriptState()).then(funcs.expectNoCall(), funcs.expectCall());

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnPaymentResponse(std::move(response));
}

// If the merchant requests shipping information, but the browser provides an
// empty string for shipping option, reject the show() promise.
TEST(OnPaymentResponseTest, RejectEmptyShippingOption)
{
    V8TestingScope scope;
    PaymentRequestMockFunctionScope funcs(scope.getScriptState());
    makePaymentRequestOriginSecure(scope.document());
    PaymentOptions options;
    options.setRequestShipping(true);
    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), options, scope.getExceptionState());
    ASSERT_FALSE(scope.getExceptionState().hadException());
    mojom::blink::PaymentResponsePtr response = buildPaymentResponseForTest();
    response->shipping_option = "";
    response->shipping_address = mojom::blink::PaymentAddress::New();
    response->shipping_address->country = "US";
    response->shipping_address->language_code = "en";
    response->shipping_address->script_code = "Latn";

    request->show(scope.getScriptState()).then(funcs.expectNoCall(), funcs.expectCall());

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnPaymentResponse(std::move(response));
}

// If the merchant requests shipping information, but the browser provides an
// empty shipping address, reject the show() promise.
TEST(OnPaymentResponseTest, RejectEmptyAddress)
{
    V8TestingScope scope;
    PaymentRequestMockFunctionScope funcs(scope.getScriptState());
    makePaymentRequestOriginSecure(scope.document());
    PaymentOptions options;
    options.setRequestShipping(true);
    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), options, scope.getExceptionState());
    ASSERT_FALSE(scope.getExceptionState().hadException());
    mojom::blink::PaymentResponsePtr response = buildPaymentResponseForTest();
    response->shipping_option = "standardShipping";
    response->shipping_address = mojom::blink::PaymentAddress::New();

    request->show(scope.getScriptState()).then(funcs.expectNoCall(), funcs.expectCall());

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnPaymentResponse(std::move(response));
}

// If the merchant requests an email, but the browser provides an empty string
// for email, reject the show() promise.
TEST(PaymentRequestTest, RejectEmptyEmail)
{
    V8TestingScope scope;
    PaymentRequestMockFunctionScope funcs(scope.getScriptState());
    makePaymentRequestOriginSecure(scope.document());
    PaymentOptions options;
    options.setRequestPayerEmail(true);
    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), options, scope.getExceptionState());
    EXPECT_FALSE(scope.getExceptionState().hadException());
    mojom::blink::PaymentResponsePtr response = mojom::blink::PaymentResponse::New();
    response->payer_email = "";

    request->show(scope.getScriptState()).then(funcs.expectNoCall(), funcs.expectCall());

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnPaymentResponse(std::move(response));
}

// If the merchant requests a phone number, but the browser provides an empty
// string for the phone number, reject the show() promise.
TEST(PaymentRequestTest, RejectEmptyPhone)
{
    V8TestingScope scope;
    PaymentRequestMockFunctionScope funcs(scope.getScriptState());
    makePaymentRequestOriginSecure(scope.document());
    PaymentOptions options;
    options.setRequestPayerPhone(true);
    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), options, scope.getExceptionState());
    EXPECT_FALSE(scope.getExceptionState().hadException());
    mojom::blink::PaymentResponsePtr response = mojom::blink::PaymentResponse::New();
    response->payer_phone = "";

    request->show(scope.getScriptState()).then(funcs.expectNoCall(), funcs.expectCall());

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnPaymentResponse(std::move(response));
}

// If the merchant does not request shipping information, but the browser
// provides a shipping address, reject the show() promise.
TEST(OnPaymentResponseTest, RejectNotRequestedAddress)
{
    V8TestingScope scope;
    PaymentRequestMockFunctionScope funcs(scope.getScriptState());
    makePaymentRequestOriginSecure(scope.document());
    PaymentOptions options;
    options.setRequestShipping(false);
    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), options, scope.getExceptionState());
    ASSERT_FALSE(scope.getExceptionState().hadException());
    mojom::blink::PaymentResponsePtr response = mojom::blink::PaymentResponse::New();
    response->shipping_address = mojom::blink::PaymentAddress::New();
    response->shipping_address->country = "US";
    response->shipping_address->language_code = "en";
    response->shipping_address->script_code = "Latn";

    request->show(scope.getScriptState()).then(funcs.expectNoCall(), funcs.expectCall());

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnPaymentResponse(std::move(response));
}

// If the merchant does not request shipping information, but the browser
// provides a shipping option, reject the show() promise.
TEST(OnPaymentResponseTest, RejectNotRequestedShippingOption)
{
    V8TestingScope scope;
    PaymentRequestMockFunctionScope funcs(scope.getScriptState());
    makePaymentRequestOriginSecure(scope.document());
    PaymentOptions options;
    options.setRequestShipping(false);
    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), options, scope.getExceptionState());
    ASSERT_FALSE(scope.getExceptionState().hadException());
    mojom::blink::PaymentResponsePtr response = mojom::blink::PaymentResponse::New();
    response->shipping_option = "";

    request->show(scope.getScriptState()).then(funcs.expectNoCall(), funcs.expectCall());

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnPaymentResponse(std::move(response));
}

// If the merchant does not request an email, but the browser provides it,
// reject the show() promise.
TEST(PaymentRequestTest, RejectNotRequestedEmail)
{
    V8TestingScope scope;
    PaymentRequestMockFunctionScope funcs(scope.getScriptState());
    makePaymentRequestOriginSecure(scope.document());
    PaymentOptions options;
    options.setRequestPayerEmail(false);
    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), options, scope.getExceptionState());
    EXPECT_FALSE(scope.getExceptionState().hadException());
    mojom::blink::PaymentResponsePtr response = mojom::blink::PaymentResponse::New();
    response->payer_email = "";

    request->show(scope.getScriptState()).then(funcs.expectNoCall(), funcs.expectCall());

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnPaymentResponse(std::move(response));
}

// If the merchant does not request a phone number, but the browser provides it,
// reject the show() promise.
TEST(PaymentRequestTest, RejectNotRequestedPhone)
{
    V8TestingScope scope;
    PaymentRequestMockFunctionScope funcs(scope.getScriptState());
    makePaymentRequestOriginSecure(scope.document());
    PaymentOptions options;
    options.setRequestPayerPhone(false);
    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), options, scope.getExceptionState());
    EXPECT_FALSE(scope.getExceptionState().hadException());
    mojom::blink::PaymentResponsePtr response = mojom::blink::PaymentResponse::New();
    response->payer_phone = "";

    request->show(scope.getScriptState()).then(funcs.expectNoCall(), funcs.expectCall());

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnPaymentResponse(std::move(response));
}

// If the merchant requests shipping information, but the browser provides an
// invalid shipping address, reject the show() promise.
TEST(OnPaymentResponseTest, RejectInvalidAddress)
{
    V8TestingScope scope;
    PaymentRequestMockFunctionScope funcs(scope.getScriptState());
    makePaymentRequestOriginSecure(scope.document());
    PaymentOptions options;
    options.setRequestShipping(true);
    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), options, scope.getExceptionState());
    ASSERT_FALSE(scope.getExceptionState().hadException());
    mojom::blink::PaymentResponsePtr response = buildPaymentResponseForTest();
    response->shipping_option = "standardShipping";
    response->shipping_address = mojom::blink::PaymentAddress::New();
    response->shipping_address->country = "Atlantis";

    request->show(scope.getScriptState()).then(funcs.expectNoCall(), funcs.expectCall());

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnPaymentResponse(std::move(response));
}

class PaymentResponseFunction : public ScriptFunction {
public:
    static v8::Local<v8::Function> create(ScriptState* scriptState, ScriptValue* outValue)
    {
        PaymentResponseFunction* self = new PaymentResponseFunction(scriptState, outValue);
        return self->bindToV8Function();
    }

private:
    PaymentResponseFunction(ScriptState* scriptState, ScriptValue* outValue)
        : ScriptFunction(scriptState)
        , m_value(outValue)
    {
        DCHECK(m_value);
    }

    ScriptValue call(ScriptValue value) override
    {
        DCHECK(!value.isEmpty());
        *m_value = value;
        return value;
    }

    ScriptValue* const m_value;
};

// If the merchant requests shipping information, the resolved show() promise
// should contain a shipping option and an address.
TEST(OnPaymentResponseTest, CanRequestShippingInformation)
{
    V8TestingScope scope;
    PaymentRequestMockFunctionScope funcs(scope.getScriptState());
    makePaymentRequestOriginSecure(scope.document());
    PaymentOptions options;
    options.setRequestShipping(true);
    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), options, scope.getExceptionState());
    ASSERT_FALSE(scope.getExceptionState().hadException());
    mojom::blink::PaymentResponsePtr response = buildPaymentResponseForTest();
    response->shipping_option = "standardShipping";
    response->shipping_address = mojom::blink::PaymentAddress::New();
    response->shipping_address->country = "US";
    response->shipping_address->language_code = "en";
    response->shipping_address->script_code = "Latn";
    ScriptValue outValue;
    request->show(scope.getScriptState()).then(PaymentResponseFunction::create(scope.getScriptState(), &outValue), funcs.expectNoCall());

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnPaymentResponse(std::move(response));

    v8::MicrotasksScope::PerformCheckpoint(scope.isolate());
    PaymentResponse* resp = V8PaymentResponse::toImplWithTypeCheck(scope.isolate(), outValue.v8Value());
    EXPECT_EQ("standardShipping", resp->shippingOption());
    EXPECT_EQ("US", resp->shippingAddress()->country());
    EXPECT_EQ("en-Latn", resp->shippingAddress()->languageCode());
}

// If the merchant requests an email address, the resolved show() promise should
// contain an email address.
TEST(PaymentRequestTest, CanRequestEmail)
{
    V8TestingScope scope;
    PaymentRequestMockFunctionScope funcs(scope.getScriptState());
    makePaymentRequestOriginSecure(scope.document());
    PaymentOptions options;
    options.setRequestPayerEmail(true);
    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), options, scope.getExceptionState());
    EXPECT_FALSE(scope.getExceptionState().hadException());
    mojom::blink::PaymentResponsePtr response = mojom::blink::PaymentResponse::New();
    response->payer_email = "abc@gmail.com";
    ScriptValue outValue;
    request->show(scope.getScriptState()).then(PaymentResponseFunction::create(scope.getScriptState(), &outValue), funcs.expectNoCall());

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnPaymentResponse(std::move(response));

    v8::MicrotasksScope::PerformCheckpoint(scope.isolate());
    PaymentResponse* pr = V8PaymentResponse::toImplWithTypeCheck(scope.isolate(), outValue.v8Value());
    EXPECT_EQ("abc@gmail.com", pr->payerEmail());
}

// If the merchant requests a phone number, the resolved show() promise should
// contain a phone number.
TEST(PaymentRequestTest, CanRequestPhone)
{
    V8TestingScope scope;
    PaymentRequestMockFunctionScope funcs(scope.getScriptState());
    makePaymentRequestOriginSecure(scope.document());
    PaymentOptions options;
    options.setRequestPayerPhone(true);
    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), options, scope.getExceptionState());
    EXPECT_FALSE(scope.getExceptionState().hadException());
    mojom::blink::PaymentResponsePtr response = mojom::blink::PaymentResponse::New();
    response->payer_phone = "0123";

    ScriptValue outValue;
    request->show(scope.getScriptState()).then(PaymentResponseFunction::create(scope.getScriptState(), &outValue), funcs.expectNoCall());

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnPaymentResponse(std::move(response));
    v8::MicrotasksScope::PerformCheckpoint(scope.isolate());
    PaymentResponse* pr = V8PaymentResponse::toImplWithTypeCheck(scope.isolate(), outValue.v8Value());

    EXPECT_EQ("0123", pr->payerPhone());
}

// If the merchant does not request shipping information, the resolved show()
// promise should contain null shipping option and address.
TEST(OnPaymentResponseTest, ShippingInformationNotRequired)
{
    V8TestingScope scope;
    PaymentRequestMockFunctionScope funcs(scope.getScriptState());
    makePaymentRequestOriginSecure(scope.document());
    PaymentOptions options;
    options.setRequestShipping(false);
    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), options, scope.getExceptionState());
    ASSERT_FALSE(scope.getExceptionState().hadException());
    ScriptValue outValue;
    request->show(scope.getScriptState()).then(PaymentResponseFunction::create(scope.getScriptState(), &outValue), funcs.expectNoCall());

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnPaymentResponse(buildPaymentResponseForTest());

    v8::MicrotasksScope::PerformCheckpoint(scope.isolate());
    PaymentResponse* resp = V8PaymentResponse::toImplWithTypeCheck(scope.isolate(), outValue.v8Value());
    EXPECT_TRUE(resp->shippingOption().isNull());
    EXPECT_EQ(nullptr, resp->shippingAddress());
}

// If the merchant does not request a phone number, the resolved show() promise
// should contain null phone number.
TEST(PaymentRequestTest, PhoneNotRequred)
{
    V8TestingScope scope;
    PaymentRequestMockFunctionScope funcs(scope.getScriptState());
    makePaymentRequestOriginSecure(scope.document());
    PaymentOptions options;
    options.setRequestPayerPhone(false);
    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), options, scope.getExceptionState());
    EXPECT_FALSE(scope.getExceptionState().hadException());
    mojom::blink::PaymentResponsePtr response = mojom::blink::PaymentResponse::New();
    response->payer_phone = String();
    ScriptValue outValue;
    request->show(scope.getScriptState()).then(PaymentResponseFunction::create(scope.getScriptState(), &outValue), funcs.expectNoCall());

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnPaymentResponse(std::move(response));

    v8::MicrotasksScope::PerformCheckpoint(scope.isolate());
    PaymentResponse* pr = V8PaymentResponse::toImplWithTypeCheck(scope.isolate(), outValue.v8Value());
    EXPECT_TRUE(pr->payerPhone().isNull());
}

// If the merchant does not request an email address, the resolved show()
// promise should contain null email address.
TEST(PaymentRequestTest, EmailNotRequired)
{
    V8TestingScope scope;
    PaymentRequestMockFunctionScope funcs(scope.getScriptState());
    makePaymentRequestOriginSecure(scope.document());
    PaymentOptions options;
    options.setRequestPayerEmail(false);
    PaymentRequest* request = PaymentRequest::create(scope.getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), options, scope.getExceptionState());
    EXPECT_FALSE(scope.getExceptionState().hadException());
    mojom::blink::PaymentResponsePtr response = mojom::blink::PaymentResponse::New();
    response->payer_email = String();
    ScriptValue outValue;
    request->show(scope.getScriptState()).then(PaymentResponseFunction::create(scope.getScriptState(), &outValue), funcs.expectNoCall());

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnPaymentResponse(std::move(response));

    v8::MicrotasksScope::PerformCheckpoint(scope.isolate());
    PaymentResponse* pr = V8PaymentResponse::toImplWithTypeCheck(scope.isolate(), outValue.v8Value());
    EXPECT_TRUE(pr->payerEmail().isNull());
}

} // namespace
} // namespace blink
