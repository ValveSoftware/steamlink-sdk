/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/peerconnection/RTCDataChannel.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMArrayBufferView.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/MessageEvent.h"
#include "core/fileapi/Blob.h"
#include "modules/peerconnection/RTCPeerConnection.h"
#include "public/platform/WebRTCPeerConnectionHandler.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

static void throwNotOpenException(ExceptionState& exceptionState)
{
    exceptionState.throwDOMException(InvalidStateError, "RTCDataChannel.readyState is not 'open'");
}

static void throwCouldNotSendDataException(ExceptionState& exceptionState)
{
    exceptionState.throwDOMException(NetworkError, "Could not send data");
}

static void throwNoBlobSupportException(ExceptionState& exceptionState)
{
    exceptionState.throwDOMException(NotSupportedError, "Blob support not implemented yet");
}

RTCDataChannel* RTCDataChannel::create(ExecutionContext* context, std::unique_ptr<WebRTCDataChannelHandler> handler)
{
    DCHECK(handler);
    RTCDataChannel* channel = new RTCDataChannel(context, std::move(handler));
    channel->suspendIfNeeded();

    return channel;
}

RTCDataChannel* RTCDataChannel::create(ExecutionContext* context, WebRTCPeerConnectionHandler* peerConnectionHandler, const String& label, const WebRTCDataChannelInit& init, ExceptionState& exceptionState)
{
    std::unique_ptr<WebRTCDataChannelHandler> handler = wrapUnique(peerConnectionHandler->createDataChannel(label, init));
    if (!handler) {
        exceptionState.throwDOMException(NotSupportedError, "RTCDataChannel is not supported");
        return nullptr;
    }
    RTCDataChannel* channel = new RTCDataChannel(context, std::move(handler));
    channel->suspendIfNeeded();

    return channel;
}

RTCDataChannel::RTCDataChannel(ExecutionContext* context, std::unique_ptr<WebRTCDataChannelHandler> handler)
    : ActiveScriptWrappable(this)
    , ActiveDOMObject(context)
    , m_handler(std::move(handler))
    , m_readyState(ReadyStateConnecting)
    , m_binaryType(BinaryTypeArrayBuffer)
    , m_scheduledEventTimer(this, &RTCDataChannel::scheduledEventTimerFired)
    , m_bufferedAmountLowThreshold(0U)
    , m_stopped(false)
{
    ThreadState::current()->registerPreFinalizer(this);
    m_handler->setClient(this);
}

RTCDataChannel::~RTCDataChannel()
{
}

void RTCDataChannel::dispose()
{
    if (m_stopped)
        return;

    // Promptly clears a raw reference from content/ to an on-heap object
    // so that content/ doesn't access it in a lazy sweeping phase.
    m_handler->setClient(nullptr);
    m_handler.reset();
}

RTCDataChannel::ReadyState RTCDataChannel::getHandlerState() const
{
    return m_handler->state();
}

String RTCDataChannel::label() const
{
    return m_handler->label();
}

bool RTCDataChannel::reliable() const
{
    return m_handler->isReliable();
}

bool RTCDataChannel::ordered() const
{
    return m_handler->ordered();
}

unsigned short RTCDataChannel::maxRetransmitTime() const
{
    return m_handler->maxRetransmitTime();
}

unsigned short RTCDataChannel::maxRetransmits() const
{
    return m_handler->maxRetransmits();
}

String RTCDataChannel::protocol() const
{
    return m_handler->protocol();
}

bool RTCDataChannel::negotiated() const
{
    return m_handler->negotiated();
}

unsigned short RTCDataChannel::id() const
{
    return m_handler->id();
}

String RTCDataChannel::readyState() const
{
    switch (m_readyState) {
    case ReadyStateConnecting:
        return "connecting";
    case ReadyStateOpen:
        return "open";
    case ReadyStateClosing:
        return "closing";
    case ReadyStateClosed:
        return "closed";
    }

    NOTREACHED();
    return String();
}

unsigned RTCDataChannel::bufferedAmount() const
{
    return m_handler->bufferedAmount();
}

unsigned RTCDataChannel::bufferedAmountLowThreshold() const
{
    return m_bufferedAmountLowThreshold;
}

void RTCDataChannel::setBufferedAmountLowThreshold(unsigned threshold)
{
    m_bufferedAmountLowThreshold = threshold;
}

String RTCDataChannel::binaryType() const
{
    switch (m_binaryType) {
    case BinaryTypeBlob:
        return "blob";
    case BinaryTypeArrayBuffer:
        return "arraybuffer";
    }
    NOTREACHED();
    return String();
}

void RTCDataChannel::setBinaryType(const String& binaryType, ExceptionState& exceptionState)
{
    if (binaryType == "blob")
        throwNoBlobSupportException(exceptionState);
    else if (binaryType == "arraybuffer")
        m_binaryType = BinaryTypeArrayBuffer;
    else
        exceptionState.throwDOMException(TypeMismatchError, "Unknown binary type : " + binaryType);
}

void RTCDataChannel::send(const String& data, ExceptionState& exceptionState)
{
    if (m_readyState != ReadyStateOpen) {
        throwNotOpenException(exceptionState);
        return;
    }
    if (!m_handler->sendStringData(data)) {
        // FIXME: This should not throw an exception but instead forcefully close the data channel.
        throwCouldNotSendDataException(exceptionState);
    }
}

void RTCDataChannel::send(DOMArrayBuffer* data, ExceptionState& exceptionState)
{
    if (m_readyState != ReadyStateOpen) {
        throwNotOpenException(exceptionState);
        return;
    }

    size_t dataLength = data->byteLength();
    if (!dataLength)
        return;

    if (!m_handler->sendRawData(static_cast<const char*>((data->data())), dataLength)) {
        // FIXME: This should not throw an exception but instead forcefully close the data channel.
        throwCouldNotSendDataException(exceptionState);
    }
}

void RTCDataChannel::send(DOMArrayBufferView* data, ExceptionState& exceptionState)
{
    if (!m_handler->sendRawData(static_cast<const char*>(data->baseAddress()), data->byteLength())) {
        // FIXME: This should not throw an exception but instead forcefully close the data channel.
        throwCouldNotSendDataException(exceptionState);
    }
}

void RTCDataChannel::send(Blob* data, ExceptionState& exceptionState)
{
    // FIXME: implement
    throwNoBlobSupportException(exceptionState);
}

void RTCDataChannel::close()
{
    m_handler->close();
}

void RTCDataChannel::didChangeReadyState(WebRTCDataChannelHandlerClient::ReadyState newState)
{
    if (m_readyState == ReadyStateClosed)
        return;

    m_readyState = newState;

    switch (m_readyState) {
    case ReadyStateOpen:
        scheduleDispatchEvent(Event::create(EventTypeNames::open));
        break;
    case ReadyStateClosed:
        scheduleDispatchEvent(Event::create(EventTypeNames::close));
        break;
    default:
        break;
    }
}

void RTCDataChannel::didDecreaseBufferedAmount(unsigned previousAmount)
{
    if (previousAmount > m_bufferedAmountLowThreshold
        && bufferedAmount() <= m_bufferedAmountLowThreshold) {
        scheduleDispatchEvent(Event::create(EventTypeNames::bufferedamountlow));
    }
}

void RTCDataChannel::didReceiveStringData(const WebString& text)
{
    scheduleDispatchEvent(MessageEvent::create(text));
}

void RTCDataChannel::didReceiveRawData(const char* data, size_t dataLength)
{
    if (m_binaryType == BinaryTypeBlob) {
        // FIXME: Implement.
        return;
    }
    if (m_binaryType == BinaryTypeArrayBuffer) {
        DOMArrayBuffer* buffer = DOMArrayBuffer::create(data, dataLength);
        scheduleDispatchEvent(MessageEvent::create(buffer));
        return;
    }
    NOTREACHED();
}

void RTCDataChannel::didDetectError()
{
    scheduleDispatchEvent(Event::create(EventTypeNames::error));
}

const AtomicString& RTCDataChannel::interfaceName() const
{
    return EventTargetNames::RTCDataChannel;
}

ExecutionContext* RTCDataChannel::getExecutionContext() const
{
    return ActiveDOMObject::getExecutionContext();
}

// ActiveDOMObject
void RTCDataChannel::suspend()
{
    m_scheduledEventTimer.stop();
}

void RTCDataChannel::resume()
{
    if (!m_scheduledEvents.isEmpty() && !m_scheduledEventTimer.isActive())
        m_scheduledEventTimer.startOneShot(0, BLINK_FROM_HERE);
}

void RTCDataChannel::stop()
{
    if (m_stopped)
        return;

    m_stopped = true;
    m_handler->setClient(nullptr);
    m_handler.reset();
}

// ActiveScriptWrappable
bool RTCDataChannel::hasPendingActivity() const
{
    if (m_stopped)
        return false;

    // A RTCDataChannel object must not be garbage collected if its
    // * readyState is connecting and at least one event listener is registered
    //   for open events, message events, error events, or close events.
    // * readyState is open and at least one event listener is registered for
    //   message events, error events, or close events.
    // * readyState is closing and at least one event listener is registered for
    //   error events, or close events.
    // * underlying data transport is established and data is queued to be
    //   transmitted.
    bool hasValidListeners = false;
    switch (m_readyState) {
    case ReadyStateConnecting:
        hasValidListeners |= hasEventListeners(EventTypeNames::open);
        // fallthrough intended
    case ReadyStateOpen:
        hasValidListeners |= hasEventListeners(EventTypeNames::message);
        // fallthrough intended
    case ReadyStateClosing:
        hasValidListeners |= hasEventListeners(EventTypeNames::error) || hasEventListeners(EventTypeNames::close);
        break;
    default:
        break;
    }

    if (hasValidListeners)
        return true;

    return m_readyState != ReadyStateClosed && bufferedAmount() > 0;
}

void RTCDataChannel::scheduleDispatchEvent(Event* event)
{
    m_scheduledEvents.append(event);

    if (!m_scheduledEventTimer.isActive())
        m_scheduledEventTimer.startOneShot(0, BLINK_FROM_HERE);
}

void RTCDataChannel::scheduledEventTimerFired(Timer<RTCDataChannel>*)
{
    HeapVector<Member<Event>> events;
    events.swap(m_scheduledEvents);

    HeapVector<Member<Event>>::iterator it = events.begin();
    for (; it != events.end(); ++it)
        dispatchEvent((*it).release());

    events.clear();
}

DEFINE_TRACE(RTCDataChannel)
{
    visitor->trace(m_scheduledEvents);
    EventTargetWithInlineData::trace(visitor);
    ActiveDOMObject::trace(visitor);
}

} // namespace blink
