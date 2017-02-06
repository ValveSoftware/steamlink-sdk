// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentRequestUpdateEvent.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptFunction.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "modules/payments/PaymentUpdater.h"

namespace blink {
namespace {

class UpdatePaymentDetailsFunction : public ScriptFunction {
public:
    static v8::Local<v8::Function> createFunction(ScriptState* scriptState, PaymentUpdater* updater)
    {
        UpdatePaymentDetailsFunction* self = new UpdatePaymentDetailsFunction(scriptState, updater);
        return self->bindToV8Function();
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_updater);
        ScriptFunction::trace(visitor);
    }

private:
    UpdatePaymentDetailsFunction(ScriptState* scriptState, PaymentUpdater* updater)
        : ScriptFunction(scriptState)
        , m_updater(updater)
    {
        DCHECK(m_updater);
    }

    ScriptValue call(ScriptValue value) override
    {
        m_updater->onUpdatePaymentDetails(value);
        return ScriptValue();
    }

    Member<PaymentUpdater> m_updater;
};

class UpdatePaymentDetailsErrorFunction : public ScriptFunction {
public:
    static v8::Local<v8::Function> createFunction(ScriptState* scriptState, PaymentUpdater* updater)
    {
        UpdatePaymentDetailsErrorFunction* self = new UpdatePaymentDetailsErrorFunction(scriptState, updater);
        return self->bindToV8Function();
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_updater);
        ScriptFunction::trace(visitor);
    }

private:
    UpdatePaymentDetailsErrorFunction(ScriptState* scriptState, PaymentUpdater* updater)
        : ScriptFunction(scriptState)
        , m_updater(updater)
    {
        DCHECK(m_updater);
    }

    ScriptValue call(ScriptValue value) override
    {
        m_updater->onUpdatePaymentDetailsFailure(value);
        return ScriptValue();
    }

    Member<PaymentUpdater> m_updater;
};

} // namespace

PaymentRequestUpdateEvent::~PaymentRequestUpdateEvent()
{
}

PaymentRequestUpdateEvent* PaymentRequestUpdateEvent::create()
{
    return new PaymentRequestUpdateEvent();
}

PaymentRequestUpdateEvent* PaymentRequestUpdateEvent::create(const AtomicString& type, const PaymentRequestUpdateEventInit& init)
{
    return new PaymentRequestUpdateEvent(type, init);
}

void PaymentRequestUpdateEvent::setPaymentDetailsUpdater(PaymentUpdater* updater)
{
    m_updater = updater;
}

void PaymentRequestUpdateEvent::updateWith(ScriptState* scriptState, ScriptPromise promise, ExceptionState& exceptionState)
{
    if (!m_updater)
        return;

    if (!isBeingDispatched()) {
        exceptionState.throwDOMException(InvalidStateError, "Cannot update details when the event is not being dispatched");
        return;
    }

    if (m_waitForUpdate) {
        exceptionState.throwDOMException(InvalidStateError, "Cannot update details twice");
        return;
    }

    stopImmediatePropagation();
    m_waitForUpdate = true;

    promise.then(UpdatePaymentDetailsFunction::createFunction(scriptState, m_updater),
        UpdatePaymentDetailsErrorFunction::createFunction(scriptState, m_updater));
}

DEFINE_TRACE(PaymentRequestUpdateEvent)
{
    visitor->trace(m_updater);
    Event::trace(visitor);
}

PaymentRequestUpdateEvent::PaymentRequestUpdateEvent()
    : m_waitForUpdate(false)
{
}

PaymentRequestUpdateEvent::PaymentRequestUpdateEvent(const AtomicString& type, const PaymentRequestUpdateEventInit& init)
    : Event(type, init)
    , m_waitForUpdate(false)
{
}

} // namespace blink
