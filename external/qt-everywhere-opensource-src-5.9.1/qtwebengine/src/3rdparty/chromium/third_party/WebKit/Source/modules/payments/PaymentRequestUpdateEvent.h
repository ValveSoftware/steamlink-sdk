// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaymentRequestUpdateEvent_h
#define PaymentRequestUpdateEvent_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/events/Event.h"
#include "modules/ModulesExport.h"
#include "modules/payments/PaymentRequestUpdateEventInit.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"

namespace blink {

class ExceptionState;
class PaymentUpdater;
class ScriptState;

class MODULES_EXPORT PaymentRequestUpdateEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~PaymentRequestUpdateEvent() override;

  static PaymentRequestUpdateEvent* create(
      const AtomicString& type,
      const PaymentRequestUpdateEventInit& = PaymentRequestUpdateEventInit());

  void setPaymentDetailsUpdater(PaymentUpdater*);

  void updateWith(ScriptState*, ScriptPromise, ExceptionState&);

  DECLARE_VIRTUAL_TRACE();

  void onUpdateEventTimeoutForTesting();

 private:
  PaymentRequestUpdateEvent(const AtomicString& type,
                            const PaymentRequestUpdateEventInit&);

  void onUpdateEventTimeout(TimerBase*);

  Member<PaymentUpdater> m_updater;
  bool m_waitForUpdate;
  Timer<PaymentRequestUpdateEvent> m_abortTimer;
};

}  // namespace blink

#endif  // PaymentRequestUpdateEvent_h
