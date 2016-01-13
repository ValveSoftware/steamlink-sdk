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

#include "config.h"
#include "web/InspectorClientImpl.h"

#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/page/Page.h"
#include "platform/JSONValues.h"
#include "platform/geometry/FloatRect.h"
#include "public/platform/WebRect.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLRequest.h"
#include "public/web/WebViewClient.h"
#include "web/WebDevToolsAgentImpl.h"
#include "web/WebViewImpl.h"
#include "wtf/Vector.h"

using namespace WebCore;

namespace blink {

InspectorClientImpl::InspectorClientImpl(WebViewImpl* webView)
    : m_inspectedWebView(webView)
{
    ASSERT(m_inspectedWebView);
}

InspectorClientImpl::~InspectorClientImpl()
{
}

void InspectorClientImpl::highlight()
{
    if (WebDevToolsAgentImpl* agent = devToolsAgent())
        agent->highlight();
}

void InspectorClientImpl::hideHighlight()
{
    if (WebDevToolsAgentImpl* agent = devToolsAgent())
        agent->hideHighlight();
}

void InspectorClientImpl::sendMessageToFrontend(PassRefPtr<WebCore::JSONObject> message)
{
    if (WebDevToolsAgentImpl* agent = devToolsAgent())
        agent->sendMessageToFrontend(message);
}

void InspectorClientImpl::flush()
{
    if (WebDevToolsAgentImpl* agent = devToolsAgent())
        agent->flush();
}

void InspectorClientImpl::updateInspectorStateCookie(const WTF::String& inspectorState)
{
    if (WebDevToolsAgentImpl* agent = devToolsAgent())
        agent->updateInspectorStateCookie(inspectorState);
}

void InspectorClientImpl::setDeviceMetricsOverride(int width, int height, float deviceScaleFactor, bool emulateViewport, bool fitWindow)
{
    if (WebDevToolsAgentImpl* agent = devToolsAgent())
        agent->setDeviceMetricsOverride(width, height, deviceScaleFactor, emulateViewport, fitWindow);
}

void InspectorClientImpl::clearDeviceMetricsOverride()
{
    if (WebDevToolsAgentImpl* agent = devToolsAgent())
        agent->clearDeviceMetricsOverride();
}

void InspectorClientImpl::setTouchEventEmulationEnabled(bool enabled)
{
    if (WebDevToolsAgentImpl* agent = devToolsAgent())
        agent->setTouchEventEmulationEnabled(enabled);
}

bool InspectorClientImpl::overridesShowPaintRects()
{
    return m_inspectedWebView->isAcceleratedCompositingActive();
}

void InspectorClientImpl::setShowPaintRects(bool show)
{
    m_inspectedWebView->setShowPaintRects(show);
}

void InspectorClientImpl::setShowDebugBorders(bool show)
{
    m_inspectedWebView->setShowDebugBorders(show);
}

void InspectorClientImpl::setShowFPSCounter(bool show)
{
    m_inspectedWebView->setShowFPSCounter(show);
}

void InspectorClientImpl::setContinuousPaintingEnabled(bool enabled)
{
    m_inspectedWebView->setContinuousPaintingEnabled(enabled);
}

void InspectorClientImpl::setShowScrollBottleneckRects(bool show)
{
    m_inspectedWebView->setShowScrollBottleneckRects(show);
}

void InspectorClientImpl::requestPageScaleFactor(float scale, const IntPoint& origin)
{
    m_inspectedWebView->setPageScaleFactor(scale);
    m_inspectedWebView->setMainFrameScrollOffset(origin);
}

void InspectorClientImpl::getAllocatedObjects(HashSet<const void*>& set)
{
    if (WebDevToolsAgentImpl* agent = devToolsAgent())
        agent->getAllocatedObjects(set);
}

void InspectorClientImpl::dumpUncountedAllocatedObjects(const HashMap<const void*, size_t>& map)
{
    if (WebDevToolsAgentImpl* agent = devToolsAgent())
        agent->dumpUncountedAllocatedObjects(map);
}

void InspectorClientImpl::dispatchKeyEvent(const PlatformKeyboardEvent& event)
{
    if (WebDevToolsAgentImpl* agent = devToolsAgent())
        agent->dispatchKeyEvent(event);
}

void InspectorClientImpl::dispatchMouseEvent(const PlatformMouseEvent& event)
{
    if (WebDevToolsAgentImpl* agent = devToolsAgent())
        agent->dispatchMouseEvent(event);
}

void InspectorClientImpl::setTraceEventCallback(const String& categoryFilter, TraceEventCallback callback)
{
    if (WebDevToolsAgentImpl* agent = devToolsAgent())
        agent->setTraceEventCallback(categoryFilter, callback);
}

void InspectorClientImpl::enableTracing(const String& categoryFilter)
{
    if (WebDevToolsAgentImpl* agent = devToolsAgent())
        agent->enableTracing(categoryFilter);
}

void InspectorClientImpl::disableTracing()
{
    if (WebDevToolsAgentImpl* agent = devToolsAgent())
        agent->disableTracing();
}

void InspectorClientImpl::resetTraceEventCallback()
{
    if (WebDevToolsAgentImpl* agent = devToolsAgent())
        agent->resetTraceEventCallback();
}

void InspectorClientImpl::startGPUEventsRecording()
{
    if (WebDevToolsAgentImpl* agent = devToolsAgent())
        agent->startGPUEventsRecording();
}

void InspectorClientImpl::stopGPUEventsRecording()
{
    if (WebDevToolsAgentImpl* agent = devToolsAgent())
        agent->stopGPUEventsRecording();
}

WebDevToolsAgentImpl* InspectorClientImpl::devToolsAgent()
{
    return static_cast<WebDevToolsAgentImpl*>(m_inspectedWebView->devToolsAgent());
}

} // namespace blink
