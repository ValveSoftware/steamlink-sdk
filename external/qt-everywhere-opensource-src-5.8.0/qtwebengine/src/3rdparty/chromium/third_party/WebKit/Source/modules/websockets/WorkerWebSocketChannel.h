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

#ifndef WorkerWebSocketChannel_h
#define WorkerWebSocketChannel_h

#include "bindings/core/v8/SourceLocation.h"
#include "core/workers/WorkerThreadLifecycleObserver.h"
#include "modules/websockets/WebSocketChannel.h"
#include "modules/websockets/WebSocketChannelClient.h"
#include "platform/heap/Handle.h"
#include "wtf/Assertions.h"
#include "wtf/Forward.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"
#include <memory>
#include <stdint.h>

namespace blink {

class BlobDataHandle;
class KURL;
class ExecutionContext;
class ExecutionContextTask;
class WebSocketChannelSyncHelper;
class WorkerGlobalScope;
class WorkerLoaderProxy;

class WorkerWebSocketChannel final : public WebSocketChannel {
    WTF_MAKE_NONCOPYABLE(WorkerWebSocketChannel);
public:
    static WebSocketChannel* create(WorkerGlobalScope& workerGlobalScope, WebSocketChannelClient* client, std::unique_ptr<SourceLocation> location)
    {
        return new WorkerWebSocketChannel(workerGlobalScope, client, std::move(location));
    }
    ~WorkerWebSocketChannel() override;

    // WebSocketChannel functions.
    bool connect(const KURL&, const String& protocol) override;
    void send(const CString&) override;
    void send(const DOMArrayBuffer&, unsigned byteOffset, unsigned byteLength) override;
    void send(PassRefPtr<BlobDataHandle>) override;
    void sendTextAsCharVector(std::unique_ptr<Vector<char>>) override
    {
        ASSERT_NOT_REACHED();
    }
    void sendBinaryAsCharVector(std::unique_ptr<Vector<char>>) override
    {
        ASSERT_NOT_REACHED();
    }
    void close(int code, const String& reason) override;
    void fail(const String& reason, MessageLevel, std::unique_ptr<SourceLocation>) override;
    void disconnect() override; // Will suppress didClose().

    DECLARE_VIRTUAL_TRACE();

    class Bridge;
    // Allocated and used in the main thread.
    class Peer final : public GarbageCollectedFinalized<Peer>, public WebSocketChannelClient, public WorkerThreadLifecycleObserver {
        USING_GARBAGE_COLLECTED_MIXIN(Peer);
        WTF_MAKE_NONCOPYABLE(Peer);
    public:
        Peer(Bridge*, PassRefPtr<WorkerLoaderProxy>, WebSocketChannelSyncHelper*, WorkerThreadLifecycleContext*);
        ~Peer() override;

        // SourceLocation parameter may be shown when the connection fails.
        bool initialize(std::unique_ptr<SourceLocation>, ExecutionContext*);

        void connect(const KURL&, const String& protocol);
        void sendTextAsCharVector(std::unique_ptr<Vector<char>>);
        void sendBinaryAsCharVector(std::unique_ptr<Vector<char>>);
        void sendBlob(PassRefPtr<BlobDataHandle>);
        void close(int code, const String& reason);
        void fail(const String& reason, MessageLevel, std::unique_ptr<SourceLocation>);
        void disconnect();

        DECLARE_VIRTUAL_TRACE();
        // Promptly clear connection to bridge + loader proxy.
        EAGERLY_FINALIZE();

        // WebSocketChannelClient functions.
        void didConnect(const String& subprotocol, const String& extensions) override;
        void didReceiveTextMessage(const String& payload) override;
        void didReceiveBinaryMessage(std::unique_ptr<Vector<char>>) override;
        void didConsumeBufferedAmount(uint64_t) override;
        void didStartClosingHandshake() override;
        void didClose(ClosingHandshakeCompletionStatus, unsigned short code, const String& reason) override;
        void didError() override;

        // WorkerThreadLifecycleObserver function.
        void contextDestroyed() override;

    private:
        CrossThreadWeakPersistent<Bridge> m_bridge;
        RefPtr<WorkerLoaderProxy> m_loaderProxy;
        Member<WebSocketChannel> m_mainWebSocketChannel;
        Member<WebSocketChannelSyncHelper> m_syncHelper;
    };

    // Bridge for Peer. Running on the worker thread.
    class Bridge final : public GarbageCollectedFinalized<Bridge> {
        WTF_MAKE_NONCOPYABLE(Bridge);
    public:
        Bridge(WebSocketChannelClient*, WorkerGlobalScope&);
        ~Bridge();
        // SourceLocation parameter may be shown when the connection fails.
        void initialize(std::unique_ptr<SourceLocation>);
        bool connect(const KURL&, const String& protocol);
        void send(const CString& message);
        void send(const DOMArrayBuffer&, unsigned byteOffset, unsigned byteLength);
        void send(PassRefPtr<BlobDataHandle>);
        void close(int code, const String& reason);
        void fail(const String& reason, MessageLevel, std::unique_ptr<SourceLocation>);
        void disconnect();

        void createPeerOnMainThread(std::unique_ptr<SourceLocation>, WorkerThreadLifecycleContext*, ExecutionContext*);

        // Returns null when |disconnect| has already been called.
        WebSocketChannelClient* client() { return m_client; }

        DECLARE_TRACE();
        // Promptly clear connection to peer + loader proxy.
        EAGERLY_FINALIZE();

    private:
        // Returns false if shutdown event is received before method completion.
        bool waitForMethodCompletion(std::unique_ptr<ExecutionContextTask>);

        Member<WebSocketChannelClient> m_client;
        Member<WorkerGlobalScope> m_workerGlobalScope;
        RefPtr<WorkerLoaderProxy> m_loaderProxy;
        Member<WebSocketChannelSyncHelper> m_syncHelper;
        CrossThreadPersistent<Peer> m_peer;
    };

private:
    WorkerWebSocketChannel(WorkerGlobalScope&, WebSocketChannelClient*, std::unique_ptr<SourceLocation>);

    Member<Bridge> m_bridge;
    std::unique_ptr<SourceLocation> m_locationAtConnection;
};

} // namespace blink

#endif // WorkerWebSocketChannel_h
