// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentResponse.h"

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "bindings/core/v8/V8ObjectBuilder.h"
#include "modules/payments/PaymentAddress.h"
#include "modules/payments/PaymentCompleter.h"
#include "wtf/Assertions.h"

namespace blink {

PaymentResponse::PaymentResponse(
    payments::mojom::blink::PaymentResponsePtr response,
    PaymentCompleter* paymentCompleter)
    : m_methodName(response->method_name),
      m_stringifiedDetails(response->stringified_details),
      m_shippingAddress(
          response->shipping_address
              ? new PaymentAddress(std::move(response->shipping_address))
              : nullptr),
      m_shippingOption(response->shipping_option),
      m_payerName(response->payer_name),
      m_payerEmail(response->payer_email),
      m_payerPhone(response->payer_phone),
      m_paymentCompleter(paymentCompleter) {
  DCHECK(m_paymentCompleter);
}

PaymentResponse::~PaymentResponse() {}

ScriptValue PaymentResponse::toJSONForBinding(ScriptState* scriptState) const {
  V8ObjectBuilder result(scriptState);
  result.addString("methodName", methodName());
  result.add("details", details(scriptState, ASSERT_NO_EXCEPTION));

  if (shippingAddress())
    result.add("shippingAddress",
               shippingAddress()->toJSONForBinding(scriptState));
  else
    result.addNull("shippingAddress");

  result.addStringOrNull("shippingOption", shippingOption())
      .addStringOrNull("payerName", payerName())
      .addStringOrNull("payerEmail", payerEmail())
      .addStringOrNull("payerPhone", payerPhone());

  return result.scriptValue();
}

ScriptValue PaymentResponse::details(ScriptState* scriptState,
                                     ExceptionState& exceptionState) const {
  return ScriptValue(
      scriptState, fromJSONString(scriptState->isolate(), m_stringifiedDetails,
                                  exceptionState));
}

ScriptPromise PaymentResponse::complete(ScriptState* scriptState,
                                        const String& result) {
  PaymentCompleter::PaymentComplete convertedResult = PaymentCompleter::Unknown;
  if (result == "success")
    convertedResult = PaymentCompleter::Success;
  else if (result == "fail")
    convertedResult = PaymentCompleter::Fail;
  return m_paymentCompleter->complete(scriptState, convertedResult);
}

DEFINE_TRACE(PaymentResponse) {
  visitor->trace(m_shippingAddress);
  visitor->trace(m_paymentCompleter);
}

}  // namespace blink
