/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef InProcessWorkerObjectProxy_h
#define InProcessWorkerObjectProxy_h

#include "core/CoreExport.h"
#include "core/dom/MessagePort.h"
#include "core/workers/WorkerReportingProxy.h"
#include "platform/heap/Handle.h"
#include "wtf/PassRefPtr.h"
#include <memory>

namespace blink {

class ConsoleMessage;
class ExecutionContext;
class ExecutionContextTask;
class InProcessWorkerMessagingProxy;

// A proxy to talk to the worker object. This object is created on the
// parent context thread (i.e. usually the main thread), passed on to
// the worker thread, and used just to proxy messages to the
// InProcessWorkerMessagingProxy on the parent context thread.
//
// Used only by in-process workers (DedicatedWorker and CompositorWorker.)
class CORE_EXPORT InProcessWorkerObjectProxy : public WorkerReportingProxy {
    USING_FAST_MALLOC(InProcessWorkerObjectProxy);
    WTF_MAKE_NONCOPYABLE(InProcessWorkerObjectProxy);
public:
    static std::unique_ptr<InProcessWorkerObjectProxy> create(InProcessWorkerMessagingProxy*);
    ~InProcessWorkerObjectProxy() override { }

    void postMessageToWorkerObject(PassRefPtr<SerializedScriptValue>, std::unique_ptr<MessagePortChannelArray>);
    void postTaskToMainExecutionContext(std::unique_ptr<ExecutionContextTask>);
    void confirmMessageFromWorkerObject(bool hasPendingActivity);
    void reportPendingActivity(bool hasPendingActivity);

    // WorkerReportingProxy overrides.
    void reportException(const String& errorMessage, std::unique_ptr<SourceLocation>) override;
    void reportConsoleMessage(ConsoleMessage*) override;
    void postMessageToPageInspector(const String&) override;
    void postWorkerConsoleAgentEnabled() override;
    void didEvaluateWorkerScript(bool success) override { }
    void workerGlobalScopeStarted(WorkerGlobalScope*) override { }
    void workerGlobalScopeClosed() override;
    void workerThreadTerminated() override;
    void willDestroyWorkerGlobalScope() override { }

protected:
    InProcessWorkerObjectProxy(InProcessWorkerMessagingProxy*);
    virtual ExecutionContext* getExecutionContext();

private:
    // This object always outlives this proxy.
    InProcessWorkerMessagingProxy* m_messagingProxy;
};

} // namespace blink

#endif // InProcessWorkerObjectProxy_h
