/*
 * Copyright (C) 2010-2011 Google Inc. All rights reserved.
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

#include "web/WebDevToolsAgentImpl.h"

#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/InstrumentingAgents.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/inspector/InspectedFrames.h"
#include "core/inspector/InspectorAnimationAgent.h"
#include "core/inspector/InspectorApplicationCacheAgent.h"
#include "core/inspector/InspectorCSSAgent.h"
#include "core/inspector/InspectorDOMAgent.h"
#include "core/inspector/InspectorDOMDebuggerAgent.h"
#include "core/inspector/InspectorInputAgent.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorLayerTreeAgent.h"
#include "core/inspector/InspectorMemoryAgent.h"
#include "core/inspector/InspectorNetworkAgent.h"
#include "core/inspector/InspectorPageAgent.h"
#include "core/inspector/InspectorResourceContainer.h"
#include "core/inspector/InspectorResourceContentLoader.h"
#include "core/inspector/InspectorTaskRunner.h"
#include "core/inspector/InspectorTracingAgent.h"
#include "core/inspector/InspectorWorkerAgent.h"
#include "core/inspector/LayoutEditor.h"
#include "core/inspector/MainThreadDebugger.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "modules/accessibility/InspectorAccessibilityAgent.h"
#include "modules/cachestorage/InspectorCacheStorageAgent.h"
#include "modules/device_orientation/DeviceOrientationInspectorAgent.h"
#include "modules/indexeddb/InspectorIndexedDBAgent.h"
#include "modules/storage/InspectorDOMStorageAgent.h"
#include "modules/webdatabase/InspectorDatabaseAgent.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/TraceEvent.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/inspector_protocol/Values.h"
#include "platform/v8_inspector/public/V8Debugger.h"
#include "platform/v8_inspector/public/V8InspectorSession.h"
#include "public/platform/Platform.h"
#include "public/platform/WebLayerTreeView.h"
#include "public/platform/WebRect.h"
#include "public/platform/WebString.h"
#include "public/web/WebDevToolsAgentClient.h"
#include "public/web/WebSettings.h"
#include "web/DevToolsEmulator.h"
#include "web/InspectorEmulationAgent.h"
#include "web/InspectorOverlay.h"
#include "web/InspectorRenderingAgent.h"
#include "web/WebFrameWidgetImpl.h"
#include "web/WebInputEventConversion.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebSettingsImpl.h"
#include "web/WebViewImpl.h"
#include "wtf/MathExtras.h"
#include "wtf/Noncopyable.h"
#include "wtf/PtrUtil.h"
#include "wtf/text/WTFString.h"
#include <memory>

namespace blink {

namespace {

bool isMainFrame(WebLocalFrameImpl* frame)
{
    // TODO(dgozman): sometimes view->mainFrameImpl() does return null, even though |frame| is meant to be main frame.
    // See http://crbug.com/526162.
    return frame->viewImpl() && !frame->parent();
}

}

class ClientMessageLoopAdapter : public MainThreadDebugger::ClientMessageLoop {
public:
    ~ClientMessageLoopAdapter() override
    {
        s_instance = nullptr;
    }

    static void ensureMainThreadDebuggerCreated(WebDevToolsAgentClient* client)
    {
        if (s_instance)
            return;
        std::unique_ptr<ClientMessageLoopAdapter> instance = wrapUnique(new ClientMessageLoopAdapter(wrapUnique(client->createClientMessageLoop())));
        s_instance = instance.get();
        MainThreadDebugger::instance()->setClientMessageLoop(std::move(instance));
    }

    static void webViewImplClosed(WebViewImpl* view)
    {
        if (s_instance)
            s_instance->m_frozenViews.remove(view);
    }

    static void webFrameWidgetImplClosed(WebFrameWidgetImpl* widget)
    {
        if (s_instance)
            s_instance->m_frozenWidgets.remove(widget);
    }

    static void continueProgram()
    {
        // Release render thread if necessary.
        if (s_instance)
            s_instance->quitNow();
    }

    static void pauseForCreateWindow(WebLocalFrameImpl* frame)
    {
        if (s_instance)
            s_instance->runForCreateWindow(frame);
    }

    static bool resumeForCreateWindow()
    {
        return s_instance ? s_instance->quitForCreateWindow() : false;
    }

private:
    ClientMessageLoopAdapter(std::unique_ptr<WebDevToolsAgentClient::WebKitClientMessageLoop> messageLoop)
        : m_runningForDebugBreak(false)
        , m_runningForCreateWindow(false)
        , m_messageLoop(std::move(messageLoop))
    {
        DCHECK(m_messageLoop.get());
    }

    void run(LocalFrame* frame) override
    {
        if (m_runningForDebugBreak)
            return;

        m_runningForDebugBreak = true;
        if (!m_runningForCreateWindow)
            runLoop(WebLocalFrameImpl::fromFrame(frame));
    }

    void runForCreateWindow(WebLocalFrameImpl* frame)
    {
        if (m_runningForCreateWindow)
            return;

        m_runningForCreateWindow = true;
        if (!m_runningForDebugBreak)
            runLoop(frame);
    }

    void runLoop(WebLocalFrameImpl* frame)
    {
        // 0. Flush pending frontend messages.
        WebDevToolsAgentImpl* agent = frame->devToolsAgentImpl();
        agent->flushProtocolNotifications();

        Vector<WebViewImpl*> views;
        HeapVector<Member<WebFrameWidgetImpl>> widgets;

        // 1. Disable input events.
        const HashSet<WebViewImpl*>& viewImpls = WebViewImpl::allInstances();
        HashSet<WebViewImpl*>::const_iterator viewImplsEnd = viewImpls.end();
        for (HashSet<WebViewImpl*>::const_iterator it =  viewImpls.begin(); it != viewImplsEnd; ++it) {
            WebViewImpl* view = *it;
            m_frozenViews.add(view);
            views.append(view);
            view->setIgnoreInputEvents(true);
            view->chromeClient().notifyPopupOpeningObservers();
        }

        const WebFrameWidgetsSet& widgetImpls = WebFrameWidgetImpl::allInstances();
        WebFrameWidgetsSet::const_iterator widgetImplsEnd = widgetImpls.end();
        for (WebFrameWidgetsSet::const_iterator it =  widgetImpls.begin(); it != widgetImplsEnd; ++it) {
            WebFrameWidgetImpl* widget = *it;
            m_frozenWidgets.add(widget);
            widgets.append(widget);
            widget->setIgnoreInputEvents(true);
        }

        // 2. Notify embedder about pausing.
        if (agent->client())
            agent->client()->willEnterDebugLoop();

        // 3. Disable active objects
        WebView::willEnterModalLoop();

        // 4. Process messages until quitNow is called.
        m_messageLoop->run();

        // 5. Resume active objects
        WebView::didExitModalLoop();

        // 6. Resume input events.
        for (Vector<WebViewImpl*>::iterator it = views.begin(); it != views.end(); ++it) {
            if (m_frozenViews.contains(*it)) {
                // The view was not closed during the dispatch.
                (*it)->setIgnoreInputEvents(false);
            }
        }
        for (HeapVector<Member<WebFrameWidgetImpl>>::iterator it = widgets.begin(); it != widgets.end(); ++it) {
            if (m_frozenWidgets.contains(*it)) {
                // The widget was not closed during the dispatch.
                (*it)->setIgnoreInputEvents(false);
            }
        }

        // 7. Notify embedder about resuming.
        if (agent->client())
            agent->client()->didExitDebugLoop();

        // 8. All views have been resumed, clear the set.
        m_frozenViews.clear();
        m_frozenWidgets.clear();
    }

    void quitNow() override
    {
        if (m_runningForDebugBreak) {
            m_runningForDebugBreak = false;
            if (!m_runningForCreateWindow)
                m_messageLoop->quitNow();
        }
    }

    bool quitForCreateWindow()
    {
        if (m_runningForCreateWindow) {
            m_runningForCreateWindow = false;
            if (!m_runningForDebugBreak)
                m_messageLoop->quitNow();
            return true;
        }
        return false;
    }

    bool m_runningForDebugBreak;
    bool m_runningForCreateWindow;
    std::unique_ptr<WebDevToolsAgentClient::WebKitClientMessageLoop> m_messageLoop;
    typedef HashSet<WebViewImpl*> FrozenViewsSet;
    FrozenViewsSet m_frozenViews;
    WebFrameWidgetsSet m_frozenWidgets;
    static ClientMessageLoopAdapter* s_instance;
};

ClientMessageLoopAdapter* ClientMessageLoopAdapter::s_instance = nullptr;

// static
WebDevToolsAgentImpl* WebDevToolsAgentImpl::create(WebLocalFrameImpl* frame, WebDevToolsAgentClient* client)
{
    if (!isMainFrame(frame)) {
        WebDevToolsAgentImpl* agent = new WebDevToolsAgentImpl(frame, client, nullptr, false);
        if (frame->frameWidget())
            agent->layerTreeViewChanged(toWebFrameWidgetImpl(frame->frameWidget())->layerTreeView());
        return agent;
    }

    WebViewImpl* view = frame->viewImpl();
    WebDevToolsAgentImpl* agent = new WebDevToolsAgentImpl(frame, client, InspectorOverlay::create(view), true);
    agent->layerTreeViewChanged(view->layerTreeView());
    return agent;
}

WebDevToolsAgentImpl::WebDevToolsAgentImpl(
    WebLocalFrameImpl* webLocalFrameImpl,
    WebDevToolsAgentClient* client,
    InspectorOverlay* overlay,
    bool includeViewAgents)
    : m_client(client)
    , m_webLocalFrameImpl(webLocalFrameImpl)
    , m_instrumentingAgents(m_webLocalFrameImpl->frame()->instrumentingAgents())
    , m_resourceContentLoader(InspectorResourceContentLoader::create(m_webLocalFrameImpl->frame()))
    , m_overlay(overlay)
    , m_inspectedFrames(InspectedFrames::create(m_webLocalFrameImpl->frame()))
    , m_resourceContainer(new InspectorResourceContainer(m_inspectedFrames))
    , m_domAgent(nullptr)
    , m_pageAgent(nullptr)
    , m_networkAgent(nullptr)
    , m_layerTreeAgent(nullptr)
    , m_tracingAgent(nullptr)
    , m_includeViewAgents(includeViewAgents)
    , m_layerTreeId(0)
{
    DCHECK(isMainThread());
    DCHECK(m_webLocalFrameImpl->frame());
}

WebDevToolsAgentImpl::~WebDevToolsAgentImpl()
{
    DCHECK(!m_client);
}

// static
void WebDevToolsAgentImpl::webViewImplClosed(WebViewImpl* webViewImpl)
{
    ClientMessageLoopAdapter::webViewImplClosed(webViewImpl);
}

// static
void WebDevToolsAgentImpl::webFrameWidgetImplClosed(WebFrameWidgetImpl* webFrameWidgetImpl)
{
    ClientMessageLoopAdapter::webFrameWidgetImplClosed(webFrameWidgetImpl);
}

DEFINE_TRACE(WebDevToolsAgentImpl)
{
    visitor->trace(m_webLocalFrameImpl);
    visitor->trace(m_instrumentingAgents);
    visitor->trace(m_resourceContentLoader);
    visitor->trace(m_overlay);
    visitor->trace(m_inspectedFrames);
    visitor->trace(m_resourceContainer);
    visitor->trace(m_domAgent);
    visitor->trace(m_pageAgent);
    visitor->trace(m_networkAgent);
    visitor->trace(m_layerTreeAgent);
    visitor->trace(m_tracingAgent);
    visitor->trace(m_session);
}

void WebDevToolsAgentImpl::willBeDestroyed()
{
    DCHECK(m_webLocalFrameImpl->frame());
    DCHECK(m_inspectedFrames->root()->view());
    detach();
    m_resourceContentLoader->dispose();
    m_client = nullptr;
}

void WebDevToolsAgentImpl::initializeSession(int sessionId, const String& hostId, String* state)
{
    DCHECK(m_client);
    ClientMessageLoopAdapter::ensureMainThreadDebuggerCreated(m_client);
    MainThreadDebugger* mainThreadDebugger = MainThreadDebugger::instance();
    v8::Isolate* isolate = V8PerIsolateData::mainThreadIsolate();

    m_session = new InspectorSession(this, m_inspectedFrames.get(), m_instrumentingAgents.get(), sessionId, false /* autoFlush */, mainThreadDebugger->debugger(), mainThreadDebugger->contextGroupId(m_inspectedFrames->root()), state);

    InspectorDOMAgent* domAgent = new InspectorDOMAgent(isolate, m_inspectedFrames.get(), m_session->v8Session(), m_overlay.get());
    m_domAgent = domAgent;
    m_session->append(domAgent);

    InspectorLayerTreeAgent* layerTreeAgent = InspectorLayerTreeAgent::create(m_inspectedFrames.get());
    m_layerTreeAgent = layerTreeAgent;
    m_session->append(layerTreeAgent);

    InspectorNetworkAgent* networkAgent = InspectorNetworkAgent::create(m_inspectedFrames.get());
    m_networkAgent = networkAgent;
    m_session->append(networkAgent);

    InspectorCSSAgent* cssAgent = InspectorCSSAgent::create(m_domAgent, m_inspectedFrames.get(), m_networkAgent, m_resourceContentLoader.get(), m_resourceContainer.get());
    m_session->append(cssAgent);

    m_session->append(new InspectorAnimationAgent(m_inspectedFrames.get(), m_domAgent, cssAgent, m_session->v8Session()));

    m_session->append(InspectorMemoryAgent::create());

    m_session->append(InspectorApplicationCacheAgent::create(m_inspectedFrames.get()));

    m_session->append(InspectorIndexedDBAgent::create(m_inspectedFrames.get()));

    InspectorWorkerAgent* workerAgent = new InspectorWorkerAgent(m_inspectedFrames.get());
    m_session->append(workerAgent);

    InspectorTracingAgent* tracingAgent = InspectorTracingAgent::create(this, workerAgent, m_inspectedFrames.get());
    m_tracingAgent = tracingAgent;
    m_session->append(tracingAgent);

    m_session->append(new InspectorDOMDebuggerAgent(isolate, m_domAgent, m_session->v8Session()));

    m_session->append(InspectorInputAgent::create(m_inspectedFrames.get()));

    InspectorPageAgent* pageAgent = InspectorPageAgent::create(m_inspectedFrames.get(), this, m_resourceContentLoader.get(), m_session->v8Session());
    m_pageAgent = pageAgent;
    m_session->append(pageAgent);

    m_tracingAgent->setLayerTreeId(m_layerTreeId);
    m_networkAgent->setHostId(hostId);

    if (m_includeViewAgents) {
        // TODO(dgozman): we should actually pass the view instead of frame, but during
        // remote->local transition we cannot access mainFrameImpl() yet, so we have to store the
        // frame which will become the main frame later.
        m_session->append(InspectorRenderingAgent::create(m_webLocalFrameImpl, m_overlay.get()));
        m_session->append(InspectorEmulationAgent::create(m_webLocalFrameImpl, this));
        // TODO(dgozman): migrate each of the following agents to frame once module is ready.
        Page* page = m_webLocalFrameImpl->viewImpl()->page();
        m_session->append(InspectorDatabaseAgent::create(page));
        m_session->append(DeviceOrientationInspectorAgent::create(page));
        m_session->append(new InspectorAccessibilityAgent(page, m_domAgent));
        m_session->append(InspectorDOMStorageAgent::create(page));
        m_session->append(InspectorCacheStorageAgent::create());
    }

    if (m_overlay)
        m_overlay->init(cssAgent, m_session->v8Session(), m_domAgent);

    Platform::current()->currentThread()->addTaskObserver(this);
}

void WebDevToolsAgentImpl::destroySession()
{
    if (m_overlay)
        m_overlay->clear();

    m_tracingAgent.clear();
    m_layerTreeAgent.clear();
    m_networkAgent.clear();
    m_pageAgent.clear();
    m_domAgent.clear();

    m_session->dispose();
    m_session.clear();

    Platform::current()->currentThread()->removeTaskObserver(this);
}

void WebDevToolsAgentImpl::attach(const WebString& hostId, int sessionId)
{
    if (attached())
        return;
    initializeSession(sessionId, hostId, nullptr);
}

void WebDevToolsAgentImpl::reattach(const WebString& hostId, int sessionId, const WebString& savedState)
{
    if (attached())
        return;
    String state = savedState;
    initializeSession(sessionId, hostId, &state);
    m_session->restore();
}

void WebDevToolsAgentImpl::detach()
{
    if (!attached())
        return;
    destroySession();
}

void WebDevToolsAgentImpl::continueProgram()
{
    ClientMessageLoopAdapter::continueProgram();
}

void WebDevToolsAgentImpl::didCommitLoadForLocalFrame(LocalFrame* frame)
{
    m_resourceContainer->didCommitLoadForLocalFrame(frame);
    m_resourceContentLoader->didCommitLoadForLocalFrame(frame);
    if (m_session)
        m_session->didCommitLoadForLocalFrame(frame);
}

bool WebDevToolsAgentImpl::screencastEnabled()
{
    return m_pageAgent && m_pageAgent->screencastEnabled();
}

void WebDevToolsAgentImpl::willAddPageOverlay(const GraphicsLayer* layer)
{
    if (m_layerTreeAgent)
        m_layerTreeAgent->willAddPageOverlay(layer);
}

void WebDevToolsAgentImpl::didRemovePageOverlay(const GraphicsLayer* layer)
{
    if (m_layerTreeAgent)
        m_layerTreeAgent->didRemovePageOverlay(layer);
}

void WebDevToolsAgentImpl::layerTreeViewChanged(WebLayerTreeView* layerTreeView)
{
    m_layerTreeId = layerTreeView ? layerTreeView->layerTreeId() : 0;
    if (m_tracingAgent)
        m_tracingAgent->setLayerTreeId(m_layerTreeId);
}

void WebDevToolsAgentImpl::enableTracing(const String& categoryFilter)
{
    if (m_client)
        m_client->enableTracing(categoryFilter);
}

void WebDevToolsAgentImpl::disableTracing()
{
    if (m_client)
        m_client->disableTracing();
}

void WebDevToolsAgentImpl::setCPUThrottlingRate(double rate)
{
    if (m_client)
        m_client->setCPUThrottlingRate(rate);
}

void WebDevToolsAgentImpl::dispatchOnInspectorBackend(int sessionId, int callId, const WebString& method, const WebString& message)
{
    if (!attached())
        return;
    if (WebDevToolsAgent::shouldInterruptForMethod(method))
        MainThreadDebugger::instance()->taskRunner()->runAllTasksDontWait();
    else
        dispatchMessageFromFrontend(sessionId, method, message);
}

void WebDevToolsAgentImpl::dispatchMessageFromFrontend(int sessionId, const String& method, const String& message)
{
    if (!attached() || sessionId != m_session->sessionId())
        return;
    InspectorTaskRunner::IgnoreInterruptsScope scope(MainThreadDebugger::instance()->taskRunner());
    m_session->dispatchProtocolMessage(method, message);
}

void WebDevToolsAgentImpl::inspectElementAt(const WebPoint& pointInRootFrame)
{
    if (!m_domAgent)
        return;
    HitTestRequest::HitTestRequestType hitType = HitTestRequest::Move | HitTestRequest::ReadOnly | HitTestRequest::AllowChildFrameContent;
    HitTestRequest request(hitType);
    WebMouseEvent dummyEvent;
    dummyEvent.type = WebInputEvent::MouseDown;
    dummyEvent.x = pointInRootFrame.x;
    dummyEvent.y = pointInRootFrame.y;
    IntPoint transformedPoint = PlatformMouseEventBuilder(m_webLocalFrameImpl->frameView(), dummyEvent).position();
    HitTestResult result(request, m_webLocalFrameImpl->frameView()->rootFrameToContents(transformedPoint));
    m_webLocalFrameImpl->frame()->contentLayoutItem().hitTest(result);
    Node* node = result.innerNode();
    if (!node && m_webLocalFrameImpl->frame()->document())
        node = m_webLocalFrameImpl->frame()->document()->documentElement();
    m_domAgent->inspect(node);
}

void WebDevToolsAgentImpl::failedToRequestDevTools()
{
    ClientMessageLoopAdapter::resumeForCreateWindow();
}

void WebDevToolsAgentImpl::sendProtocolMessage(int sessionId, int callId, const String& response, const String& state)
{
    ASSERT(attached());
    if (m_client)
        m_client->sendProtocolMessage(sessionId, callId, response, state);
}

void WebDevToolsAgentImpl::resumeStartup()
{
    // If we've paused for createWindow, handle it ourselves.
    if (ClientMessageLoopAdapter::resumeForCreateWindow())
        return;
    // Otherwise, pass to the client (embedded workers do it differently).
    if (m_client)
        m_client->resumeStartup();
}

void WebDevToolsAgentImpl::profilingStarted()
{
    if (m_overlay)
        m_overlay->suspend();
}

void WebDevToolsAgentImpl::profilingStopped()
{
    if (m_overlay)
        m_overlay->resume();
}

void WebDevToolsAgentImpl::consoleCleared()
{
    if (m_domAgent)
        m_domAgent->releaseDanglingNodes();
}

void WebDevToolsAgentImpl::pageLayoutInvalidated(bool resized)
{
    if (m_overlay)
        m_overlay->pageLayoutInvalidated(resized);
}

void WebDevToolsAgentImpl::setPausedInDebuggerMessage(const String& message)
{
    if (m_overlay)
        m_overlay->setPausedInDebuggerMessage(message);
}

void WebDevToolsAgentImpl::waitForCreateWindow(LocalFrame* frame)
{
    if (!attached())
        return;
    if (m_client && m_client->requestDevToolsForFrame(WebLocalFrameImpl::fromFrame(frame)))
        ClientMessageLoopAdapter::pauseForCreateWindow(m_webLocalFrameImpl);
}

WebString WebDevToolsAgentImpl::evaluateInWebInspectorOverlay(const WebString& script)
{
    if (!m_overlay)
        return WebString();

    return m_overlay->evaluateInOverlayForTest(script);
}

void WebDevToolsAgentImpl::flushProtocolNotifications()
{
    if (m_session)
        m_session->flushProtocolNotifications();
}

void WebDevToolsAgentImpl::willProcessTask()
{
    if (!attached())
        return;
    ThreadDebugger::idleFinished(V8PerIsolateData::mainThreadIsolate());
}

void WebDevToolsAgentImpl::didProcessTask()
{
    if (!attached())
        return;
    ThreadDebugger::idleStarted(V8PerIsolateData::mainThreadIsolate());
    flushProtocolNotifications();
}

void WebDevToolsAgentImpl::runDebuggerTask(int sessionId, std::unique_ptr<WebDevToolsAgent::MessageDescriptor> descriptor)
{
    WebDevToolsAgent* webagent = descriptor->agent();
    if (!webagent)
        return;

    WebDevToolsAgentImpl* agentImpl = static_cast<WebDevToolsAgentImpl*>(webagent);
    if (agentImpl->attached())
        agentImpl->dispatchMessageFromFrontend(sessionId, descriptor->method(), descriptor->message());
}

void WebDevToolsAgent::interruptAndDispatch(int sessionId, MessageDescriptor* rawDescriptor)
{
    // rawDescriptor can't be a std::unique_ptr because interruptAndDispatch is a WebKit API function.
    MainThreadDebugger::interruptMainThreadAndRun(crossThreadBind(WebDevToolsAgentImpl::runDebuggerTask, sessionId, passed(wrapUnique(rawDescriptor))));
}

bool WebDevToolsAgent::shouldInterruptForMethod(const WebString& method)
{
    return method == "Debugger.pause"
        || method == "Debugger.setBreakpoint"
        || method == "Debugger.setBreakpointByUrl"
        || method == "Debugger.removeBreakpoint"
        || method == "Debugger.setBreakpointsActive";
}

} // namespace blink
