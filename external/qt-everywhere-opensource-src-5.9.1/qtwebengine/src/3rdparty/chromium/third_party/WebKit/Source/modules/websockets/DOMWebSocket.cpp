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

#include "modules/websockets/DOMWebSocket.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/SourceLocation.h"
#include "bindings/modules/v8/StringOrStringSequence.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMArrayBufferView.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/SecurityContext.h"
#include "core/events/MessageEvent.h"
#include "core/fileapi/Blob.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/inspector/ConsoleMessage.h"
#include "modules/websockets/CloseEvent.h"
#include "platform/Histogram.h"
#include "platform/blob/BlobData.h"
#include "platform/network/NetworkLog.h"
#include "platform/weborigin/KnownPorts.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "public/platform/WebInsecureRequestPolicy.h"
#include "wtf/Assertions.h"
#include "wtf/HashSet.h"
#include "wtf/MathExtras.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/CString.h"
#include "wtf/text/StringBuilder.h"

static const size_t kMaxByteSizeForHistogram = 100 * 1000 * 1000;
static const int32_t kBucketCountForMessageSizeHistogram = 50;

namespace blink {

DOMWebSocket::EventQueue::EventQueue(EventTarget* target)
    : m_state(Active),
      m_target(target),
      m_resumeTimer(this, &EventQueue::resumeTimerFired) {}

DOMWebSocket::EventQueue::~EventQueue() {
  contextDestroyed();
}

void DOMWebSocket::EventQueue::dispatch(Event* event) {
  switch (m_state) {
    case Active:
      DCHECK(m_events.isEmpty());
      DCHECK(m_target->getExecutionContext());
      m_target->dispatchEvent(event);
      break;
    case Suspended:
      m_events.append(event);
      break;
    case Stopped:
      DCHECK(m_events.isEmpty());
      // Do nothing.
      break;
  }
}

bool DOMWebSocket::EventQueue::isEmpty() const {
  return m_events.isEmpty();
}

void DOMWebSocket::EventQueue::suspend() {
  m_resumeTimer.stop();
  if (m_state != Active)
    return;

  m_state = Suspended;
}

void DOMWebSocket::EventQueue::resume() {
  if (m_state != Suspended || m_resumeTimer.isActive())
    return;

  m_resumeTimer.startOneShot(0, BLINK_FROM_HERE);
}

void DOMWebSocket::EventQueue::contextDestroyed() {
  if (m_state == Stopped)
    return;

  m_state = Stopped;
  m_resumeTimer.stop();
  m_events.clear();
}

void DOMWebSocket::EventQueue::dispatchQueuedEvents() {
  if (m_state != Active)
    return;

  HeapDeque<Member<Event>> events;
  events.swap(m_events);
  while (!events.isEmpty()) {
    if (m_state == Stopped || m_state == Suspended)
      break;
    DCHECK_EQ(m_state, Active);
    DCHECK(m_target->getExecutionContext());
    m_target->dispatchEvent(events.takeFirst());
    // |this| can be stopped here.
  }
  if (m_state == Suspended) {
    while (!m_events.isEmpty())
      events.append(m_events.takeFirst());
    events.swap(m_events);
  }
}

void DOMWebSocket::EventQueue::resumeTimerFired(TimerBase*) {
  DCHECK_EQ(m_state, Suspended);
  m_state = Active;
  dispatchQueuedEvents();
}

DEFINE_TRACE(DOMWebSocket::EventQueue) {
  visitor->trace(m_target);
  visitor->trace(m_events);
}

const size_t maxReasonSizeInBytes = 123;

static inline bool isValidSubprotocolCharacter(UChar character) {
  const UChar minimumProtocolCharacter = '!';  // U+0021.
  const UChar maximumProtocolCharacter = '~';  // U+007E.
  // Set to true if character does not matches "separators" ABNF defined in
  // RFC2616. SP and HT are excluded since the range check excludes them.
  bool isNotSeparator =
      character != '"' && character != '(' && character != ')' &&
      character != ',' && character != '/' &&
      !(character >= ':' &&
        character <=
            '@')  // U+003A - U+0040 (':', ';', '<', '=', '>', '?', '@').
      &&
      !(character >= '[' &&
        character <= ']')  // U+005B - U+005D ('[', '\\', ']').
      && character != '{' && character != '}';
  return character >= minimumProtocolCharacter &&
         character <= maximumProtocolCharacter && isNotSeparator;
}

bool DOMWebSocket::isValidSubprotocolString(const String& protocol) {
  if (protocol.isEmpty())
    return false;
  for (size_t i = 0; i < protocol.length(); ++i) {
    if (!isValidSubprotocolCharacter(protocol[i]))
      return false;
  }
  return true;
}

static String encodeSubprotocolString(const String& protocol) {
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

static String joinStrings(const Vector<String>& strings,
                          const char* separator) {
  StringBuilder builder;
  for (size_t i = 0; i < strings.size(); ++i) {
    if (i)
      builder.append(separator);
    builder.append(strings[i]);
  }
  return builder.toString();
}

static void setInvalidStateErrorForSendMethod(ExceptionState& exceptionState) {
  exceptionState.throwDOMException(InvalidStateError,
                                   "Still in CONNECTING state.");
}

const char* DOMWebSocket::subprotocolSeperator() {
  return ", ";
}

DOMWebSocket::DOMWebSocket(ExecutionContext* context)
    : ActiveScriptWrappable(this),
      ActiveDOMObject(context),
      m_state(kConnecting),
      m_bufferedAmount(0),
      m_consumedBufferedAmount(0),
      m_bufferedAmountAfterClose(0),
      m_binaryType(BinaryTypeBlob),
      m_binaryTypeChangesAfterOpen(0),
      m_subprotocol(""),
      m_extensions(""),
      m_eventQueue(EventQueue::create(this)),
      m_bufferedAmountConsumeTimer(
          this,
          &DOMWebSocket::reflectBufferedAmountConsumption) {}

DOMWebSocket::~DOMWebSocket() {
  DCHECK(!m_channel);
}

void DOMWebSocket::logError(const String& message) {
  if (getExecutionContext())
    getExecutionContext()->addConsoleMessage(
        ConsoleMessage::create(JSMessageSource, ErrorMessageLevel, message));
}

DOMWebSocket* DOMWebSocket::create(ExecutionContext* context,
                                   const String& url,
                                   ExceptionState& exceptionState) {
  StringOrStringSequence protocols;
  return create(context, url, protocols, exceptionState);
}

DOMWebSocket* DOMWebSocket::create(ExecutionContext* context,
                                   const String& url,
                                   const StringOrStringSequence& protocols,
                                   ExceptionState& exceptionState) {
  if (url.isNull()) {
    exceptionState.throwDOMException(
        SyntaxError,
        "Failed to create a WebSocket: the provided URL is invalid.");
    return nullptr;
  }

  DOMWebSocket* webSocket = new DOMWebSocket(context);
  webSocket->suspendIfNeeded();

  if (protocols.isNull()) {
    Vector<String> protocolsVector;
    webSocket->connect(url, protocolsVector, exceptionState);
  } else if (protocols.isString()) {
    Vector<String> protocolsVector;
    protocolsVector.append(protocols.getAsString());
    webSocket->connect(url, protocolsVector, exceptionState);
  } else {
    DCHECK(protocols.isStringSequence());
    webSocket->connect(url, protocols.getAsStringSequence(), exceptionState);
  }

  if (exceptionState.hadException())
    return nullptr;

  return webSocket;
}

void DOMWebSocket::connect(const String& url,
                           const Vector<String>& protocols,
                           ExceptionState& exceptionState) {
  UseCounter::count(getExecutionContext(), UseCounter::WebSocket);

  NETWORK_DVLOG(1) << "WebSocket " << this << " connect() url=" << url;
  m_url = KURL(KURL(), url);

  if (getExecutionContext()->securityContext().getInsecureRequestPolicy() &
          kUpgradeInsecureRequests &&
      m_url.protocol() == "ws") {
    UseCounter::count(getExecutionContext(),
                      UseCounter::UpgradeInsecureRequestsUpgradedRequest);
    m_url.setProtocol("wss");
    if (m_url.port() == 80)
      m_url.setPort(443);
  }

  if (!m_url.isValid()) {
    m_state = kClosed;
    exceptionState.throwDOMException(SyntaxError,
                                     "The URL '" + url + "' is invalid.");
    return;
  }
  if (!m_url.protocolIs("ws") && !m_url.protocolIs("wss")) {
    m_state = kClosed;
    exceptionState.throwDOMException(
        SyntaxError, "The URL's scheme must be either 'ws' or 'wss'. '" +
                         m_url.protocol() + "' is not allowed.");
    return;
  }

  if (m_url.hasFragmentIdentifier()) {
    m_state = kClosed;
    exceptionState.throwDOMException(
        SyntaxError,
        "The URL contains a fragment identifier ('" +
            m_url.fragmentIdentifier() +
            "'). Fragment identifiers are not allowed in WebSocket URLs.");
    return;
  }

  if (!isPortAllowedForScheme(m_url)) {
    m_state = kClosed;
    exceptionState.throwSecurityError(
        "The port " + String::number(m_url.port()) + " is not allowed.");
    return;
  }

  // FIXME: Convert this to check the isolated world's Content Security Policy
  // once webkit.org/b/104520 is solved.
  if (!ContentSecurityPolicy::shouldBypassMainWorld(getExecutionContext()) &&
      !getExecutionContext()->contentSecurityPolicy()->allowConnectToSource(
          m_url)) {
    m_state = kClosed;
    // The URL is safe to expose to JavaScript, as this check happens
    // synchronously before redirection.
    exceptionState.throwSecurityError(
        "Refused to connect to '" + m_url.elidedString() +
        "' because it violates the document's Content Security Policy.");
    return;
  }

  // Fail if not all elements in |protocols| are valid.
  for (size_t i = 0; i < protocols.size(); ++i) {
    if (!isValidSubprotocolString(protocols[i])) {
      m_state = kClosed;
      exceptionState.throwDOMException(
          SyntaxError, "The subprotocol '" +
                           encodeSubprotocolString(protocols[i]) +
                           "' is invalid.");
      return;
    }
  }

  // Fail if there're duplicated elements in |protocols|.
  HashSet<String> visited;
  for (size_t i = 0; i < protocols.size(); ++i) {
    if (!visited.add(protocols[i]).isNewEntry) {
      m_state = kClosed;
      exceptionState.throwDOMException(
          SyntaxError, "The subprotocol '" +
                           encodeSubprotocolString(protocols[i]) +
                           "' is duplicated.");
      return;
    }
  }

  if (getExecutionContext()->getSecurityOrigin()->hasSuborigin()) {
    m_state = kClosed;
    exceptionState.throwSecurityError(
        "Connecting to a WebSocket from a suborigin is not allowed.");
    return;
  }

  String protocolString;
  if (!protocols.isEmpty())
    protocolString = joinStrings(protocols, subprotocolSeperator());

  m_channel = createChannel(getExecutionContext(), this);

  if (!m_channel->connect(m_url, protocolString)) {
    m_state = kClosed;
    exceptionState.throwSecurityError(
        "An insecure WebSocket connection may not be initiated from a page "
        "loaded over HTTPS.");
    releaseChannel();
    return;
  }
}

void DOMWebSocket::updateBufferedAmountAfterClose(uint64_t payloadSize) {
  m_bufferedAmountAfterClose += payloadSize;

  logError("WebSocket is already in CLOSING or CLOSED state.");
}

void DOMWebSocket::reflectBufferedAmountConsumption(TimerBase*) {
  DCHECK_GE(m_bufferedAmount, m_consumedBufferedAmount);
  // Cast to unsigned long long is required since clang doesn't accept
  // combination of %llu and uint64_t (known as unsigned long).
  NETWORK_DVLOG(1) << "WebSocket " << this
                   << " reflectBufferedAmountConsumption() " << m_bufferedAmount
                   << " => " << (m_bufferedAmount - m_consumedBufferedAmount);

  m_bufferedAmount -= m_consumedBufferedAmount;
  m_consumedBufferedAmount = 0;
}

void DOMWebSocket::releaseChannel() {
  DCHECK(m_channel);
  m_channel->disconnect();
  m_channel = nullptr;
}

void DOMWebSocket::logBinaryTypeChangesAfterOpen() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      CustomCountHistogram, binaryTypeChangesHistogram,
      new CustomCountHistogram("WebCore.WebSocket.BinaryTypeChangesAfterOpen",
                               1, 1024, 10));
  DVLOG(3) << "WebSocket " << static_cast<void*>(this)
           << " logBinaryTypeChangesAfterOpen() logging "
           << m_binaryTypeChangesAfterOpen;
  binaryTypeChangesHistogram.count(m_binaryTypeChangesAfterOpen);
}

void DOMWebSocket::send(const String& message, ExceptionState& exceptionState) {
  CString encodedMessage = message.utf8();

  NETWORK_DVLOG(1) << "WebSocket " << this << " send() Sending String "
                   << message;
  if (m_state == kConnecting) {
    setInvalidStateErrorForSendMethod(exceptionState);
    return;
  }
  // No exception is raised if the connection was once established but has
  // subsequently been closed.
  if (m_state == kClosing || m_state == kClosed) {
    updateBufferedAmountAfterClose(encodedMessage.length());
    return;
  }

  recordSendTypeHistogram(WebSocketSendTypeString);

  DCHECK(m_channel);
  m_bufferedAmount += encodedMessage.length();
  m_channel->send(encodedMessage);
}

void DOMWebSocket::send(DOMArrayBuffer* binaryData,
                        ExceptionState& exceptionState) {
  NETWORK_DVLOG(1) << "WebSocket " << this << " send() Sending ArrayBuffer "
                   << binaryData;
  DCHECK(binaryData);
  DCHECK(binaryData->buffer());
  if (m_state == kConnecting) {
    setInvalidStateErrorForSendMethod(exceptionState);
    return;
  }
  if (m_state == kClosing || m_state == kClosed) {
    updateBufferedAmountAfterClose(binaryData->byteLength());
    return;
  }
  recordSendTypeHistogram(WebSocketSendTypeArrayBuffer);
  recordSendMessageSizeHistogram(WebSocketSendTypeArrayBuffer,
                                 binaryData->byteLength());
  DCHECK(m_channel);
  m_bufferedAmount += binaryData->byteLength();
  m_channel->send(*binaryData, 0, binaryData->byteLength());
}

void DOMWebSocket::send(DOMArrayBufferView* arrayBufferView,
                        ExceptionState& exceptionState) {
  NETWORK_DVLOG(1) << "WebSocket " << this << " send() Sending ArrayBufferView "
                   << arrayBufferView;
  DCHECK(arrayBufferView);
  if (m_state == kConnecting) {
    setInvalidStateErrorForSendMethod(exceptionState);
    return;
  }
  if (m_state == kClosing || m_state == kClosed) {
    updateBufferedAmountAfterClose(arrayBufferView->byteLength());
    return;
  }
  recordSendTypeHistogram(WebSocketSendTypeArrayBufferView);
  recordSendMessageSizeHistogram(WebSocketSendTypeArrayBufferView,
                                 arrayBufferView->byteLength());
  DCHECK(m_channel);
  m_bufferedAmount += arrayBufferView->byteLength();
  m_channel->send(*arrayBufferView->buffer(), arrayBufferView->byteOffset(),
                  arrayBufferView->byteLength());
}

void DOMWebSocket::send(Blob* binaryData, ExceptionState& exceptionState) {
  NETWORK_DVLOG(1) << "WebSocket " << this << " send() Sending Blob "
                   << binaryData->uuid();
  DCHECK(binaryData);
  if (m_state == kConnecting) {
    setInvalidStateErrorForSendMethod(exceptionState);
    return;
  }
  if (m_state == kClosing || m_state == kClosed) {
    updateBufferedAmountAfterClose(binaryData->size());
    return;
  }
  unsigned long long size = binaryData->size();
  recordSendTypeHistogram(WebSocketSendTypeBlob);
  recordSendMessageSizeHistogram(
      WebSocketSendTypeBlob,
      clampTo<size_t>(size, 0, kMaxByteSizeForHistogram));
  m_bufferedAmount += size;
  DCHECK(m_channel);

  // When the runtime type of |binaryData| is File,
  // binaryData->blobDataHandle()->size() returns -1. However, in order to
  // maintain the value of |m_bufferedAmount| correctly, the WebSocket code
  // needs to fix the size of the File at this point. For this reason,
  // construct a new BlobDataHandle here with the size that this method
  // observed.
  m_channel->send(
      BlobDataHandle::create(binaryData->uuid(), binaryData->type(), size));
}

void DOMWebSocket::close(unsigned short code,
                         const String& reason,
                         ExceptionState& exceptionState) {
  closeInternal(code, reason, exceptionState);
}

void DOMWebSocket::close(ExceptionState& exceptionState) {
  closeInternal(WebSocketChannel::CloseEventCodeNotSpecified, String(),
                exceptionState);
}

void DOMWebSocket::close(unsigned short code, ExceptionState& exceptionState) {
  closeInternal(code, String(), exceptionState);
}

void DOMWebSocket::closeInternal(int code,
                                 const String& reason,
                                 ExceptionState& exceptionState) {
  String cleansedReason = reason;
  if (code == WebSocketChannel::CloseEventCodeNotSpecified) {
    NETWORK_DVLOG(1) << "WebSocket " << this
                     << " close() without code and reason";
  } else {
    NETWORK_DVLOG(1) << "WebSocket " << this << " close() code=" << code
                     << " reason=" << reason;
    if (!(code == WebSocketChannel::CloseEventCodeNormalClosure ||
          (WebSocketChannel::CloseEventCodeMinimumUserDefined <= code &&
           code <= WebSocketChannel::CloseEventCodeMaximumUserDefined))) {
      exceptionState.throwDOMException(
          InvalidAccessError,
          "The code must be either 1000, or between 3000 and 4999. " +
              String::number(code) + " is neither.");
      return;
    }
    // Bindings specify USVString, so unpaired surrogates are already replaced
    // with U+FFFD.
    CString utf8 = reason.utf8();
    if (utf8.length() > maxReasonSizeInBytes) {
      exceptionState.throwDOMException(
          SyntaxError, "The message must not be greater than " +
                           String::number(maxReasonSizeInBytes) + " bytes.");
      return;
    }
    if (!reason.isEmpty() && !reason.is8Bit()) {
      DCHECK_GT(utf8.length(), 0u);
      // reason might contain unpaired surrogates. Reconstruct it from
      // utf8.
      cleansedReason = String::fromUTF8(utf8.data(), utf8.length());
    }
  }

  if (m_state == kClosing || m_state == kClosed)
    return;
  if (m_state == kConnecting) {
    m_state = kClosing;
    m_channel->fail("WebSocket is closed before the connection is established.",
                    WarningMessageLevel,
                    SourceLocation::create(String(), 0, 0, nullptr));
    return;
  }
  m_state = kClosing;
  if (m_channel)
    m_channel->close(code, cleansedReason);
}

const KURL& DOMWebSocket::url() const {
  return m_url;
}

DOMWebSocket::State DOMWebSocket::readyState() const {
  return m_state;
}

unsigned DOMWebSocket::bufferedAmount() const {
  uint64_t sum = m_bufferedAmountAfterClose + m_bufferedAmount;

  if (sum > std::numeric_limits<unsigned>::max())
    return std::numeric_limits<unsigned>::max();
  return sum;
}

String DOMWebSocket::protocol() const {
  return m_subprotocol;
}

String DOMWebSocket::extensions() const {
  return m_extensions;
}

String DOMWebSocket::binaryType() const {
  switch (m_binaryType) {
    case BinaryTypeBlob:
      return "blob";
    case BinaryTypeArrayBuffer:
      return "arraybuffer";
  }
  NOTREACHED();
  return String();
}

void DOMWebSocket::setBinaryType(const String& binaryType) {
  if (binaryType == "blob") {
    setBinaryTypeInternal(BinaryTypeBlob);
    return;
  }
  if (binaryType == "arraybuffer") {
    setBinaryTypeInternal(BinaryTypeArrayBuffer);
    return;
  }
  NOTREACHED();
}

void DOMWebSocket::setBinaryTypeInternal(BinaryType binaryType) {
  if (m_binaryType == binaryType)
    return;
  m_binaryType = binaryType;
  if (m_state == kOpen || m_state == kClosing)
    ++m_binaryTypeChangesAfterOpen;
}

const AtomicString& DOMWebSocket::interfaceName() const {
  return EventTargetNames::DOMWebSocket;
}

ExecutionContext* DOMWebSocket::getExecutionContext() const {
  return ActiveDOMObject::getExecutionContext();
}

void DOMWebSocket::contextDestroyed() {
  NETWORK_DVLOG(1) << "WebSocket " << this << " contextDestroyed()";
  m_eventQueue->contextDestroyed();
  if (m_channel) {
    m_channel->close(WebSocketChannel::CloseEventCodeGoingAway, String());
    releaseChannel();
  }
  if (m_state != kClosed) {
    m_state = kClosed;
    logBinaryTypeChangesAfterOpen();
  }
}

bool DOMWebSocket::hasPendingActivity() const {
  return m_channel || !m_eventQueue->isEmpty();
}

void DOMWebSocket::suspend() {
  m_eventQueue->suspend();
}

void DOMWebSocket::resume() {
  m_eventQueue->resume();
}

void DOMWebSocket::didConnect(const String& subprotocol,
                              const String& extensions) {
  NETWORK_DVLOG(1) << "WebSocket " << this << " didConnect()";
  if (m_state != kConnecting)
    return;
  m_state = kOpen;
  m_subprotocol = subprotocol;
  m_extensions = extensions;
  m_eventQueue->dispatch(Event::create(EventTypeNames::open));
}

void DOMWebSocket::didReceiveTextMessage(const String& msg) {
  NETWORK_DVLOG(1) << "WebSocket " << this
                   << " didReceiveTextMessage() Text message " << msg;
  if (m_state != kOpen)
    return;
  recordReceiveTypeHistogram(WebSocketReceiveTypeString);

  m_eventQueue->dispatch(
      MessageEvent::create(msg, SecurityOrigin::create(m_url)->toString()));
}

void DOMWebSocket::didReceiveBinaryMessage(
    std::unique_ptr<Vector<char>> binaryData) {
  NETWORK_DVLOG(1) << "WebSocket " << this << " didReceiveBinaryMessage() "
                   << binaryData->size() << " byte binary message";
  switch (m_binaryType) {
    case BinaryTypeBlob: {
      size_t size = binaryData->size();
      RefPtr<RawData> rawData = RawData::create();
      binaryData->swap(*rawData->mutableData());
      std::unique_ptr<BlobData> blobData = BlobData::create();
      blobData->appendData(rawData.release(), 0, BlobDataItem::toEndOfFile);
      Blob* blob =
          Blob::create(BlobDataHandle::create(std::move(blobData), size));
      recordReceiveTypeHistogram(WebSocketReceiveTypeBlob);
      recordReceiveMessageSizeHistogram(WebSocketReceiveTypeBlob, size);
      m_eventQueue->dispatch(MessageEvent::create(
          blob, SecurityOrigin::create(m_url)->toString()));
      break;
    }

    case BinaryTypeArrayBuffer:
      DOMArrayBuffer* arrayBuffer =
          DOMArrayBuffer::create(binaryData->data(), binaryData->size());
      recordReceiveTypeHistogram(WebSocketReceiveTypeArrayBuffer);
      recordReceiveMessageSizeHistogram(WebSocketReceiveTypeArrayBuffer,
                                        binaryData->size());
      m_eventQueue->dispatch(MessageEvent::create(
          arrayBuffer, SecurityOrigin::create(m_url)->toString()));
      break;
  }
}

void DOMWebSocket::didError() {
  NETWORK_DVLOG(1) << "WebSocket " << this << " didError()";
  m_state = kClosed;
  logBinaryTypeChangesAfterOpen();
  m_eventQueue->dispatch(Event::create(EventTypeNames::error));
}

void DOMWebSocket::didConsumeBufferedAmount(uint64_t consumed) {
  DCHECK_GE(m_bufferedAmount, consumed + m_consumedBufferedAmount);
  NETWORK_DVLOG(1) << "WebSocket " << this << " didConsumeBufferedAmount("
                   << consumed << ")";
  if (m_state == kClosed)
    return;
  m_consumedBufferedAmount += consumed;
  if (!m_bufferedAmountConsumeTimer.isActive())
    m_bufferedAmountConsumeTimer.startOneShot(0, BLINK_FROM_HERE);
}

void DOMWebSocket::didStartClosingHandshake() {
  NETWORK_DVLOG(1) << "WebSocket " << this << " didStartClosingHandshake()";
  m_state = kClosing;
}

void DOMWebSocket::didClose(
    ClosingHandshakeCompletionStatus closingHandshakeCompletion,
    unsigned short code,
    const String& reason) {
  NETWORK_DVLOG(1) << "WebSocket " << this << " didClose()";
  if (!m_channel)
    return;
  bool allDataHasBeenConsumed = m_bufferedAmount == m_consumedBufferedAmount;
  bool wasClean = m_state == kClosing && allDataHasBeenConsumed &&
                  closingHandshakeCompletion == ClosingHandshakeComplete &&
                  code != WebSocketChannel::CloseEventCodeAbnormalClosure;
  m_state = kClosed;

  m_eventQueue->dispatch(CloseEvent::create(wasClean, code, reason));
  releaseChannel();
}

void DOMWebSocket::recordSendTypeHistogram(WebSocketSendType type) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, sendTypeHistogram,
      new EnumerationHistogram("WebCore.WebSocket.SendType",
                               WebSocketSendTypeMax));
  sendTypeHistogram.count(type);
}

void DOMWebSocket::recordSendMessageSizeHistogram(WebSocketSendType type,
                                                  size_t size) {
  // Truncate |size| to avoid overflowing int32_t.
  int32_t sizeToCount = clampTo<int32_t>(size, 0, kMaxByteSizeForHistogram);
  switch (type) {
    case WebSocketSendTypeArrayBuffer: {
      DEFINE_THREAD_SAFE_STATIC_LOCAL(
          CustomCountHistogram, arrayBufferMessageSizeHistogram,
          new CustomCountHistogram(
              "WebCore.WebSocket.MessageSize.Send.ArrayBuffer", 1,
              kMaxByteSizeForHistogram, kBucketCountForMessageSizeHistogram));
      arrayBufferMessageSizeHistogram.count(sizeToCount);
      return;
    }

    case WebSocketSendTypeArrayBufferView: {
      DEFINE_THREAD_SAFE_STATIC_LOCAL(
          CustomCountHistogram, arrayBufferViewMessageSizeHistogram,
          new CustomCountHistogram(
              "WebCore.WebSocket.MessageSize.Send.ArrayBufferView", 1,
              kMaxByteSizeForHistogram, kBucketCountForMessageSizeHistogram));
      arrayBufferViewMessageSizeHistogram.count(sizeToCount);
      return;
    }

    case WebSocketSendTypeBlob: {
      DEFINE_THREAD_SAFE_STATIC_LOCAL(
          CustomCountHistogram, blobMessageSizeHistogram,
          new CustomCountHistogram("WebCore.WebSocket.MessageSize.Send.Blob", 1,
                                   kMaxByteSizeForHistogram,
                                   kBucketCountForMessageSizeHistogram));
      blobMessageSizeHistogram.count(sizeToCount);
      return;
    }

    default:
      NOTREACHED();
  }
}

void DOMWebSocket::recordReceiveTypeHistogram(WebSocketReceiveType type) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, receiveTypeHistogram,
      new EnumerationHistogram("WebCore.WebSocket.ReceiveType",
                               WebSocketReceiveTypeMax));
  receiveTypeHistogram.count(type);
}

void DOMWebSocket::recordReceiveMessageSizeHistogram(WebSocketReceiveType type,
                                                     size_t size) {
  // Truncate |size| to avoid overflowing int32_t.
  int32_t sizeToCount = clampTo<int32_t>(size, 0, kMaxByteSizeForHistogram);
  switch (type) {
    case WebSocketReceiveTypeArrayBuffer: {
      DEFINE_THREAD_SAFE_STATIC_LOCAL(
          CustomCountHistogram, arrayBufferMessageSizeHistogram,
          new CustomCountHistogram(
              "WebCore.WebSocket.MessageSize.Receive.ArrayBuffer", 1,
              kMaxByteSizeForHistogram, kBucketCountForMessageSizeHistogram));
      arrayBufferMessageSizeHistogram.count(sizeToCount);
      return;
    }

    case WebSocketReceiveTypeBlob: {
      DEFINE_THREAD_SAFE_STATIC_LOCAL(
          CustomCountHistogram, blobMessageSizeHistogram,
          new CustomCountHistogram("WebCore.WebSocket.MessageSize.Receive.Blob",
                                   1, kMaxByteSizeForHistogram,
                                   kBucketCountForMessageSizeHistogram));
      blobMessageSizeHistogram.count(sizeToCount);
      return;
    }

    default:
      NOTREACHED();
  }
}

DEFINE_TRACE(DOMWebSocket) {
  visitor->trace(m_channel);
  visitor->trace(m_eventQueue);
  WebSocketChannelClient::trace(visitor);
  EventTargetWithInlineData::trace(visitor);
  ActiveDOMObject::trace(visitor);
}

}  // namespace blink
