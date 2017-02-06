// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaymentResponse_h
#define PaymentResponse_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/ModulesExport.h"
#include "modules/payments/PaymentCurrencyAmount.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/payments/payment_request.mojom-blink.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExceptionState;
class PaymentAddress;
class PaymentCompleter;
class ScriptState;

class MODULES_EXPORT PaymentResponse final : public GarbageCollectedFinalized<PaymentResponse>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
    WTF_MAKE_NONCOPYABLE(PaymentResponse);

public:
    PaymentResponse(mojom::blink::PaymentResponsePtr, PaymentCompleter*);
    virtual ~PaymentResponse();

    const String& methodName() const { return m_methodName; }
    ScriptValue details(ScriptState*, ExceptionState&) const;
    PaymentAddress* shippingAddress() const { return m_shippingAddress.get(); }
    const String& shippingOption() const { return m_shippingOption; }
    const String& payerEmail() const { return m_payerEmail; }
    const String& payerPhone() const { return m_payerPhone; }

    ScriptPromise complete(ScriptState*, const String& result = "");

    DECLARE_TRACE();

private:
    String m_methodName;
    String m_stringifiedDetails;
    Member<PaymentAddress> m_shippingAddress;
    String m_shippingOption;
    String m_payerEmail;
    String m_payerPhone;
    Member<PaymentCompleter> m_paymentCompleter;
};

} // namespace blink

#endif // PaymentResponse_h
