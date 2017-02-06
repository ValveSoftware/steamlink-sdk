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

#include "core/inspector/InspectorWorkerAgent.h"

#include "core/dom/Document.h"
#include "core/inspector/InspectedFrames.h"
#include "platform/weborigin/KURL.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

namespace WorkerAgentState {
static const char workerInspectionEnabled[] = "workerInspectionEnabled";
static const char waitForDebuggerOnStart[] = "waitForDebuggerOnStart";
};

InspectorWorkerAgent::InspectorWorkerAgent(InspectedFrames* inspectedFrames)
    : m_inspectedFrames(inspectedFrames)
{
}

InspectorWorkerAgent::~InspectorWorkerAgent()
{
}

void InspectorWorkerAgent::restore()
{
    if (!enabled())
        return;
    m_instrumentingAgents->addInspectorWorkerAgent(this);
    connectToAllProxies();
}

void InspectorWorkerAgent::enable(ErrorString*)
{
    if (enabled())
        return;
    m_state->setBoolean(WorkerAgentState::workerInspectionEnabled, true);
    m_instrumentingAgents->addInspectorWorkerAgent(this);
    connectToAllProxies();
}

void InspectorWorkerAgent::disable(ErrorString*)
{
    if (!enabled())
        return;
    m_state->setBoolean(WorkerAgentState::workerInspectionEnabled, false);
    m_state->setBoolean(WorkerAgentState::waitForDebuggerOnStart, false);
    m_instrumentingAgents->removeInspectorWorkerAgent(this);
    for (auto& idProxy : m_connectedProxies)
        idProxy.value->disconnectFromInspector(this);
    m_connectedProxies.clear();
}

bool InspectorWorkerAgent::enabled()
{
    return m_state->booleanProperty(WorkerAgentState::workerInspectionEnabled, false);
}

void InspectorWorkerAgent::sendMessageToWorker(ErrorString* error, const String& workerId, const String& message)
{
    if (!enabled()) {
        *error = "Worker inspection is not enabled";
        return;
    }

    WorkerInspectorProxy* proxy = m_connectedProxies.get(workerId);
    if (proxy)
        proxy->sendMessageToInspector(message);
    else
        *error = "Worker is gone";
}

void InspectorWorkerAgent::setWaitForDebuggerOnStart(ErrorString*, bool value)
{
    m_state->setBoolean(WorkerAgentState::waitForDebuggerOnStart, value);
}

void InspectorWorkerAgent::setTracingSessionId(const String& sessionId)
{
    m_tracingSessionId = sessionId;
    if (sessionId.isEmpty())
        return;
    for (auto& idProxy : m_connectedProxies)
        idProxy.value->writeTimelineStartedEvent(sessionId);
}

bool InspectorWorkerAgent::shouldWaitForDebuggerOnWorkerStart()
{
    return enabled() && m_state->booleanProperty(WorkerAgentState::waitForDebuggerOnStart, false);
}

void InspectorWorkerAgent::didStartWorker(WorkerInspectorProxy* proxy, bool waitingForDebugger)
{
    ASSERT(frontend() && enabled());
    connectToProxy(proxy, waitingForDebugger);
    if (!m_tracingSessionId.isEmpty())
        proxy->writeTimelineStartedEvent(m_tracingSessionId);
}

void InspectorWorkerAgent::workerTerminated(WorkerInspectorProxy* proxy)
{
    ASSERT(frontend() && enabled());
    if (m_connectedProxies.find(proxy->inspectorId()) == m_connectedProxies.end())
        return;
    frontend()->workerTerminated(proxy->inspectorId());
    proxy->disconnectFromInspector(this);
    m_connectedProxies.remove(proxy->inspectorId());
}

void InspectorWorkerAgent::connectToAllProxies()
{
    for (WorkerInspectorProxy* proxy : WorkerInspectorProxy::allProxies()) {
        if (proxy->getDocument()->frame() && m_inspectedFrames->contains(proxy->getDocument()->frame()))
            connectToProxy(proxy, false);
    }
}

void InspectorWorkerAgent::didCommitLoadForLocalFrame(LocalFrame* frame)
{
    if (!enabled() || frame != m_inspectedFrames->root())
        return;

    // During navigation workers from old page may die after a while.
    // Usually, it's fine to report them terminated later, but some tests
    // expect strict set of workers, and we reuse renderer between tests.
    for (auto& idProxy : m_connectedProxies) {
        frontend()->workerTerminated(idProxy.key);
        idProxy.value->disconnectFromInspector(this);
    }
    m_connectedProxies.clear();
}

void InspectorWorkerAgent::connectToProxy(WorkerInspectorProxy* proxy, bool waitingForDebugger)
{
    m_connectedProxies.set(proxy->inspectorId(), proxy);
    proxy->connectToInspector(this);
    ASSERT(frontend());
    frontend()->workerCreated(proxy->inspectorId(), proxy->url(), waitingForDebugger);
}

void InspectorWorkerAgent::dispatchMessageFromWorker(WorkerInspectorProxy* proxy, const String& message)
{
    frontend()->dispatchMessageFromWorker(proxy->inspectorId(), message);
}

DEFINE_TRACE(InspectorWorkerAgent)
{
    visitor->trace(m_connectedProxies);
    visitor->trace(m_inspectedFrames);
    InspectorBaseAgent::trace(visitor);
}

} // namespace blink
