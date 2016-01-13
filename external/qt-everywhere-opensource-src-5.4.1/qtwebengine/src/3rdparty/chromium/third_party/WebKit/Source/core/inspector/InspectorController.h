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

#ifndef InspectorController_h
#define InspectorController_h

#include "core/inspector/InspectorBaseAgent.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/Noncopyable.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class DOMWrapperWorld;
class LocalFrame;
class GraphicsContext;
class GraphicsLayer;
class InjectedScriptManager;
class InspectorBackendDispatcher;
class InspectorAgent;
class InspectorClient;
class InspectorDOMAgent;
class InspectorFrontend;
class InspectorFrontendChannel;
class InspectorFrontendClient;
class InspectorLayerTreeAgent;
class InspectorPageAgent;
class InspectorResourceAgent;
class InspectorTimelineAgent;
class InspectorTracingAgent;
class InspectorOverlay;
class InspectorState;
class InstrumentingAgents;
class IntPoint;
class IntSize;
class Page;
class PlatformGestureEvent;
class PlatformKeyboardEvent;
class PlatformMouseEvent;
class PlatformTouchEvent;
class Node;

class InspectorController {
    WTF_MAKE_NONCOPYABLE(InspectorController);
    WTF_MAKE_FAST_ALLOCATED;
public:
    ~InspectorController();

    static PassOwnPtr<InspectorController> create(Page*, InspectorClient*);

    // Settings overrides.
    void setTextAutosizingEnabled(bool);
    void setDeviceScaleAdjustment(float);

    void willBeDestroyed();
    void registerModuleAgent(PassOwnPtr<InspectorAgent>);

    void setInspectorFrontendClient(PassOwnPtr<InspectorFrontendClient>);
    void didClearDocumentOfWindowObject(LocalFrame*);
    void setInjectedScriptForOrigin(const String& origin, const String& source);

    void dispatchMessageFromFrontend(const String& message);

    void connectFrontend(const String& hostId, InspectorFrontendChannel*);
    void disconnectFrontend();
    void reconnectFrontend();
    void reuseFrontend(const String& hostId, InspectorFrontendChannel*, const String& inspectorStateCookie);
    void setProcessId(long);
    void setLayerTreeId(int);

    void inspect(Node*);
    void drawHighlight(GraphicsContext&) const;

    bool handleGestureEvent(LocalFrame*, const PlatformGestureEvent&);
    bool handleMouseEvent(LocalFrame*, const PlatformMouseEvent&);
    bool handleTouchEvent(LocalFrame*, const PlatformTouchEvent&);
    bool handleKeyboardEvent(LocalFrame*, const PlatformKeyboardEvent&);

    void requestPageScaleFactor(float scale, const IntPoint& origin);
    bool deviceEmulationEnabled();

    bool isUnderTest();
    void evaluateForTestInFrontend(long callId, const String& script);

    void resume();

    void setResourcesDataSizeLimitsFromInternals(int maximumResourcesContentSize, int maximumSingleResourceContentSize);

    void willProcessTask();
    void didProcessTask();
    void flushPendingFrontendMessages();

    void didCommitLoadForMainFrame();
    void didBeginFrame(int frameId);
    void didCancelFrame();
    void willComposite();
    void didComposite();

    void processGPUEvent(double timestamp, int phase, bool foreign, uint64_t usedGPUMemoryBytes, uint64_t limitGPUMemoryBytes);

    void scriptsEnabled(bool);

    void willAddPageOverlay(const GraphicsLayer*);
    void didRemovePageOverlay(const GraphicsLayer*);

private:
    InspectorController(Page*, InspectorClient*);

    void initializeDeferredAgents();

    friend InstrumentingAgents* instrumentationForPage(Page*);

    RefPtr<InstrumentingAgents> m_instrumentingAgents;
    OwnPtr<InjectedScriptManager> m_injectedScriptManager;
    OwnPtr<InspectorCompositeState> m_state;
    OwnPtr<InspectorOverlay> m_overlay;

    InspectorDOMAgent* m_domAgent;
    InspectorPageAgent* m_pageAgent;
    InspectorResourceAgent* m_resourceAgent;
    InspectorTimelineAgent* m_timelineAgent;
    InspectorLayerTreeAgent* m_layerTreeAgent;
    InspectorTracingAgent* m_tracingAgent;

    RefPtr<InspectorBackendDispatcher> m_inspectorBackendDispatcher;
    OwnPtr<InspectorFrontendClient> m_inspectorFrontendClient;
    OwnPtr<InspectorFrontend> m_inspectorFrontend;
    Page* m_page;
    InspectorClient* m_inspectorClient;
    InspectorAgentRegistry m_agents;
    Vector<InspectorAgent*> m_moduleAgents;
    bool m_isUnderTest;
    bool m_deferredAgentsInitialized;
    String m_hostId;
};

}


#endif // !defined(InspectorController_h)
