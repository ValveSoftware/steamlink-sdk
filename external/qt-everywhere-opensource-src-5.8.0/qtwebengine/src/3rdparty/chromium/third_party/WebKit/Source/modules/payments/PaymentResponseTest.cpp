// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentResponse.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "modules/payments/PaymentCompleter.h"
#include "modules/payments/PaymentTestHelper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>
#include <utility>

namespace blink {
namespace {

class MockPaymentCompleter : public GarbageCollectedFinalized<MockPaymentCompleter>, public PaymentCompleter {
    USING_GARBAGE_COLLECTED_MIXIN(MockPaymentCompleter);
    WTF_MAKE_NONCOPYABLE(MockPaymentCompleter);

public:
    MockPaymentCompleter()
    {
        ON_CALL(*this, complete(testing::_, testing::_))
            .WillByDefault(testing::ReturnPointee(&m_dummyPromise));
    }

    ~MockPaymentCompleter() override {}

    MOCK_METHOD2(complete, ScriptPromise(ScriptState*, PaymentComplete result));

    DEFINE_INLINE_TRACE() {}

private:
    ScriptPromise m_dummyPromise;
};

TEST(PaymentResponseTest, DataCopiedOver)
{
    V8TestingScope scope;
    mojom::blink::PaymentResponsePtr input = buildPaymentResponseForTest();
    input->method_name = "foo";
    input->stringified_details = "{\"transactionId\": 123}";
    input->shipping_option = "standardShippingOption";
    input->payer_email = "abc@gmail.com";
    input->payer_phone = "0123";
    MockPaymentCompleter* completeCallback = new MockPaymentCompleter;

    PaymentResponse output(std::move(input), completeCallback);

    EXPECT_EQ("foo", output.methodName());
    EXPECT_EQ("standardShippingOption", output.shippingOption());
    EXPECT_EQ("abc@gmail.com", output.payerEmail());
    EXPECT_EQ("0123", output.payerPhone());

    ScriptValue details = output.details(scope.getScriptState(), scope.getExceptionState());

    ASSERT_FALSE(scope.getExceptionState().hadException());
    ASSERT_TRUE(details.v8Value()->IsObject());

    ScriptValue transactionId(scope.getScriptState(), details.v8Value().As<v8::Object>()->Get(v8String(scope.getScriptState()->isolate(), "transactionId")));

    ASSERT_TRUE(transactionId.v8Value()->IsNumber());
    EXPECT_EQ(123, transactionId.v8Value().As<v8::Number>()->Value());
}

TEST(PaymentResponseTest, CompleteCalledWithSuccess)
{
    V8TestingScope scope;
    mojom::blink::PaymentResponsePtr input = buildPaymentResponseForTest();
    input->method_name = "foo";
    input->stringified_details = "{\"transactionId\": 123}";
    MockPaymentCompleter* completeCallback = new MockPaymentCompleter;
    PaymentResponse output(std::move(input), completeCallback);

    EXPECT_CALL(*completeCallback, complete(scope.getScriptState(), Success));

    output.complete(scope.getScriptState(), "success");
}

TEST(PaymentResponseTest, CompleteCalledWithFailure)
{
    V8TestingScope scope;
    mojom::blink::PaymentResponsePtr input = buildPaymentResponseForTest();
    input->method_name = "foo";
    input->stringified_details = "{\"transactionId\": 123}";
    MockPaymentCompleter* completeCallback = new MockPaymentCompleter;
    PaymentResponse output(std::move(input), completeCallback);

    EXPECT_CALL(*completeCallback, complete(scope.getScriptState(), Fail));

    output.complete(scope.getScriptState(), "fail");
}

} // namespace
} // namespace blink
