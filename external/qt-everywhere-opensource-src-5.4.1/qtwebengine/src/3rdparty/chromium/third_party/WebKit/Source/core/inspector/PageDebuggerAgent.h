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

#ifndef PageDebuggerAgent_h
#define PageDebuggerAgent_h

#include "bindings/v8/PageScriptDebugServer.h"
#include "core/inspector/InspectorDebuggerAgent.h"
#include "core/inspector/InspectorOverlayHost.h"

namespace WebCore {

class DocumentLoader;
class InspectorOverlay;
class InspectorPageAgent;
class Page;
class PageScriptDebugServer;
class ScriptSourceCode;

class PageDebuggerAgent FINAL :
    public InspectorDebuggerAgent,
    public InspectorOverlayHost::Listener {
    WTF_MAKE_NONCOPYABLE(PageDebuggerAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<PageDebuggerAgent> create(PageScriptDebugServer*, InspectorPageAgent*, InjectedScriptManager*, InspectorOverlay*);
    virtual ~PageDebuggerAgent();

    void didClearDocumentOfWindowObject(LocalFrame*);
    String preprocessEventListener(LocalFrame*, const String& source, const String& url, const String& functionName);
    PassOwnPtr<ScriptSourceCode> preprocess(LocalFrame*, const ScriptSourceCode&);
    void didCommitLoad(LocalFrame*, DocumentLoader*);

protected:
    virtual void enable() OVERRIDE;
    virtual void disable() OVERRIDE;

private:
    virtual void startListeningScriptDebugServer() OVERRIDE;
    virtual void stopListeningScriptDebugServer() OVERRIDE;
    virtual PageScriptDebugServer& scriptDebugServer() OVERRIDE;
    virtual void muteConsole() OVERRIDE;
    virtual void unmuteConsole() OVERRIDE;

    // InspectorOverlayHost::Listener implementation.
    virtual void overlayResumed() OVERRIDE;
    virtual void overlaySteppedOver() OVERRIDE;

    virtual InjectedScript injectedScriptForEval(ErrorString*, const int* executionContextId) OVERRIDE;
    virtual void setOverlayMessage(ErrorString*, const String*) OVERRIDE;

    PageDebuggerAgent(PageScriptDebugServer*, InspectorPageAgent*, InjectedScriptManager*, InspectorOverlay*);
    PageScriptDebugServer* m_pageScriptDebugServer;
    InspectorPageAgent* m_pageAgent;
    InspectorOverlay* m_overlay;
};

} // namespace WebCore


#endif // !defined(PageDebuggerAgent_h)
