/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef InspectorWorkerAgent_h
#define InspectorWorkerAgent_h

#include "core/CoreExport.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/protocol/Worker.h"
#include "core/workers/WorkerInspectorProxy.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"

namespace blink {
class InspectedFrames;
class KURL;
class WorkerInspectorProxy;

class CORE_EXPORT InspectorWorkerAgent final
    : public InspectorBaseAgent<protocol::Worker::Metainfo>
    , public WorkerInspectorProxy::PageInspector {
    WTF_MAKE_NONCOPYABLE(InspectorWorkerAgent);
public:
    explicit InspectorWorkerAgent(InspectedFrames*);
    ~InspectorWorkerAgent() override;
    DECLARE_VIRTUAL_TRACE();

    void disable(ErrorString*) override;
    void restore() override;
    void didCommitLoadForLocalFrame(LocalFrame*) override;

    // Called from InspectorInstrumentation
    bool shouldWaitForDebuggerOnWorkerStart();
    void didStartWorker(WorkerInspectorProxy*, bool waitingForDebugger);
    void workerTerminated(WorkerInspectorProxy*);

    // Called from Dispatcher
    void enable(ErrorString*) override;
    void sendMessageToWorker(ErrorString*, const String& workerId, const String& message) override;
    void setWaitForDebuggerOnStart(ErrorString*, bool value) override;

    void setTracingSessionId(const String&);

private:
    bool enabled();
    void connectToAllProxies();
    void connectToProxy(WorkerInspectorProxy*, bool waitingForDebugger);

    // WorkerInspectorProxy::PageInspector implementation.
    void dispatchMessageFromWorker(WorkerInspectorProxy*, const String& message) override;

    Member<InspectedFrames> m_inspectedFrames;
    HeapHashMap<String, Member<WorkerInspectorProxy>> m_connectedProxies;
    String m_tracingSessionId;
};

} // namespace blink

#endif // !defined(InspectorWorkerAgent_h)
