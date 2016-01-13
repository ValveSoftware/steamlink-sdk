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
#include "web/WebPluginContainerImpl.h"

#include "core/page/Chrome.h"
#include "core/page/EventHandler.h"
#include "platform/exported/WrappedResourceResponse.h"
#include "public/web/WebElement.h"
#include "public/web/WebInputEvent.h"
#include "public/web/WebPlugin.h"
#include "public/web/WebViewClient.h"
#include "web/ChromeClientImpl.h"
#include "web/ScrollbarGroup.h"
#include "web/WebDataSourceImpl.h"
#include "web/WebInputEventConversion.h"
#include "web/WebViewImpl.h"

#include "bindings/v8/ScriptController.h"
#include "core/HTMLNames.h"
#include "core/clipboard/Clipboard.h"
#include "core/clipboard/DataObject.h"
#include "core/events/GestureEvent.h"
#include "core/events/KeyboardEvent.h"
#include "core/events/MouseEvent.h"
#include "core/events/TouchEvent.h"
#include "core/events/WheelEvent.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLPlugInElement.h"
#include "core/loader/FormState.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "core/plugins/PluginOcclusionSupport.h"
#include "core/rendering/HitTestResult.h"
#include "core/rendering/RenderBox.h"
#include "core/rendering/RenderLayer.h"
#include "platform/HostWindow.h"
#include "platform/KeyboardCodes.h"
#include "platform/PlatformGestureEvent.h"
#include "platform/UserGestureIndicator.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/scroll/ScrollAnimator.h"
#include "platform/scroll/ScrollView.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "public/platform/Platform.h"
#include "public/platform/WebClipboard.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebCursorInfo.h"
#include "public/platform/WebDragData.h"
#include "public/platform/WebExternalTextureLayer.h"
#include "public/platform/WebRect.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLError.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/WebVector.h"
#include "public/web/WebPrintParams.h"

using namespace WebCore;

namespace blink {

// Public methods --------------------------------------------------------------

void WebPluginContainerImpl::setFrameRect(const IntRect& frameRect)
{
    Widget::setFrameRect(frameRect);
    reportGeometry();
}

void WebPluginContainerImpl::paint(GraphicsContext* gc, const IntRect& damageRect)
{
    if (gc->updatingControlTints() && m_scrollbarGroup) {
        // See comment in FrameView::updateControlTints().
        if (m_scrollbarGroup->horizontalScrollbar())
            m_scrollbarGroup->horizontalScrollbar()->invalidate();
        if (m_scrollbarGroup->verticalScrollbar())
            m_scrollbarGroup->verticalScrollbar()->invalidate();
    }

    if (gc->paintingDisabled())
        return;

    if (!parent())
        return;

    // Don't paint anything if the plugin doesn't intersect the damage rect.
    if (!frameRect().intersects(damageRect))
        return;

    gc->save();

    ASSERT(parent()->isFrameView());
    ScrollView* view =  toScrollView(parent());

    // The plugin is positioned in window coordinates, so it needs to be painted
    // in window coordinates.
    IntPoint origin = view->contentsToWindow(IntPoint(0, 0));
    gc->translate(static_cast<float>(-origin.x()), static_cast<float>(-origin.y()));

    WebCanvas* canvas = gc->canvas();

    IntRect windowRect = view->contentsToWindow(damageRect);
    m_webPlugin->paint(canvas, windowRect);

    gc->restore();
}

void WebPluginContainerImpl::invalidateRect(const IntRect& rect)
{
    if (!parent())
        return;

    RenderBox* renderer = toRenderBox(m_element->renderer());

    IntRect dirtyRect = rect;
    dirtyRect.move(renderer->borderLeft() + renderer->paddingLeft(),
                   renderer->borderTop() + renderer->paddingTop());
    renderer->invalidatePaintRectangle(dirtyRect);
}

void WebPluginContainerImpl::setFocus(bool focused)
{
    Widget::setFocus(focused);
    m_webPlugin->updateFocus(focused);
}

void WebPluginContainerImpl::show()
{
    setSelfVisible(true);
    m_webPlugin->updateVisibility(true);

    Widget::show();
}

void WebPluginContainerImpl::hide()
{
    setSelfVisible(false);
    m_webPlugin->updateVisibility(false);

    Widget::hide();
}

void WebPluginContainerImpl::handleEvent(Event* event)
{
    if (!m_webPlugin->acceptsInputEvents())
        return;

    RefPtr<WebPluginContainerImpl> protector(this);
    // The events we pass are defined at:
    //    http://devedge-temp.mozilla.org/library/manuals/2002/plugin/1.0/structures5.html#1000000
    // Don't take the documentation as truth, however.  There are many cases
    // where mozilla behaves differently than the spec.
    if (event->isMouseEvent())
        handleMouseEvent(toMouseEvent(event));
    else if (event->isWheelEvent())
        handleWheelEvent(toWheelEvent(event));
    else if (event->isKeyboardEvent())
        handleKeyboardEvent(toKeyboardEvent(event));
    else if (event->isTouchEvent())
        handleTouchEvent(toTouchEvent(event));
    else if (event->isGestureEvent())
        handleGestureEvent(toGestureEvent(event));

    // FIXME: it would be cleaner if Widget::handleEvent returned true/false and
    // HTMLPluginElement called setDefaultHandled or defaultEventHandler.
    if (!event->defaultHandled())
        m_element->Node::defaultEventHandler(event);
}

void WebPluginContainerImpl::frameRectsChanged()
{
    Widget::frameRectsChanged();
    reportGeometry();
}

void WebPluginContainerImpl::widgetPositionsUpdated()
{
    Widget::widgetPositionsUpdated();
    reportGeometry();
}

void WebPluginContainerImpl::eventListenersRemoved()
{
    // We're no longer registered to receive touch events, so don't try to remove
    // the touch event handlers in our destructor.
    m_touchEventRequestType = TouchEventRequestTypeNone;
}

void WebPluginContainerImpl::setParentVisible(bool parentVisible)
{
    // We override this function to make sure that geometry updates are sent
    // over to the plugin. For e.g. when a plugin is instantiated it does not
    // have a valid parent. As a result the first geometry update from webkit
    // is ignored. This function is called when the plugin eventually gets a
    // parent.

    if (isParentVisible() == parentVisible)
        return;  // No change.

    Widget::setParentVisible(parentVisible);
    if (!isSelfVisible())
        return;  // This widget has explicitely been marked as not visible.

    m_webPlugin->updateVisibility(isVisible());
}

void WebPluginContainerImpl::setParent(Widget* widget)
{
    // We override this function so that if the plugin is windowed, we can call
    // NPP_SetWindow at the first possible moment.  This ensures that
    // NPP_SetWindow is called before the manual load data is sent to a plugin.
    // If this order is reversed, Flash won't load videos.

    Widget::setParent(widget);
    if (widget)
        reportGeometry();
    else if (m_webPlugin)
        m_webPlugin->containerDidDetachFromParent();
}

void WebPluginContainerImpl::setPlugin(WebPlugin* plugin)
{
    if (plugin != m_webPlugin) {
        m_element->resetInstance();
        m_webPlugin = plugin;
    }
}

float WebPluginContainerImpl::deviceScaleFactor()
{
    Page* page = m_element->document().page();
    if (!page)
        return 1.0;
    return page->deviceScaleFactor();
}

float WebPluginContainerImpl::pageScaleFactor()
{
    Page* page = m_element->document().page();
    if (!page)
        return 1.0;
    return page->pageScaleFactor();
}

float WebPluginContainerImpl::pageZoomFactor()
{
    LocalFrame* frame = m_element->document().frame();
    if (!frame)
        return 1.0;
    return frame->pageZoomFactor();
}

void WebPluginContainerImpl::setWebLayer(WebLayer* layer)
{
    if (m_webLayer == layer)
        return;

    if (m_webLayer)
        GraphicsLayer::unregisterContentsLayer(m_webLayer);
    if (layer)
        GraphicsLayer::registerContentsLayer(layer);

    // If either of the layers is null we need to switch between hardware
    // and software compositing.
    bool needsCompositingUpdate = !m_webLayer || !layer;

    m_webLayer = layer;

    if (!needsCompositingUpdate)
        return;

    m_element->setNeedsCompositingUpdate();
    // Being composited or not affects the self painting layer bit
    // on the RenderLayer.
    if (RenderPart* renderer = m_element->renderPart()) {
        ASSERT(renderer->hasLayer());
        renderer->layer()->updateSelfPaintingLayer();
    }
}

bool WebPluginContainerImpl::supportsPaginatedPrint() const
{
    return m_webPlugin->supportsPaginatedPrint();
}

bool WebPluginContainerImpl::isPrintScalingDisabled() const
{
    return m_webPlugin->isPrintScalingDisabled();
}

int WebPluginContainerImpl::printBegin(const WebPrintParams& printParams) const
{
    return m_webPlugin->printBegin(printParams);
}

bool WebPluginContainerImpl::printPage(int pageNumber,
                                       WebCore::GraphicsContext* gc)
{
    if (gc->paintingDisabled())
        return true;
    gc->save();
    WebCanvas* canvas = gc->canvas();
    bool ret = m_webPlugin->printPage(pageNumber, canvas);
    gc->restore();
    return ret;
}

void WebPluginContainerImpl::printEnd()
{
    m_webPlugin->printEnd();
}

void WebPluginContainerImpl::copy()
{
    if (!m_webPlugin->hasSelection())
        return;

    blink::Platform::current()->clipboard()->writeHTML(m_webPlugin->selectionAsMarkup(), WebURL(), m_webPlugin->selectionAsText(), false);
}

bool WebPluginContainerImpl::executeEditCommand(const WebString& name)
{
    if (m_webPlugin->executeEditCommand(name))
        return true;

    if (name != "Copy")
        return false;

    copy();
    return true;
}

bool WebPluginContainerImpl::executeEditCommand(const WebString& name, const WebString& value)
{
    return m_webPlugin->executeEditCommand(name, value);
}

WebElement WebPluginContainerImpl::element()
{
    return WebElement(m_element);
}

void WebPluginContainerImpl::invalidate()
{
    Widget::invalidate();
}

void WebPluginContainerImpl::invalidateRect(const WebRect& rect)
{
    invalidateRect(static_cast<IntRect>(rect));
}

void WebPluginContainerImpl::scrollRect(int dx, int dy, const WebRect& rect)
{
    Widget* parentWidget = parent();
    if (parentWidget->isFrameView()) {
        FrameView* parentFrameView = toFrameView(parentWidget);
        if (!parentFrameView->isOverlapped()) {
            IntRect damageRect = convertToContainingWindow(static_cast<IntRect>(rect));
            IntSize scrollDelta(dx, dy);
            // scroll() only uses the second rectangle, clipRect, and ignores the first
            // rectangle.
            parent()->hostWindow()->scroll(scrollDelta, damageRect, damageRect);
            return;
        }
    }

    // Use slow scrolling instead.
    invalidateRect(rect);
}

void WebPluginContainerImpl::reportGeometry()
{
    if (!parent())
        return;

    IntRect windowRect, clipRect;
    Vector<IntRect> cutOutRects;
    calculateGeometry(frameRect(), windowRect, clipRect, cutOutRects);

    m_webPlugin->updateGeometry(windowRect, clipRect, cutOutRects, isVisible());

    if (m_scrollbarGroup) {
        m_scrollbarGroup->scrollAnimator()->contentsResized();
        m_scrollbarGroup->setFrameRect(frameRect());
    }
}

void WebPluginContainerImpl::allowScriptObjects()
{
}

void WebPluginContainerImpl::clearScriptObjects()
{
    LocalFrame* frame = m_element->document().frame();
    if (!frame)
        return;
    frame->script().cleanupScriptObjectsForPlugin(this);
}

NPObject* WebPluginContainerImpl::scriptableObjectForElement()
{
    return m_element->getNPObject();
}

WebString WebPluginContainerImpl::executeScriptURL(const WebURL& url, bool popupsAllowed)
{
    LocalFrame* frame = m_element->document().frame();
    if (!frame)
        return WebString();

    const KURL& kurl = url;
    ASSERT(kurl.protocolIs("javascript"));

    String script = decodeURLEscapeSequences(
        kurl.string().substring(strlen("javascript:")));

    UserGestureIndicator gestureIndicator(popupsAllowed ? DefinitelyProcessingNewUserGesture : PossiblyProcessingUserGesture);
    v8::HandleScope handleScope(toIsolate(frame));
    v8::Local<v8::Value> result = frame->script().executeScriptInMainWorldAndReturnValue(ScriptSourceCode(script));

    // Failure is reported as a null string.
    if (result.IsEmpty() || !result->IsString())
        return WebString();
    return toCoreString(v8::Handle<v8::String>::Cast(result));
}

void WebPluginContainerImpl::loadFrameRequest(const WebURLRequest& request, const WebString& target, bool notifyNeeded, void* notifyData)
{
    LocalFrame* frame = m_element->document().frame();
    if (!frame || !frame->loader().documentLoader())
        return;  // FIXME: send a notification in this case?

    if (notifyNeeded) {
        // FIXME: This is a bit of hack to allow us to observe completion of
        // our frame request.  It would be better to evolve FrameLoader to
        // support a completion callback instead.
        OwnPtr<WebPluginLoadObserver> observer = adoptPtr(new WebPluginLoadObserver(this, request.url(), notifyData));
        // FIXME: Calling get here is dangerous! What if observer is freed?
        m_pluginLoadObservers.append(observer.get());
        WebDataSourceImpl::setNextPluginLoadObserver(observer.release());
    }

    FrameLoadRequest frameRequest(frame->document(), request.toResourceRequest(), target);
    UserGestureIndicator gestureIndicator(request.hasUserGesture() ? DefinitelyProcessingNewUserGesture : PossiblyProcessingUserGesture);
    frame->loader().load(frameRequest);
}

void WebPluginContainerImpl::zoomLevelChanged(double zoomLevel)
{
    WebViewImpl* view = WebViewImpl::fromPage(m_element->document().frame()->page());
    view->fullFramePluginZoomLevelChanged(zoomLevel);
}

bool WebPluginContainerImpl::isRectTopmost(const WebRect& rect)
{
    LocalFrame* frame = m_element->document().frame();
    if (!frame)
        return false;

    // hitTestResultAtPoint() takes a padding rectangle.
    // FIXME: We'll be off by 1 when the width or height is even.
    IntRect documentRect(x() + rect.x, y() + rect.y, rect.width, rect.height);
    LayoutPoint center = documentRect.center();
    // Make the rect we're checking (the point surrounded by padding rects) contained inside the requested rect. (Note that -1/2 is 0.)
    LayoutSize padding((documentRect.width() - 1) / 2, (documentRect.height() - 1) / 2);
    HitTestResult result = frame->eventHandler().hitTestResultAtPoint(center, HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::ConfusingAndOftenMisusedDisallowShadowContent, padding);
    const HitTestResult::NodeSet& nodes = result.rectBasedTestResult();
    if (nodes.size() != 1)
        return false;
    return nodes.first().get() == m_element;
}

void WebPluginContainerImpl::requestTouchEventType(TouchEventRequestType requestType)
{
    if (m_touchEventRequestType == requestType)
        return;

    if (requestType != TouchEventRequestTypeNone && m_touchEventRequestType == TouchEventRequestTypeNone)
        m_element->document().didAddTouchEventHandler(m_element);
    else if (requestType == TouchEventRequestTypeNone && m_touchEventRequestType != TouchEventRequestTypeNone)
        m_element->document().didRemoveTouchEventHandler(m_element);
    m_touchEventRequestType = requestType;
}

void WebPluginContainerImpl::setWantsWheelEvents(bool wantsWheelEvents)
{
    if (m_wantsWheelEvents == wantsWheelEvents)
        return;
    m_wantsWheelEvents = wantsWheelEvents;
    if (Page* page = m_element->document().page()) {
        if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator()) {
            if (parent() && parent()->isFrameView())
                scrollingCoordinator->notifyLayoutUpdated();
        }
    }
}

WebPoint WebPluginContainerImpl::windowToLocalPoint(const WebPoint& point)
{
    ScrollView* view = toScrollView(parent());
    if (!view)
        return point;
    WebPoint windowPoint = view->windowToContents(point);
    return roundedIntPoint(m_element->renderer()->absoluteToLocal(LayoutPoint(windowPoint), UseTransforms));
}

WebPoint WebPluginContainerImpl::localToWindowPoint(const WebPoint& point)
{
    ScrollView* view = toScrollView(parent());
    if (!view)
        return point;
    IntPoint absolutePoint = roundedIntPoint(m_element->renderer()->localToAbsolute(LayoutPoint(point), UseTransforms));
    return view->contentsToWindow(absolutePoint);
}

void WebPluginContainerImpl::didReceiveResponse(const ResourceResponse& response)
{
    // Make sure that the plugin receives window geometry before data, or else
    // plugins misbehave.
    frameRectsChanged();

    WrappedResourceResponse urlResponse(response);
    m_webPlugin->didReceiveResponse(urlResponse);
}

void WebPluginContainerImpl::didReceiveData(const char *data, int dataLength)
{
    m_webPlugin->didReceiveData(data, dataLength);
}

void WebPluginContainerImpl::didFinishLoading()
{
    m_webPlugin->didFinishLoading();
}

void WebPluginContainerImpl::didFailLoading(const ResourceError& error)
{
    m_webPlugin->didFailLoading(error);
}

WebLayer* WebPluginContainerImpl::platformLayer() const
{
    return m_webLayer;
}

NPObject* WebPluginContainerImpl::scriptableObject()
{
    return m_webPlugin->scriptableObject();
}

bool WebPluginContainerImpl::getFormValue(String& value)
{
    WebString webValue;
    if (m_webPlugin->getFormValue(webValue)) {
        value = webValue;
        return true;
    }
    return false;
}

bool WebPluginContainerImpl::supportsKeyboardFocus() const
{
    return m_webPlugin->supportsKeyboardFocus();
}

bool WebPluginContainerImpl::supportsInputMethod() const
{
    return m_webPlugin->supportsInputMethod();
}

bool WebPluginContainerImpl::canProcessDrag() const
{
    return m_webPlugin->canProcessDrag();
}

bool WebPluginContainerImpl::wantsWheelEvents()
{
    return m_wantsWheelEvents;
}

void WebPluginContainerImpl::willDestroyPluginLoadObserver(WebPluginLoadObserver* observer)
{
    size_t pos = m_pluginLoadObservers.find(observer);
    if (pos == kNotFound)
        return;
    m_pluginLoadObservers.remove(pos);
}

ScrollbarGroup* WebPluginContainerImpl::scrollbarGroup()
{
    if (!m_scrollbarGroup)
        m_scrollbarGroup = adoptPtr(new ScrollbarGroup(m_element->document().frame()->view(), frameRect()));
    return m_scrollbarGroup.get();
}

void WebPluginContainerImpl::willStartLiveResize()
{
    if (m_scrollbarGroup)
        m_scrollbarGroup->willStartLiveResize();
}

void WebPluginContainerImpl::willEndLiveResize()
{
    if (m_scrollbarGroup)
        m_scrollbarGroup->willEndLiveResize();
}

bool WebPluginContainerImpl::paintCustomOverhangArea(GraphicsContext* context, const IntRect& horizontalOverhangArea, const IntRect& verticalOverhangArea, const IntRect& dirtyRect)
{
    context->save();
    context->setFillColor(Color(0xCC, 0xCC, 0xCC));
    context->fillRect(intersection(horizontalOverhangArea, dirtyRect));
    context->fillRect(intersection(verticalOverhangArea, dirtyRect));
    context->restore();
    return true;
}

// Private methods -------------------------------------------------------------

WebPluginContainerImpl::WebPluginContainerImpl(WebCore::HTMLPlugInElement* element, WebPlugin* webPlugin)
    : m_element(element)
    , m_webPlugin(webPlugin)
    , m_webLayer(0)
    , m_touchEventRequestType(TouchEventRequestTypeNone)
    , m_wantsWheelEvents(false)
{
}

WebPluginContainerImpl::~WebPluginContainerImpl()
{
    if (m_touchEventRequestType != TouchEventRequestTypeNone)
        m_element->document().didRemoveTouchEventHandler(m_element);

    for (size_t i = 0; i < m_pluginLoadObservers.size(); ++i)
        m_pluginLoadObservers[i]->clearPluginContainer();
    m_webPlugin->destroy();
    if (m_webLayer)
        GraphicsLayer::unregisterContentsLayer(m_webLayer);
}

void WebPluginContainerImpl::handleMouseEvent(MouseEvent* event)
{
    ASSERT(parent()->isFrameView());

    if (event->isDragEvent()) {
        if (m_webPlugin->canProcessDrag())
            handleDragEvent(event);
        return;
    }

    // We cache the parent FrameView here as the plugin widget could be deleted
    // in the call to HandleEvent. See http://b/issue?id=1362948
    FrameView* parentView = toFrameView(parent());

    WebMouseEventBuilder webEvent(this, m_element->renderer(), *event);
    if (webEvent.type == WebInputEvent::Undefined)
        return;

    if (event->type() == EventTypeNames::mousedown)
        focusPlugin();

    if (m_scrollbarGroup) {
        // This needs to be set before the other callbacks in this scope, since
        // the scroll animator class might query the position in response.
        m_scrollbarGroup->setLastMousePosition(IntPoint(event->x(), event->y()));
        if (event->type() == EventTypeNames::mousemove)
            m_scrollbarGroup->scrollAnimator()->mouseMovedInContentArea();
        else if (event->type() == EventTypeNames::mouseover)
            m_scrollbarGroup->scrollAnimator()->mouseEnteredContentArea();
        else if (event->type() == EventTypeNames::mouseout)
            m_scrollbarGroup->scrollAnimator()->mouseExitedContentArea();
    }

    WebCursorInfo cursorInfo;
    if (m_webPlugin->handleInputEvent(webEvent, cursorInfo))
        event->setDefaultHandled();

    // A windowless plugin can change the cursor in response to a mouse move
    // event.  We need to reflect the changed cursor in the frame view as the
    // mouse is moved in the boundaries of the windowless plugin.
    Page* page = parentView->frame().page();
    if (!page)
        return;
    toChromeClientImpl(page->chrome().client()).setCursorForPlugin(cursorInfo);
}

void WebPluginContainerImpl::handleDragEvent(MouseEvent* event)
{
    ASSERT(event->isDragEvent());

    WebDragStatus dragStatus = WebDragStatusUnknown;
    if (event->type() == EventTypeNames::dragenter)
        dragStatus = WebDragStatusEnter;
    else if (event->type() == EventTypeNames::dragleave)
        dragStatus = WebDragStatusLeave;
    else if (event->type() == EventTypeNames::dragover)
        dragStatus = WebDragStatusOver;
    else if (event->type() == EventTypeNames::drop)
        dragStatus = WebDragStatusDrop;

    if (dragStatus == WebDragStatusUnknown)
        return;

    Clipboard* clipboard = event->dataTransfer();
    WebDragData dragData(clipboard->dataObject());
    WebDragOperationsMask dragOperationMask = static_cast<WebDragOperationsMask>(clipboard->sourceOperation());
    WebPoint dragScreenLocation(event->screenX(), event->screenY());
    WebPoint dragLocation(event->absoluteLocation().x() - location().x(), event->absoluteLocation().y() - location().y());

    m_webPlugin->handleDragStatusUpdate(dragStatus, dragData, dragOperationMask, dragLocation, dragScreenLocation);
}

void WebPluginContainerImpl::handleWheelEvent(WheelEvent* event)
{
    WebMouseWheelEventBuilder webEvent(this, m_element->renderer(), *event);
    if (webEvent.type == WebInputEvent::Undefined)
        return;

    WebCursorInfo cursorInfo;
    if (m_webPlugin->handleInputEvent(webEvent, cursorInfo))
        event->setDefaultHandled();
}

void WebPluginContainerImpl::handleKeyboardEvent(KeyboardEvent* event)
{
    WebKeyboardEventBuilder webEvent(*event);
    if (webEvent.type == WebInputEvent::Undefined)
        return;

    if (webEvent.type == WebInputEvent::KeyDown) {
#if OS(MACOSX)
        if (webEvent.modifiers == WebInputEvent::MetaKey
#else
        if (webEvent.modifiers == WebInputEvent::ControlKey
#endif
            && webEvent.windowsKeyCode == VKEY_C
            // Only copy if there's a selection, so that we only ever do this
            // for Pepper plugins that support copying.  Windowless NPAPI
            // plugins will get the event as before.
            && m_webPlugin->hasSelection()) {
            copy();
            event->setDefaultHandled();
            return;
        }
    }

    const WebInputEvent* currentInputEvent = WebViewImpl::currentInputEvent();

    // Copy stashed info over, and only copy here in order not to interfere
    // the ctrl-c logic above.
    if (currentInputEvent
        && WebInputEvent::isKeyboardEventType(currentInputEvent->type)) {
        webEvent.modifiers |= currentInputEvent->modifiers &
            (WebInputEvent::CapsLockOn | WebInputEvent::NumLockOn);
    }

    // Give the client a chance to issue edit comamnds.
    WebViewImpl* view = WebViewImpl::fromPage(m_element->document().frame()->page());
    if (m_webPlugin->supportsEditCommands() && view->client())
        view->client()->handleCurrentKeyboardEvent();

    WebCursorInfo cursorInfo;
    if (m_webPlugin->handleInputEvent(webEvent, cursorInfo))
        event->setDefaultHandled();
}

void WebPluginContainerImpl::handleTouchEvent(TouchEvent* event)
{
    switch (m_touchEventRequestType) {
    case TouchEventRequestTypeNone:
        return;
    case TouchEventRequestTypeRaw: {
        WebTouchEventBuilder webEvent(this, m_element->renderer(), *event);
        if (webEvent.type == WebInputEvent::Undefined)
            return;

        if (event->type() == EventTypeNames::touchstart)
            focusPlugin();

        WebCursorInfo cursorInfo;
        if (m_webPlugin->handleInputEvent(webEvent, cursorInfo))
            event->setDefaultHandled();
        // FIXME: Can a plugin change the cursor from a touch-event callback?
        return;
    }
    case TouchEventRequestTypeSynthesizedMouse:
        synthesizeMouseEventIfPossible(event);
        return;
    }
}

static inline bool gestureScrollHelper(ScrollbarGroup* scrollbarGroup, ScrollDirection positiveDirection, ScrollDirection negativeDirection, float delta)
{
    if (!delta)
        return false;
    float absDelta = delta > 0 ? delta : -delta;
    return scrollbarGroup->scroll(delta < 0 ? negativeDirection : positiveDirection, ScrollByPrecisePixel, absDelta);
}

void WebPluginContainerImpl::handleGestureEvent(GestureEvent* event)
{
    WebGestureEventBuilder webEvent(this, m_element->renderer(), *event);
    if (webEvent.type == WebInputEvent::Undefined)
        return;
    if (event->type() == EventTypeNames::gesturetapdown)
        focusPlugin();
    WebCursorInfo cursorInfo;
    if (m_webPlugin->handleInputEvent(webEvent, cursorInfo)) {
        event->setDefaultHandled();
        return;
    }

    if (webEvent.type == WebInputEvent::GestureScrollUpdate || webEvent.type == WebInputEvent::GestureScrollUpdateWithoutPropagation) {
        if (!m_scrollbarGroup)
            return;
        if (gestureScrollHelper(m_scrollbarGroup.get(), ScrollLeft, ScrollRight, webEvent.data.scrollUpdate.deltaX))
            event->setDefaultHandled();
        if (gestureScrollHelper(m_scrollbarGroup.get(), ScrollUp, ScrollDown, webEvent.data.scrollUpdate.deltaY))
            event->setDefaultHandled();
    }
    // FIXME: Can a plugin change the cursor from a touch-event callback?
}

void WebPluginContainerImpl::synthesizeMouseEventIfPossible(TouchEvent* event)
{
    WebMouseEventBuilder webEvent(this, m_element->renderer(), *event);
    if (webEvent.type == WebInputEvent::Undefined)
        return;

    WebCursorInfo cursorInfo;
    if (m_webPlugin->handleInputEvent(webEvent, cursorInfo))
        event->setDefaultHandled();
}

void WebPluginContainerImpl::focusPlugin()
{
    LocalFrame& containingFrame = toFrameView(parent())->frame();
    if (Page* currentPage = containingFrame.page())
        currentPage->focusController().setFocusedElement(m_element, &containingFrame);
    else
        containingFrame.document()->setFocusedElement(m_element);
}

void WebPluginContainerImpl::calculateGeometry(const IntRect& frameRect,
                                               IntRect& windowRect,
                                               IntRect& clipRect,
                                               Vector<IntRect>& cutOutRects)
{
    windowRect = toScrollView(parent())->contentsToWindow(frameRect);

    // Calculate a clip-rect so that we don't overlap the scrollbars, etc.
    clipRect = windowClipRect();
    clipRect.move(-windowRect.x(), -windowRect.y());

    getPluginOcclusions(m_element, this->parent(), frameRect, cutOutRects);
    // Convert to the plugin position.
    for (size_t i = 0; i < cutOutRects.size(); i++)
        cutOutRects[i].move(-frameRect.x(), -frameRect.y());
}

WebCore::IntRect WebPluginContainerImpl::windowClipRect() const
{
    // Start by clipping to our bounds.
    IntRect clipRect =
        convertToContainingWindow(IntRect(0, 0, width(), height()));

    // document().renderView() can be 0 when we receive messages from the
    // plugins while we are destroying a frame.
    // FIXME: Can we just check m_element->document().isActive() ?
    if (m_element->renderer()->document().renderView()) {
        // Take our element and get the clip rect from the enclosing layer and
        // frame view.
        clipRect.intersect(
            m_element->document().view()->windowClipRectForFrameOwner(m_element));
    }

    return clipRect;
}

bool WebPluginContainerImpl::pluginShouldPersist() const
{
    return m_webPlugin->shouldPersist();
}

} // namespace blink
