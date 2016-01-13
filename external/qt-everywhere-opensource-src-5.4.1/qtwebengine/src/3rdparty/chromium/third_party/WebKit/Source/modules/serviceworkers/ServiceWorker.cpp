/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ServiceWorker.h"

#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ScriptPromiseResolverWithContext.h"
#include "bindings/v8/ScriptState.h"
#include "core/dom/MessagePort.h"
#include "modules/EventTargetModules.h"
#include "platform/NotImplemented.h"
#include "public/platform/WebMessagePortChannel.h"
#include "public/platform/WebServiceWorkerState.h"
#include "public/platform/WebString.h"
#include <v8.h>

namespace WebCore {

class ServiceWorker::ThenFunction FINAL : public ScriptFunction {
public:
    static PassOwnPtr<ScriptFunction> create(PassRefPtr<ServiceWorker> observer)
    {
        ExecutionContext* executionContext = observer->executionContext();
        return adoptPtr(new ThenFunction(toIsolate(executionContext), observer));
    }
private:
    ThenFunction(v8::Isolate* isolate, PassRefPtr<ServiceWorker> observer)
        : ScriptFunction(isolate)
        , m_observer(observer)
    {
    }

    virtual ScriptValue call(ScriptValue value) OVERRIDE
    {
        m_observer->onPromiseResolved();
        return value;
    }

    RefPtr<ServiceWorker> m_observer;
};

const AtomicString& ServiceWorker::interfaceName() const
{
    return EventTargetNames::ServiceWorker;
}

void ServiceWorker::postMessage(PassRefPtr<SerializedScriptValue> message, const MessagePortArray* ports, ExceptionState& exceptionState)
{
    // Disentangle the port in preparation for sending it to the remote context.
    OwnPtr<MessagePortChannelArray> channels = MessagePort::disentanglePorts(ports, exceptionState);
    if (exceptionState.hadException())
        return;

    blink::WebString messageString = message->toWireString();
    OwnPtr<blink::WebMessagePortChannelArray> webChannels = MessagePort::toWebMessagePortChannelArray(channels.release());
    m_outerWorker->postMessage(messageString, webChannels.leakPtr());
}

bool ServiceWorker::isReady()
{
    return m_proxyState == Ready;
}

void ServiceWorker::dispatchStateChangeEvent()
{
    ASSERT(isReady());
    this->dispatchEvent(Event::create(EventTypeNames::statechange));
}

String ServiceWorker::scope() const
{
    return m_outerWorker->scope().string();
}

String ServiceWorker::url() const
{
    return m_outerWorker->url().string();
}

const AtomicString& ServiceWorker::state() const
{
    DEFINE_STATIC_LOCAL(AtomicString, unknown, ("unknown", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(AtomicString, parsed, ("parsed", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(AtomicString, installing, ("installing", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(AtomicString, installed, ("installed", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(AtomicString, activating, ("activating", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(AtomicString, active, ("active", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(AtomicString, deactivated, ("deactivated", AtomicString::ConstructFromLiteral));

    switch (m_outerWorker->state()) {
    case blink::WebServiceWorkerStateUnknown:
        // The web platform should never see this internal state
        ASSERT_NOT_REACHED();
        return unknown;
    case blink::WebServiceWorkerStateParsed:
        return parsed;
    case blink::WebServiceWorkerStateInstalling:
        return installing;
    case blink::WebServiceWorkerStateInstalled:
        return installed;
    case blink::WebServiceWorkerStateActivating:
        return activating;
    case blink::WebServiceWorkerStateActive:
        return active;
    case blink::WebServiceWorkerStateDeactivated:
        return deactivated;
    default:
        ASSERT_NOT_REACHED();
        return nullAtom;
    }
}

PassRefPtr<ServiceWorker> ServiceWorker::from(ExecutionContext* executionContext, WebType* worker)
{
    if (!worker)
        return PassRefPtr<ServiceWorker>();

    blink::WebServiceWorkerProxy* proxy = worker->proxy();
    ServiceWorker* existingServiceWorker = proxy ? proxy->unwrap() : 0;
    if (existingServiceWorker) {
        ASSERT(existingServiceWorker->executionContext() == executionContext);
        return existingServiceWorker;
    }

    return create(executionContext, adoptPtr(worker));
}

PassRefPtr<ServiceWorker> ServiceWorker::from(ScriptPromiseResolverWithContext* resolver, WebType* worker)
{
    RefPtr<ServiceWorker> serviceWorker = ServiceWorker::from(resolver->scriptState()->executionContext(), worker);
    ScriptState::Scope scope(resolver->scriptState());
    serviceWorker->waitOnPromise(resolver->promise());
    return serviceWorker;
}

void ServiceWorker::setProxyState(ProxyState state)
{
    if (m_proxyState == state)
        return;
    switch (m_proxyState) {
    case Initial:
        ASSERT(state == RegisterPromisePending || state == ContextStopped);
        break;
    case RegisterPromisePending:
        ASSERT(state == Ready || state == ContextStopped);
        break;
    case Ready:
        ASSERT(state == ContextStopped);
        break;
    case ContextStopped:
        ASSERT_NOT_REACHED();
        break;
    }

    ProxyState oldState = m_proxyState;
    m_proxyState = state;
    if (oldState == Ready || state == Ready)
        m_outerWorker->proxyReadyChanged();
}

void ServiceWorker::onPromiseResolved()
{
    if (m_proxyState == ContextStopped)
        return;
    setProxyState(Ready);
}

void ServiceWorker::waitOnPromise(ScriptPromise promise)
{
    if (promise.isEmpty()) {
        // The document was detached during registration. The state doesn't really
        // matter since this ServiceWorker will immediately die.
        setProxyState(ContextStopped);
        return;
    }
    setProxyState(RegisterPromisePending);
    promise.then(ThenFunction::create(this));
}

bool ServiceWorker::hasPendingActivity() const
{
    if (AbstractWorker::hasPendingActivity())
        return true;
    if (m_proxyState == ContextStopped)
        return false;
    return m_outerWorker->state() != blink::WebServiceWorkerStateDeactivated;
}

void ServiceWorker::stop()
{
    setProxyState(ContextStopped);
}

PassRefPtr<ServiceWorker> ServiceWorker::create(ExecutionContext* executionContext, PassOwnPtr<blink::WebServiceWorker> outerWorker)
{
    RefPtr<ServiceWorker> worker = adoptRef(new ServiceWorker(executionContext, outerWorker));
    worker->suspendIfNeeded();
    return worker.release();
}

ServiceWorker::ServiceWorker(ExecutionContext* executionContext, PassOwnPtr<blink::WebServiceWorker> worker)
    : AbstractWorker(executionContext)
    , WebServiceWorkerProxy(this)
    , m_outerWorker(worker)
    , m_proxyState(Initial)
{
    ScriptWrappable::init(this);
    ASSERT(m_outerWorker);
    m_outerWorker->setProxy(this);
}

} // namespace WebCore
