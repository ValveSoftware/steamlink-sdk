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
class WebSocketChannelSyncHelper {
 public:
  WebSocketChannelSyncHelper() {}
  ~WebSocketChannelSyncHelper() {}

  // All setters are called on the main thread.
  void setConnectRequestResult(bool connectRequestResult) {
    DCHECK(isMainThread());
    m_connectRequestResult = connectRequestResult;
  }

  // All getters are called on the worker thread.
  bool connectRequestResult() const {
    DCHECK(!isMainThread());
    return m_connectRequestResult;
  }

  // This should be called after all setters are called and before any
  // getters are called.
  void signalWorkerThread() {
    DCHECK(isMainThread());
    m_event.signal();
  }

  void wait() {
    DCHECK(!isMainThread());
    m_event.wait();
  }

 private:
  WaitableEvent m_event;
  bool m_connectRequestResult = false;
};

WorkerWebSocketChannel::WorkerWebSocketChannel(
    WorkerGlobalScope& workerGlobalScope,
    WebSocketChannelClient* client,
    std::unique_ptr<SourceLocation> location)
    : m_bridge(new Bridge(client, workerGlobalScope)),
      m_locationAtConnection(std::move(location)) {}

WorkerWebSocketChannel::~WorkerWebSocketChannel() {
  DCHECK(!m_bridge);
}

bool WorkerWebSocketChannel::connect(const KURL& url, const String& protocol) {
  DCHECK(m_bridge);
  return m_bridge->connect(m_locationAtConnection->clone(), url, protocol);
}

void WorkerWebSocketChannel::send(const CString& message) {
  DCHECK(m_bridge);
  m_bridge->send(message);
}

void WorkerWebSocketChannel::send(const DOMArrayBuffer& binaryData,
                                  unsigned byteOffset,
                                  unsigned byteLength) {
  DCHECK(m_bridge);
  m_bridge->send(binaryData, byteOffset, byteLength);
}

void WorkerWebSocketChannel::send(PassRefPtr<BlobDataHandle> blobData) {
  DCHECK(m_bridge);
  m_bridge->send(std::move(blobData));
}

void WorkerWebSocketChannel::close(int code, const String& reason) {
  DCHECK(m_bridge);
  m_bridge->close(code, reason);
}

void WorkerWebSocketChannel::fail(const String& reason,
                                  MessageLevel level,
                                  std::unique_ptr<SourceLocation> location) {
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

void WorkerWebSocketChannel::disconnect() {
  m_bridge->disconnect();
  m_bridge.clear();
}

DEFINE_TRACE(WorkerWebSocketChannel) {
  visitor->trace(m_bridge);
  WebSocketChannel::trace(visitor);
}

Peer::Peer(Bridge* bridge,
           PassRefPtr<WorkerLoaderProxy> loaderProxy,
           WorkerThreadLifecycleContext* workerThreadLifecycleContext)
    : WorkerThreadLifecycleObserver(workerThreadLifecycleContext),
      m_bridge(bridge),
      m_loaderProxy(loaderProxy),
      m_mainWebSocketChannel(nullptr) {
  DCHECK(isMainThread());
}

Peer::~Peer() {
  DCHECK(isMainThread());
}

bool Peer::initialize(std::unique_ptr<SourceLocation> location,
                      ExecutionContext* context) {
  DCHECK(isMainThread());
  if (wasContextDestroyedBeforeObserverCreation())
    return false;
  Document* document = toDocument(context);
  m_mainWebSocketChannel =
      DocumentWebSocketChannel::create(document, this, std::move(location));
  return true;
}

bool Peer::connect(const KURL& url, const String& protocol) {
  DCHECK(isMainThread());
  if (!m_mainWebSocketChannel)
    return false;
  return m_mainWebSocketChannel->connect(url, protocol);
}

void Peer::sendTextAsCharVector(std::unique_ptr<Vector<char>> data) {
  DCHECK(isMainThread());
  if (m_mainWebSocketChannel)
    m_mainWebSocketChannel->sendTextAsCharVector(std::move(data));
}

void Peer::sendBinaryAsCharVector(std::unique_ptr<Vector<char>> data) {
  DCHECK(isMainThread());
  if (m_mainWebSocketChannel)
    m_mainWebSocketChannel->sendBinaryAsCharVector(std::move(data));
}

void Peer::sendBlob(PassRefPtr<BlobDataHandle> blobData) {
  DCHECK(isMainThread());
  if (m_mainWebSocketChannel)
    m_mainWebSocketChannel->send(std::move(blobData));
}

void Peer::close(int code, const String& reason) {
  DCHECK(isMainThread());
  if (!m_mainWebSocketChannel)
    return;
  m_mainWebSocketChannel->close(code, reason);
}

void Peer::fail(const String& reason,
                MessageLevel level,
                std::unique_ptr<SourceLocation> location) {
  DCHECK(isMainThread());
  if (!m_mainWebSocketChannel)
    return;
  m_mainWebSocketChannel->fail(reason, level, std::move(location));
}

void Peer::disconnect() {
  DCHECK(isMainThread());
  if (!m_mainWebSocketChannel)
    return;
  m_mainWebSocketChannel->disconnect();
  m_mainWebSocketChannel = nullptr;
}

static void workerGlobalScopeDidConnect(Bridge* bridge,
                                        const String& subprotocol,
                                        const String& extensions,
                                        ExecutionContext* context) {
  DCHECK(context->isWorkerGlobalScope());
  if (bridge && bridge->client())
    bridge->client()->didConnect(subprotocol, extensions);
}

void Peer::didConnect(const String& subprotocol, const String& extensions) {
  DCHECK(isMainThread());
  m_loaderProxy->postTaskToWorkerGlobalScope(
      BLINK_FROM_HERE,
      createCrossThreadTask(&workerGlobalScopeDidConnect, m_bridge, subprotocol,
                            extensions));
}

static void workerGlobalScopeDidReceiveTextMessage(Bridge* bridge,
                                                   const String& payload,
                                                   ExecutionContext* context) {
  DCHECK(context->isWorkerGlobalScope());
  if (bridge && bridge->client())
    bridge->client()->didReceiveTextMessage(payload);
}

void Peer::didReceiveTextMessage(const String& payload) {
  DCHECK(isMainThread());
  m_loaderProxy->postTaskToWorkerGlobalScope(
      BLINK_FROM_HERE,
      createCrossThreadTask(&workerGlobalScopeDidReceiveTextMessage, m_bridge,
                            payload));
}

static void workerGlobalScopeDidReceiveBinaryMessage(
    Bridge* bridge,
    std::unique_ptr<Vector<char>> payload,
    ExecutionContext* context) {
  DCHECK(context->isWorkerGlobalScope());
  if (bridge && bridge->client())
    bridge->client()->didReceiveBinaryMessage(std::move(payload));
}

void Peer::didReceiveBinaryMessage(std::unique_ptr<Vector<char>> payload) {
  DCHECK(isMainThread());
  m_loaderProxy->postTaskToWorkerGlobalScope(
      BLINK_FROM_HERE,
      createCrossThreadTask(&workerGlobalScopeDidReceiveBinaryMessage, m_bridge,
                            passed(std::move(payload))));
}

static void workerGlobalScopeDidConsumeBufferedAmount(
    Bridge* bridge,
    uint64_t consumed,
    ExecutionContext* context) {
  DCHECK(context->isWorkerGlobalScope());
  if (bridge && bridge->client())
    bridge->client()->didConsumeBufferedAmount(consumed);
}

void Peer::didConsumeBufferedAmount(uint64_t consumed) {
  DCHECK(isMainThread());
  m_loaderProxy->postTaskToWorkerGlobalScope(
      BLINK_FROM_HERE,
      createCrossThreadTask(&workerGlobalScopeDidConsumeBufferedAmount,
                            m_bridge, consumed));
}

static void workerGlobalScopeDidStartClosingHandshake(
    Bridge* bridge,
    ExecutionContext* context) {
  DCHECK(context->isWorkerGlobalScope());
  if (bridge && bridge->client())
    bridge->client()->didStartClosingHandshake();
}

void Peer::didStartClosingHandshake() {
  DCHECK(isMainThread());
  m_loaderProxy->postTaskToWorkerGlobalScope(
      BLINK_FROM_HERE,
      createCrossThreadTask(&workerGlobalScopeDidStartClosingHandshake,
                            m_bridge));
}

static void workerGlobalScopeDidClose(
    Bridge* bridge,
    WebSocketChannelClient::ClosingHandshakeCompletionStatus
        closingHandshakeCompletion,
    unsigned short code,
    const String& reason,
    ExecutionContext* context) {
  DCHECK(context->isWorkerGlobalScope());
  if (bridge && bridge->client())
    bridge->client()->didClose(closingHandshakeCompletion, code, reason);
}

void Peer::didClose(ClosingHandshakeCompletionStatus closingHandshakeCompletion,
                    unsigned short code,
                    const String& reason) {
  DCHECK(isMainThread());
  if (m_mainWebSocketChannel) {
    m_mainWebSocketChannel->disconnect();
    m_mainWebSocketChannel = nullptr;
  }
  m_loaderProxy->postTaskToWorkerGlobalScope(
      BLINK_FROM_HERE,
      createCrossThreadTask(&workerGlobalScopeDidClose, m_bridge,
                            closingHandshakeCompletion, code, reason));
}

static void workerGlobalScopeDidError(Bridge* bridge,
                                      ExecutionContext* context) {
  DCHECK(context->isWorkerGlobalScope());
  if (bridge && bridge->client())
    bridge->client()->didError();
}

void Peer::didError() {
  DCHECK(isMainThread());
  m_loaderProxy->postTaskToWorkerGlobalScope(
      BLINK_FROM_HERE,
      createCrossThreadTask(&workerGlobalScopeDidError, m_bridge));
}

void Peer::contextDestroyed() {
  DCHECK(isMainThread());
  if (m_mainWebSocketChannel) {
    m_mainWebSocketChannel->disconnect();
    m_mainWebSocketChannel = nullptr;
  }
  m_bridge = nullptr;
}

DEFINE_TRACE(Peer) {
  visitor->trace(m_mainWebSocketChannel);
  WebSocketChannelClient::trace(visitor);
  WorkerThreadLifecycleObserver::trace(visitor);
}

Bridge::Bridge(WebSocketChannelClient* client,
               WorkerGlobalScope& workerGlobalScope)
    : m_client(client),
      m_workerGlobalScope(workerGlobalScope),
      m_loaderProxy(m_workerGlobalScope->thread()->workerLoaderProxy()) {}

Bridge::~Bridge() {
  DCHECK(!m_peer);
}

void Bridge::connectOnMainThread(
    std::unique_ptr<SourceLocation> location,
    WorkerThreadLifecycleContext* workerThreadLifecycleContext,
    const KURL& url,
    const String& protocol,
    WebSocketChannelSyncHelper* syncHelper,
    ExecutionContext* context) {
  DCHECK(isMainThread());
  DCHECK(!m_peer);
  Peer* peer = new Peer(this, m_loaderProxy, workerThreadLifecycleContext);
  if (peer->initialize(std::move(location), context)) {
    m_peer = peer;
    syncHelper->setConnectRequestResult(m_peer->connect(url, protocol));
  }
  syncHelper->signalWorkerThread();
}

bool Bridge::connect(std::unique_ptr<SourceLocation> location,
                     const KURL& url,
                     const String& protocol) {
  // Wait for completion of the task on the main thread because the mixed
  // content check must synchronously be conducted.
  WebSocketChannelSyncHelper syncHelper;
  m_loaderProxy->postTaskToLoader(
      BLINK_FROM_HERE,
      createCrossThreadTask(
          &Bridge::connectOnMainThread, wrapCrossThreadPersistent(this),
          passed(location->clone()),
          wrapCrossThreadPersistent(
              m_workerGlobalScope->thread()->getWorkerThreadLifecycleContext()),
          url, protocol, crossThreadUnretained(&syncHelper)));
  syncHelper.wait();
  return syncHelper.connectRequestResult();
}

void Bridge::send(const CString& message) {
  DCHECK(m_peer);
  std::unique_ptr<Vector<char>> data =
      wrapUnique(new Vector<char>(message.length()));
  if (message.length())
    memcpy(data->data(), static_cast<const char*>(message.data()),
           message.length());

  m_loaderProxy->postTaskToLoader(
      BLINK_FROM_HERE, createCrossThreadTask(&Peer::sendTextAsCharVector,
                                             m_peer, passed(std::move(data))));
}

void Bridge::send(const DOMArrayBuffer& binaryData,
                  unsigned byteOffset,
                  unsigned byteLength) {
  DCHECK(m_peer);
  // ArrayBuffer isn't thread-safe, hence the content of ArrayBuffer is copied
  // into Vector<char>.
  std::unique_ptr<Vector<char>> data = makeUnique<Vector<char>>(byteLength);
  if (binaryData.byteLength())
    memcpy(data->data(),
           static_cast<const char*>(binaryData.data()) + byteOffset,
           byteLength);

  m_loaderProxy->postTaskToLoader(
      BLINK_FROM_HERE, createCrossThreadTask(&Peer::sendBinaryAsCharVector,
                                             m_peer, passed(std::move(data))));
}

void Bridge::send(PassRefPtr<BlobDataHandle> data) {
  DCHECK(m_peer);
  m_loaderProxy->postTaskToLoader(
      BLINK_FROM_HERE,
      createCrossThreadTask(&Peer::sendBlob, m_peer, std::move(data)));
}

void Bridge::close(int code, const String& reason) {
  DCHECK(m_peer);
  m_loaderProxy->postTaskToLoader(
      BLINK_FROM_HERE,
      createCrossThreadTask(&Peer::close, m_peer, code, reason));
}

void Bridge::fail(const String& reason,
                  MessageLevel level,
                  std::unique_ptr<SourceLocation> location) {
  DCHECK(m_peer);
  m_loaderProxy->postTaskToLoader(
      BLINK_FROM_HERE, createCrossThreadTask(&Peer::fail, m_peer, reason, level,
                                             passed(location->clone())));
}

void Bridge::disconnect() {
  if (!m_peer)
    return;

  m_loaderProxy->postTaskToLoader(
      BLINK_FROM_HERE, createCrossThreadTask(&Peer::disconnect, m_peer));

  m_client = nullptr;
  m_peer = nullptr;
  m_workerGlobalScope.clear();
}

DEFINE_TRACE(Bridge) {
  visitor->trace(m_client);
  visitor->trace(m_workerGlobalScope);
}

}  // namespace blink
