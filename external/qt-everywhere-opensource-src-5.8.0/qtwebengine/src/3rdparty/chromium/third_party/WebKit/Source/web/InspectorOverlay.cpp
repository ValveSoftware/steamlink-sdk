/*
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

#include "web/InspectorOverlay.h"

#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8InspectorOverlayHost.h"
#include "core/dom/Node.h"
#include "core/dom/StaticNodeList.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/VisualViewport.h"
#include "core/input/EventHandler.h"
#include "core/inspector/InspectorCSSAgent.h"
#include "core/inspector/InspectorOverlayHost.h"
#include "core/inspector/LayoutEditor.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/loader/EmptyClients.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "platform/ScriptForbiddenScope.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/CullRect.h"
#include "platform/inspector_protocol/Values.h"
#include "platform/v8_inspector/public/V8InspectorSession.h"
#include "public/platform/Platform.h"
#include "public/platform/WebData.h"
#include "web/PageOverlay.h"
#include "web/WebInputEventConversion.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebViewImpl.h"
#include <memory>
#include <v8.h>

namespace blink {

namespace {

Node* hoveredNodeForPoint(LocalFrame* frame, const IntPoint& pointInRootFrame, bool ignorePointerEventsNone)
{
    HitTestRequest::HitTestRequestType hitType = HitTestRequest::Move | HitTestRequest::ReadOnly | HitTestRequest::AllowChildFrameContent;
    if (ignorePointerEventsNone)
        hitType |= HitTestRequest::IgnorePointerEventsNone;
    HitTestRequest request(hitType);
    HitTestResult result(request, frame->view()->rootFrameToContents(pointInRootFrame));
    frame->contentLayoutItem().hitTest(result);
    Node* node = result.innerPossiblyPseudoNode();
    while (node && node->getNodeType() == Node::TEXT_NODE)
        node = node->parentNode();
    return node;
}

Node* hoveredNodeForEvent(LocalFrame* frame, const PlatformGestureEvent& event, bool ignorePointerEventsNone)
{
    return hoveredNodeForPoint(frame, event.position(), ignorePointerEventsNone);
}

Node* hoveredNodeForEvent(LocalFrame* frame, const PlatformMouseEvent& event, bool ignorePointerEventsNone)
{
    return hoveredNodeForPoint(frame, event.position(), ignorePointerEventsNone);
}

Node* hoveredNodeForEvent(LocalFrame* frame, const PlatformTouchEvent& event, bool ignorePointerEventsNone)
{
    const Vector<PlatformTouchPoint>& points = event.touchPoints();
    if (!points.size())
        return nullptr;
    return hoveredNodeForPoint(frame, roundedIntPoint(points[0].pos()), ignorePointerEventsNone);
}
} // namespace

class InspectorOverlay::InspectorPageOverlayDelegate final : public PageOverlay::Delegate {
public:
    explicit InspectorPageOverlayDelegate(InspectorOverlay& overlay)
        : m_overlay(&overlay)
    { }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_overlay);
        PageOverlay::Delegate::trace(visitor);
    }

    void paintPageOverlay(const PageOverlay&, GraphicsContext& graphicsContext, const WebSize& webViewSize) const override
    {
        if (m_overlay->isEmpty())
            return;

        FrameView* view = m_overlay->overlayMainFrame()->view();
        DCHECK(!view->needsLayout());
        view->paint(graphicsContext, CullRect(IntRect(0, 0, view->width(), view->height())));
    }

private:
    Member<InspectorOverlay> m_overlay;
};


class InspectorOverlay::InspectorOverlayChromeClient final : public EmptyChromeClient {
public:
    static InspectorOverlayChromeClient* create(ChromeClient& client, InspectorOverlay& overlay)
    {
        return new InspectorOverlayChromeClient(client, overlay);
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_client);
        visitor->trace(m_overlay);
        EmptyChromeClient::trace(visitor);
    }

    void setCursor(const Cursor& cursor, LocalFrame* localRoot) override
    {
        toChromeClientImpl(m_client)->setCursorOverridden(false);
        toChromeClientImpl(m_client)->setCursor(cursor, localRoot);
        bool overrideCursor = m_overlay->m_layoutEditor;
        toChromeClientImpl(m_client)->setCursorOverridden(overrideCursor);
    }

    void setToolTip(const String& tooltip, TextDirection direction) override
    {
        m_client->setToolTip(tooltip, direction);
    }

    void invalidateRect(const IntRect&) override
    {
        m_overlay->invalidate();
    }

    void scheduleAnimation(Widget* widget) override
    {
        if (m_overlay->m_inLayout)
            return;

        m_client->scheduleAnimation(widget);
    }

private:
    InspectorOverlayChromeClient(ChromeClient& client, InspectorOverlay& overlay)
        : m_client(&client)
        , m_overlay(&overlay)
    { }

    Member<ChromeClient> m_client;
    Member<InspectorOverlay> m_overlay;
};


InspectorOverlay::InspectorOverlay(WebViewImpl* webViewImpl)
    : m_webViewImpl(webViewImpl)
    , m_overlayHost(InspectorOverlayHost::create())
    , m_drawViewSize(false)
    , m_resizeTimerActive(false)
    , m_omitTooltip(false)
    , m_timer(this, &InspectorOverlay::onTimer)
    , m_suspendCount(0)
    , m_inLayout(false)
    , m_needsUpdate(false)
    , m_inspectMode(InspectorDOMAgent::NotSearching)
{
}

InspectorOverlay::~InspectorOverlay()
{
    DCHECK(!m_overlayPage);
}

DEFINE_TRACE(InspectorOverlay)
{
    visitor->trace(m_highlightNode);
    visitor->trace(m_eventTargetNode);
    visitor->trace(m_overlayPage);
    visitor->trace(m_overlayChromeClient);
    visitor->trace(m_overlayHost);
    visitor->trace(m_domAgent);
    visitor->trace(m_cssAgent);
    visitor->trace(m_layoutEditor);
    visitor->trace(m_hoveredNodeForInspectMode);
}

void InspectorOverlay::init(InspectorCSSAgent* cssAgent, V8InspectorSession* v8Session, InspectorDOMAgent* domAgent)
{
    m_v8Session = v8Session;
    m_domAgent = domAgent;
    m_cssAgent = cssAgent;
    m_overlayHost->setListener(this);
}

void InspectorOverlay::invalidate()
{
    if (!m_pageOverlay)
        m_pageOverlay = PageOverlay::create(m_webViewImpl, new InspectorPageOverlayDelegate(*this));

    m_pageOverlay->update();
}

void InspectorOverlay::updateAllLifecyclePhases()
{
    if (isEmpty())
        return;

    TemporaryChange<bool> scoped(m_inLayout, true);
    if (m_needsUpdate) {
        m_needsUpdate = false;
        rebuildOverlayPage();
    }
    overlayMainFrame()->view()->updateAllLifecyclePhases();
}

bool InspectorOverlay::handleInputEvent(const WebInputEvent& inputEvent)
{
    bool handled = false;

    if (isEmpty())
        return false;

    if (WebInputEvent::isGestureEventType(inputEvent.type) && inputEvent.type == WebInputEvent::GestureTap) {
        // Only let GestureTab in (we only need it and we know PlatformGestureEventBuilder supports it).
        PlatformGestureEvent gestureEvent = PlatformGestureEventBuilder(m_webViewImpl->mainFrameImpl()->frameView(), static_cast<const WebGestureEvent&>(inputEvent));
        handled = handleGestureEvent(gestureEvent);
        if (handled)
            return true;

        overlayMainFrame()->eventHandler().handleGestureEvent(gestureEvent);
    }
    if (WebInputEvent::isMouseEventType(inputEvent.type) && inputEvent.type != WebInputEvent::MouseEnter) {
        // PlatformMouseEventBuilder does not work with MouseEnter type, so we filter it out manually.
        PlatformMouseEvent mouseEvent = PlatformMouseEventBuilder(m_webViewImpl->mainFrameImpl()->frameView(), static_cast<const WebMouseEvent&>(inputEvent));

        if (mouseEvent.type() == PlatformEvent::MouseMoved)
            handled = handleMouseMove(mouseEvent);
        else if (mouseEvent.type() == PlatformEvent::MousePressed)
            handled = handleMousePress();

        if (handled)
            return true;

        if (mouseEvent.type() == PlatformEvent::MouseMoved)
            handled = overlayMainFrame()->eventHandler().handleMouseMoveEvent(mouseEvent) != WebInputEventResult::NotHandled;
        if (mouseEvent.type() == PlatformEvent::MousePressed)
            handled = overlayMainFrame()->eventHandler().handleMousePressEvent(mouseEvent) != WebInputEventResult::NotHandled;
        if (mouseEvent.type() == PlatformEvent::MouseReleased)
            handled = overlayMainFrame()->eventHandler().handleMouseReleaseEvent(mouseEvent) != WebInputEventResult::NotHandled;
    }

    if (WebInputEvent::isTouchEventType(inputEvent.type)) {
        PlatformTouchEvent touchEvent = PlatformTouchEventBuilder(m_webViewImpl->mainFrameImpl()->frameView(), static_cast<const WebTouchEvent&>(inputEvent));
        handled = handleTouchEvent(touchEvent);
        if (handled)
            return true;
        overlayMainFrame()->eventHandler().handleTouchEvent(touchEvent);
    }
    if (WebInputEvent::isKeyboardEventType(inputEvent.type)) {
        PlatformKeyboardEvent keyboardEvent = PlatformKeyboardEventBuilder(static_cast<const WebKeyboardEvent&>(inputEvent));
        overlayMainFrame()->eventHandler().keyEvent(keyboardEvent);
    }

    if (inputEvent.type == WebInputEvent::MouseWheel) {
        PlatformWheelEvent wheelEvent = PlatformWheelEventBuilder(m_webViewImpl->mainFrameImpl()->frameView(), static_cast<const WebMouseWheelEvent&>(inputEvent));
        handled = overlayMainFrame()->eventHandler().handleWheelEvent(wheelEvent) != WebInputEventResult::NotHandled;
    }

    return handled;
}

void InspectorOverlay::setPausedInDebuggerMessage(const String& message)
{
    m_pausedInDebuggerMessage = message;
    scheduleUpdate();
}

void InspectorOverlay::hideHighlight()
{
    m_highlightNode.clear();
    m_eventTargetNode.clear();
    m_highlightQuad.reset();
    scheduleUpdate();
}

void InspectorOverlay::highlightNode(Node* node, const InspectorHighlightConfig& highlightConfig, bool omitTooltip)
{
    highlightNode(node, nullptr, highlightConfig, omitTooltip);
}

void InspectorOverlay::highlightNode(Node* node, Node* eventTarget, const InspectorHighlightConfig& highlightConfig, bool omitTooltip)
{
    m_nodeHighlightConfig = highlightConfig;
    m_highlightNode = node;
    m_eventTargetNode = eventTarget;
    m_omitTooltip = omitTooltip;
    scheduleUpdate();
}

void InspectorOverlay::setInspectMode(InspectorDOMAgent::SearchMode searchMode, std::unique_ptr<InspectorHighlightConfig> highlightConfig)
{
    if (m_layoutEditor)
        overlayClearSelection(true);

    m_inspectMode = searchMode;
    scheduleUpdate();

    if (searchMode != InspectorDOMAgent::NotSearching) {
        m_inspectModeHighlightConfig = std::move(highlightConfig);
    } else {
        m_hoveredNodeForInspectMode.clear();
        hideHighlight();
    }
}

void InspectorOverlay::setInspectedNode(Node* node)
{
    if (m_inspectMode != InspectorDOMAgent::ShowLayoutEditor || (m_layoutEditor && m_layoutEditor->element() == node))
        return;

    if (m_layoutEditor) {
        m_layoutEditor->commitChanges();
        m_layoutEditor.clear();
    }
    initializeLayoutEditorIfNeeded(node);
}

void InspectorOverlay::highlightQuad(std::unique_ptr<FloatQuad> quad, const InspectorHighlightConfig& highlightConfig)
{
    m_quadHighlightConfig = highlightConfig;
    m_highlightQuad = std::move(quad);
    m_omitTooltip = false;
    scheduleUpdate();
}

bool InspectorOverlay::isEmpty()
{
    if (m_suspendCount)
        return true;
    bool hasVisibleElements = m_highlightNode || m_eventTargetNode || m_highlightQuad  || (m_resizeTimerActive && m_drawViewSize) || !m_pausedInDebuggerMessage.isNull();
    return !hasVisibleElements && m_inspectMode == InspectorDOMAgent::NotSearching;
}

void InspectorOverlay::scheduleUpdate()
{
    if (isEmpty()) {
        if (m_pageOverlay)
            m_pageOverlay.reset();
        return;
    }
    m_needsUpdate = true;
    FrameView* view = m_webViewImpl->mainFrameImpl()->frameView();
    if (view)
        m_webViewImpl->page()->chromeClient().scheduleAnimation(view);
}

void InspectorOverlay::rebuildOverlayPage()
{
    FrameView* view = m_webViewImpl->mainFrameImpl()->frameView();
    if (!view)
        return;

    IntRect visibleRectInDocument = view->getScrollableArea()->visibleContentRect();
    IntSize viewportSize = m_webViewImpl->page()->frameHost().visualViewport().size();
    LocalFrame* frame = toLocalFrame(overlayPage()->mainFrame());
    frame->view()->resize(viewportSize);
    overlayPage()->frameHost().visualViewport().setSize(viewportSize);
    frame->setPageZoomFactor(windowToViewportScale());

    reset(viewportSize, visibleRectInDocument.location());

    drawNodeHighlight();
    drawQuadHighlight();
    drawPausedInDebuggerMessage();
    drawViewSize();
    if (m_layoutEditor && !m_highlightNode)
        m_layoutEditor->rebuild();
}

static std::unique_ptr<protocol::DictionaryValue> buildObjectForSize(const IntSize& size)
{
    std::unique_ptr<protocol::DictionaryValue> result = protocol::DictionaryValue::create();
    result->setNumber("width", size.width());
    result->setNumber("height", size.height());
    return result;
}

void InspectorOverlay::drawNodeHighlight()
{
    if (!m_highlightNode)
        return;

    String selectors = m_nodeHighlightConfig.selectorList;
    StaticElementList* elements = nullptr;
    TrackExceptionState exceptionState;
    ContainerNode* queryBase = m_highlightNode->containingShadowRoot();
    if (!queryBase)
        queryBase = m_highlightNode->ownerDocument();
    if (selectors.length())
        elements = queryBase->querySelectorAll(AtomicString(selectors), exceptionState);
    if (elements && !exceptionState.hadException()) {
        for (unsigned i = 0; i < elements->length(); ++i) {
            Element* element = elements->item(i);
            InspectorHighlight highlight(element, m_nodeHighlightConfig, false);
            std::unique_ptr<protocol::DictionaryValue> highlightJSON = highlight.asProtocolValue();
            evaluateInOverlay("drawHighlight", std::move(highlightJSON));
        }
    }

    bool appendElementInfo = m_highlightNode->isElementNode() && !m_omitTooltip && m_nodeHighlightConfig.showInfo && m_highlightNode->layoutObject() && m_highlightNode->document().frame();
    InspectorHighlight highlight(m_highlightNode.get(), m_nodeHighlightConfig, appendElementInfo);
    if (m_eventTargetNode)
        highlight.appendEventTargetQuads(m_eventTargetNode.get(), m_nodeHighlightConfig);

    std::unique_ptr<protocol::DictionaryValue> highlightJSON = highlight.asProtocolValue();
    evaluateInOverlay("drawHighlight", std::move(highlightJSON));
}

void InspectorOverlay::drawQuadHighlight()
{
    if (!m_highlightQuad)
        return;

    InspectorHighlight highlight(windowToViewportScale());
    highlight.appendQuad(*m_highlightQuad, m_quadHighlightConfig.content, m_quadHighlightConfig.contentOutline);
    evaluateInOverlay("drawHighlight", highlight.asProtocolValue());
}

void InspectorOverlay::drawPausedInDebuggerMessage()
{
    if (m_inspectMode == InspectorDOMAgent::NotSearching && !m_pausedInDebuggerMessage.isNull())
        evaluateInOverlay("drawPausedInDebuggerMessage", m_pausedInDebuggerMessage);
}

void InspectorOverlay::drawViewSize()
{
    if (m_resizeTimerActive && m_drawViewSize)
        evaluateInOverlay("drawViewSize", "");
}

float InspectorOverlay::windowToViewportScale() const
{
    return m_webViewImpl->chromeClient().windowToViewportScalar(1.0f);
}

Page* InspectorOverlay::overlayPage()
{
    if (m_overlayPage)
        return m_overlayPage.get();

    ScriptForbiddenScope::AllowUserAgentScript allowScript;

    DEFINE_STATIC_LOCAL(FrameLoaderClient, dummyFrameLoaderClient, (EmptyFrameLoaderClient::create()));
    Page::PageClients pageClients;
    fillWithEmptyClients(pageClients);
    DCHECK(!m_overlayChromeClient);
    m_overlayChromeClient = InspectorOverlayChromeClient::create(m_webViewImpl->page()->chromeClient(), *this);
    pageClients.chromeClient = m_overlayChromeClient.get();
    m_overlayPage = Page::create(pageClients);

    Settings& settings = m_webViewImpl->page()->settings();
    Settings& overlaySettings = m_overlayPage->settings();

    overlaySettings.genericFontFamilySettings().updateStandard(settings.genericFontFamilySettings().standard());
    overlaySettings.genericFontFamilySettings().updateSerif(settings.genericFontFamilySettings().serif());
    overlaySettings.genericFontFamilySettings().updateSansSerif(settings.genericFontFamilySettings().sansSerif());
    overlaySettings.genericFontFamilySettings().updateCursive(settings.genericFontFamilySettings().cursive());
    overlaySettings.genericFontFamilySettings().updateFantasy(settings.genericFontFamilySettings().fantasy());
    overlaySettings.genericFontFamilySettings().updatePictograph(settings.genericFontFamilySettings().pictograph());
    overlaySettings.setMinimumFontSize(settings.minimumFontSize());
    overlaySettings.setMinimumLogicalFontSize(settings.minimumLogicalFontSize());
    overlaySettings.setScriptEnabled(true);
    overlaySettings.setPluginsEnabled(false);
    overlaySettings.setLoadsImagesAutomatically(true);
    // FIXME: http://crbug.com/363843. Inspector should probably create its
    // own graphics layers and attach them to the tree rather than going
    // through some non-composited paint function.
    overlaySettings.setAcceleratedCompositingEnabled(false);

    LocalFrame* frame = LocalFrame::create(&dummyFrameLoaderClient, &m_overlayPage->frameHost(), 0);
    frame->setView(FrameView::create(frame));
    frame->init();
    FrameLoader& loader = frame->loader();
    frame->view()->setCanHaveScrollbars(false);
    frame->view()->setTransparent(true);

    const WebData& overlayPageHTMLResource = Platform::current()->loadResource("InspectorOverlayPage.html");
    RefPtr<SharedBuffer> data = SharedBuffer::create(overlayPageHTMLResource.data(), overlayPageHTMLResource.size());
    loader.load(FrameLoadRequest(0, blankURL(), SubstituteData(data, "text/html", "UTF-8", KURL(), ForceSynchronousLoad)));
    v8::Isolate* isolate = toIsolate(frame);
    ScriptState* scriptState = ScriptState::forMainWorld(frame);
    DCHECK(scriptState);
    ScriptState::Scope scope(scriptState);
    v8::Local<v8::Object> global = scriptState->context()->Global();
    v8::Local<v8::Value> overlayHostObj = toV8(m_overlayHost.get(), global, isolate);
    DCHECK(!overlayHostObj.IsEmpty());
    v8CallOrCrash(global->Set(scriptState->context(), v8AtomicString(isolate, "InspectorOverlayHost"), overlayHostObj));

#if OS(WIN)
    evaluateInOverlay("setPlatform", "windows");
#elif OS(MACOSX)
    evaluateInOverlay("setPlatform", "mac");
#elif OS(POSIX)
    evaluateInOverlay("setPlatform", "linux");
#endif

    return m_overlayPage.get();
}

LocalFrame* InspectorOverlay::overlayMainFrame()
{
    return toLocalFrame(overlayPage()->mainFrame());
}

void InspectorOverlay::reset(const IntSize& viewportSize, const IntPoint& documentScrollOffset)
{
    std::unique_ptr<protocol::DictionaryValue> resetData = protocol::DictionaryValue::create();
    resetData->setNumber("deviceScaleFactor", m_webViewImpl->page()->deviceScaleFactor());
    resetData->setNumber("pageScaleFactor", m_webViewImpl->page()->pageScaleFactor());

    IntRect viewportInScreen = m_webViewImpl->chromeClient().viewportToScreen(
        IntRect(IntPoint(), viewportSize), m_webViewImpl->mainFrameImpl()->frame()->view());
    resetData->setObject("viewportSize", buildObjectForSize(viewportInScreen.size()));

    // The zoom factor in the overlay frame already has been multiplied by the window to viewport scale
    // (aka device scale factor), so cancel it.
    resetData->setNumber("pageZoomFactor", m_webViewImpl->mainFrameImpl()->frame()->pageZoomFactor() / windowToViewportScale());

    resetData->setNumber("scrollX", documentScrollOffset.x());
    resetData->setNumber("scrollY", documentScrollOffset.y());
    evaluateInOverlay("reset", std::move(resetData));
}

void InspectorOverlay::evaluateInOverlay(const String& method, const String& argument)
{
    ScriptForbiddenScope::AllowUserAgentScript allowScript;
    std::unique_ptr<protocol::ListValue> command = protocol::ListValue::create();
    command->pushValue(protocol::StringValue::create(method));
    command->pushValue(protocol::StringValue::create(argument));
    toLocalFrame(overlayPage()->mainFrame())->script().executeScriptInMainWorld("dispatch(" + command->toJSONString() + ")", ScriptController::ExecuteScriptWhenScriptsDisabled);
}

void InspectorOverlay::evaluateInOverlay(const String& method, std::unique_ptr<protocol::Value> argument)
{
    ScriptForbiddenScope::AllowUserAgentScript allowScript;
    std::unique_ptr<protocol::ListValue> command = protocol::ListValue::create();
    command->pushValue(protocol::StringValue::create(method));
    command->pushValue(std::move(argument));
    toLocalFrame(overlayPage()->mainFrame())->script().executeScriptInMainWorld("dispatch(" + command->toJSONString() + ")", ScriptController::ExecuteScriptWhenScriptsDisabled);
}

String InspectorOverlay::evaluateInOverlayForTest(const String& script)
{
    ScriptForbiddenScope::AllowUserAgentScript allowScript;
    v8::HandleScope handleScope(toIsolate(overlayMainFrame()));
    v8::Local<v8::Value> string = toLocalFrame(overlayPage()->mainFrame())->script().executeScriptInMainWorldAndReturnValue(ScriptSourceCode(script), ScriptController::ExecuteScriptWhenScriptsDisabled);
    return toCoreStringWithUndefinedOrNullCheck(string);
}

void InspectorOverlay::onTimer(Timer<InspectorOverlay>*)
{
    m_resizeTimerActive = false;
    scheduleUpdate();
}

void InspectorOverlay::clearInternal()
{
    if (m_layoutEditor)
        m_layoutEditor.clear();

    if (m_overlayPage) {
        m_overlayPage->willBeDestroyed();
        m_overlayPage.clear();
        m_overlayChromeClient.clear();
    }
    m_resizeTimerActive = false;
    m_pausedInDebuggerMessage = String();
    m_inspectMode = InspectorDOMAgent::NotSearching;
    m_timer.stop();
    hideHighlight();
}

void InspectorOverlay::clear()
{
    clearInternal();
    m_v8Session = nullptr;
    m_domAgent.clear();
    m_cssAgent.clear();
    m_overlayHost->setListener(nullptr);
}

void InspectorOverlay::overlayResumed()
{
    if (m_v8Session)
        m_v8Session->resume();
}

void InspectorOverlay::overlaySteppedOver()
{
    if (m_v8Session)
        m_v8Session->stepOver();
}

void InspectorOverlay::overlayStartedPropertyChange(const String& property)
{
    DCHECK(m_layoutEditor);
    m_layoutEditor->overlayStartedPropertyChange(property);
}

void InspectorOverlay::overlayPropertyChanged(float value)
{
    DCHECK(m_layoutEditor);
    m_layoutEditor->overlayPropertyChanged(value);
}

void InspectorOverlay::overlayEndedPropertyChange()
{
    DCHECK(m_layoutEditor);
    m_layoutEditor->overlayEndedPropertyChange();
}

void InspectorOverlay::overlayNextSelector()
{
    DCHECK(m_layoutEditor);
    m_layoutEditor->nextSelector();
}

void InspectorOverlay::overlayPreviousSelector()
{
    DCHECK(m_layoutEditor);
    m_layoutEditor->previousSelector();
}

void InspectorOverlay::overlayClearSelection(bool commitChanges)
{
    DCHECK(m_layoutEditor);
    m_hoveredNodeForInspectMode = m_layoutEditor->element();

    if (commitChanges)
        m_layoutEditor->commitChanges();

    if (m_layoutEditor) {
        m_layoutEditor->dispose();
        m_layoutEditor.clear();
    }

    if (m_inspectModeHighlightConfig)
        highlightNode(m_hoveredNodeForInspectMode.get(), *m_inspectModeHighlightConfig, false);

    toChromeClientImpl(m_webViewImpl->page()->chromeClient()).setCursorOverridden(false);
    toChromeClientImpl(m_webViewImpl->page()->chromeClient()).setCursor(pointerCursor(), overlayMainFrame());
}

void InspectorOverlay::suspend()
{
    if (!m_suspendCount++)
        clearInternal();
}

void InspectorOverlay::resume()
{
    --m_suspendCount;
}

void InspectorOverlay::pageLayoutInvalidated(bool resized)
{
    if (resized && m_drawViewSize) {
        m_resizeTimerActive = true;
        m_timer.startOneShot(1, BLINK_FROM_HERE);
    }
    scheduleUpdate();
}

void InspectorOverlay::setShowViewportSizeOnResize(bool show)
{
    m_drawViewSize = show;
}

bool InspectorOverlay::handleMouseMove(const PlatformMouseEvent& event)
{
    if (!shouldSearchForNode())
        return false;

    LocalFrame* frame = m_webViewImpl->mainFrameImpl()->frame();
    if (!frame->view() || frame->contentLayoutItem().isNull())
        return false;
    Node* node = hoveredNodeForEvent(frame, event, event.shiftKey());

    // Do not highlight within user agent shadow root unless requested.
    if (m_inspectMode != InspectorDOMAgent::SearchingForUAShadow) {
        ShadowRoot* shadowRoot = InspectorDOMAgent::userAgentShadowRoot(node);
        if (shadowRoot)
            node = &shadowRoot->host();
    }

    // Shadow roots don't have boxes - use host element instead.
    if (node && node->isShadowRoot())
        node = node->parentOrShadowHostNode();

    if (!node)
        return true;

    Node* eventTarget = event.shiftKey() ? hoveredNodeForEvent(m_webViewImpl->mainFrameImpl()->frame(), event, false) : nullptr;
    if (eventTarget == node)
        eventTarget = nullptr;

    if (node && m_inspectModeHighlightConfig) {
        m_hoveredNodeForInspectMode = node;
        if (m_domAgent)
            m_domAgent->nodeHighlightedInOverlay(node);
        highlightNode(node, eventTarget, *m_inspectModeHighlightConfig, event.ctrlKey() || event.metaKey());
    }
    return true;
}

bool InspectorOverlay::handleMousePress()
{
    if (!shouldSearchForNode())
        return false;

    if (m_hoveredNodeForInspectMode) {
        inspect(m_hoveredNodeForInspectMode.get());
        m_hoveredNodeForInspectMode.clear();
        return true;
    }
    return false;
}

bool InspectorOverlay::handleGestureEvent(const PlatformGestureEvent& event)
{
    if (!shouldSearchForNode() || event.type() != PlatformEvent::GestureTap)
        return false;
    Node* node = hoveredNodeForEvent(m_webViewImpl->mainFrameImpl()->frame(), event, false);
    if (node && m_inspectModeHighlightConfig) {
        highlightNode(node, *m_inspectModeHighlightConfig, false);
        inspect(node);
        return true;
    }
    return false;
}

bool InspectorOverlay::handleTouchEvent(const PlatformTouchEvent& event)
{
    if (!shouldSearchForNode())
        return false;
    Node* node = hoveredNodeForEvent(m_webViewImpl->mainFrameImpl()->frame(), event, false);
    if (node && m_inspectModeHighlightConfig) {
        highlightNode(node, *m_inspectModeHighlightConfig, false);
        inspect(node);
        return true;
    }
    return false;
}

bool InspectorOverlay::shouldSearchForNode()
{
    return m_inspectMode != InspectorDOMAgent::NotSearching && !m_layoutEditor;
}

void InspectorOverlay::inspect(Node* node)
{
    if (m_domAgent)
        m_domAgent->inspect(node);

    initializeLayoutEditorIfNeeded(node);
    if (m_layoutEditor)
        hideHighlight();
}

void InspectorOverlay::initializeLayoutEditorIfNeeded(Node* node)
{
    if (m_inspectMode != InspectorDOMAgent::ShowLayoutEditor || !node || !node->isElementNode() || !node->ownerDocument()->isActive() || !m_cssAgent || !m_domAgent)
        return;
    m_layoutEditor = LayoutEditor::create(toElement(node), m_cssAgent, m_domAgent, &overlayMainFrame()->script());
    toChromeClientImpl(m_webViewImpl->page()->chromeClient()).setCursorOverridden(true);
}

} // namespace blink
