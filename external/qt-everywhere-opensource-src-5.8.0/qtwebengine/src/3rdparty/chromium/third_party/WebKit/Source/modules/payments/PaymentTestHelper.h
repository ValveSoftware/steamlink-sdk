// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaymentTestHelper_h
#define PaymentTestHelper_h

#include "bindings/core/v8/ScriptFunction.h"
#include "bindings/core/v8/V8DOMException.h"
#include "modules/payments/PaymentDetails.h"
#include "modules/payments/PaymentItem.h"
#include "modules/payments/PaymentShippingOption.h"
#include "platform/heap/HeapAllocator.h"
#include "platform/heap/Persistent.h"
#include "public/platform/modules/payments/payment_request.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "wtf/Allocator.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

class Document;
class PaymentMethodData;
class ScriptState;
class ScriptValue;

enum PaymentTestDetailToChange {
    PaymentTestDetailNone,
    PaymentTestDetailTotal,
    PaymentTestDetailItem,
    PaymentTestDetailShippingOption,
    PaymentTestDetailModifierTotal,
    PaymentTestDetailModifierItem
};

enum PaymentTestDataToChange {
    PaymentTestDataNone,
    PaymentTestDataId,
    PaymentTestDataLabel,
    PaymentTestDataAmount,
    PaymentTestDataCurrencyCode,
    PaymentTestDataValue,
};

enum PaymentTestModificationType {
    PaymentTestOverwriteValue,
    PaymentTestRemoveKey
};

PaymentItem buildPaymentItemForTest(PaymentTestDataToChange = PaymentTestDataNone, PaymentTestModificationType = PaymentTestOverwriteValue, const String& valueToUse = String());

PaymentShippingOption buildShippingOptionForTest(PaymentTestDataToChange = PaymentTestDataNone, PaymentTestModificationType = PaymentTestOverwriteValue, const String& valueToUse = String());

PaymentDetailsModifier buildPaymentDetailsModifierForTest(PaymentTestDetailToChange = PaymentTestDetailNone, PaymentTestDataToChange = PaymentTestDataNone, PaymentTestModificationType = PaymentTestOverwriteValue, const String& valueToUse = String());

PaymentDetails buildPaymentDetailsForTest(PaymentTestDetailToChange = PaymentTestDetailNone, PaymentTestDataToChange = PaymentTestDataNone, PaymentTestModificationType = PaymentTestOverwriteValue, const String& valueToUse = String());

HeapVector<PaymentMethodData> buildPaymentMethodDataForTest();

mojom::blink::PaymentResponsePtr buildPaymentResponseForTest();

void makePaymentRequestOriginSecure(Document&);

class PaymentRequestMockFunctionScope {
    STACK_ALLOCATED();
public:
    explicit PaymentRequestMockFunctionScope(ScriptState*);
    ~PaymentRequestMockFunctionScope();

    v8::Local<v8::Function> expectCall();
    v8::Local<v8::Function> expectCall(String* captor);
    v8::Local<v8::Function> expectNoCall();

private:
    class MockFunction : public ScriptFunction {
    public:
        explicit MockFunction(ScriptState*);
        MockFunction(ScriptState*, String* captor);
        v8::Local<v8::Function> bind();
        MOCK_METHOD1(call, ScriptValue(ScriptValue));
        String* m_value;
    };

    ScriptState* m_scriptState;
    Vector<Persistent<MockFunction>> m_mockFunctions;
};

} // namespace blink

#endif // PaymentTestHelper_h
