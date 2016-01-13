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

#ifndef InspectorRuntimeAgent_h
#define InspectorRuntimeAgent_h

#include "core/InspectorFrontend.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"

namespace WebCore {

class InjectedScript;
class InjectedScriptManager;
class InstrumentingAgents;
class JSONArray;
class ScriptState;
class ScriptDebugServer;

typedef String ErrorString;

class InspectorRuntimeAgent : public InspectorBaseAgent<InspectorRuntimeAgent>, public InspectorBackendDispatcher::RuntimeCommandHandler {
    WTF_MAKE_NONCOPYABLE(InspectorRuntimeAgent);
public:
    virtual ~InspectorRuntimeAgent();

    // Part of the protocol.
    virtual void enable(ErrorString*) OVERRIDE;
    virtual void disable(ErrorString*) OVERRIDE FINAL;
    virtual void evaluate(ErrorString*,
        const String& expression,
        const String* objectGroup,
        const bool* includeCommandLineAPI,
        const bool* doNotPauseOnExceptionsAndMuteConsole,
        const int* executionContextId,
        const bool* returnByValue,
        const bool* generatePreview,
        RefPtr<TypeBuilder::Runtime::RemoteObject>& result,
        TypeBuilder::OptOutput<bool>* wasThrown) OVERRIDE FINAL;
    virtual void callFunctionOn(ErrorString*,
                        const String& objectId,
                        const String& expression,
                        const RefPtr<JSONArray>* optionalArguments,
                        const bool* doNotPauseOnExceptionsAndMuteConsole,
                        const bool* returnByValue,
                        const bool* generatePreview,
                        RefPtr<TypeBuilder::Runtime::RemoteObject>& result,
                        TypeBuilder::OptOutput<bool>* wasThrown) OVERRIDE FINAL;
    virtual void releaseObject(ErrorString*, const String& objectId) OVERRIDE FINAL;
    virtual void getProperties(ErrorString*, const String& objectId, const bool* ownProperties, const bool* accessorPropertiesOnly, RefPtr<TypeBuilder::Array<TypeBuilder::Runtime::PropertyDescriptor> >& result, RefPtr<TypeBuilder::Array<TypeBuilder::Runtime::InternalPropertyDescriptor> >& internalProperties) OVERRIDE FINAL;
    virtual void releaseObjectGroup(ErrorString*, const String& objectGroup) OVERRIDE FINAL;
    virtual void run(ErrorString*) OVERRIDE;
    virtual void isRunRequired(ErrorString*, bool* out_result) OVERRIDE;

    virtual void setFrontend(InspectorFrontend*) OVERRIDE FINAL;
    virtual void clearFrontend() OVERRIDE FINAL;
    virtual void restore() OVERRIDE FINAL;

protected:
    InspectorRuntimeAgent(InjectedScriptManager*, ScriptDebugServer*);
    virtual InjectedScript injectedScriptForEval(ErrorString*, const int* executionContextId) = 0;

    virtual void muteConsole() = 0;
    virtual void unmuteConsole() = 0;

    InjectedScriptManager* injectedScriptManager() { return m_injectedScriptManager; }
    void addExecutionContextToFrontend(ScriptState*, bool isPageContext, const String& name, const String& frameId);

    bool m_enabled;
    InspectorFrontend::Runtime* m_frontend;

    typedef HashMap<RefPtr<ScriptState>, int> ScriptStateToId;
    ScriptStateToId m_scriptStateToId;

private:
    InjectedScriptManager* m_injectedScriptManager;
    ScriptDebugServer* m_scriptDebugServer;
};

} // namespace WebCore

#endif // InspectorRuntimeAgent_h
