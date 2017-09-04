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

#include "core/inspector/WorkerInspectorController.h"

#include "core/InstrumentingAgents.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorLogAgent.h"
#include "core/inspector/WorkerThreadDebugger.h"
#include "core/inspector/protocol/Protocol.h"
#include "core/workers/WorkerBackingThread.h"
#include "core/workers/WorkerReportingProxy.h"
#include "core/workers/WorkerThread.h"
#include "platform/WebThreadSupportingGC.h"

namespace blink {

WorkerInspectorController* WorkerInspectorController::create(
    WorkerThread* thread) {
  WorkerThreadDebugger* debugger =
      WorkerThreadDebugger::from(thread->isolate());
  return debugger ? new WorkerInspectorController(thread, debugger) : nullptr;
}

WorkerInspectorController::WorkerInspectorController(
    WorkerThread* thread,
    WorkerThreadDebugger* debugger)
    : m_debugger(debugger),
      m_thread(thread),
      m_instrumentingAgents(new InstrumentingAgents()) {}

WorkerInspectorController::~WorkerInspectorController() {
  DCHECK(!m_thread);
}

void WorkerInspectorController::connectFrontend() {
  if (m_session)
    return;

  // sessionId will be overwritten by WebDevToolsAgent::sendProtocolNotification
  // call.
  m_session = new InspectorSession(
      this, m_instrumentingAgents.get(), 0, m_debugger->v8Inspector(),
      m_debugger->contextGroupId(m_thread), nullptr);
  m_session->append(
      new InspectorLogAgent(m_thread->consoleMessageStorage(), nullptr));
  m_thread->workerBackingThread().backingThread().addTaskObserver(this);
}

void WorkerInspectorController::disconnectFrontend() {
  if (!m_session)
    return;
  m_session->dispose();
  m_session.clear();
  m_thread->workerBackingThread().backingThread().removeTaskObserver(this);
}

void WorkerInspectorController::dispatchMessageFromFrontend(
    const String& message) {
  if (!m_session)
    return;
  String method;
  if (!protocol::DispatcherBase::getCommandName(message, &method))
    return;
  m_session->dispatchProtocolMessage(method, message);
}

void WorkerInspectorController::dispose() {
  disconnectFrontend();
  m_thread = nullptr;
}

void WorkerInspectorController::flushProtocolNotifications() {
  if (m_session)
    m_session->flushProtocolNotifications();
}

void WorkerInspectorController::sendProtocolMessage(int sessionId,
                                                    int callId,
                                                    const String& response,
                                                    const String& state) {
  // Worker messages are wrapped, no need to handle callId or state.
  m_thread->workerReportingProxy().postMessageToPageInspector(response);
}

void WorkerInspectorController::willProcessTask() {}

void WorkerInspectorController::didProcessTask() {
  if (m_session)
    m_session->flushProtocolNotifications();
}

DEFINE_TRACE(WorkerInspectorController) {
  visitor->trace(m_instrumentingAgents);
  visitor->trace(m_session);
}

}  // namespace blink
