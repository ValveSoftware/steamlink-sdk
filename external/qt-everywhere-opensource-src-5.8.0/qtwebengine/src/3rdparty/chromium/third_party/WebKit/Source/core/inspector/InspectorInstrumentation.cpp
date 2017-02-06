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

#include "core/inspector/InspectorInstrumentation.h"

#include "core/InstrumentingAgents.h"
#include "core/events/Event.h"
#include "core/events/EventTarget.h"
#include "core/fetch/FetchInitiatorInfo.h"
#include "core/frame/FrameHost.h"
#include "core/inspector/InspectorCSSAgent.h"
#include "core/inspector/InspectorDOMDebuggerAgent.h"
#include "core/inspector/InspectorNetworkAgent.h"
#include "core/inspector/InspectorPageAgent.h"
#include "core/inspector/InspectorSession.h"
#include "core/inspector/MainThreadDebugger.h"
#include "core/inspector/ThreadDebugger.h"
#include "core/inspector/WorkerInspectorController.h"
#include "core/page/Page.h"
#include "core/workers/MainThreadWorkletGlobalScope.h"
#include "core/workers/WorkerGlobalScope.h"
#include "platform/v8_inspector/public/V8Debugger.h"

namespace blink {

namespace InspectorInstrumentation {

AsyncTask::AsyncTask(ExecutionContext* context, void* task) : AsyncTask(context, task, true)
{
}

AsyncTask::AsyncTask(ExecutionContext* context, void* task, bool enabled)
    : m_debugger(enabled ? ThreadDebugger::from(toIsolate(context)) : nullptr)
    , m_task(task)
{
    if (m_debugger)
        m_debugger->asyncTaskStarted(m_task);
}

AsyncTask::~AsyncTask()
{
    if (m_debugger)
        m_debugger->asyncTaskFinished(m_task);
}

void asyncTaskScheduled(ExecutionContext* context, const String& name, void* task)
{
    if (ThreadDebugger* debugger = ThreadDebugger::from(toIsolate(context)))
        debugger->asyncTaskScheduled(name, task, false);
}

void asyncTaskScheduled(ExecutionContext* context, const String& name, void* task, bool recurring)
{
    if (ThreadDebugger* debugger = ThreadDebugger::from(toIsolate(context)))
        debugger->asyncTaskScheduled(name, task, recurring);
}

void asyncTaskCanceled(ExecutionContext* context, void* task)
{
    if (ThreadDebugger* debugger = ThreadDebugger::from(toIsolate(context)))
        debugger->asyncTaskCanceled(task);
}

void allAsyncTasksCanceled(ExecutionContext* context)
{
    if (ThreadDebugger* debugger = ThreadDebugger::from(toIsolate(context)))
        debugger->allAsyncTasksCanceled();
}

NativeBreakpoint::NativeBreakpoint(ExecutionContext* context, const char* name, bool sync)
    : m_instrumentingAgents(instrumentingAgentsFor(context))
    , m_sync(sync)
{
    if (!m_instrumentingAgents || !m_instrumentingAgents->hasInspectorDOMDebuggerAgents())
        return;
    for (InspectorDOMDebuggerAgent* domDebuggerAgent : m_instrumentingAgents->inspectorDOMDebuggerAgents())
        domDebuggerAgent->allowNativeBreakpoint(name, nullptr, m_sync);
}

NativeBreakpoint::NativeBreakpoint(ExecutionContext* context, EventTarget* eventTarget, Event* event)
    : m_instrumentingAgents(instrumentingAgentsFor(context))
    , m_sync(false)
{
    if (!m_instrumentingAgents || !m_instrumentingAgents->hasInspectorDOMDebuggerAgents())
        return;
    Node* node = eventTarget->toNode();
    String targetName = node ? node->nodeName() : eventTarget->interfaceName();
    for (InspectorDOMDebuggerAgent* domDebuggerAgent : m_instrumentingAgents->inspectorDOMDebuggerAgents())
        domDebuggerAgent->allowNativeBreakpoint(event->type(), &targetName, m_sync);
}

NativeBreakpoint::~NativeBreakpoint()
{
    if (m_sync || !m_instrumentingAgents || !m_instrumentingAgents->hasInspectorDOMDebuggerAgents())
        return;
    for (InspectorDOMDebuggerAgent* domDebuggerAgent : m_instrumentingAgents->inspectorDOMDebuggerAgents())
        domDebuggerAgent->cancelNativeBreakpoint();
}

StyleRecalc::StyleRecalc(Document* document)
    : m_instrumentingAgents(instrumentingAgentsFor(document))
{
    if (!m_instrumentingAgents || m_instrumentingAgents->hasInspectorNetworkAgents())
        return;
    for (InspectorNetworkAgent* networkAgent : m_instrumentingAgents->inspectorNetworkAgents())
        networkAgent->willRecalculateStyle(document);
}

StyleRecalc::~StyleRecalc()
{
    if (!m_instrumentingAgents)
        return;
    if (m_instrumentingAgents->hasInspectorNetworkAgents()) {
        for (InspectorNetworkAgent* networkAgent : m_instrumentingAgents->inspectorNetworkAgents())
            networkAgent->didRecalculateStyle();
    }
    if (m_instrumentingAgents->hasInspectorPageAgents()) {
        for (InspectorPageAgent* pageAgent : m_instrumentingAgents->inspectorPageAgents())
            pageAgent->didRecalculateStyle();
    }
}

JavaScriptDialog::JavaScriptDialog(LocalFrame* frame, const String& message, ChromeClient::DialogType dialogType)
    : m_instrumentingAgents(instrumentingAgentsFor(frame))
    , m_result(false)
{
    if (!m_instrumentingAgents || !m_instrumentingAgents->hasInspectorPageAgents())
        return;
    for (InspectorPageAgent* pageAgent : m_instrumentingAgents->inspectorPageAgents())
        pageAgent->willRunJavaScriptDialog(message, dialogType);
}

void JavaScriptDialog::setResult(bool result)
{
    m_result = result;
}

JavaScriptDialog::~JavaScriptDialog()
{
    if (!m_instrumentingAgents || !m_instrumentingAgents->hasInspectorPageAgents())
        return;
    for (InspectorPageAgent* pageAgent : m_instrumentingAgents->inspectorPageAgents())
        pageAgent->didRunJavaScriptDialog(m_result);
}

int FrontendCounter::s_frontendCounter = 0;

bool isDebuggerPaused(LocalFrame*)
{
    return MainThreadDebugger::instance()->debugger()->isPaused();
}

void didReceiveResourceResponseButCanceled(LocalFrame* frame, DocumentLoader* loader, unsigned long identifier, const ResourceResponse& r, Resource* resource)
{
    didReceiveResourceResponse(frame, identifier, loader, r, resource);
}

void continueAfterXFrameOptionsDenied(LocalFrame* frame, DocumentLoader* loader, unsigned long identifier, const ResourceResponse& r, Resource* resource)
{
    didReceiveResourceResponseButCanceled(frame, loader, identifier, r, resource);
}

void continueWithPolicyIgnore(LocalFrame* frame, DocumentLoader* loader, unsigned long identifier, const ResourceResponse& r, Resource* resource)
{
    didReceiveResourceResponseButCanceled(frame, loader, identifier, r, resource);
}

InstrumentingAgents* instrumentingAgentsFor(WorkerGlobalScope* workerGlobalScope)
{
    if (!workerGlobalScope)
        return nullptr;
    if (WorkerInspectorController* controller = workerGlobalScope->workerInspectorController())
        return controller->instrumentingAgents();
    return nullptr;
}

InstrumentingAgents* instrumentingAgentsForNonDocumentContext(ExecutionContext* context)
{
    if (context->isWorkerGlobalScope())
        return instrumentingAgentsFor(toWorkerGlobalScope(context));
    if (context->isMainThreadWorkletGlobalScope())
        return instrumentingAgentsFor(toMainThreadWorkletGlobalScope(context)->frame());
    return nullptr;
}

} // namespace InspectorInstrumentation

} // namespace blink
