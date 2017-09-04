// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentRequestUpdateEvent.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptFunction.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "modules/payments/PaymentUpdater.h"
#include "public/platform/WebTraceLocation.h"
#include "wtf/text/WTFString.h"

namespace blink {
namespace {

// Reject the payment request if the page does not resolve the promise from
// updateWith within 60 seconds.
static const int abortTimeout = 60;

class UpdatePaymentDetailsFunction : public ScriptFunction {
 public:
  static v8::Local<v8::Function> createFunction(ScriptState* scriptState,
                                                PaymentUpdater* updater) {
    UpdatePaymentDetailsFunction* self =
        new UpdatePaymentDetailsFunction(scriptState, updater);
    return self->bindToV8Function();
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(m_updater);
    ScriptFunction::trace(visitor);
  }

 private:
  UpdatePaymentDetailsFunction(ScriptState* scriptState,
                               PaymentUpdater* updater)
      : ScriptFunction(scriptState), m_updater(updater) {
    DCHECK(m_updater);
  }

  ScriptValue call(ScriptValue value) override {
    m_updater->onUpdatePaymentDetails(value);
    return ScriptValue();
  }

  Member<PaymentUpdater> m_updater;
};

class UpdatePaymentDetailsErrorFunction : public ScriptFunction {
 public:
  static v8::Local<v8::Function> createFunction(ScriptState* scriptState,
                                                PaymentUpdater* updater) {
    UpdatePaymentDetailsErrorFunction* self =
        new UpdatePaymentDetailsErrorFunction(scriptState, updater);
    return self->bindToV8Function();
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(m_updater);
    ScriptFunction::trace(visitor);
  }

 private:
  UpdatePaymentDetailsErrorFunction(ScriptState* scriptState,
                                    PaymentUpdater* updater)
      : ScriptFunction(scriptState), m_updater(updater) {
    DCHECK(m_updater);
  }

  ScriptValue call(ScriptValue value) override {
    m_updater->onUpdatePaymentDetailsFailure(
        toCoreString(value.v8Value()
                         ->ToString(getScriptState()->context())
                         .ToLocalChecked()));
    return ScriptValue();
  }

  Member<PaymentUpdater> m_updater;
};

}  // namespace

PaymentRequestUpdateEvent::~PaymentRequestUpdateEvent() {}

PaymentRequestUpdateEvent* PaymentRequestUpdateEvent::create(
    const AtomicString& type,
    const PaymentRequestUpdateEventInit& init) {
  return new PaymentRequestUpdateEvent(type, init);
}

void PaymentRequestUpdateEvent::setPaymentDetailsUpdater(
    PaymentUpdater* updater) {
  DCHECK(!m_abortTimer.isActive());
  m_abortTimer.startOneShot(abortTimeout, BLINK_FROM_HERE);
  m_updater = updater;
}

void PaymentRequestUpdateEvent::updateWith(ScriptState* scriptState,
                                           ScriptPromise promise,
                                           ExceptionState& exceptionState) {
  if (!m_updater)
    return;

  if (!isBeingDispatched()) {
    exceptionState.throwDOMException(
        InvalidStateError,
        "Cannot update details when the event is not being dispatched");
    return;
  }

  if (m_waitForUpdate) {
    exceptionState.throwDOMException(InvalidStateError,
                                     "Cannot update details twice");
    return;
  }

  stopPropagation();
  stopImmediatePropagation();
  m_waitForUpdate = true;
  m_abortTimer.stop();

  promise.then(
      UpdatePaymentDetailsFunction::createFunction(scriptState, m_updater),
      UpdatePaymentDetailsErrorFunction::createFunction(scriptState,
                                                        m_updater));
}

DEFINE_TRACE(PaymentRequestUpdateEvent) {
  visitor->trace(m_updater);
  Event::trace(visitor);
}

void PaymentRequestUpdateEvent::onUpdateEventTimeoutForTesting() {
  onUpdateEventTimeout(0);
}

void PaymentRequestUpdateEvent::onUpdateEventTimeout(TimerBase*) {
  if (!m_updater)
    return;

  m_updater->onUpdatePaymentDetailsFailure(
      "Timed out as the page didn't resolve the promise from change event");
}

PaymentRequestUpdateEvent::PaymentRequestUpdateEvent(
    const AtomicString& type,
    const PaymentRequestUpdateEventInit& init)
    : Event(type, init),
      m_waitForUpdate(false),
      m_abortTimer(this, &PaymentRequestUpdateEvent::onUpdateEventTimeout) {}

}  // namespace blink
