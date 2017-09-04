// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaymentRequest_h
#define PaymentRequest_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "components/payments/payment_request.mojom-blink.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/events/EventTarget.h"
#include "modules/ModulesExport.h"
#include "modules/payments/PaymentCompleter.h"
#include "modules/payments/PaymentDetails.h"
#include "modules/payments/PaymentMethodData.h"
#include "modules/payments/PaymentOptions.h"
#include "modules/payments/PaymentUpdater.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"
#include "wtf/Compiler.h"
#include "wtf/Noncopyable.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExceptionState;
class PaymentAddress;
class ScriptPromiseResolver;
class ScriptState;

class MODULES_EXPORT PaymentRequest final
    : public EventTargetWithInlineData,
      WTF_NON_EXPORTED_BASE(
          public payments::mojom::blink::PaymentRequestClient),
      public PaymentCompleter,
      public PaymentUpdater,
      public ContextLifecycleObserver,
      public ActiveScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(PaymentRequest)
  WTF_MAKE_NONCOPYABLE(PaymentRequest);

 public:
  static PaymentRequest* create(ScriptState*,
                                const HeapVector<PaymentMethodData>&,
                                const PaymentDetails&,
                                ExceptionState&);
  static PaymentRequest* create(ScriptState*,
                                const HeapVector<PaymentMethodData>&,
                                const PaymentDetails&,
                                const PaymentOptions&,
                                ExceptionState&);

  virtual ~PaymentRequest();

  ScriptPromise show(ScriptState*);
  ScriptPromise abort(ScriptState*);

  PaymentAddress* getShippingAddress() const { return m_shippingAddress.get(); }
  const String& shippingOption() const { return m_shippingOption; }
  const String& shippingType() const { return m_shippingType; }

  DEFINE_ATTRIBUTE_EVENT_LISTENER(shippingaddresschange);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(shippingoptionchange);

  ScriptPromise canMakePayment(ScriptState*);

  // ScriptWrappable:
  bool hasPendingActivity() const override;

  // EventTargetWithInlineData:
  const AtomicString& interfaceName() const override;
  ExecutionContext* getExecutionContext() const override;

  // PaymentCompleter:
  ScriptPromise complete(ScriptState*, PaymentComplete result) override;

  // PaymentUpdater:
  void onUpdatePaymentDetails(const ScriptValue& detailsScriptValue) override;
  void onUpdatePaymentDetailsFailure(const String& error) override;

  DECLARE_TRACE();

  void onCompleteTimeoutForTesting();

 private:
  PaymentRequest(ScriptState*,
                 const HeapVector<PaymentMethodData>&,
                 const PaymentDetails&,
                 const PaymentOptions&,
                 ExceptionState&);

  // LifecycleObserver:
  void contextDestroyed() override;

  // payments::mojom::blink::PaymentRequestClient:
  void OnShippingAddressChange(
      payments::mojom::blink::PaymentAddressPtr) override;
  void OnShippingOptionChange(const String& shippingOptionId) override;
  void OnPaymentResponse(payments::mojom::blink::PaymentResponsePtr) override;
  void OnError(payments::mojom::blink::PaymentErrorReason) override;
  void OnComplete() override;
  void OnAbort(bool abortedSuccessfully) override;
  void OnCanMakePayment(
      payments::mojom::blink::CanMakePaymentQueryResult) override;

  void onCompleteTimeout(TimerBase*);

  // Clears the promise resolvers and closes the Mojo connection.
  void clearResolversAndCloseMojoConnection();

  PaymentOptions m_options;
  Member<PaymentAddress> m_shippingAddress;
  String m_shippingOption;
  String m_shippingType;
  Member<ScriptPromiseResolver> m_showResolver;
  Member<ScriptPromiseResolver> m_completeResolver;
  Member<ScriptPromiseResolver> m_abortResolver;
  Member<ScriptPromiseResolver> m_canMakePaymentResolver;
  payments::mojom::blink::PaymentRequestPtr m_paymentProvider;
  mojo::Binding<payments::mojom::blink::PaymentRequestClient> m_clientBinding;
  Timer<PaymentRequest> m_completeTimer;
};

}  // namespace blink

#endif  // PaymentRequest_h
