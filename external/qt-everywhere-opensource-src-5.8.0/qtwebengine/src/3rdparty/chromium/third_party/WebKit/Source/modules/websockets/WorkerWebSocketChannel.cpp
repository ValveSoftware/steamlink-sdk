/*
 * Copyright (C) 2011, 2012 Google Inc.  All rights reserved.
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

#include "modules/websockets/WorkerWebSocketChannel.h"

#include "core/dom/CrossThreadTask.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/fileapi/Blob.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerLoaderProxy.h"
#include "core/workers/WorkerThread.h"
#include "modules/websockets/DocumentWebSocketChannel.h"
#include "platform/WaitableEvent.h"
#include "platform/heap/SafePoint.h"
#include "public/platform/Platform.h"
#include "wtf/Assertions.h"
#include "wtf/Functional.h"
#include "wtf/PtrUtil.h"
#include "wtf/text/CString.h"
#include "wtf/text/WTFString.h"
#include <memory>

namespace blink {

typedef WorkerWebSocketChannel::Bridge Bridge;
typedef WorkerWebSocketChannel::Peer Peer;

// Created and destroyed on the worker thread. All setters of this class are
// called on the main thread, while all getters are called on the worker
// thread. signalWorkerThread() must be called before any getters are called.
class WebSocketChannelSyncHelper : public GarbageCollectedFinalized<WebSocketChannelSyncHelper> {
public:
    static WebSocketChannelSyncHelper* create(std::unique_ptr<WaitableEvent> event)
    {
        return new WebSocketChannelSyncHelper(std::move(event));
    }

    ~WebSocketChannelSyncHelper()
    {
    }

    // All setters are called on the main thread.
    void setConnectRequestResult(bool connectRequestResult)
    {
        m_connectRequestResult = connectRequestResult;
    }

    // All getter are called on the worker thread.
    bool connectRequestResult() const
    {
        return m_connectRequestResult;
    }

    // This should be called after all setters are called and before any
    // getters are called.
    void signalWorkerThread()
    {
        m_event->signal();
    }
    void wait()
    {
        m_event->wait();
    }

    DEFINE_INLINE_TRACE() { }

private:
    explicit WebSocketChannelSyncHelper(std::unique_ptr<WaitableEvent> event)
        : m_event(std::move(event))
        , m_connectRequestResult(false)
    {
    }

    std::unique_ptr<WaitableEvent> m_event;
    bool m_connectRequestResult;
};

WorkerWebSocketChannel::WorkerWebSocketChannel(WorkerGlobalScope& workerGlobalScope, WebSocketChannelClient* client, std::unique_ptr<SourceLocation> location)
    : m_bridge(new Bridge(client, workerGlobalScope))
    , m_locationAtConnection(std::move(location))
{
    m_bridge->initialize(m_locationAtConnection->clone());
}

WorkerWebSocketChannel::~WorkerWebSocketChannel()
{
    ASSERT(!m_bridge);
}

bool WorkerWebSocketChannel::connect(const KURL& url, const String& protocol)
{
    ASSERT(m_bridge);
    return m_bridge->connect(url, protocol);
}

void WorkerWebSocketChannel::send(const CString& message)
{
    ASSERT(m_bridge);
    m_bridge->send(message);
}

void WorkerWebSocketChannel::send(const DOMArrayBuffer& binaryData, unsigned byteOffset, unsigned byteLength)
{
    ASSERT(m_bridge);
    m_bridge->send(binaryData, byteOffset, byteLength);
}

void WorkerWebSocketChannel::send(PassRefPtr<BlobDataHandle> blobData)
{
    ASSERT(m_bridge);
    m_bridge->send(blobData);
}

void WorkerWebSocketChannel::close(int code, const String& reason)
{
    ASSERT(m_bridge);
    m_bridge->close(code, reason);
}

void WorkerWebSocketChannel::fail(const String& reason, MessageLevel level, std::unique_ptr<SourceLocation> location)
{
    if (!m_bridge)
        return;

    std::unique_ptr<SourceLocation> capturedLocation = SourceLocation::capture();
    if (!capturedLocation->isUnknown()) {
        // If we are in JavaScript context, use the current location instead
        // of passed one - it's more precise.
        m_bridge->fail(reason, level, std::move(capturedLocation));
    } else if (location->isUnknown()) {
        // No information is specified by the caller - use the url
        // and the line number at the connection.
        m_bridge->fail(reason, level, m_locationAtConnection->clone());
    } else {
        // Use the specified information.
        m_bridge->fail(reason, level, std::move(location));
    }
}

void WorkerWebSocketChannel::disconnect()
{
    m_bridge->disconnect();
    m_bridge.clear();
}

DEFINE_TRACE(WorkerWebSocketChannel)
{
    visitor->trace(m_bridge);
    WebSocketChannel::trace(visitor);
}

Peer::Peer(Bridge* bridge, PassRefPtr<WorkerLoaderProxy> loaderProxy, WebSocketChannelSyncHelper* syncHelper, WorkerThreadLifecycleContext* workerThreadLifecycleContext)
    : WorkerThreadLifecycleObserver(workerThreadLifecycleContext)
    , m_bridge(bridge)
    , m_loaderProxy(loaderProxy)
    , m_mainWebSocketChannel(nullptr)
    , m_syncHelper(syncHelper)
{
    DCHECK(isMainThread());
}

Peer::~Peer()
{
    DCHECK(isMainThread());
}

bool Peer::initialize(std::unique_ptr<SourceLocation> location, ExecutionContext* context)
{
    ASSERT(isMainThread());
    if (wasContextDestroyedBeforeObserverCreation())
        return false;
    Document* document = toDocument(context);
    m_mainWebSocketChannel = DocumentWebSocketChannel::create(document, this, std::move(location));
    return true;
}

void Peer::connect(const KURL& url, const String& protocol)
{
    ASSERT(isMainThread());
    ASSERT(m_syncHelper);
    if (!m_mainWebSocketChannel) {
        m_syncHelper->setConnectRequestResult(false);
    } else {
        bool connectRequestResult = m_mainWebSocketChannel->connect(url, protocol);
        m_syncHelper->setConnectRequestResult(connectRequestResult);
    }
    m_syncHelper->signalWorkerThread();
}

void Peer::sendTextAsCharVector(std::unique_ptr<Vector<char>> data)
{
    ASSERT(isMainThread());
    if (m_mainWebSocketChannel)
        m_mainWebSocketChannel->sendTextAsCharVector(std::move(data));
}

void Peer::sendBinaryAsCharVector(std::unique_ptr<Vector<char>> data)
{
    ASSERT(isMainThread());
    if (m_mainWebSocketChannel)
        m_mainWebSocketChannel->sendBinaryAsCharVector(std::move(data));
}

void Peer::sendBlob(PassRefPtr<BlobDataHandle> blobData)
{
    ASSERT(isMainThread());
    if (m_mainWebSocketChannel)
        m_mainWebSocketChannel->send(blobData);
}

void Peer::close(int code, const String& reason)
{
    ASSERT(isMainThread());
    ASSERT(m_syncHelper);
    if (!m_mainWebSocketChannel)
        return;
    m_mainWebSocketChannel->close(code, reason);
}

void Peer::fail(const String& reason, MessageLevel level, std::unique_ptr<SourceLocation> location)
{
    ASSERT(isMainThread());
    ASSERT(m_syncHelper);
    if (!m_mainWebSocketChannel)
        return;
    m_mainWebSocketChannel->fail(reason, level, std::move(location));
}

void Peer::disconnect()
{
    ASSERT(isMainThread());
    ASSERT(m_syncHelper);
    if (m_mainWebSocketChannel) {
        m_mainWebSocketChannel->disconnect();
        m_mainWebSocketChannel = nullptr;
    }
    m_syncHelper->signalWorkerThread();
}

static void workerGlobalScopeDidConnect(Bridge* bridge, const String& subprotocol, const String& extensions, ExecutionContext* context)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    if (bridge && bridge->client())
        bridge->client()->didConnect(subprotocol, extensions);
}

void Peer::didConnect(const String& subprotocol, const String& extensions)
{
    ASSERT(isMainThread());
    m_loaderProxy->postTaskToWorkerGlobalScope(createCrossThreadTask(&workerGlobalScopeDidConnect, m_bridge, subprotocol, extensions));
}

static void workerGlobalScopeDidReceiveTextMessage(Bridge* bridge, const String& payload, ExecutionContext* context)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    if (bridge && bridge->client())
        bridge->client()->didReceiveTextMessage(payload);
}

void Peer::didReceiveTextMessage(const String& payload)
{
    ASSERT(isMainThread());
    m_loaderProxy->postTaskToWorkerGlobalScope(createCrossThreadTask(&workerGlobalScopeDidReceiveTextMessage, m_bridge, payload));
}

static void workerGlobalScopeDidReceiveBinaryMessage(Bridge* bridge, std::unique_ptr<Vector<char>> payload, ExecutionContext* context)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    if (bridge && bridge->client())
        bridge->client()->didReceiveBinaryMessage(std::move(payload));
}

void Peer::didReceiveBinaryMessage(std::unique_ptr<Vector<char>> payload)
{
    ASSERT(isMainThread());
    m_loaderProxy->postTaskToWorkerGlobalScope(createCrossThreadTask(&workerGlobalScopeDidReceiveBinaryMessage, m_bridge, passed(std::move(payload))));
}

static void workerGlobalScopeDidConsumeBufferedAmount(Bridge* bridge, uint64_t consumed, ExecutionContext* context)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    if (bridge && bridge->client())
        bridge->client()->didConsumeBufferedAmount(consumed);
}

void Peer::didConsumeBufferedAmount(uint64_t consumed)
{
    ASSERT(isMainThread());
    m_loaderProxy->postTaskToWorkerGlobalScope(createCrossThreadTask(&workerGlobalScopeDidConsumeBufferedAmount, m_bridge, consumed));
}

static void workerGlobalScopeDidStartClosingHandshake(Bridge* bridge, ExecutionContext* context)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    if (bridge && bridge->client())
        bridge->client()->didStartClosingHandshake();
}

void Peer::didStartClosingHandshake()
{
    ASSERT(isMainThread());
    m_loaderProxy->postTaskToWorkerGlobalScope(createCrossThreadTask(&workerGlobalScopeDidStartClosingHandshake, m_bridge));
}

static void workerGlobalScopeDidClose(Bridge* bridge, WebSocketChannelClient::ClosingHandshakeCompletionStatus closingHandshakeCompletion, unsigned short code, const String& reason, ExecutionContext* context)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    if (bridge && bridge->client())
        bridge->client()->didClose(closingHandshakeCompletion, code, reason);
}

void Peer::didClose(ClosingHandshakeCompletionStatus closingHandshakeCompletion, unsigned short code, const String& reason)
{
    ASSERT(isMainThread());
    if (m_mainWebSocketChannel) {
        m_mainWebSocketChannel->disconnect();
        m_mainWebSocketChannel = nullptr;
    }
    m_loaderProxy->postTaskToWorkerGlobalScope(createCrossThreadTask(&workerGlobalScopeDidClose, m_bridge, closingHandshakeCompletion, code, reason));
}

static void workerGlobalScopeDidError(Bridge* bridge, ExecutionContext* context)
{
    ASSERT_UNUSED(context, context->isWorkerGlobalScope());
    if (bridge && bridge->client())
        bridge->client()->didError();
}

void Peer::didError()
{
    ASSERT(isMainThread());
    m_loaderProxy->postTaskToWorkerGlobalScope(createCrossThreadTask(&workerGlobalScopeDidError, m_bridge));
}

void Peer::contextDestroyed()
{
    DCHECK(isMainThread());
    if (m_mainWebSocketChannel) {
        m_mainWebSocketChannel->disconnect();
        m_mainWebSocketChannel = nullptr;
    }
    m_bridge = nullptr;
}

DEFINE_TRACE(Peer)
{
    visitor->trace(m_mainWebSocketChannel);
    visitor->trace(m_syncHelper);
    WebSocketChannelClient::trace(visitor);
    WorkerThreadLifecycleObserver::trace(visitor);
}

Bridge::Bridge(WebSocketChannelClient* client, WorkerGlobalScope& workerGlobalScope)
    : m_client(client)
    , m_workerGlobalScope(workerGlobalScope)
    , m_loaderProxy(m_workerGlobalScope->thread()->workerLoaderProxy())
    , m_syncHelper(WebSocketChannelSyncHelper::create(wrapUnique(new WaitableEvent())))
{
}

Bridge::~Bridge()
{
    ASSERT(!m_peer);
}

void Bridge::createPeerOnMainThread(std::unique_ptr<SourceLocation> location, WorkerThreadLifecycleContext* workerThreadLifecycleContext, ExecutionContext* context)
{
    DCHECK(isMainThread());
    DCHECK(!m_peer);
    Peer* peer = new Peer(this, m_loaderProxy, m_syncHelper, workerThreadLifecycleContext);
    if (peer->initialize(std::move(location), context))
        m_peer = peer;
    m_syncHelper->signalWorkerThread();
}

void Bridge::initialize(std::unique_ptr<SourceLocation> location)
{
    // Wait for completion of the task on the main thread because the connection
    // must synchronously be established (see Bridge::connect).
    if (!waitForMethodCompletion(createCrossThreadTask(&Bridge::createPeerOnMainThread, wrapCrossThreadPersistent(this), passed(std::move(location)), wrapCrossThreadPersistent(m_workerGlobalScope->thread()->getWorkerThreadLifecycleContext())))) {
        // The worker thread has been signalled to shutdown before method completion.
        disconnect();
    }
}

bool Bridge::connect(const KURL& url, const String& protocol)
{
    if (!m_peer)
        return false;

    // Wait for completion of the task on the main thread because the mixed
    // content check must synchronously be conducted.
    if (!waitForMethodCompletion(createCrossThreadTask(&Peer::connect, wrapCrossThreadPersistent(m_peer.get()), url, protocol)))
        return false;

    return m_syncHelper->connectRequestResult();
}

void Bridge::send(const CString& message)
{
    ASSERT(m_peer);
    std::unique_ptr<Vector<char>> data = wrapUnique(new Vector<char>(message.length()));
    if (message.length())
        memcpy(data->data(), static_cast<const char*>(message.data()), message.length());

    m_loaderProxy->postTaskToLoader(createCrossThreadTask(&Peer::sendTextAsCharVector, wrapCrossThreadPersistent(m_peer.get()), passed(std::move(data))));
}

void Bridge::send(const DOMArrayBuffer& binaryData, unsigned byteOffset, unsigned byteLength)
{
    ASSERT(m_peer);
    // ArrayBuffer isn't thread-safe, hence the content of ArrayBuffer is copied into Vector<char>.
    std::unique_ptr<Vector<char>> data = wrapUnique(new Vector<char>(byteLength));
    if (binaryData.byteLength())
        memcpy(data->data(), static_cast<const char*>(binaryData.data()) + byteOffset, byteLength);

    m_loaderProxy->postTaskToLoader(createCrossThreadTask(&Peer::sendBinaryAsCharVector, wrapCrossThreadPersistent(m_peer.get()), passed(std::move(data))));
}

void Bridge::send(PassRefPtr<BlobDataHandle> data)
{
    ASSERT(m_peer);
    m_loaderProxy->postTaskToLoader(createCrossThreadTask(&Peer::sendBlob, wrapCrossThreadPersistent(m_peer.get()), data));
}

void Bridge::close(int code, const String& reason)
{
    ASSERT(m_peer);
    m_loaderProxy->postTaskToLoader(createCrossThreadTask(&Peer::close, wrapCrossThreadPersistent(m_peer.get()), code, reason));
}

void Bridge::fail(const String& reason, MessageLevel level, std::unique_ptr<SourceLocation> location)
{
    ASSERT(m_peer);
    m_loaderProxy->postTaskToLoader(createCrossThreadTask(&Peer::fail, wrapCrossThreadPersistent(m_peer.get()), reason, level, passed(std::move(location))));
}

void Bridge::disconnect()
{
    if (!m_peer)
        return;

    // Wait for completion of the task on the main thread to ensure that
    // |m_peer| does not touch this Bridge object after this point.
    waitForMethodCompletion(createCrossThreadTask(&Peer::disconnect, wrapCrossThreadPersistent(m_peer.get())));

    m_client = nullptr;
    m_peer = nullptr;
    m_syncHelper = nullptr;
    // We won't use this any more.
    m_workerGlobalScope.clear();
}

// Caller of this function should hold a reference to the bridge, because this function may call WebSocket::didClose() in the end,
// which causes the bridge to get disconnected from the WebSocket and deleted if there is no other reference.
bool Bridge::waitForMethodCompletion(std::unique_ptr<ExecutionContextTask> task)
{
    ASSERT(m_workerGlobalScope);
    ASSERT(m_syncHelper);

    m_loaderProxy->postTaskToLoader(std::move(task));

    // We wait for the syncHelper event even if a shutdown event is fired.
    // See https://codereview.chromium.org/267323004/#msg43 for why we need to wait this.
    SafePointScope scope(BlinkGC::HeapPointersOnStack);
    m_syncHelper->wait();
    // This is checking whether a shutdown event is fired or not.
    return !m_workerGlobalScope->thread()->terminated();
}

DEFINE_TRACE(Bridge)
{
    visitor->trace(m_client);
    visitor->trace(m_workerGlobalScope);
    visitor->trace(m_syncHelper);
}

} // namespace blink
