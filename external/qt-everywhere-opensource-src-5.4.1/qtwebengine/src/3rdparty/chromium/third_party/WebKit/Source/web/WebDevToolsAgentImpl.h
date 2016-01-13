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

#include "core/inspector/InspectorClient.h"
#include "core/inspector/InspectorFrontendChannel.h"

#include "public/platform/WebSize.h"
#include "public/platform/WebThread.h"
#include "public/web/WebPageOverlay.h"
#include "web/WebDevToolsAgentPrivate.h"
#include "wtf/Forward.h"
#include "wtf/OwnPtr.h"
#include "wtf/Vector.h"

namespace WebCore {
class Document;
class LocalFrame;
class FrameView;
class GraphicsContext;
class InspectorClient;
class InspectorController;
class Node;
class Page;
class PlatformKeyboardEvent;
}

namespace blink {

class WebDevToolsAgentClient;
class WebFrame;
class WebLocalFrameImpl;
class WebString;
class WebURLRequest;
class WebURLResponse;
class WebViewImpl;
struct WebMemoryUsageInfo;
struct WebURLError;
struct WebDevToolsMessageData;

class WebDevToolsAgentImpl FINAL :
    public WebDevToolsAgentPrivate,
    public WebCore::InspectorClient,
    public WebCore::InspectorFrontendChannel,
    public WebPageOverlay,
    private WebThread::TaskObserver {
public:
    WebDevToolsAgentImpl(WebViewImpl* webViewImpl, WebDevToolsAgentClient* client);
    virtual ~WebDevToolsAgentImpl();

    WebDevToolsAgentClient* client() { return m_client; }

    // WebDevToolsAgentPrivate implementation.
    virtual void didCreateScriptContext(WebLocalFrameImpl*, int worldId) OVERRIDE;
    virtual bool handleInputEvent(WebCore::Page*, const WebInputEvent&) OVERRIDE;

    // WebDevToolsAgent implementation.
    virtual void attach() OVERRIDE;
    virtual void reattach(const WebString& savedState) OVERRIDE;
    virtual void attach(const WebString& hostId) OVERRIDE;
    virtual void reattach(const WebString& hostId, const WebString& savedState) OVERRIDE;
    virtual void detach() OVERRIDE;
    virtual void didNavigate() OVERRIDE;
    virtual void didBeginFrame(int frameId) OVERRIDE;
    virtual void didCancelFrame() OVERRIDE;
    virtual void willComposite() OVERRIDE;
    virtual void didComposite() OVERRIDE;
    virtual void dispatchOnInspectorBackend(const WebString& message) OVERRIDE;
    virtual void inspectElementAt(const WebPoint&) OVERRIDE;
    virtual void evaluateInWebInspector(long callId, const WebString& script) OVERRIDE;
    virtual void setProcessId(long) OVERRIDE;
    virtual void setLayerTreeId(int) OVERRIDE;
    virtual void processGPUEvent(const GPUEvent&) OVERRIDE;

    // InspectorClient implementation.
    virtual void highlight() OVERRIDE;
    virtual void hideHighlight() OVERRIDE;
    virtual void updateInspectorStateCookie(const WTF::String&) OVERRIDE;
    virtual void sendMessageToFrontend(PassRefPtr<WebCore::JSONObject> message) OVERRIDE;
    virtual void flush() OVERRIDE;

    virtual void setDeviceMetricsOverride(int width, int height, float deviceScaleFactor, bool emulateViewport, bool fitWindow) OVERRIDE;
    virtual void clearDeviceMetricsOverride() OVERRIDE;
    virtual void setTouchEventEmulationEnabled(bool) OVERRIDE;

    virtual void getAllocatedObjects(HashSet<const void*>&) OVERRIDE;
    virtual void dumpUncountedAllocatedObjects(const HashMap<const void*, size_t>&) OVERRIDE;
    virtual void setTraceEventCallback(const WTF::String& categoryFilter, TraceEventCallback) OVERRIDE;
    virtual void resetTraceEventCallback() OVERRIDE;
    virtual void enableTracing(const WTF::String& categoryFilter) OVERRIDE;
    virtual void disableTracing() OVERRIDE;

    virtual void startGPUEventsRecording() OVERRIDE;
    virtual void stopGPUEventsRecording() OVERRIDE;

    virtual void dispatchKeyEvent(const WebCore::PlatformKeyboardEvent&) OVERRIDE;
    virtual void dispatchMouseEvent(const WebCore::PlatformMouseEvent&) OVERRIDE;

    // WebPageOverlay
    virtual void paintPageOverlay(WebCanvas*) OVERRIDE;

    void flushPendingFrontendMessages();

private:
    // WebThread::TaskObserver
    virtual void willProcessTask() OVERRIDE;
    virtual void didProcessTask() OVERRIDE;

    void enableViewportEmulation();
    void disableViewportEmulation();
    void updatePageScaleFactorLimits();

    WebCore::InspectorController* inspectorController();
    WebCore::LocalFrame* mainFrame();

    int m_debuggerId;
    int m_layerTreeId;
    WebDevToolsAgentClient* m_client;
    WebViewImpl* m_webViewImpl;
    bool m_attached;
    bool m_generatingEvent;

    bool m_deviceMetricsEnabled;
    bool m_emulateViewportEnabled;
    bool m_originalViewportEnabled;
    bool m_isOverlayScrollbarsEnabled;

    float m_originalMinimumPageScaleFactor;
    float m_originalMaximumPageScaleFactor;
    bool m_pageScaleLimitsOverriden;

    bool m_touchEventEmulationEnabled;
    OwnPtr<WebCore::IntPoint> m_lastPinchAnchorCss;
    OwnPtr<WebCore::IntPoint> m_lastPinchAnchorDip;

    typedef Vector<RefPtr<WebCore::JSONObject> > FrontendMessageQueue;
    FrontendMessageQueue m_frontendMessageQueue;
};

} // namespace blink

#endif
