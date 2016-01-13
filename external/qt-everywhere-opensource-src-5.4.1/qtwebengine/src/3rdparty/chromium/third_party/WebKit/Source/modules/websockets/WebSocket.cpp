/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
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

#include "modules/websockets/WebSocket.h"

#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ScriptController.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/MessageEvent.h"
#include "core/fileapi/Blob.h"
#include "core/frame/ConsoleTypes.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/inspector/ScriptCallStack.h"
#include "modules/websockets/CloseEvent.h"
#include "platform/Logging.h"
#include "platform/blob/BlobData.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KnownPorts.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "wtf/ArrayBuffer.h"
#include "wtf/ArrayBufferView.h"
#include "wtf/Assertions.h"
#include "wtf/HashSet.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/CString.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

WebSocket::EventQueue::EventQueue(EventTarget* target)
    : m_state(Active)
    , m_target(target)
    , m_resumeTimer(this, &EventQueue::resumeTimerFired) { }

WebSocket::EventQueue::~EventQueue() { stop(); }

void WebSocket::EventQueue::dispatch(PassRefPtrWillBeRawPtr<Event> event)
{
    switch (m_state) {
    case Active:
        ASSERT(m_events.isEmpty());
        ASSERT(m_target->executionContext());
        m_target->dispatchEvent(event);
        break;
    case Suspended:
        m_events.append(event);
        break;
    case Stopped:
        ASSERT(m_events.isEmpty());
        // Do nothing.
        break;
    }
}

bool WebSocket::EventQueue::isEmpty() const
{
    return m_events.isEmpty();
}

void WebSocket::EventQueue::suspend()
{
    m_resumeTimer.stop();
    if (m_state != Active)
        return;

    m_state = Suspended;
}

void WebSocket::EventQueue::resume()
{
    if (m_state != Suspended || m_resumeTimer.isActive())
        return;

    m_resumeTimer.startOneShot(0, FROM_HERE);
}

void WebSocket::EventQueue::stop()
{
    if (m_state == Stopped)
        return;

    m_state = Stopped;
    m_resumeTimer.stop();
    m_events.clear();
}

void WebSocket::EventQueue::dispatchQueuedEvents()
{
    if (m_state != Active)
        return;

    RefPtrWillBeRawPtr<EventQueue> protect(this);

    WillBeHeapDeque<RefPtrWillBeMember<Event> > events;
    events.swap(m_events);
    while (!events.isEmpty()) {
        if (m_state == Stopped || m_state == Suspended)
            break;
        ASSERT(m_state == Active);
        ASSERT(m_target->executionContext());
        m_target->dispatchEvent(events.takeFirst());
        // |this| can be stopped here.
    }
    if (m_state == Suspended) {
        while (!m_events.isEmpty())
            events.append(m_events.takeFirst());
        events.swap(m_events);
    }
}

void WebSocket::EventQueue::resumeTimerFired(Timer<EventQueue>*)
{
    ASSERT(m_state == Suspended);
    m_state = Active;
    dispatchQueuedEvents();
}

void WebSocket::EventQueue::trace(Visitor* visitor)
{
    visitor->trace(m_events);
}

const size_t maxReasonSizeInBytes = 123;

static inline bool isValidSubprotocolCharacter(UChar character)
{
    const UChar minimumProtocolCharacter = '!'; // U+0021.
    const UChar maximumProtocolCharacter = '~'; // U+007E.
    // Set to true if character does not matches "separators" ABNF defined in
    // RFC2616. SP and HT are excluded since the range check excludes them.
    bool isNotSeparator = character != '"' && character != '(' && character != ')' && character != ',' && character != '/'
        && !(character >= ':' && character <= '@') // U+003A - U+0040 (':', ';', '<', '=', '>', '?', '@').
        && !(character >= '[' && character <= ']') // U+005B - U+005D ('[', '\\', ']').
        && character != '{' && character != '}';
    return character >= minimumProtocolCharacter && character <= maximumProtocolCharacter && isNotSeparator;
}

bool WebSocket::isValidSubprotocolString(const String& protocol)
{
    if (protocol.isEmpty())
        return false;
    for (size_t i = 0; i < protocol.length(); ++i) {
        if (!isValidSubprotocolCharacter(protocol[i]))
            return false;
    }
    return true;
}

static String encodeSubprotocolString(const String& protocol)
{
    StringBuilder builder;
    for (size_t i = 0; i < protocol.length(); i++) {
        if (protocol[i] < 0x20 || protocol[i] > 0x7E)
            builder.append(String::format("\\u%04X", protocol[i]));
        else if (protocol[i] == 0x5c)
            builder.append("\\\\");
        else
            builder.append(protocol[i]);
    }
    return builder.toString();
}

static String joinStrings(const Vector<String>& strings, const char* separator)
{
    StringBuilder builder;
    for (size_t i = 0; i < strings.size(); ++i) {
        if (i)
            builder.append(separator);
        builder.append(strings[i]);
    }
    return builder.toString();
}

static unsigned long saturateAdd(unsigned long a, unsigned long b)
{
    if (std::numeric_limits<unsigned long>::max() - a < b)
        return std::numeric_limits<unsigned long>::max();
    return a + b;
}

static void setInvalidStateErrorForSendMethod(ExceptionState& exceptionState)
{
    exceptionState.throwDOMException(InvalidStateError, "Still in CONNECTING state.");
}

const char* WebSocket::subprotocolSeperator()
{
    return ", ";
}

WebSocket::WebSocket(ExecutionContext* context)
    : ActiveDOMObject(context)
    , m_state(CONNECTING)
    , m_bufferedAmount(0)
    , m_consumedBufferedAmount(0)
    , m_bufferedAmountAfterClose(0)
    , m_binaryType(BinaryTypeBlob)
    , m_subprotocol("")
    , m_extensions("")
    , m_eventQueue(EventQueue::create(this))
    , m_bufferedAmountConsumeTimer(this, &WebSocket::reflectBufferedAmountConsumption)
{
    ScriptWrappable::init(this);
}

WebSocket::~WebSocket()
{
    ASSERT(!m_channel);
}

void WebSocket::logError(const String& message)
{
    executionContext()->addConsoleMessage(JSMessageSource, ErrorMessageLevel, message);
}

PassRefPtrWillBeRawPtr<WebSocket> WebSocket::create(ExecutionContext* context, const String& url, ExceptionState& exceptionState)
{
    Vector<String> protocols;
    return create(context, url, protocols, exceptionState);
}

PassRefPtrWillBeRawPtr<WebSocket> WebSocket::create(ExecutionContext* context, const String& url, const Vector<String>& protocols, ExceptionState& exceptionState)
{
    if (url.isNull()) {
        exceptionState.throwDOMException(SyntaxError, "Failed to create a WebSocket: the provided URL is invalid.");
        return nullptr;
    }

    RefPtrWillBeRawPtr<WebSocket> webSocket(adoptRefWillBeRefCountedGarbageCollected(new WebSocket(context)));
    webSocket->suspendIfNeeded();

    webSocket->connect(url, protocols, exceptionState);
    if (exceptionState.hadException())
        return nullptr;

    return webSocket.release();
}

PassRefPtrWillBeRawPtr<WebSocket> WebSocket::create(ExecutionContext* context, const String& url, const String& protocol, ExceptionState& exceptionState)
{
    Vector<String> protocols;
    protocols.append(protocol);
    return create(context, url, protocols, exceptionState);
}

void WebSocket::connect(const String& url, const Vector<String>& protocols, ExceptionState& exceptionState)
{
    WTF_LOG(Network, "WebSocket %p connect() url='%s'", this, url.utf8().data());
    m_url = KURL(KURL(), url);

    if (!m_url.isValid()) {
        m_state = CLOSED;
        exceptionState.throwDOMException(SyntaxError, "The URL '" + url + "' is invalid.");
        return;
    }
    if (!m_url.protocolIs("ws") && !m_url.protocolIs("wss")) {
        m_state = CLOSED;
        exceptionState.throwDOMException(SyntaxError, "The URL's scheme must be either 'ws' or 'wss'. '" + m_url.protocol() + "' is not allowed.");
        return;
    }

    if (m_url.hasFragmentIdentifier()) {
        m_state = CLOSED;
        exceptionState.throwDOMException(SyntaxError, "The URL contains a fragment identifier ('" + m_url.fragmentIdentifier() + "'). Fragment identifiers are not allowed in WebSocket URLs.");
        return;
    }
    if (!portAllowed(m_url)) {
        m_state = CLOSED;
        exceptionState.throwSecurityError("The port " + String::number(m_url.port()) + " is not allowed.");
        return;
    }

    // FIXME: Convert this to check the isolated world's Content Security Policy once webkit.org/b/104520 is solved.
    bool shouldBypassMainWorldContentSecurityPolicy = false;
    if (executionContext()->isDocument()) {
        Document* document = toDocument(executionContext());
        shouldBypassMainWorldContentSecurityPolicy = document->frame()->script().shouldBypassMainWorldContentSecurityPolicy();
    }
    if (!shouldBypassMainWorldContentSecurityPolicy && !executionContext()->contentSecurityPolicy()->allowConnectToSource(m_url)) {
        m_state = CLOSED;
        // The URL is safe to expose to JavaScript, as this check happens synchronously before redirection.
        exceptionState.throwSecurityError("Refused to connect to '" + m_url.elidedString() + "' because it violates the document's Content Security Policy.");
        return;
    }

    m_channel = createChannel(executionContext(), this);

    for (size_t i = 0; i < protocols.size(); ++i) {
        if (!isValidSubprotocolString(protocols[i])) {
            m_state = CLOSED;
            exceptionState.throwDOMException(SyntaxError, "The subprotocol '" + encodeSubprotocolString(protocols[i]) + "' is invalid.");
            releaseChannel();
            return;
        }
    }
    HashSet<String> visited;
    for (size_t i = 0; i < protocols.size(); ++i) {
        if (!visited.add(protocols[i]).isNewEntry) {
            m_state = CLOSED;
            exceptionState.throwDOMException(SyntaxError, "The subprotocol '" + encodeSubprotocolString(protocols[i]) + "' is duplicated.");
            releaseChannel();
            return;
        }
    }

    String protocolString;
    if (!protocols.isEmpty())
        protocolString = joinStrings(protocols, subprotocolSeperator());

    if (!m_channel->connect(m_url, protocolString)) {
        m_state = CLOSED;
        exceptionState.throwSecurityError("An insecure WebSocket connection may not be initiated from a page loaded over HTTPS.");
        releaseChannel();
        return;
    }
}

void WebSocket::handleSendResult(WebSocketChannel::SendResult result, ExceptionState& exceptionState, WebSocketSendType dataType)
{
    switch (result) {
    case WebSocketChannel::InvalidMessage:
        exceptionState.throwDOMException(SyntaxError, "The message contains invalid characters.");
        return;
    case WebSocketChannel::SendFail:
        logError("WebSocket send() failed.");
        return;
    case WebSocketChannel::SendSuccess:
        blink::Platform::current()->histogramEnumeration("WebCore.WebSocket.SendType", dataType, WebSocketSendTypeMax);
        return;
    }
    ASSERT_NOT_REACHED();
}

void WebSocket::updateBufferedAmountAfterClose(unsigned long payloadSize)
{
    m_bufferedAmountAfterClose = saturateAdd(m_bufferedAmountAfterClose, payloadSize);
    m_bufferedAmountAfterClose = saturateAdd(m_bufferedAmountAfterClose, getFramingOverhead(payloadSize));

    logError("WebSocket is already in CLOSING or CLOSED state.");
}

void WebSocket::reflectBufferedAmountConsumption(Timer<WebSocket>*)
{
    ASSERT(m_bufferedAmount >= m_consumedBufferedAmount);
    WTF_LOG(Network, "WebSocket %p reflectBufferedAmountConsumption() %lu => %lu", this, m_bufferedAmount, m_bufferedAmount - m_consumedBufferedAmount);

    m_bufferedAmount -= m_consumedBufferedAmount;
    m_consumedBufferedAmount = 0;
}

void WebSocket::releaseChannel()
{
    ASSERT(m_channel);
    m_channel->disconnect();
    m_channel = nullptr;
}

void WebSocket::send(const String& message, ExceptionState& exceptionState)
{
    WTF_LOG(Network, "WebSocket %p send() Sending String '%s'", this, message.utf8().data());
    if (m_state == CONNECTING) {
        setInvalidStateErrorForSendMethod(exceptionState);
        return;
    }
    // No exception is raised if the connection was once established but has subsequently been closed.
    if (m_state == CLOSING || m_state == CLOSED) {
        updateBufferedAmountAfterClose(message.utf8().length());
        return;
    }
    ASSERT(m_channel);
    m_bufferedAmount += message.utf8().length();
    handleSendResult(m_channel->send(message), exceptionState, WebSocketSendTypeString);
}

void WebSocket::send(ArrayBuffer* binaryData, ExceptionState& exceptionState)
{
    WTF_LOG(Network, "WebSocket %p send() Sending ArrayBuffer %p", this, binaryData);
    ASSERT(binaryData);
    if (m_state == CONNECTING) {
        setInvalidStateErrorForSendMethod(exceptionState);
        return;
    }
    if (m_state == CLOSING || m_state == CLOSED) {
        updateBufferedAmountAfterClose(binaryData->byteLength());
        return;
    }
    ASSERT(m_channel);
    m_bufferedAmount += binaryData->byteLength();
    handleSendResult(m_channel->send(*binaryData, 0, binaryData->byteLength()), exceptionState, WebSocketSendTypeArrayBuffer);
}

void WebSocket::send(ArrayBufferView* arrayBufferView, ExceptionState& exceptionState)
{
    WTF_LOG(Network, "WebSocket %p send() Sending ArrayBufferView %p", this, arrayBufferView);
    ASSERT(arrayBufferView);
    if (m_state == CONNECTING) {
        setInvalidStateErrorForSendMethod(exceptionState);
        return;
    }
    if (m_state == CLOSING || m_state == CLOSED) {
        updateBufferedAmountAfterClose(arrayBufferView->byteLength());
        return;
    }
    ASSERT(m_channel);
    m_bufferedAmount += arrayBufferView->byteLength();
    RefPtr<ArrayBuffer> arrayBuffer(arrayBufferView->buffer());
    handleSendResult(m_channel->send(*arrayBuffer, arrayBufferView->byteOffset(), arrayBufferView->byteLength()), exceptionState, WebSocketSendTypeArrayBufferView);
}

void WebSocket::send(Blob* binaryData, ExceptionState& exceptionState)
{
    WTF_LOG(Network, "WebSocket %p send() Sending Blob '%s'", this, binaryData->uuid().utf8().data());
    ASSERT(binaryData);
    if (m_state == CONNECTING) {
        setInvalidStateErrorForSendMethod(exceptionState);
        return;
    }
    if (m_state == CLOSING || m_state == CLOSED) {
        updateBufferedAmountAfterClose(static_cast<unsigned long>(binaryData->size()));
        return;
    }
    m_bufferedAmount += binaryData->size();
    ASSERT(m_channel);
    handleSendResult(m_channel->send(binaryData->blobDataHandle()), exceptionState, WebSocketSendTypeBlob);
}

void WebSocket::close(unsigned short code, const String& reason, ExceptionState& exceptionState)
{
    closeInternal(code, reason, exceptionState);
}

void WebSocket::close(ExceptionState& exceptionState)
{
    closeInternal(WebSocketChannel::CloseEventCodeNotSpecified, String(), exceptionState);
}

void WebSocket::close(unsigned short code, ExceptionState& exceptionState)
{
    closeInternal(code, String(), exceptionState);
}

void WebSocket::closeInternal(int code, const String& reason, ExceptionState& exceptionState)
{
    if (code == WebSocketChannel::CloseEventCodeNotSpecified) {
        WTF_LOG(Network, "WebSocket %p close() without code and reason", this);
    } else {
        WTF_LOG(Network, "WebSocket %p close() code=%d reason='%s'", this, code, reason.utf8().data());
        if (!(code == WebSocketChannel::CloseEventCodeNormalClosure || (WebSocketChannel::CloseEventCodeMinimumUserDefined <= code && code <= WebSocketChannel::CloseEventCodeMaximumUserDefined))) {
            exceptionState.throwDOMException(InvalidAccessError, "The code must be either 1000, or between 3000 and 4999. " + String::number(code) + " is neither.");
            return;
        }
        CString utf8 = reason.utf8(StrictUTF8ConversionReplacingUnpairedSurrogatesWithFFFD);
        if (utf8.length() > maxReasonSizeInBytes) {
            exceptionState.throwDOMException(SyntaxError, "The message must not be greater than " + String::number(maxReasonSizeInBytes) + " bytes.");
            return;
        }
    }

    if (m_state == CLOSING || m_state == CLOSED)
        return;
    if (m_state == CONNECTING) {
        m_state = CLOSING;
        m_channel->fail("WebSocket is closed before the connection is established.", WarningMessageLevel, String(), 0);
        return;
    }
    m_state = CLOSING;
    if (m_channel)
        m_channel->close(code, reason);
}

const KURL& WebSocket::url() const
{
    return m_url;
}

WebSocket::State WebSocket::readyState() const
{
    return m_state;
}

unsigned long WebSocket::bufferedAmount() const
{
    return saturateAdd(m_bufferedAmount, m_bufferedAmountAfterClose);
}

String WebSocket::protocol() const
{
    return m_subprotocol;
}

String WebSocket::extensions() const
{
    return m_extensions;
}

String WebSocket::binaryType() const
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

void WebSocket::setBinaryType(const String& binaryType)
{
    if (binaryType == "blob") {
        m_binaryType = BinaryTypeBlob;
        return;
    }
    if (binaryType == "arraybuffer") {
        m_binaryType = BinaryTypeArrayBuffer;
        return;
    }
    logError("'" + binaryType + "' is not a valid value for binaryType; binaryType remains unchanged.");
}

const AtomicString& WebSocket::interfaceName() const
{
    return EventTargetNames::WebSocket;
}

ExecutionContext* WebSocket::executionContext() const
{
    return ActiveDOMObject::executionContext();
}

void WebSocket::contextDestroyed()
{
    WTF_LOG(Network, "WebSocket %p contextDestroyed()", this);
    ASSERT(!m_channel);
    ASSERT(m_state == CLOSED);
    ActiveDOMObject::contextDestroyed();
}

bool WebSocket::hasPendingActivity() const
{
    return m_channel || !m_eventQueue->isEmpty();
}

void WebSocket::suspend()
{
    if (m_channel)
        m_channel->suspend();
    m_eventQueue->suspend();
}

void WebSocket::resume()
{
    if (m_channel)
        m_channel->resume();
    m_eventQueue->resume();
}

void WebSocket::stop()
{
    m_eventQueue->stop();
    if (m_channel) {
        m_channel->close(WebSocketChannel::CloseEventCodeGoingAway, String());
        releaseChannel();
    }
    m_state = CLOSED;
}

void WebSocket::didConnect(const String& subprotocol, const String& extensions)
{
    WTF_LOG(Network, "WebSocket %p didConnect()", this);
    if (m_state != CONNECTING)
        return;
    m_state = OPEN;
    m_subprotocol = subprotocol;
    m_extensions = extensions;
    m_eventQueue->dispatch(Event::create(EventTypeNames::open));
}

void WebSocket::didReceiveMessage(const String& msg)
{
    WTF_LOG(Network, "WebSocket %p didReceiveMessage() Text message '%s'", this, msg.utf8().data());
    if (m_state != OPEN)
        return;
    m_eventQueue->dispatch(MessageEvent::create(msg, SecurityOrigin::create(m_url)->toString()));
}

void WebSocket::didReceiveBinaryData(PassOwnPtr<Vector<char> > binaryData)
{
    WTF_LOG(Network, "WebSocket %p didReceiveBinaryData() %lu byte binary message", this, static_cast<unsigned long>(binaryData->size()));
    switch (m_binaryType) {
    case BinaryTypeBlob: {
        size_t size = binaryData->size();
        RefPtr<RawData> rawData = RawData::create();
        binaryData->swap(*rawData->mutableData());
        OwnPtr<BlobData> blobData = BlobData::create();
        blobData->appendData(rawData.release(), 0, BlobDataItem::toEndOfFile);
        RefPtrWillBeRawPtr<Blob> blob = Blob::create(BlobDataHandle::create(blobData.release(), size));
        m_eventQueue->dispatch(MessageEvent::create(blob.release(), SecurityOrigin::create(m_url)->toString()));
        break;
    }

    case BinaryTypeArrayBuffer:
        RefPtr<ArrayBuffer> arrayBuffer = ArrayBuffer::create(binaryData->data(), binaryData->size());
        if (!arrayBuffer) {
            // Failed to allocate an ArrayBuffer. We need to crash the renderer
            // since there's no way defined in the spec to tell this to the
            // user.
            CRASH();
        }
        m_eventQueue->dispatch(MessageEvent::create(arrayBuffer.release(), SecurityOrigin::create(m_url)->toString()));
        break;
    }
}

void WebSocket::didReceiveMessageError()
{
    WTF_LOG(Network, "WebSocket %p didReceiveMessageError()", this);
    m_state = CLOSED;
    m_eventQueue->dispatch(Event::create(EventTypeNames::error));
}

void WebSocket::didConsumeBufferedAmount(unsigned long consumed)
{
    ASSERT(m_bufferedAmount >= consumed);
    WTF_LOG(Network, "WebSocket %p didConsumeBufferedAmount(%lu)", this, consumed);
    if (m_state == CLOSED)
        return;
    m_consumedBufferedAmount += consumed;
    if (!m_bufferedAmountConsumeTimer.isActive())
        m_bufferedAmountConsumeTimer.startOneShot(0, FROM_HERE);
}

void WebSocket::didStartClosingHandshake()
{
    WTF_LOG(Network, "WebSocket %p didStartClosingHandshake()", this);
    m_state = CLOSING;
}

void WebSocket::didClose(ClosingHandshakeCompletionStatus closingHandshakeCompletion, unsigned short code, const String& reason)
{
    WTF_LOG(Network, "WebSocket %p didClose()", this);
    if (!m_channel)
        return;
    bool hasAllDataConsumed = m_bufferedAmount == m_consumedBufferedAmount;
    bool wasClean = m_state == CLOSING && hasAllDataConsumed && closingHandshakeCompletion == ClosingHandshakeComplete && code != WebSocketChannel::CloseEventCodeAbnormalClosure;
    m_state = CLOSED;

    m_eventQueue->dispatch(CloseEvent::create(wasClean, code, reason));
    releaseChannel();
}

size_t WebSocket::getFramingOverhead(size_t payloadSize)
{
    static const size_t hybiBaseFramingOverhead = 2; // Every frame has at least two-byte header.
    static const size_t hybiMaskingKeyLength = 4; // Every frame from client must have masking key.
    static const size_t minimumPayloadSizeWithTwoByteExtendedPayloadLength = 126;
    static const size_t minimumPayloadSizeWithEightByteExtendedPayloadLength = 0x10000;
    size_t overhead = hybiBaseFramingOverhead + hybiMaskingKeyLength;
    if (payloadSize >= minimumPayloadSizeWithEightByteExtendedPayloadLength)
        overhead += 8;
    else if (payloadSize >= minimumPayloadSizeWithTwoByteExtendedPayloadLength)
        overhead += 2;
    return overhead;
}

void WebSocket::trace(Visitor* visitor)
{
    visitor->trace(m_channel);
    visitor->trace(m_eventQueue);
    EventTargetWithInlineData::trace(visitor);
}

} // namespace WebCore
