/*
 * Copyright (C) 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InspectorInspectorAgent_h
#define InspectorInspectorAgent_h

#include "core/inspector/InspectorBaseAgent.h"
#include "wtf/HashMap.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

class DOMWrapperWorld;
class DocumentLoader;
class LocalFrame;
class InjectedScriptManager;
class InspectorFrontend;
class InstrumentingAgents;
class JSONObject;
class Page;

typedef String ErrorString;

class InspectorInspectorAgent FINAL : public InspectorBaseAgent<InspectorInspectorAgent>, public InspectorBackendDispatcher::InspectorCommandHandler {
    WTF_MAKE_NONCOPYABLE(InspectorInspectorAgent);
public:
    static PassOwnPtr<InspectorInspectorAgent> create(Page* page, InjectedScriptManager* injectedScriptManager)
    {
        return adoptPtr(new InspectorInspectorAgent(page, injectedScriptManager));
    }

    virtual ~InspectorInspectorAgent();

    // Inspector front-end API.
    virtual void enable(ErrorString*) OVERRIDE;
    virtual void disable(ErrorString*) OVERRIDE;
    virtual void reset(ErrorString*) OVERRIDE;

    virtual void init() OVERRIDE;
    virtual void setFrontend(InspectorFrontend*) OVERRIDE;
    virtual void clearFrontend() OVERRIDE;

    void didClearDocumentOfWindowObject(LocalFrame*);

    void domContentLoadedEventFired(LocalFrame*);

    bool hasFrontend() const { return m_frontend; }

    // Generic code called from custom implementations.
    void evaluateForTestInFrontend(long testCallId, const String& script);

    void setInjectedScriptForOrigin(const String& origin, const String& source);

    void inspect(PassRefPtr<TypeBuilder::Runtime::RemoteObject> objectToInspect, PassRefPtr<JSONObject> hints);

private:
    InspectorInspectorAgent(Page*, InjectedScriptManager*);

    Page* m_inspectedPage;
    InspectorFrontend* m_frontend;
    InjectedScriptManager* m_injectedScriptManager;

    Vector<pair<long, String> > m_pendingEvaluateTestCommands;
    pair<RefPtr<TypeBuilder::Runtime::RemoteObject>, RefPtr<JSONObject> > m_pendingInspectData;
    typedef HashMap<String, String> InjectedScriptForOriginMap;
    InjectedScriptForOriginMap m_injectedScriptForOrigin;
};

} // namespace WebCore

#endif // !defined(InspectorInspectorAgent_h)
