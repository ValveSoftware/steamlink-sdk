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

#ifndef PageRuntimeAgent_h
#define PageRuntimeAgent_h

#include "bindings/v8/ScriptState.h"
#include "core/inspector/InspectorRuntimeAgent.h"
#include "wtf/PassOwnPtr.h"

namespace WebCore {

class InspectorPageAgent;
class Page;
class SecurityOrigin;

class PageRuntimeAgent FINAL : public InspectorRuntimeAgent {
public:
    static PassOwnPtr<PageRuntimeAgent> create(InjectedScriptManager* injectedScriptManager, ScriptDebugServer* scriptDebugServer, Page* page, InspectorPageAgent* pageAgent)
    {
        return adoptPtr(new PageRuntimeAgent(injectedScriptManager, scriptDebugServer, page, pageAgent));
    }
    virtual ~PageRuntimeAgent();
    virtual void init() OVERRIDE;
    virtual void enable(ErrorString*) OVERRIDE;

    void didClearDocumentOfWindowObject(LocalFrame*);
    void didCreateIsolatedContext(LocalFrame*, ScriptState*, SecurityOrigin*);
    void frameWindowDiscarded(LocalDOMWindow*);

private:
    PageRuntimeAgent(InjectedScriptManager*, ScriptDebugServer*, Page*, InspectorPageAgent*);

    virtual InjectedScript injectedScriptForEval(ErrorString*, const int* executionContextId) OVERRIDE;
    virtual void muteConsole() OVERRIDE;
    virtual void unmuteConsole() OVERRIDE;
    void reportExecutionContextCreation();

    Page* m_inspectedPage;
    InspectorPageAgent* m_pageAgent;
    bool m_mainWorldContextCreated;
};

} // namespace WebCore


#endif // !defined(InspectorPagerAgent_h)
