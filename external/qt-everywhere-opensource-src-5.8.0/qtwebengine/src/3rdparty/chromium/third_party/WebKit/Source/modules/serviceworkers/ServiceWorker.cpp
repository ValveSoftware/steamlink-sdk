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

#include "modules/serviceworkers/ServiceWorker.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/MessagePort.h"
#include "core/events/Event.h"
#include "core/inspector/ConsoleMessage.h"
#include "modules/EventTargetModules.h"
#include "modules/serviceworkers/ServiceWorkerContainerClient.h"
#include "public/platform/WebMessagePortChannel.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/platform/WebString.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerState.h"
#include <memory>

namespace blink {

const AtomicString& ServiceWorker::interfaceName() const
{
    return EventTargetNames::ServiceWorker;
}

void ServiceWorker::postMessage(ExecutionContext* context, PassRefPtr<SerializedScriptValue> message, const MessagePortArray& ports, ExceptionState& exceptionState)
{
    ServiceWorkerContainerClient* client = ServiceWorkerContainerClient::from(getExecutionContext());
    if (!client || !client->provider()) {
        exceptionState.throwDOMException(InvalidStateError, "Failed to post a message: No associated provider is available.");
        return;
    }

    // Disentangle the port in preparation for sending it to the remote context.
    std::unique_ptr<MessagePortChannelArray> channels = MessagePort::disentanglePorts(context, ports, exceptionState);
    if (exceptionState.hadException())
        return;
    if (m_handle->serviceWorker()->state() == WebServiceWorkerStateRedundant) {
        exceptionState.throwDOMException(InvalidStateError, "ServiceWorker is in redundant state.");
        return;
    }

    if (message->containsTransferableArrayBuffer())
        context->addConsoleMessage(ConsoleMessage::create(JSMessageSource, WarningMessageLevel, "ServiceWorker cannot send an ArrayBuffer as a transferable object yet. See http://crbug.com/511119"));

    WebString messageString = message->toWireString();
    std::unique_ptr<WebMessagePortChannelArray> webChannels = MessagePort::toWebMessagePortChannelArray(std::move(channels));
    m_handle->serviceWorker()->postMessage(client->provider(), messageString, WebSecurityOrigin(getExecutionContext()->getSecurityOrigin()), webChannels.release());
}

void ServiceWorker::internalsTerminate()
{
    m_handle->serviceWorker()->terminate();
}

void ServiceWorker::dispatchStateChangeEvent()
{
    this->dispatchEvent(Event::create(EventTypeNames::statechange));
}

String ServiceWorker::scriptURL() const
{
    return m_handle->serviceWorker()->url().string();
}

String ServiceWorker::state() const
{
    switch (m_handle->serviceWorker()->state()) {
    case WebServiceWorkerStateUnknown:
        // The web platform should never see this internal state
        ASSERT_NOT_REACHED();
        return "unknown";
    case WebServiceWorkerStateInstalling:
        return "installing";
    case WebServiceWorkerStateInstalled:
        return "installed";
    case WebServiceWorkerStateActivating:
        return "activating";
    case WebServiceWorkerStateActivated:
        return "activated";
    case WebServiceWorkerStateRedundant:
        return "redundant";
    default:
        ASSERT_NOT_REACHED();
        return nullAtom;
    }
}

ServiceWorker* ServiceWorker::from(ExecutionContext* executionContext, std::unique_ptr<WebServiceWorker::Handle> handle)
{
    return getOrCreate(executionContext, std::move(handle));
}

bool ServiceWorker::hasPendingActivity() const
{
    if (m_wasStopped)
        return false;
    return m_handle->serviceWorker()->state() != WebServiceWorkerStateRedundant;
}

void ServiceWorker::stop()
{
    m_wasStopped = true;
}

ServiceWorker* ServiceWorker::getOrCreate(ExecutionContext* executionContext, std::unique_ptr<WebServiceWorker::Handle> handle)
{
    if (!handle)
        return nullptr;

    ServiceWorker* existingWorker = static_cast<ServiceWorker*>(handle->serviceWorker()->proxy());
    if (existingWorker) {
        ASSERT(existingWorker->getExecutionContext() == executionContext);
        return existingWorker;
    }

    ServiceWorker* newWorker = new ServiceWorker(executionContext, std::move(handle));
    newWorker->suspendIfNeeded();
    return newWorker;
}

ServiceWorker::ServiceWorker(ExecutionContext* executionContext, std::unique_ptr<WebServiceWorker::Handle> handle)
    : AbstractWorker(executionContext)
    , ActiveScriptWrappable(this)
    , m_handle(std::move(handle))
    , m_wasStopped(false)
{
    ASSERT(m_handle);
    m_handle->serviceWorker()->setProxy(this);
}

ServiceWorker::~ServiceWorker()
{
}

DEFINE_TRACE(ServiceWorker)
{
    AbstractWorker::trace(visitor);
}

} // namespace blink
