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
#include "core/inspector/PageRuntimeAgent.h"

#include "bindings/v8/DOMWrapperWorld.h"
#include "bindings/v8/ScriptController.h"
#include "bindings/v8/ScriptState.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/InjectedScript.h"
#include "core/inspector/InjectedScriptManager.h"
#include "core/inspector/InspectorPageAgent.h"
#include "core/inspector/InstrumentingAgents.h"
#include "core/page/Page.h"
#include "platform/weborigin/SecurityOrigin.h"

namespace WebCore {

PageRuntimeAgent::PageRuntimeAgent(InjectedScriptManager* injectedScriptManager, ScriptDebugServer* scriptDebugServer, Page* page, InspectorPageAgent* pageAgent)
    : InspectorRuntimeAgent(injectedScriptManager, scriptDebugServer)
    , m_inspectedPage(page)
    , m_pageAgent(pageAgent)
    , m_mainWorldContextCreated(false)
{
}

PageRuntimeAgent::~PageRuntimeAgent()
{
    m_instrumentingAgents->setPageRuntimeAgent(0);
}

void PageRuntimeAgent::init()
{
    m_instrumentingAgents->setPageRuntimeAgent(this);
}

void PageRuntimeAgent::enable(ErrorString* errorString)
{
    if (m_enabled)
        return;

    InspectorRuntimeAgent::enable(errorString);

    // Only report existing contexts if the page did commit load, otherwise we may
    // unintentionally initialize contexts in the frames which may trigger some listeners
    // that are expected to be triggered only after the load is committed, see http://crbug.com/131623
    if (m_mainWorldContextCreated)
        reportExecutionContextCreation();
}

void PageRuntimeAgent::didClearDocumentOfWindowObject(LocalFrame* frame)
{
    m_mainWorldContextCreated = true;

    if (!m_enabled)
        return;
    ASSERT(m_frontend);

    if (frame == m_inspectedPage->mainFrame()) {
        m_scriptStateToId.clear();
        m_frontend->executionContextsCleared();
    }
    String frameId = m_pageAgent->frameId(frame);
    addExecutionContextToFrontend(ScriptState::forMainWorld(frame), true, "", frameId);
}

void PageRuntimeAgent::didCreateIsolatedContext(LocalFrame* frame, ScriptState* scriptState, SecurityOrigin* origin)
{
    if (!m_enabled)
        return;
    ASSERT(m_frontend);
    String frameId = m_pageAgent->frameId(frame);
    addExecutionContextToFrontend(scriptState, false, origin->toRawString(), frameId);
}

InjectedScript PageRuntimeAgent::injectedScriptForEval(ErrorString* errorString, const int* executionContextId)
{
    if (!executionContextId) {
        ScriptState* scriptState = ScriptState::forMainWorld(m_inspectedPage->deprecatedLocalMainFrame());
        InjectedScript result = injectedScriptManager()->injectedScriptFor(scriptState);
        if (result.isEmpty())
            *errorString = "Internal error: main world execution context not found.";
        return result;
    }
    InjectedScript injectedScript = injectedScriptManager()->injectedScriptForId(*executionContextId);
    if (injectedScript.isEmpty())
        *errorString = "Execution context with given id not found.";
    return injectedScript;
}

void PageRuntimeAgent::muteConsole()
{
    FrameConsole::mute();
}

void PageRuntimeAgent::unmuteConsole()
{
    FrameConsole::unmute();
}

void PageRuntimeAgent::reportExecutionContextCreation()
{
    Vector<std::pair<ScriptState*, SecurityOrigin*> > isolatedContexts;
    for (Frame* frame = m_inspectedPage->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (!frame->isLocalFrame())
            continue;
        LocalFrame* localFrame = toLocalFrame(frame);
        if (!localFrame->script().canExecuteScripts(NotAboutToExecuteScript))
            continue;
        String frameId = m_pageAgent->frameId(localFrame);

        ScriptState* scriptState = ScriptState::forMainWorld(localFrame);
        addExecutionContextToFrontend(scriptState, true, "", frameId);
        localFrame->script().collectIsolatedContexts(isolatedContexts);
        if (isolatedContexts.isEmpty())
            continue;
        for (size_t i = 0; i< isolatedContexts.size(); i++)
            addExecutionContextToFrontend(isolatedContexts[i].first, false, isolatedContexts[i].second->toRawString(), frameId);
        isolatedContexts.clear();
    }
}

void PageRuntimeAgent::frameWindowDiscarded(LocalDOMWindow* window)
{
    Vector<RefPtr<ScriptState> > scriptStatesToRemove;
    for (ScriptStateToId::iterator it = m_scriptStateToId.begin(); it != m_scriptStateToId.end(); ++it) {
        RefPtr<ScriptState> scriptState = it->key;
        if (scriptState->contextIsEmpty() || window == scriptState->domWindow()) {
            scriptStatesToRemove.append(scriptState);
            m_frontend->executionContextDestroyed(it->value);
        }
    }
    m_scriptStateToId.removeAll(scriptStatesToRemove);
}

} // namespace WebCore

