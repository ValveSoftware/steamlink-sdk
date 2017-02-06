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

#ifndef InProcessWorkerMessagingProxy_h
#define InProcessWorkerMessagingProxy_h

#include "core/CoreExport.h"
#include "core/dom/ExecutionContext.h"
#include "core/workers/InProcessWorkerGlobalScopeProxy.h"
#include "core/workers/WorkerLoaderProxy.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include <memory>

namespace blink {

class InProcessWorkerObjectProxy;
class WorkerThread;
class ExecutionContext;
class InProcessWorkerBase;
class WorkerClients;
class WorkerInspectorProxy;

// TODO(nhiroki): "MessagingProxy" is not well-defined term among worker
// components. Probably we should rename this to something more suitable.
// (http://crbug.com/603785)
class CORE_EXPORT InProcessWorkerMessagingProxy
    : public InProcessWorkerGlobalScopeProxy
    , private WorkerLoaderProxyProvider {
    WTF_MAKE_NONCOPYABLE(InProcessWorkerMessagingProxy);
public:
    // Implementations of InProcessWorkerGlobalScopeProxy.
    // (Only use these methods in the parent context thread.)
    void startWorkerGlobalScope(const KURL& scriptURL, const String& userAgent, const String& sourceCode) override;
    void terminateWorkerGlobalScope() override;
    void postMessageToWorkerGlobalScope(PassRefPtr<SerializedScriptValue>, std::unique_ptr<MessagePortChannelArray>) override;
    bool hasPendingActivity() const final;
    void workerObjectDestroyed() override;

    // These methods come from worker context thread via
    // InProcessWorkerObjectProxy and are called on the parent context thread.
    void postMessageToWorkerObject(PassRefPtr<SerializedScriptValue>, std::unique_ptr<MessagePortChannelArray>);
    void reportException(const String& errorMessage, std::unique_ptr<SourceLocation>);
    void reportConsoleMessage(MessageSource, MessageLevel, const String& message, std::unique_ptr<SourceLocation>);
    void postMessageToPageInspector(const String&);
    void postWorkerConsoleAgentEnabled();
    void confirmMessageFromWorkerObject(bool hasPendingActivity);
    void reportPendingActivity(bool hasPendingActivity);
    void workerThreadTerminated();
    void workerThreadCreated();

    ExecutionContext* getExecutionContext() const { return m_executionContext.get(); }

    // Number of live messaging proxies, used by leak detection.
    static int proxyCount();

protected:
    InProcessWorkerMessagingProxy(InProcessWorkerBase*, WorkerClients*);
    ~InProcessWorkerMessagingProxy() override;

    virtual std::unique_ptr<WorkerThread> createWorkerThread(double originTime) = 0;

    PassRefPtr<WorkerLoaderProxy> loaderProxy() { return m_loaderProxy; }
    InProcessWorkerObjectProxy& workerObjectProxy() { return *m_workerObjectProxy.get(); }

private:
    void workerObjectDestroyedInternal();
    void terminateInternally();

    // WorkerLoaderProxyProvider
    // These methods are called on different threads to schedule loading
    // requests and to send callbacks back to WorkerGlobalScope.
    void postTaskToLoader(std::unique_ptr<ExecutionContextTask>) override;
    bool postTaskToWorkerGlobalScope(std::unique_ptr<ExecutionContextTask>) override;

    // Returns true if this is called on the parent context thread.
    bool isParentContextThread() const;

    Persistent<ExecutionContext> m_executionContext;
    std::unique_ptr<InProcessWorkerObjectProxy> m_workerObjectProxy;
    WeakPersistent<InProcessWorkerBase> m_workerObject;
    bool m_mayBeDestroyed;
    std::unique_ptr<WorkerThread> m_workerThread;

    // Unconfirmed messages from the parent context thread to the worker thread.
    unsigned m_unconfirmedMessageCount;

    // The latest confirmation from worker thread reported that it was still
    // active.
    bool m_workerThreadHadPendingActivity;

    bool m_askedToTerminate;

    // Tasks are queued here until there's a thread object created.
    Vector<std::unique_ptr<ExecutionContextTask>> m_queuedEarlyTasks;

    Persistent<WorkerInspectorProxy> m_workerInspectorProxy;

    Persistent<WorkerClients> m_workerClients;

    RefPtr<WorkerLoaderProxy> m_loaderProxy;
};

} // namespace blink

#endif // InProcessWorkerMessagingProxy_h
