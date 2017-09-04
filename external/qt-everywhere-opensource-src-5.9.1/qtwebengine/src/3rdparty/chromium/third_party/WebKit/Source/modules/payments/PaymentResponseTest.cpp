// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentResponse.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "modules/payments/PaymentAddress.h"
#include "modules/payments/PaymentCompleter.h"
#include "modules/payments/PaymentTestHelper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>
#include <utility>

namespace blink {
namespace {

class MockPaymentCompleter
    : public GarbageCollectedFinalized<MockPaymentCompleter>,
      public PaymentCompleter {
  USING_GARBAGE_COLLECTED_MIXIN(MockPaymentCompleter);
  WTF_MAKE_NONCOPYABLE(MockPaymentCompleter);

 public:
  MockPaymentCompleter() {
    ON_CALL(*this, complete(testing::_, testing::_))
        .WillByDefault(testing::ReturnPointee(&m_dummyPromise));
  }

  ~MockPaymentCompleter() override {}

  MOCK_METHOD2(complete, ScriptPromise(ScriptState*, PaymentComplete result));

  DEFINE_INLINE_TRACE() {}

 private:
  ScriptPromise m_dummyPromise;
};

TEST(PaymentResponseTest, DataCopiedOver) {
  V8TestingScope scope;
  payments::mojom::blink::PaymentResponsePtr input =
      buildPaymentResponseForTest();
  input->method_name = "foo";
  input->stringified_details = "{\"transactionId\": 123}";
  input->shipping_option = "standardShippingOption";
  input->payer_name = "Jon Doe";
  input->payer_email = "abc@gmail.com";
  input->payer_phone = "0123";
  MockPaymentCompleter* completeCallback = new MockPaymentCompleter;

  PaymentResponse output(std::move(input), completeCallback);

  EXPECT_EQ("foo", output.methodName());
  EXPECT_EQ("standardShippingOption", output.shippingOption());
  EXPECT_EQ("Jon Doe", output.payerName());
  EXPECT_EQ("abc@gmail.com", output.payerEmail());
  EXPECT_EQ("0123", output.payerPhone());

  ScriptValue details =
      output.details(scope.getScriptState(), scope.getExceptionState());

  ASSERT_FALSE(scope.getExceptionState().hadException());
  ASSERT_TRUE(details.v8Value()->IsObject());

  ScriptValue transactionId(
      scope.getScriptState(),
      details.v8Value().As<v8::Object>()->Get(
          v8String(scope.getScriptState()->isolate(), "transactionId")));

  ASSERT_TRUE(transactionId.v8Value()->IsNumber());
  EXPECT_EQ(123, transactionId.v8Value().As<v8::Number>()->Value());
}

TEST(PaymentResponseTest, PaymentResponseDetailsJSONObject) {
  V8TestingScope scope;
  payments::mojom::blink::PaymentResponsePtr input =
      buildPaymentResponseForTest();
  input->stringified_details = "transactionId";
  MockPaymentCompleter* completeCallback = new MockPaymentCompleter;
  PaymentResponse output(std::move(input), completeCallback);

  ScriptValue details =
      output.details(scope.getScriptState(), scope.getExceptionState());

  ASSERT_TRUE(scope.getExceptionState().hadException());
}

TEST(PaymentResponseTest, CompleteCalledWithSuccess) {
  V8TestingScope scope;
  payments::mojom::blink::PaymentResponsePtr input =
      buildPaymentResponseForTest();
  input->method_name = "foo";
  input->stringified_details = "{\"transactionId\": 123}";
  MockPaymentCompleter* completeCallback = new MockPaymentCompleter;
  PaymentResponse output(std::move(input), completeCallback);

  EXPECT_CALL(*completeCallback,
              complete(scope.getScriptState(), PaymentCompleter::Success));

  output.complete(scope.getScriptState(), "success");
}

TEST(PaymentResponseTest, CompleteCalledWithFailure) {
  V8TestingScope scope;
  payments::mojom::blink::PaymentResponsePtr input =
      buildPaymentResponseForTest();
  input->method_name = "foo";
  input->stringified_details = "{\"transactionId\": 123}";
  MockPaymentCompleter* completeCallback = new MockPaymentCompleter;
  PaymentResponse output(std::move(input), completeCallback);

  EXPECT_CALL(*completeCallback,
              complete(scope.getScriptState(), PaymentCompleter::Fail));

  output.complete(scope.getScriptState(), "fail");
}

TEST(PaymentResponseTest, JSONSerializerTest) {
  V8TestingScope scope;
  payments::mojom::blink::PaymentResponsePtr input =
      payments::mojom::blink::PaymentResponse::New();
  input->method_name = "foo";
  input->stringified_details = "{\"transactionId\": 123}";
  input->shipping_option = "standardShippingOption";
  input->payer_email = "abc@gmail.com";
  input->payer_phone = "0123";
  input->payer_name = "Jon Doe";
  input->shipping_address = payments::mojom::blink::PaymentAddress::New();
  input->shipping_address->country = "US";
  input->shipping_address->language_code = "en";
  input->shipping_address->script_code = "Latn";
  input->shipping_address->address_line.append("340 Main St");
  input->shipping_address->address_line.append("BIN1");
  input->shipping_address->address_line.append("First floor");

  PaymentResponse output(std::move(input), new MockPaymentCompleter);
  ScriptValue jsonObject = output.toJSONForBinding(scope.getScriptState());
  EXPECT_TRUE(jsonObject.isObject());

  String jsonString = v8StringToWebCoreString<String>(
      v8::JSON::Stringify(scope.context(),
                          jsonObject.v8Value().As<v8::Object>())
          .ToLocalChecked(),
      DoNotExternalize);
  String expected =
      "{\"methodName\":\"foo\",\"details\":{\"transactionId\":123},"
      "\"shippingAddress\":{\"country\":\"US\",\"addressLine\":[\"340 Main "
      "St\","
      "\"BIN1\",\"First "
      "floor\"],\"region\":\"\",\"city\":\"\",\"dependentLocality\":"
      "\"\",\"postalCode\":\"\",\"sortingCode\":\"\",\"languageCode\":\"en-"
      "Latn\","
      "\"organization\":\"\",\"recipient\":\"\",\"phone\":\"\"},"
      "\"shippingOption\":"
      "\"standardShippingOption\",\"payerName\":\"Jon Doe\","
      "\"payerEmail\":\"abc@gmail.com\",\"payerPhone\":\"0123\"}";
  EXPECT_EQ(expected, jsonString);
}

}  // namespace
}  // namespace blink
