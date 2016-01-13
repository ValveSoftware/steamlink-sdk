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

#ifndef InspectorDOMDebuggerAgent_h
#define InspectorDOMDebuggerAgent_h


#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/InspectorDOMAgent.h"
#include "core/inspector/InspectorDebuggerAgent.h"
#include "wtf/HashMap.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class Document;
class Element;
class Event;
class EventListener;
class EventTarget;
class InspectorDOMAgent;
class InspectorDebuggerAgent;
class InspectorFrontend;
class InstrumentingAgents;
class JSONObject;
class Node;

typedef String ErrorString;

class InspectorDOMDebuggerAgent FINAL :
    public InspectorBaseAgent<InspectorDOMDebuggerAgent>,
    public InspectorDebuggerAgent::Listener,
    public InspectorDOMAgent::Listener,
    public InspectorBackendDispatcher::DOMDebuggerCommandHandler {
    WTF_MAKE_NONCOPYABLE(InspectorDOMDebuggerAgent);
public:
    static PassOwnPtr<InspectorDOMDebuggerAgent> create(InspectorDOMAgent*, InspectorDebuggerAgent*);

    virtual ~InspectorDOMDebuggerAgent();

    // DOMDebugger API for InspectorFrontend
    virtual void setXHRBreakpoint(ErrorString*, const String& url) OVERRIDE;
    virtual void removeXHRBreakpoint(ErrorString*, const String& url) OVERRIDE;
    virtual void setEventListenerBreakpoint(ErrorString*, const String& eventName, const String* targetName) OVERRIDE;
    virtual void removeEventListenerBreakpoint(ErrorString*, const String& eventName, const String* targetName) OVERRIDE;
    virtual void setInstrumentationBreakpoint(ErrorString*, const String& eventName) OVERRIDE;
    virtual void removeInstrumentationBreakpoint(ErrorString*, const String& eventName) OVERRIDE;
    virtual void setDOMBreakpoint(ErrorString*, int nodeId, const String& type) OVERRIDE;
    virtual void removeDOMBreakpoint(ErrorString*, int nodeId, const String& type) OVERRIDE;

    // InspectorInstrumentation API
    void willInsertDOMNode(Node* parent);
    void didInvalidateStyleAttr(Node*);
    void didInsertDOMNode(Node*);
    void willRemoveDOMNode(Node*);
    void didRemoveDOMNode(Node*);
    void willModifyDOMAttr(Element*, const AtomicString&, const AtomicString&);
    void willSendXMLHttpRequest(const String& url);
    void didInstallTimer(ExecutionContext*, int timerId, int timeout, bool singleShot);
    void didRemoveTimer(ExecutionContext*, int timerId);
    void willFireTimer(ExecutionContext*, int timerId);
    void didRequestAnimationFrame(Document*, int callbackId);
    void didCancelAnimationFrame(Document*, int callbackId);
    void willFireAnimationFrame(Document*, int callbackId);
    void willHandleEvent(EventTarget*, Event*, EventListener*, bool useCapture);
    void didFireWebGLError(const String& errorName);
    void didFireWebGLWarning();
    void didFireWebGLErrorOrWarning(const String& message);
    void willExecuteCustomElementCallback(Element*);

    void didProcessTask();

    virtual void clearFrontend() OVERRIDE;
    virtual void discardAgent() OVERRIDE;

private:
    InspectorDOMDebuggerAgent(InspectorDOMAgent*, InspectorDebuggerAgent*);

    void pauseOnNativeEventIfNeeded(PassRefPtr<JSONObject> eventData, bool synchronous);
    PassRefPtr<JSONObject> preparePauseOnNativeEventData(const String& eventName, const AtomicString* targetName);

    // InspectorDOMAgent::Listener implementation.
    virtual void domAgentWasEnabled() OVERRIDE;
    virtual void domAgentWasDisabled() OVERRIDE;

    // InspectorDebuggerAgent::Listener implementation.
    virtual void debuggerWasEnabled() OVERRIDE;
    virtual void debuggerWasDisabled() OVERRIDE;
    virtual void stepInto() OVERRIDE;
    virtual void didPause() OVERRIDE;
    void disable();

    void descriptionForDOMEvent(Node* target, int breakpointType, bool insertion, JSONObject* description);
    void updateSubtreeBreakpoints(Node*, uint32_t rootMask, bool set);
    bool hasBreakpoint(Node*, int type);
    void setBreakpoint(ErrorString*, const String& eventName, const String* targetName);
    void removeBreakpoint(ErrorString*, const String& eventName, const String* targetName);

    void clear();

    InspectorDOMAgent* m_domAgent;
    InspectorDebuggerAgent* m_debuggerAgent;
    HashMap<Node*, uint32_t> m_domBreakpoints;
    bool m_pauseInNextEventListener;
};

} // namespace WebCore


#endif // !defined(InspectorDOMDebuggerAgent_h)
