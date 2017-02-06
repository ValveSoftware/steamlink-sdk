// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/presentation/PresentationConnection.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMArrayBufferView.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/events/Event.h"
#include "core/events/MessageEvent.h"
#include "core/fileapi/FileReaderLoader.h"
#include "core/fileapi/FileReaderLoaderClient.h"
#include "core/frame/Deprecation.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "modules/EventTargetModules.h"
#include "modules/presentation/Presentation.h"
#include "modules/presentation/PresentationConnectionAvailableEvent.h"
#include "modules/presentation/PresentationConnectionCloseEvent.h"
#include "modules/presentation/PresentationController.h"
#include "modules/presentation/PresentationRequest.h"
#include "public/platform/modules/presentation/WebPresentationConnectionClient.h"
#include "wtf/Assertions.h"
#include "wtf/text/AtomicString.h"
#include <memory>

namespace blink {

namespace {

// TODO(mlamouri): refactor in one common place.
WebPresentationClient* presentationClient(ExecutionContext* executionContext)
{
    ASSERT(executionContext && executionContext->isDocument());

    Document* document = toDocument(executionContext);
    if (!document->frame())
        return nullptr;
    PresentationController* controller = PresentationController::from(*document->frame());
    return controller ? controller->client() : nullptr;
}

const AtomicString& connectionStateToString(WebPresentationConnectionState state)
{
    DEFINE_STATIC_LOCAL(const AtomicString, connectedValue, ("connected"));
    DEFINE_STATIC_LOCAL(const AtomicString, closedValue, ("closed"));
    DEFINE_STATIC_LOCAL(const AtomicString, terminatedValue, ("terminated"));

    switch (state) {
    case WebPresentationConnectionState::Connected:
        return connectedValue;
    case WebPresentationConnectionState::Closed:
        return closedValue;
    case WebPresentationConnectionState::Terminated:
        return terminatedValue;
    }

    ASSERT_NOT_REACHED();
    return terminatedValue;
}

const AtomicString& connectionCloseReasonToString(WebPresentationConnectionCloseReason reason)
{
    DEFINE_STATIC_LOCAL(const AtomicString, errorValue, ("error"));
    DEFINE_STATIC_LOCAL(const AtomicString, closedValue, ("closed"));
    DEFINE_STATIC_LOCAL(const AtomicString, wentAwayValue, ("wentaway"));

    switch (reason) {
    case WebPresentationConnectionCloseReason::Error:
        return errorValue;
    case WebPresentationConnectionCloseReason::Closed:
        return closedValue;
    case WebPresentationConnectionCloseReason::WentAway:
        return wentAwayValue;
    }

    ASSERT_NOT_REACHED();
    return errorValue;
}

void throwPresentationDisconnectedError(ExceptionState& exceptionState)
{
    exceptionState.throwDOMException(InvalidStateError, "Presentation connection is disconnected.");
}

} // namespace

class PresentationConnection::Message final : public GarbageCollectedFinalized<PresentationConnection::Message> {
public:
    Message(const String& text)
        : type(MessageTypeText)
        , text(text)
    {
    }

    Message(DOMArrayBuffer* arrayBuffer)
        : type(MessageTypeArrayBuffer)
        , arrayBuffer(arrayBuffer)
    {
    }

    Message(PassRefPtr<BlobDataHandle> blobDataHandle)
        : type(MessageTypeBlob)
        , blobDataHandle(blobDataHandle)
    {
    }

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(arrayBuffer);
    }

    MessageType type;
    String text;
    Member<DOMArrayBuffer> arrayBuffer;
    RefPtr<BlobDataHandle> blobDataHandle;
};

class PresentationConnection::BlobLoader final : public GarbageCollectedFinalized<PresentationConnection::BlobLoader>, public FileReaderLoaderClient {
public:
    BlobLoader(PassRefPtr<BlobDataHandle> blobDataHandle, PresentationConnection* PresentationConnection)
        : m_PresentationConnection(PresentationConnection)
        , m_loader(FileReaderLoader::ReadAsArrayBuffer, this)
    {
        m_loader.start(m_PresentationConnection->getExecutionContext(), blobDataHandle);
    }
    ~BlobLoader() override { }

    // FileReaderLoaderClient functions.
    void didStartLoading() override { }
    void didReceiveData() override { }
    void didFinishLoading() override
    {
        m_PresentationConnection->didFinishLoadingBlob(m_loader.arrayBufferResult());
    }
    void didFail(FileError::ErrorCode errorCode) override
    {
        m_PresentationConnection->didFailLoadingBlob(errorCode);
    }

    void cancel()
    {
        m_loader.cancel();
    }

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_PresentationConnection);
    }

private:
    Member<PresentationConnection> m_PresentationConnection;
    FileReaderLoader m_loader;
};

PresentationConnection::PresentationConnection(LocalFrame* frame, const String& id, const String& url)
    : DOMWindowProperty(frame)
    , m_id(id)
    , m_url(url)
    , m_state(WebPresentationConnectionState::Connected)
    , m_binaryType(BinaryTypeBlob)
{
}

PresentationConnection::~PresentationConnection()
{
    ASSERT(!m_blobLoader);
}

// static
PresentationConnection* PresentationConnection::take(ScriptPromiseResolver* resolver, std::unique_ptr<WebPresentationConnectionClient> client, PresentationRequest* request)
{
    ASSERT(resolver);
    ASSERT(client);
    ASSERT(request);
    ASSERT(resolver->getExecutionContext()->isDocument());

    Document* document = toDocument(resolver->getExecutionContext());
    if (!document->frame())
        return nullptr;

    PresentationController* controller = PresentationController::from(*document->frame());
    if (!controller)
        return nullptr;

    return take(controller, std::move(client), request);
}

// static
PresentationConnection* PresentationConnection::take(PresentationController* controller, std::unique_ptr<WebPresentationConnectionClient> client, PresentationRequest* request)
{
    ASSERT(controller);
    ASSERT(request);

    PresentationConnection* connection = new PresentationConnection(controller->frame(), client->getId(), client->getUrl());
    controller->registerConnection(connection);
    request->dispatchEvent(PresentationConnectionAvailableEvent::create(EventTypeNames::connectionavailable, connection));

    return connection;
}

const AtomicString& PresentationConnection::interfaceName() const
{
    return EventTargetNames::PresentationConnection;
}

ExecutionContext* PresentationConnection::getExecutionContext() const
{
    if (!frame())
        return nullptr;
    return frame()->document();
}

void PresentationConnection::addedEventListener(const AtomicString& eventType, RegisteredEventListener& registeredListener)
{
    EventTargetWithInlineData::addedEventListener(eventType, registeredListener);
    if (eventType == EventTypeNames::connect)
        UseCounter::count(getExecutionContext(), UseCounter::PresentationConnectionConnectEventListener);
    else if (eventType == EventTypeNames::close)
        UseCounter::count(getExecutionContext(), UseCounter::PresentationConnectionCloseEventListener);
    else if (eventType == EventTypeNames::terminate)
        UseCounter::count(getExecutionContext(), UseCounter::PresentationConnectionTerminateEventListener);
    else if (eventType == EventTypeNames::message)
        UseCounter::count(getExecutionContext(), UseCounter::PresentationConnectionMessageEventListener);
}

DEFINE_TRACE(PresentationConnection)
{
    visitor->trace(m_blobLoader);
    visitor->trace(m_messages);
    EventTargetWithInlineData::trace(visitor);
    DOMWindowProperty::trace(visitor);
}

const AtomicString& PresentationConnection::state() const
{
    return connectionStateToString(m_state);
}

void PresentationConnection::send(const String& message, ExceptionState& exceptionState)
{
    if (!canSendMessage(exceptionState))
        return;

    m_messages.append(new Message(message));
    handleMessageQueue();
}

void PresentationConnection::send(DOMArrayBuffer* arrayBuffer, ExceptionState& exceptionState)
{
    ASSERT(arrayBuffer && arrayBuffer->buffer());
    if (!canSendMessage(exceptionState))
        return;

    m_messages.append(new Message(arrayBuffer));
    handleMessageQueue();
}

void PresentationConnection::send(DOMArrayBufferView* arrayBufferView, ExceptionState& exceptionState)
{
    ASSERT(arrayBufferView);
    if (!canSendMessage(exceptionState))
        return;

    m_messages.append(new Message(arrayBufferView->buffer()));
    handleMessageQueue();
}

void PresentationConnection::send(Blob* data, ExceptionState& exceptionState)
{
    ASSERT(data);
    if (!canSendMessage(exceptionState))
        return;

    m_messages.append(new Message(data->blobDataHandle()));
    handleMessageQueue();
}

bool PresentationConnection::canSendMessage(ExceptionState& exceptionState)
{
    if (m_state != WebPresentationConnectionState::Connected) {
        throwPresentationDisconnectedError(exceptionState);
        return false;
    }

    // The connection can send a message if there is a client available.
    return !!presentationClient(getExecutionContext());
}

void PresentationConnection::handleMessageQueue()
{
    WebPresentationClient* client = presentationClient(getExecutionContext());
    if (!client)
        return;

    while (!m_messages.isEmpty() && !m_blobLoader) {
        Message* message = m_messages.first().get();
        switch (message->type) {
        case MessageTypeText:
            client->sendString(m_url, m_id, message->text);
            m_messages.removeFirst();
            break;
        case MessageTypeArrayBuffer:
            client->sendArrayBuffer(m_url, m_id, static_cast<const uint8_t*>(message->arrayBuffer->data()), message->arrayBuffer->byteLength());
            m_messages.removeFirst();
            break;
        case MessageTypeBlob:
            ASSERT(!m_blobLoader);
            m_blobLoader = new BlobLoader(message->blobDataHandle, this);
            break;
        }
    }
}

String PresentationConnection::binaryType() const
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

void PresentationConnection::setBinaryType(const String& binaryType)
{
    if (binaryType == "blob") {
        m_binaryType = BinaryTypeBlob;
        return;
    }
    if (binaryType == "arraybuffer") {
        m_binaryType = BinaryTypeArrayBuffer;
        return;
    }
    ASSERT_NOT_REACHED();
}

void PresentationConnection::didReceiveTextMessage(const String& message)
{
    if (m_state != WebPresentationConnectionState::Connected)
        return;

    dispatchEvent(MessageEvent::create(message));
}

void PresentationConnection::didReceiveBinaryMessage(const uint8_t* data, size_t length)
{
    if (m_state != WebPresentationConnectionState::Connected)
        return;

    switch (m_binaryType) {
    case BinaryTypeBlob: {
        std::unique_ptr<BlobData> blobData = BlobData::create();
        blobData->appendBytes(data, length);
        Blob* blob = Blob::create(BlobDataHandle::create(std::move(blobData), length));
        dispatchEvent(MessageEvent::create(blob));
        return;
    }
    case BinaryTypeArrayBuffer:
        DOMArrayBuffer* buffer = DOMArrayBuffer::create(data, length);
        dispatchEvent(MessageEvent::create(buffer));
        return;
    }
    ASSERT_NOT_REACHED();
}

void PresentationConnection::close()
{
    if (m_state != WebPresentationConnectionState::Connected)
        return;
    WebPresentationClient* client = presentationClient(getExecutionContext());
    if (client)
        client->closeSession(m_url, m_id);

    tearDown();
}

void PresentationConnection::terminate()
{
    if (m_state != WebPresentationConnectionState::Connected)
        return;
    WebPresentationClient* client = presentationClient(getExecutionContext());
    if (client)
        client->terminateSession(m_url, m_id);

    tearDown();
}

bool PresentationConnection::matches(WebPresentationConnectionClient* client) const
{
    return client && m_url == static_cast<String>(client->getUrl()) && m_id == static_cast<String>(client->getId());
}

void PresentationConnection::didChangeState(WebPresentationConnectionState state)
{
    if (m_state == state)
        return;

    m_state = state;
    switch (m_state) {
    case WebPresentationConnectionState::Connected:
        dispatchEvent(Event::create(EventTypeNames::connect));
        return;
    case WebPresentationConnectionState::Terminated:
        dispatchEvent(Event::create(EventTypeNames::terminate));
        return;
    // Closed state is handled in |didClose()|.
    case WebPresentationConnectionState::Closed:
        return;
    }
    ASSERT_NOT_REACHED();
}

void PresentationConnection::didClose(WebPresentationConnectionCloseReason reason, const String& message)
{
    if (m_state == WebPresentationConnectionState::Closed)
        return;

    m_state = WebPresentationConnectionState::Closed;
    dispatchEvent(PresentationConnectionCloseEvent::create(EventTypeNames::close, connectionCloseReasonToString(reason), message));
}

void PresentationConnection::didFinishLoadingBlob(DOMArrayBuffer* buffer)
{
    ASSERT(!m_messages.isEmpty() && m_messages.first()->type == MessageTypeBlob);
    ASSERT(buffer && buffer->buffer());
    // Send the loaded blob immediately here and continue processing the queue.
    WebPresentationClient* client = presentationClient(getExecutionContext());
    if (client)
        client->sendBlobData(m_url, m_id, static_cast<const uint8_t*>(buffer->data()), buffer->byteLength());

    m_messages.removeFirst();
    m_blobLoader.clear();
    handleMessageQueue();
}

void PresentationConnection::didFailLoadingBlob(FileError::ErrorCode errorCode)
{
    ASSERT(!m_messages.isEmpty() && m_messages.first()->type == MessageTypeBlob);
    // FIXME: generate error message?
    // Ignore the current failed blob item and continue with next items.
    m_messages.removeFirst();
    m_blobLoader.clear();
    handleMessageQueue();
}

void PresentationConnection::tearDown()
{
    // Cancel current Blob loading if any.
    if (m_blobLoader) {
        m_blobLoader->cancel();
        m_blobLoader.clear();
    }
    m_messages.clear();
}

} // namespace blink
