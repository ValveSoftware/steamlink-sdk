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
#include "core/inspector/PageDebuggerAgent.h"

#include "bindings/v8/DOMWrapperWorld.h"
#include "bindings/v8/ScriptController.h"
#include "bindings/v8/ScriptSourceCode.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/InspectorOverlay.h"
#include "core/inspector/InspectorPageAgent.h"
#include "core/inspector/InstrumentingAgents.h"
#include "core/loader/DocumentLoader.h"
#include "core/page/Page.h"

namespace WebCore {

PassOwnPtr<PageDebuggerAgent> PageDebuggerAgent::create(PageScriptDebugServer* pageScriptDebugServer, InspectorPageAgent* pageAgent, InjectedScriptManager* injectedScriptManager, InspectorOverlay* overlay)
{
    return adoptPtr(new PageDebuggerAgent(pageScriptDebugServer, pageAgent, injectedScriptManager, overlay));
}

PageDebuggerAgent::PageDebuggerAgent(PageScriptDebugServer* pageScriptDebugServer, InspectorPageAgent* pageAgent, InjectedScriptManager* injectedScriptManager, InspectorOverlay* overlay)
    : InspectorDebuggerAgent(injectedScriptManager)
    , m_pageScriptDebugServer(pageScriptDebugServer)
    , m_pageAgent(pageAgent)
    , m_overlay(overlay)
{
    m_overlay->overlayHost()->setListener(this);
}

PageDebuggerAgent::~PageDebuggerAgent()
{
}

void PageDebuggerAgent::enable()
{
    InspectorDebuggerAgent::enable();
    m_instrumentingAgents->setPageDebuggerAgent(this);
}

void PageDebuggerAgent::disable()
{
    InspectorDebuggerAgent::disable();
    m_instrumentingAgents->setPageDebuggerAgent(0);
}

void PageDebuggerAgent::startListeningScriptDebugServer()
{
    scriptDebugServer().addListener(this, m_pageAgent->page());
}

void PageDebuggerAgent::stopListeningScriptDebugServer()
{
    scriptDebugServer().removeListener(this, m_pageAgent->page());
}

PageScriptDebugServer& PageDebuggerAgent::scriptDebugServer()
{
    return *m_pageScriptDebugServer;
}

void PageDebuggerAgent::muteConsole()
{
    FrameConsole::mute();
}

void PageDebuggerAgent::unmuteConsole()
{
    FrameConsole::unmute();
}

void PageDebuggerAgent::overlayResumed()
{
    ErrorString error;
    resume(&error);
}

void PageDebuggerAgent::overlaySteppedOver()
{
    ErrorString error;
    stepOver(&error);
}

InjectedScript PageDebuggerAgent::injectedScriptForEval(ErrorString* errorString, const int* executionContextId)
{
    if (!executionContextId) {
        ScriptState* scriptState = ScriptState::forMainWorld(m_pageAgent->mainFrame());
        return injectedScriptManager()->injectedScriptFor(scriptState);
    }
    InjectedScript injectedScript = injectedScriptManager()->injectedScriptForId(*executionContextId);
    if (injectedScript.isEmpty())
        *errorString = "Execution context with given id not found.";
    return injectedScript;
}

void PageDebuggerAgent::setOverlayMessage(ErrorString*, const String* message)
{
    m_overlay->setPausedInDebuggerMessage(message);
}

void PageDebuggerAgent::didClearDocumentOfWindowObject(LocalFrame* frame)
{
    if (frame != m_pageAgent->mainFrame())
        return;

    reset();

    scriptDebugServer().setPreprocessorSource(String());
    ASSERT(m_pageAgent);
    if (!m_pageAgent->scriptPreprocessorSource().isEmpty())
        scriptDebugServer().setPreprocessorSource(m_pageAgent->scriptPreprocessorSource());
}

String PageDebuggerAgent::preprocessEventListener(LocalFrame* frame, const String& source, const String& url, const String& functionName)
{
    ASSERT(frame);
    ASSERT(m_pageScriptDebugServer);
    return m_pageScriptDebugServer->preprocessEventListener(frame, source, url, functionName);
}

PassOwnPtr<ScriptSourceCode> PageDebuggerAgent::preprocess(LocalFrame* frame, const ScriptSourceCode& sourceCode)
{
    ASSERT(m_pageScriptDebugServer);
    ASSERT(frame);
    return m_pageScriptDebugServer->preprocess(frame, sourceCode);
}

void PageDebuggerAgent::didCommitLoad(LocalFrame* frame, DocumentLoader* loader)
{
    Frame* mainFrame = frame->page()->deprecatedLocalMainFrame();
    if (loader->frame() == mainFrame)
        pageDidCommitLoad();
}

} // namespace WebCore

