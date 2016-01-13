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

#include "config.h"
#include "modules/mediastream/RTCDataChannel.h"

#include "bindings/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/MessageEvent.h"
#include "core/fileapi/Blob.h"
#include "public/platform/WebRTCPeerConnectionHandler.h"
#include "wtf/ArrayBuffer.h"
#include "wtf/ArrayBufferView.h"

namespace WebCore {

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

PassRefPtrWillBeRawPtr<RTCDataChannel> RTCDataChannel::create(ExecutionContext* context, RTCPeerConnection* connection, PassOwnPtr<blink::WebRTCDataChannelHandler> handler)
{
    ASSERT(handler);
    return adoptRefWillBeRefCountedGarbageCollected(new RTCDataChannel(context, connection, handler));
}

PassRefPtrWillBeRawPtr<RTCDataChannel> RTCDataChannel::create(ExecutionContext* context, RTCPeerConnection* connection, blink::WebRTCPeerConnectionHandler* peerConnectionHandler, const String& label, const blink::WebRTCDataChannelInit& init, ExceptionState& exceptionState)
{
    OwnPtr<blink::WebRTCDataChannelHandler> handler = adoptPtr(peerConnectionHandler->createDataChannel(label, init));
    if (!handler) {
        exceptionState.throwDOMException(NotSupportedError, "RTCDataChannel is not supported");
        return nullptr;
    }
    return adoptRefWillBeRefCountedGarbageCollected(new RTCDataChannel(context, connection, handler.release()));
}

RTCDataChannel::RTCDataChannel(ExecutionContext* context, RTCPeerConnection* connection, PassOwnPtr<blink::WebRTCDataChannelHandler> handler)
    : m_executionContext(context)
    , m_handler(handler)
    , m_stopped(false)
    , m_readyState(ReadyStateConnecting)
    , m_binaryType(BinaryTypeArrayBuffer)
    , m_scheduledEventTimer(this, &RTCDataChannel::scheduledEventTimerFired)
#if ENABLE(OILPAN)
    , m_connection(connection)
#endif
{
    ScriptWrappable::init(this);
    m_handler->setClient(this);
}

RTCDataChannel::~RTCDataChannel()
{
#if ENABLE(OILPAN)
    // If the peer connection and the data channel die in the same
    // GC cycle stop has not been called and we need to notify the
    // client that the channel is gone.
    if (!m_stopped)
        m_handler->setClient(0);
#else
    ASSERT(m_stopped);
#endif
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

    ASSERT_NOT_REACHED();
    return String();
}

unsigned long RTCDataChannel::bufferedAmount() const
{
    return m_handler->bufferedAmount();
}

String RTCDataChannel::binaryType() const
{
    switch (m_binaryType) {
    case BinaryTypeBlob:
        return "blob";
    case BinaryTypeArrayBuffer:
        return "arraybuffer";
    }
    ASSERT_NOT_REACHED();
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

void RTCDataChannel::send(PassRefPtr<ArrayBuffer> prpData, ExceptionState& exceptionState)
{
    if (m_readyState != ReadyStateOpen) {
        throwNotOpenException(exceptionState);
        return;
    }

    RefPtr<ArrayBuffer> data = prpData;

    size_t dataLength = data->byteLength();
    if (!dataLength)
        return;

    if (!m_handler->sendRawData(static_cast<const char*>((data->data())), dataLength)) {
        // FIXME: This should not throw an exception but instead forcefully close the data channel.
        throwCouldNotSendDataException(exceptionState);
    }
}

void RTCDataChannel::send(PassRefPtr<ArrayBufferView> data, ExceptionState& exceptionState)
{
    if (!m_handler->sendRawData(static_cast<const char*>(data->baseAddress()), data->byteLength())) {
        // FIXME: This should not throw an exception but instead forcefully close the data channel.
        throwCouldNotSendDataException(exceptionState);
    }
}

void RTCDataChannel::send(PassRefPtrWillBeRawPtr<Blob> data, ExceptionState& exceptionState)
{
    // FIXME: implement
    throwNoBlobSupportException(exceptionState);
}

void RTCDataChannel::close()
{
    if (m_stopped)
        return;

    m_handler->close();
}

void RTCDataChannel::didChangeReadyState(blink::WebRTCDataChannelHandlerClient::ReadyState newState)
{
    if (m_stopped || m_readyState == ReadyStateClosed)
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

void RTCDataChannel::didReceiveStringData(const blink::WebString& text)
{
    if (m_stopped)
        return;

    scheduleDispatchEvent(MessageEvent::create(text));
}

void RTCDataChannel::didReceiveRawData(const char* data, size_t dataLength)
{
    if (m_stopped)
        return;

    if (m_binaryType == BinaryTypeBlob) {
        // FIXME: Implement.
        return;
    }
    if (m_binaryType == BinaryTypeArrayBuffer) {
        RefPtr<ArrayBuffer> buffer = ArrayBuffer::create(data, dataLength);
        scheduleDispatchEvent(MessageEvent::create(buffer.release()));
        return;
    }
    ASSERT_NOT_REACHED();
}

void RTCDataChannel::didDetectError()
{
    if (m_stopped)
        return;

    scheduleDispatchEvent(Event::create(EventTypeNames::error));
}

const AtomicString& RTCDataChannel::interfaceName() const
{
    return EventTargetNames::RTCDataChannel;
}

ExecutionContext* RTCDataChannel::executionContext() const
{
    return m_executionContext;
}

void RTCDataChannel::stop()
{
    m_stopped = true;
    m_readyState = ReadyStateClosed;
    m_handler->setClient(0);
    m_executionContext = 0;
}

void RTCDataChannel::scheduleDispatchEvent(PassRefPtrWillBeRawPtr<Event> event)
{
    m_scheduledEvents.append(event);

    if (!m_scheduledEventTimer.isActive())
        m_scheduledEventTimer.startOneShot(0, FROM_HERE);
}

void RTCDataChannel::scheduledEventTimerFired(Timer<RTCDataChannel>*)
{
    if (m_stopped)
        return;

    WillBeHeapVector<RefPtrWillBeMember<Event> > events;
    events.swap(m_scheduledEvents);

    WillBeHeapVector<RefPtrWillBeMember<Event> >::iterator it = events.begin();
    for (; it != events.end(); ++it)
        dispatchEvent((*it).release());

    events.clear();
}

#if ENABLE(OILPAN)
void RTCDataChannel::clearWeakMembers(Visitor* visitor)
{
    if (visitor->isAlive(m_connection))
        return;
    stop();
    m_connection = nullptr;
}
#endif

void RTCDataChannel::trace(Visitor* visitor)
{
#if ENABLE(OILPAN)
    visitor->trace(m_scheduledEvents);
    visitor->registerWeakMembers<RTCDataChannel, &RTCDataChannel::clearWeakMembers>(this);
#endif
    EventTargetWithInlineData::trace(visitor);
}

} // namespace WebCore
