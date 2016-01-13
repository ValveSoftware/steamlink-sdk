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

#include "config.h"
#include "core/inspector/InspectorInstrumentation.h"

#include "core/events/EventTarget.h"
#include "core/fetch/FetchInitiatorInfo.h"
#include "core/inspector/InspectorCSSAgent.h"
#include "core/inspector/InspectorConsoleAgent.h"
#include "core/inspector/InspectorController.h"
#include "core/inspector/InspectorDebuggerAgent.h"
#include "core/inspector/InspectorInspectorAgent.h"
#include "core/inspector/InspectorProfilerAgent.h"
#include "core/inspector/InspectorResourceAgent.h"
#include "core/inspector/InspectorTimelineAgent.h"
#include "core/inspector/InstrumentingAgents.h"
#include "core/inspector/WorkerInspectorController.h"
#include "core/workers/WorkerGlobalScope.h"

namespace WebCore {

namespace {
static HashSet<InstrumentingAgents*>* instrumentingAgentsSet = 0;
}

namespace InspectorInstrumentation {
int FrontendCounter::s_frontendCounter = 0;
}

InspectorInstrumentationCookie::InspectorInstrumentationCookie()
    : m_instrumentingAgents(nullptr)
    , m_timelineAgentId(0)
{
}

InspectorInstrumentationCookie::InspectorInstrumentationCookie(InstrumentingAgents* agents, int timelineAgentId)
    : m_instrumentingAgents(agents)
    , m_timelineAgentId(timelineAgentId)
{
}

InspectorInstrumentationCookie::InspectorInstrumentationCookie(const InspectorInstrumentationCookie& other)
    : m_instrumentingAgents(other.m_instrumentingAgents)
    , m_timelineAgentId(other.m_timelineAgentId)
{
}

InspectorInstrumentationCookie& InspectorInstrumentationCookie::operator=(const InspectorInstrumentationCookie& other)
{
    if (this != &other) {
        m_instrumentingAgents = other.m_instrumentingAgents;
        m_timelineAgentId = other.m_timelineAgentId;
    }
    return *this;
}

InspectorInstrumentationCookie::~InspectorInstrumentationCookie()
{
}

namespace InspectorInstrumentation {

bool isDebuggerPausedImpl(InstrumentingAgents* instrumentingAgents)
{
    if (InspectorDebuggerAgent* debuggerAgent = instrumentingAgents->inspectorDebuggerAgent())
        return debuggerAgent->isPaused();
    return false;
}

void didReceiveResourceResponseButCanceledImpl(LocalFrame* frame, DocumentLoader* loader, unsigned long identifier, const ResourceResponse& r)
{
    didReceiveResourceResponse(frame, identifier, loader, r, 0);
}

void continueAfterXFrameOptionsDeniedImpl(LocalFrame* frame, DocumentLoader* loader, unsigned long identifier, const ResourceResponse& r)
{
    didReceiveResourceResponseButCanceledImpl(frame, loader, identifier, r);
}

void continueWithPolicyDownloadImpl(LocalFrame* frame, DocumentLoader* loader, unsigned long identifier, const ResourceResponse& r)
{
    didReceiveResourceResponseButCanceledImpl(frame, loader, identifier, r);
}

void continueWithPolicyIgnoreImpl(LocalFrame* frame, DocumentLoader* loader, unsigned long identifier, const ResourceResponse& r)
{
    didReceiveResourceResponseButCanceledImpl(frame, loader, identifier, r);
}

void willDestroyResourceImpl(Resource* cachedResource)
{
    if (!instrumentingAgentsSet)
        return;
    HashSet<InstrumentingAgents*>::iterator end = instrumentingAgentsSet->end();
    for (HashSet<InstrumentingAgents*>::iterator it = instrumentingAgentsSet->begin(); it != end; ++it) {
        InstrumentingAgents* instrumentingAgents = *it;
        if (InspectorResourceAgent* inspectorResourceAgent = instrumentingAgents->inspectorResourceAgent())
            inspectorResourceAgent->willDestroyResource(cachedResource);
    }
}

bool collectingHTMLParseErrorsImpl(InstrumentingAgents* instrumentingAgents)
{
    if (InspectorInspectorAgent* inspectorAgent = instrumentingAgents->inspectorInspectorAgent())
        return inspectorAgent->hasFrontend();
    return false;
}

PassOwnPtr<ScriptSourceCode> preprocessImpl(InstrumentingAgents* instrumentingAgents, LocalFrame* frame, const ScriptSourceCode& sourceCode)
{
    if (InspectorDebuggerAgent* debuggerAgent = instrumentingAgents->inspectorDebuggerAgent())
        return debuggerAgent->preprocess(frame, sourceCode);
    return PassOwnPtr<ScriptSourceCode>();
}

String preprocessEventListenerImpl(InstrumentingAgents* instrumentingAgents, LocalFrame* frame, const String& source, const String& url, const String& functionName)
{
    if (InspectorDebuggerAgent* debuggerAgent = instrumentingAgents->inspectorDebuggerAgent())
        return debuggerAgent->preprocessEventListener(frame, source, url, functionName);
    return source;
}

bool canvasAgentEnabled(ExecutionContext* executionContext)
{
    InstrumentingAgents* instrumentingAgents = instrumentingAgentsFor(executionContext);
    return instrumentingAgents && instrumentingAgents->inspectorCanvasAgent();
}

bool consoleAgentEnabled(ExecutionContext* executionContext)
{
    InstrumentingAgents* instrumentingAgents = instrumentingAgentsFor(executionContext);
    InspectorConsoleAgent* consoleAgent = instrumentingAgents ? instrumentingAgents->inspectorConsoleAgent() : 0;
    return consoleAgent && consoleAgent->enabled();
}

bool timelineAgentEnabled(ExecutionContext* executionContext)
{
    InstrumentingAgents* instrumentingAgents = instrumentingAgentsFor(executionContext);
    return instrumentingAgents && instrumentingAgents->inspectorTimelineAgent();
}

void registerInstrumentingAgents(InstrumentingAgents* instrumentingAgents)
{
    if (!instrumentingAgentsSet)
        instrumentingAgentsSet = new HashSet<InstrumentingAgents*>();
    instrumentingAgentsSet->add(instrumentingAgents);
}

void unregisterInstrumentingAgents(InstrumentingAgents* instrumentingAgents)
{
    if (!instrumentingAgentsSet)
        return;
    instrumentingAgentsSet->remove(instrumentingAgents);
    if (instrumentingAgentsSet->isEmpty()) {
        delete instrumentingAgentsSet;
        instrumentingAgentsSet = 0;
    }
}

InspectorTimelineAgent* retrieveTimelineAgent(const InspectorInstrumentationCookie& cookie)
{
    if (!cookie.instrumentingAgents())
        return 0;
    InspectorTimelineAgent* timelineAgent = cookie.instrumentingAgents()->inspectorTimelineAgent();
    if (timelineAgent && cookie.hasMatchingTimelineAgentId(timelineAgent->id()))
        return timelineAgent;
    return 0;
}

InstrumentingAgents* instrumentingAgentsFor(Page* page)
{
    if (!page)
        return 0;
    return instrumentationForPage(page);
}

InstrumentingAgents* instrumentingAgentsFor(EventTarget* eventTarget)
{
    if (!eventTarget)
        return 0;
    return instrumentingAgentsFor(eventTarget->executionContext());
}

InstrumentingAgents* instrumentingAgentsFor(RenderObject* renderer)
{
    return instrumentingAgentsFor(renderer->frame());
}

InstrumentingAgents* instrumentingAgentsFor(WorkerGlobalScope* workerGlobalScope)
{
    if (!workerGlobalScope)
        return 0;
    return instrumentationForWorkerGlobalScope(workerGlobalScope);
}

InstrumentingAgents* instrumentingAgentsForNonDocumentContext(ExecutionContext* context)
{
    if (context->isWorkerGlobalScope())
        return instrumentationForWorkerGlobalScope(toWorkerGlobalScope(context));
    return 0;
}

} // namespace InspectorInstrumentation

namespace InstrumentationEvents {
const char PaintSetup[] = "PaintSetup";
const char RasterTask[] = "RasterTask";
const char Paint[] = "Paint";
const char Layer[] = "Layer";
const char RequestMainThreadFrame[] = "RequestMainThreadFrame";
const char BeginFrame[] = "BeginFrame";
const char ActivateLayerTree[] = "ActivateLayerTree";
const char DrawFrame[] = "DrawFrame";
const char EmbedderCallback[] = "EmbedderCallback";
};

namespace InstrumentationEventArguments {
const char FrameId[] = "frameId";
const char LayerId[] = "layerId";
const char LayerTreeId[] = "layerTreeId";
const char PageId[] = "pageId";
const char CallbackName[] = "callbackName";
};

InstrumentingAgents* instrumentationForPage(Page* page)
{
    ASSERT(isMainThread());
    return page->inspectorController().m_instrumentingAgents.get();
}

InstrumentingAgents* instrumentationForWorkerGlobalScope(WorkerGlobalScope* workerGlobalScope)
{
    if (WorkerInspectorController* controller = workerGlobalScope->workerInspectorController())
        return controller->m_instrumentingAgents.get();
    return 0;
}

} // namespace WebCore

