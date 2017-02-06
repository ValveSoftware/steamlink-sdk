/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "core/dom/MessagePort.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "bindings/core/v8/SerializedScriptValueFactory.h"
#include "core/dom/CrossThreadTask.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/MessageEvent.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/workers/WorkerGlobalScope.h"
#include "public/platform/WebString.h"
#include "wtf/Functional.h"
#include "wtf/PtrUtil.h"
#include "wtf/text/AtomicString.h"
#include <memory>

namespace blink {

MessagePort* MessagePort::create(ExecutionContext& executionContext)
{
    MessagePort* port = new MessagePort(executionContext);
    port->suspendIfNeeded();
    return port;
}

MessagePort::MessagePort(ExecutionContext& executionContext)
    : ActiveScriptWrappable(this)
    , ActiveDOMObject(&executionContext)
    , m_started(false)
    , m_closed(false)
{
}

MessagePort::~MessagePort()
{
    DCHECK(!m_started || !isEntangled());
    if (m_scriptStateForConversion)
        m_scriptStateForConversion->disposePerContextData();
}

void MessagePort::postMessage(ExecutionContext* context, PassRefPtr<SerializedScriptValue> message, const MessagePortArray& ports, ExceptionState& exceptionState)
{
    if (!isEntangled())
        return;
    DCHECK(getExecutionContext());
    DCHECK(m_entangledChannel);

    // Make sure we aren't connected to any of the passed-in ports.
    for (unsigned i = 0; i < ports.size(); ++i) {
        if (ports[i] == this) {
            exceptionState.throwDOMException(DataCloneError, "Port at index " + String::number(i) + " contains the source port.");
            return;
        }
    }
    std::unique_ptr<MessagePortChannelArray> channels = MessagePort::disentanglePorts(context, ports, exceptionState);
    if (exceptionState.hadException())
        return;

    if (message->containsTransferableArrayBuffer())
        getExecutionContext()->addConsoleMessage(ConsoleMessage::create(JSMessageSource, WarningMessageLevel, "MessagePort cannot send an ArrayBuffer as a transferable object yet. See http://crbug.com/334408"));

    WebString messageString = message->toWireString();
    std::unique_ptr<WebMessagePortChannelArray> webChannels = toWebMessagePortChannelArray(std::move(channels));
    m_entangledChannel->postMessage(messageString, webChannels.release());
}

// static
std::unique_ptr<WebMessagePortChannelArray> MessagePort::toWebMessagePortChannelArray(std::unique_ptr<MessagePortChannelArray> channels)
{
    std::unique_ptr<WebMessagePortChannelArray> webChannels;
    if (channels && channels->size()) {
        webChannels = wrapUnique(new WebMessagePortChannelArray(channels->size()));
        for (size_t i = 0; i < channels->size(); ++i)
            (*webChannels)[i] = (*channels)[i].release();
    }
    return webChannels;
}

// static
MessagePortArray* MessagePort::toMessagePortArray(ExecutionContext* context, const WebMessagePortChannelArray& webChannels)
{
    std::unique_ptr<MessagePortChannelArray> channels = wrapUnique(new MessagePortChannelArray(webChannels.size()));
    for (size_t i = 0; i < webChannels.size(); ++i)
        (*channels)[i] = WebMessagePortChannelUniquePtr(webChannels[i]);
    return MessagePort::entanglePorts(*context, std::move(channels));
}

WebMessagePortChannelUniquePtr MessagePort::disentangle()
{
    DCHECK(m_entangledChannel);
    m_entangledChannel->setClient(nullptr);
    return std::move(m_entangledChannel);
}

// Invoked to notify us that there are messages available for this port.
// This code may be called from another thread, and so should not call any non-threadsafe APIs (i.e. should not call into the entangled channel or access mutable variables).
void MessagePort::messageAvailable()
{
    DCHECK(getExecutionContext());
    getExecutionContext()->postTask(BLINK_FROM_HERE, createCrossThreadTask(&MessagePort::dispatchMessages, wrapCrossThreadWeakPersistent(this)));
}

void MessagePort::start()
{
    // Do nothing if we've been cloned or closed.
    if (!isEntangled())
        return;

    DCHECK(getExecutionContext());
    if (m_started)
        return;

    m_entangledChannel->setClient(this);
    m_started = true;
    messageAvailable();
}

void MessagePort::close()
{
    if (isEntangled())
        m_entangledChannel->setClient(nullptr);
    m_closed = true;
}

void MessagePort::entangle(WebMessagePortChannelUniquePtr remote)
{
    // Only invoked to set our initial entanglement.
    DCHECK(!m_entangledChannel);
    DCHECK(getExecutionContext());

    m_entangledChannel = std::move(remote);
}

const AtomicString& MessagePort::interfaceName() const
{
    return EventTargetNames::MessagePort;
}

static bool tryGetMessageFrom(WebMessagePortChannel& webChannel, RefPtr<SerializedScriptValue>& message, std::unique_ptr<MessagePortChannelArray>& channels)
{
    WebString messageString;
    WebMessagePortChannelArray webChannels;
    if (!webChannel.tryGetMessage(&messageString, webChannels))
        return false;

    if (webChannels.size()) {
        channels = wrapUnique(new MessagePortChannelArray(webChannels.size()));
        for (size_t i = 0; i < webChannels.size(); ++i)
            (*channels)[i] = WebMessagePortChannelUniquePtr(webChannels[i]);
    }
    message = SerializedScriptValue::create(messageString);
    return true;
}

bool MessagePort::tryGetMessage(RefPtr<SerializedScriptValue>& message, std::unique_ptr<MessagePortChannelArray>& channels)
{
    if (!m_entangledChannel)
        return false;
    return tryGetMessageFrom(*m_entangledChannel, message, channels);
}

void MessagePort::dispatchMessages()
{
    // Because close() doesn't cancel any in flight calls to dispatchMessages() we need to check if the port is still open before dispatch.
    if (m_closed)
        return;

    // Messages for contexts that are not fully active get dispatched too, but JSAbstractEventListener::handleEvent() doesn't call handlers for these.
    // The HTML5 spec specifies that any messages sent to a document that is not fully active should be dropped, so this behavior is OK.
    if (!started())
        return;

    RefPtr<SerializedScriptValue> message;
    std::unique_ptr<MessagePortChannelArray> channels;
    while (tryGetMessage(message, channels)) {
        // close() in Worker onmessage handler should prevent next message from dispatching.
        if (getExecutionContext()->isWorkerGlobalScope() && toWorkerGlobalScope(getExecutionContext())->isClosing())
            return;

        MessagePortArray* ports = MessagePort::entanglePorts(*getExecutionContext(), std::move(channels));
        Event* evt = MessageEvent::create(ports, message.release());

        dispatchEvent(evt);
    }
}

bool MessagePort::hasPendingActivity() const
{
    // The spec says that entangled message ports should always be treated as if they have a strong reference.
    // We'll also stipulate that the queue needs to be open (if the app drops its reference to the port before start()-ing it, then it's not really entangled as it's unreachable).
    return m_started && isEntangled();
}

std::unique_ptr<MessagePortChannelArray> MessagePort::disentanglePorts(ExecutionContext* context, const MessagePortArray& ports, ExceptionState& exceptionState)
{
    if (!ports.size())
        return nullptr;

    HeapHashSet<Member<MessagePort>> visited;

    // Walk the incoming array - if there are any duplicate ports, or null ports or cloned ports, throw an error (per section 8.3.3 of the HTML5 spec).
    for (unsigned i = 0; i < ports.size(); ++i) {
        MessagePort* port = ports[i];
        if (!port || port->isNeutered() || visited.contains(port)) {
            String type;
            if (!port)
                type = "null";
            else if (port->isNeutered())
                type = "already neutered";
            else
                type = "a duplicate";
            exceptionState.throwDOMException(DataCloneError, "Port at index "  + String::number(i) + " is " + type + ".");
            return nullptr;
        }
        visited.add(port);
    }

    UseCounter::count(context, UseCounter::MessagePortsTransferred);

    // Passed-in ports passed validity checks, so we can disentangle them.
    std::unique_ptr<MessagePortChannelArray> portArray = wrapUnique(new MessagePortChannelArray(ports.size()));
    for (unsigned i = 0; i < ports.size(); ++i)
        (*portArray)[i] = ports[i]->disentangle();
    return portArray;
}

MessagePortArray* MessagePort::entanglePorts(ExecutionContext& context, std::unique_ptr<MessagePortChannelArray> channels)
{
    // https://html.spec.whatwg.org/multipage/comms.html#message-ports
    // |ports| should be an empty array, not null even when there is no ports.
    if (!channels || !channels->size())
        return new MessagePortArray;

    MessagePortArray* portArray = new MessagePortArray(channels->size());
    for (unsigned i = 0; i < channels->size(); ++i) {
        MessagePort* port = MessagePort::create(context);
        port->entangle(std::move((*channels)[i]));
        (*portArray)[i] = port;
    }
    return portArray;
}

DEFINE_TRACE(MessagePort)
{
    ActiveDOMObject::trace(visitor);
    EventTargetWithInlineData::trace(visitor);
}

} // namespace blink
