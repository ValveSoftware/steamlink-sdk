/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef InspectorClientImpl_h
#define InspectorClientImpl_h

#include "core/inspector/InspectorClient.h"
#include "core/inspector/InspectorController.h"
#include "core/inspector/InspectorFrontendChannel.h"
#include "wtf/OwnPtr.h"

namespace blink {

class WebDevToolsAgentClient;
class WebDevToolsAgentImpl;
class WebViewImpl;

class InspectorClientImpl FINAL : public WebCore::InspectorClient, public WebCore::InspectorFrontendChannel {
public:
    explicit InspectorClientImpl(WebViewImpl*);
    virtual ~InspectorClientImpl();

    // InspectorClient methods:
    virtual void highlight() OVERRIDE;
    virtual void hideHighlight() OVERRIDE;

    virtual void sendMessageToFrontend(PassRefPtr<WebCore::JSONObject>) OVERRIDE;
    virtual void flush() OVERRIDE;

    virtual void updateInspectorStateCookie(const WTF::String&) OVERRIDE;

    virtual void setDeviceMetricsOverride(int, int, float, bool, bool) OVERRIDE;
    virtual void clearDeviceMetricsOverride() OVERRIDE;
    virtual void setTouchEventEmulationEnabled(bool) OVERRIDE;

    virtual bool overridesShowPaintRects() OVERRIDE;
    virtual void setShowPaintRects(bool) OVERRIDE;
    virtual void setShowDebugBorders(bool) OVERRIDE;
    virtual void setShowFPSCounter(bool) OVERRIDE;
    virtual void setContinuousPaintingEnabled(bool) OVERRIDE;
    virtual void setShowScrollBottleneckRects(bool) OVERRIDE;
    virtual void requestPageScaleFactor(float scale, const WebCore::IntPoint& origin) OVERRIDE;

    virtual void getAllocatedObjects(HashSet<const void*>&) OVERRIDE;
    virtual void dumpUncountedAllocatedObjects(const HashMap<const void*, size_t>&) OVERRIDE;

    virtual void dispatchKeyEvent(const WebCore::PlatformKeyboardEvent&) OVERRIDE;
    virtual void dispatchMouseEvent(const WebCore::PlatformMouseEvent&) OVERRIDE;

    virtual void setTraceEventCallback(const String& categoryFilter, TraceEventCallback) OVERRIDE;
    virtual void resetTraceEventCallback() OVERRIDE;
    virtual void enableTracing(const String& categoryFilter) OVERRIDE;
    virtual void disableTracing() OVERRIDE;

    virtual void startGPUEventsRecording() OVERRIDE;
    virtual void stopGPUEventsRecording() OVERRIDE;

private:
    WebDevToolsAgentImpl* devToolsAgent();

    // The WebViewImpl of the page being inspected; gets passed to the constructor
    WebViewImpl* m_inspectedWebView;
};

} // namespace blink

#endif
