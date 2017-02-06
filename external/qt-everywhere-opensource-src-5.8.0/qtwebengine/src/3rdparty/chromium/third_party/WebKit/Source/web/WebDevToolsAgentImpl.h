/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef WebDevToolsAgentImpl_h
#define WebDevToolsAgentImpl_h

#include "core/inspector/InspectorPageAgent.h"
#include "core/inspector/InspectorSession.h"
#include "core/inspector/InspectorTracingAgent.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebSize.h"
#include "public/platform/WebThread.h"
#include "public/web/WebDevToolsAgent.h"
#include "web/InspectorEmulationAgent.h"
#include "wtf/Forward.h"
#include "wtf/Vector.h"
#include <memory>

namespace blink {

class GraphicsLayer;
class InspectedFrames;
class InspectorOverlay;
class InspectorResourceContainer;
class InspectorResourceContentLoader;
class LocalFrame;
class Page;
class PlatformGestureEvent;
class PlatformKeyboardEvent;
class PlatformMouseEvent;
class PlatformTouchEvent;
class WebDevToolsAgentClient;
class WebFrameWidgetImpl;
class WebInputEvent;
class WebLayerTreeView;
class WebLocalFrameImpl;
class WebString;
class WebViewImpl;

namespace protocol {
class Value;
}

class WebDevToolsAgentImpl final
    : public GarbageCollectedFinalized<WebDevToolsAgentImpl>
    , public WebDevToolsAgent
    , public InspectorEmulationAgent::Client
    , public InspectorTracingAgent::Client
    , public InspectorPageAgent::Client
    , public InspectorSession::Client
    , private WebThread::TaskObserver {
public:
    static WebDevToolsAgentImpl* create(WebLocalFrameImpl*, WebDevToolsAgentClient*);
    ~WebDevToolsAgentImpl() override;
    DECLARE_VIRTUAL_TRACE();

    void willBeDestroyed();
    WebDevToolsAgentClient* client() { return m_client; }
    InspectorOverlay* overlay() const { return m_overlay.get(); }
    void flushProtocolNotifications();
    static void webViewImplClosed(WebViewImpl*);
    static void webFrameWidgetImplClosed(WebFrameWidgetImpl*);

    // Instrumentation from web/ layer.
    void didCommitLoadForLocalFrame(LocalFrame*);
    bool screencastEnabled();
    void willAddPageOverlay(const GraphicsLayer*);
    void didRemovePageOverlay(const GraphicsLayer*);
    void layerTreeViewChanged(WebLayerTreeView*);

    // WebDevToolsAgent implementation.
    void attach(const WebString& hostId, int sessionId) override;
    void reattach(const WebString& hostId, int sessionId, const WebString& savedState) override;
    void detach() override;
    void continueProgram() override;
    void dispatchOnInspectorBackend(int sessionId, int callId, const WebString& method, const WebString& message) override;
    void inspectElementAt(const WebPoint&) override;
    void failedToRequestDevTools() override;
    WebString evaluateInWebInspectorOverlay(const WebString& script) override;

private:
    WebDevToolsAgentImpl(WebLocalFrameImpl*, WebDevToolsAgentClient*, InspectorOverlay*, bool includeViewAgents);

    // InspectorTracingAgent::Client implementation.
    void enableTracing(const WTF::String& categoryFilter) override;
    void disableTracing() override;

    // InspectorEmulationAgent::Client implementation.
    void setCPUThrottlingRate(double) override;

    // InspectorPageAgent::Client implementation.
    void pageLayoutInvalidated(bool resized) override;
    void setPausedInDebuggerMessage(const String&) override;
    void waitForCreateWindow(LocalFrame*) override;

    // InspectorSession::Client implementation.
    void sendProtocolMessage(int sessionId, int callId, const String& response, const String& state) override;
    void resumeStartup() override;
    void profilingStarted() override;
    void profilingStopped() override;
    void consoleCleared() override;

    // WebThread::TaskObserver implementation.
    void willProcessTask() override;
    void didProcessTask() override;

    void initializeSession(int sessionId, const String& hostId, String* state);
    void destroySession();
    void dispatchMessageFromFrontend(int sessionId, const String& method, const String& message);

    friend class WebDevToolsAgent;
    static void runDebuggerTask(int sessionId, std::unique_ptr<WebDevToolsAgent::MessageDescriptor>);

    bool attached() const { return m_session.get(); }

    WebDevToolsAgentClient* m_client;
    Member<WebLocalFrameImpl> m_webLocalFrameImpl;

    Member<InstrumentingAgents> m_instrumentingAgents;
    Member<InspectorResourceContentLoader> m_resourceContentLoader;
    Member<InspectorOverlay> m_overlay;
    Member<InspectedFrames> m_inspectedFrames;
    Member<InspectorResourceContainer> m_resourceContainer;

    Member<InspectorDOMAgent> m_domAgent;
    Member<InspectorPageAgent> m_pageAgent;
    Member<InspectorNetworkAgent> m_networkAgent;
    Member<InspectorLayerTreeAgent> m_layerTreeAgent;
    Member<InspectorTracingAgent> m_tracingAgent;

    Member<InspectorSession> m_session;
    bool m_includeViewAgents;
    int m_layerTreeId;
};

} // namespace blink

#endif
