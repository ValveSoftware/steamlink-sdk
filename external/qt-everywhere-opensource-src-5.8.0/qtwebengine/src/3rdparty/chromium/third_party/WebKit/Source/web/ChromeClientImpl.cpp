/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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

#include "web/ChromeClientImpl.h"

#include "bindings/core/v8/ScriptController.h"
#include "core/HTMLNames.h"
#include "core/dom/AXObjectCache.h"
#include "core/dom/Document.h"
#include "core/dom/Fullscreen.h"
#include "core/dom/Node.h"
#include "core/events/UIEventWithKeyState.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/frame/VisualViewport.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/forms/ColorChooser.h"
#include "core/html/forms/ColorChooserClient.h"
#include "core/html/forms/DateTimeChooser.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/compositing/CompositedSelection.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/page/Page.h"
#include "core/page/PopupOpeningObserver.h"
#include "modules/accessibility/AXObject.h"
#include "platform/Cursor.h"
#include "platform/FileChooser.h"
#include "platform/Histogram.h"
#include "platform/KeyboardCodes.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/exported/WrappedResourceRequest.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebCursorInfo.h"
#include "public/platform/WebFloatRect.h"
#include "public/platform/WebFrameScheduler.h"
#include "public/platform/WebRect.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/WebViewScheduler.h"
#include "public/web/WebAXObject.h"
#include "public/web/WebAutofillClient.h"
#include "public/web/WebColorChooser.h"
#include "public/web/WebColorSuggestion.h"
#include "public/web/WebConsoleMessage.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebInputElement.h"
#include "public/web/WebInputEvent.h"
#include "public/web/WebKit.h"
#include "public/web/WebNode.h"
#include "public/web/WebPageImportanceSignals.h"
#include "public/web/WebPlugin.h"
#include "public/web/WebPopupMenuInfo.h"
#include "public/web/WebSelection.h"
#include "public/web/WebSettings.h"
#include "public/web/WebTextDirection.h"
#include "public/web/WebTouchAction.h"
#include "public/web/WebUserGestureIndicator.h"
#include "public/web/WebUserGestureToken.h"
#include "public/web/WebViewClient.h"
#include "public/web/WebWindowFeatures.h"
#include "web/ColorChooserPopupUIController.h"
#include "web/ColorChooserUIController.h"
#include "web/DateTimeChooserImpl.h"
#include "web/ExternalDateTimeChooser.h"
#include "web/ExternalPopupMenu.h"
#include "web/PopupMenuImpl.h"
#include "web/WebFileChooserCompletionImpl.h"
#include "web/WebFrameWidgetImpl.h"
#include "web/WebInputEventConversion.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebPluginContainerImpl.h"
#include "web/WebSettingsImpl.h"
#include "web/WebViewImpl.h"
#include "wtf/PtrUtil.h"
#include "wtf/text/CString.h"
#include "wtf/text/CharacterNames.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/StringConcatenate.h"
#include <memory>

namespace blink {

namespace {

const char* dialogTypeToString(ChromeClient::DialogType dialogType)
{
    switch (dialogType) {
    case ChromeClient::AlertDialog:
        return "alert";
    case ChromeClient::ConfirmDialog:
        return "confirm";
    case ChromeClient::PromptDialog:
        return "prompt";
    case ChromeClient::HTMLDialog:
        NOTREACHED();
    }
    NOTREACHED();
    return "";
}

const char* dismissalTypeToString(Document::PageDismissalType dismissalType)
{
    switch (dismissalType) {
    case Document::BeforeUnloadDismissal:
        return "beforeunload";
    case Document::PageHideDismissal:
        return "pagehide";
    case Document::UnloadVisibilityChangeDismissal:
        return "visibilitychange";
    case Document::UnloadDismissal:
        return "unload";
    case Document::NoDismissal:
        NOTREACHED();
    }
    NOTREACHED();
    return "";
}

} // namespace

class CompositorAnimationTimeline;

// Converts a AXObjectCache::AXNotification to a WebAXEvent
static WebAXEvent toWebAXEvent(AXObjectCache::AXNotification notification)
{
    // These enums have the same values; enforced in AssertMatchingEnums.cpp.
    return static_cast<WebAXEvent>(notification);
}

ChromeClientImpl::ChromeClientImpl(WebViewImpl* webView)
    : m_webView(webView)
    , m_cursorOverridden(false)
    , m_didRequestNonEmptyToolTip(false)
{
}

ChromeClientImpl::~ChromeClientImpl()
{
}

ChromeClientImpl* ChromeClientImpl::create(WebViewImpl* webView)
{
    return new ChromeClientImpl(webView);
}

void* ChromeClientImpl::webView() const
{
    return static_cast<void*>(m_webView);
}

void ChromeClientImpl::chromeDestroyed()
{
    // Our lifetime is bound to the WebViewImpl.
}

void ChromeClientImpl::setWindowRect(const IntRect& r)
{
    if (m_webView->client())
        m_webView->client()->setWindowRect(r);
}

IntRect ChromeClientImpl::windowRect()
{
    WebRect rect;
    if (m_webView->client()) {
        rect = m_webView->client()->rootWindowRect();
    } else {
        // These numbers will be fairly wrong. The window's x/y coordinates will
        // be the top left corner of the screen and the size will be the content
        // size instead of the window size.
        rect.width = m_webView->size().width;
        rect.height = m_webView->size().height;
    }
    return IntRect(rect);
}

IntRect ChromeClientImpl::pageRect()
{
    // We hide the details of the window's border thickness from the web page by
    // simple re-using the window position here.  So, from the point-of-view of
    // the web page, the window has no border.
    return windowRect();
}

void ChromeClientImpl::focus()
{
    if (m_webView->client())
        m_webView->client()->didFocus();
}

bool ChromeClientImpl::canTakeFocus(WebFocusType)
{
    // For now the browser can always take focus if we're not running layout
    // tests.
    return !layoutTestMode();
}

void ChromeClientImpl::takeFocus(WebFocusType type)
{
    if (!m_webView->client())
        return;
    if (type == WebFocusTypeBackward)
        m_webView->client()->focusPrevious();
    else
        m_webView->client()->focusNext();
}

void ChromeClientImpl::focusedNodeChanged(Node* fromNode, Node* toNode)
{
    if (!m_webView->client())
        return;

    m_webView->client()->focusedNodeChanged(WebNode(fromNode), WebNode(toNode));

    WebURL focusURL;
    if (toNode && toNode->isElementNode() && toElement(toNode)->isLiveLink() && toNode->shouldHaveFocusAppearance())
        focusURL = toElement(toNode)->hrefURL();
    m_webView->client()->setKeyboardFocusURL(focusURL);
}

bool ChromeClientImpl::hadFormInteraction() const
{
    return m_webView->pageImportanceSignals() && m_webView->pageImportanceSignals()->hadFormInteraction();
}

void ChromeClientImpl::startDragging(LocalFrame* frame,
    const WebDragData& dragData,
    WebDragOperationsMask mask,
    const WebImage& dragImage,
    const WebPoint& dragImageOffset)
{
    m_webView->startDragging(frame, dragData, mask, dragImage, dragImageOffset);
}

bool ChromeClientImpl::acceptsLoadDrops() const
{
    return !m_webView->client() || m_webView->client()->acceptsLoadDrops();
}

namespace {

void updatePolicyForEvent(const WebInputEvent* inputEvent, NavigationPolicy* policy)
{
    if (!inputEvent)
        return;

    unsigned short buttonNumber = 0;
    if (inputEvent->type == WebInputEvent::MouseUp) {

        const WebMouseEvent* mouseEvent = static_cast<const WebMouseEvent*>(inputEvent);

        switch (mouseEvent->button) {
        case WebMouseEvent::ButtonLeft:
            buttonNumber = 0;
            break;
        case WebMouseEvent::ButtonMiddle:
            buttonNumber = 1;
            break;
        case WebMouseEvent::ButtonRight:
            buttonNumber = 2;
            break;
        default:
            return;
        }
    } else if ((WebInputEvent::isKeyboardEventType(inputEvent->type) && static_cast<const WebKeyboardEvent*>(inputEvent)->windowsKeyCode == VKEY_RETURN) || WebInputEvent::isGestureEventType(inputEvent->type)) {
        // Keyboard and gesture events can simulate mouse events.
        buttonNumber = 0;
    } else {
        return;
    }

    bool ctrl = inputEvent->modifiers & WebInputEvent::ControlKey;
    bool shift = inputEvent->modifiers & WebInputEvent::ShiftKey;
    bool alt = inputEvent->modifiers & WebInputEvent::AltKey;
    bool meta = inputEvent->modifiers & WebInputEvent::MetaKey;

    NavigationPolicy userPolicy = *policy;
    navigationPolicyFromMouseEvent(buttonNumber, ctrl, shift, alt, meta, &userPolicy);

    // When the input event suggests a download, but the navigation was initiated
    // by script, we should not override it.
    if (userPolicy == NavigationPolicyDownload && *policy != NavigationPolicyIgnore)
        return;

    // User and app agree that we want a new window; let the app override the decorations.
    if (userPolicy == NavigationPolicyNewWindow && *policy == NavigationPolicyNewPopup)
        return;
    *policy = userPolicy;
}

WebNavigationPolicy getNavigationPolicy(const WindowFeatures& features)
{
    // If our default configuration was modified by a script or wasn't
    // created by a user gesture, then show as a popup. Else, let this
    // new window be opened as a toplevel window.
    bool asPopup = !features.toolBarVisible
        || !features.statusBarVisible
        || !features.scrollbarsVisible
        || !features.menuBarVisible
        || !features.resizable;

    NavigationPolicy policy = NavigationPolicyNewForegroundTab;
    if (asPopup)
        policy = NavigationPolicyNewPopup;
    updatePolicyForEvent(WebViewImpl::currentInputEvent(), &policy);

    return static_cast<WebNavigationPolicy>(policy);
}

WebNavigationPolicy effectiveNavigationPolicy(NavigationPolicy navigationPolicy, const WindowFeatures& features)
{
    WebNavigationPolicy policy = static_cast<WebNavigationPolicy>(navigationPolicy);
    if (policy == WebNavigationPolicyIgnore)
        return getNavigationPolicy(features);
    if (policy == WebNavigationPolicyNewBackgroundTab && getNavigationPolicy(features) != WebNavigationPolicyNewBackgroundTab && !UIEventWithKeyState::newTabModifierSetFromIsolatedWorld())
        return WebNavigationPolicyNewForegroundTab;

    return policy;
}

} // namespace

Page* ChromeClientImpl::createWindow(LocalFrame* frame, const FrameLoadRequest& r, const WindowFeatures& features,
    NavigationPolicy navigationPolicy)
{
    if (!m_webView->client())
        return nullptr;

    if (!frame->page() || frame->page()->defersLoading())
        return nullptr;

    WebNavigationPolicy policy = effectiveNavigationPolicy(navigationPolicy, features);
    DCHECK(frame->document());
    Fullscreen::fullyExitFullscreen(*frame->document());

    WebViewImpl* newView = toWebViewImpl(
        m_webView->client()->createView(WebLocalFrameImpl::fromFrame(frame), WrappedResourceRequest(r.resourceRequest()), features, r.frameName(), policy, r.getShouldSetOpener() == NeverSetOpener || features.noopener));
    if (!newView)
        return nullptr;
    return newView->page();
}

void ChromeClientImpl::didOverscroll(const FloatSize& overscrollDelta, const FloatSize& accumulatedOverscroll, const FloatPoint& positionInViewport, const FloatSize& velocityInViewport)
{
    if (!m_webView->client())
        return;

    m_webView->client()->didOverscroll(overscrollDelta, accumulatedOverscroll, positionInViewport, velocityInViewport);
}

void ChromeClientImpl::show(NavigationPolicy navigationPolicy)
{
    if (m_webView->client())
        m_webView->client()->show(effectiveNavigationPolicy(navigationPolicy, m_windowFeatures));
}

void ChromeClientImpl::setToolbarsVisible(bool value)
{
    m_windowFeatures.toolBarVisible = value;
}

bool ChromeClientImpl::toolbarsVisible()
{
    return m_windowFeatures.toolBarVisible;
}

void ChromeClientImpl::setStatusbarVisible(bool value)
{
    m_windowFeatures.statusBarVisible = value;
}

bool ChromeClientImpl::statusbarVisible()
{
    return m_windowFeatures.statusBarVisible;
}

void ChromeClientImpl::setScrollbarsVisible(bool value)
{
    m_windowFeatures.scrollbarsVisible = value;
    if (WebLocalFrameImpl* webFrame = toWebLocalFrameImpl(m_webView->mainFrame()))
        webFrame->setCanHaveScrollbars(value);
}

bool ChromeClientImpl::scrollbarsVisible()
{
    return m_windowFeatures.scrollbarsVisible;
}

void ChromeClientImpl::setMenubarVisible(bool value)
{
    m_windowFeatures.menuBarVisible = value;
}

bool ChromeClientImpl::menubarVisible()
{
    return m_windowFeatures.menuBarVisible;
}

void ChromeClientImpl::setResizable(bool value)
{
    m_windowFeatures.resizable = value;
}

bool ChromeClientImpl::shouldReportDetailedMessageForSource(LocalFrame& localFrame, const String& url)
{
    WebLocalFrameImpl* webframe = WebLocalFrameImpl::fromFrame(localFrame.localFrameRoot());
    return webframe && webframe->client() && webframe->client()->shouldReportDetailedMessageForSource(url);
}

void ChromeClientImpl::addMessageToConsole(LocalFrame* localFrame, MessageSource source, MessageLevel level, const String& message, unsigned lineNumber, const String& sourceID, const String& stackTrace)
{
    WebLocalFrameImpl* frame = WebLocalFrameImpl::fromFrame(localFrame);
    if (frame && frame->client()) {
        frame->client()->didAddMessageToConsole(
            WebConsoleMessage(static_cast<WebConsoleMessage::Level>(level), message),
            sourceID,
            lineNumber,
            stackTrace);
    }
}

bool ChromeClientImpl::canOpenBeforeUnloadConfirmPanel()
{
    return !!m_webView->client();
}

bool ChromeClientImpl::openBeforeUnloadConfirmPanelDelegate(LocalFrame* frame, bool isReload)
{
    notifyPopupOpeningObservers();
    WebLocalFrameImpl* webframe = WebLocalFrameImpl::fromFrame(frame);
    return webframe->client() && webframe->client()->runModalBeforeUnloadDialog(isReload);
}

void ChromeClientImpl::closeWindowSoon()
{
    // Make sure this Page can no longer be found by JS.
    m_webView->page()->willBeClosed();

    // Make sure that all loading is stopped.  Ensures that JS stops executing!
    m_webView->mainFrame()->stopLoading();

    if (m_webView->client())
        m_webView->client()->closeWidgetSoon();
}

// Although a LocalFrame is passed in, we don't actually use it, since we
// already know our own m_webView.
bool ChromeClientImpl::openJavaScriptAlertDelegate(LocalFrame* frame, const String& message)
{
    notifyPopupOpeningObservers();
    WebLocalFrameImpl* webframe = WebLocalFrameImpl::fromFrame(frame);
    if (webframe->client()) {
        if (WebUserGestureIndicator::isProcessingUserGesture())
            WebUserGestureIndicator::currentUserGestureToken().setJavascriptPrompt();
        webframe->client()->runModalAlertDialog(message);
        return true;
    }
    return false;
}

// See comments for openJavaScriptAlertDelegate().
bool ChromeClientImpl::openJavaScriptConfirmDelegate(LocalFrame* frame, const String& message)
{
    notifyPopupOpeningObservers();
    WebLocalFrameImpl* webframe = WebLocalFrameImpl::fromFrame(frame);
    if (webframe->client()) {
        if (WebUserGestureIndicator::isProcessingUserGesture())
            WebUserGestureIndicator::currentUserGestureToken().setJavascriptPrompt();
        return webframe->client()->runModalConfirmDialog(message);
    }
    return false;
}

// See comments for openJavaScriptAlertDelegate().
bool ChromeClientImpl::openJavaScriptPromptDelegate(LocalFrame* frame, const String& message, const String& defaultValue, String& result)
{
    notifyPopupOpeningObservers();
    WebLocalFrameImpl* webframe = WebLocalFrameImpl::fromFrame(frame);
    if (webframe->client()) {
        if (WebUserGestureIndicator::isProcessingUserGesture())
            WebUserGestureIndicator::currentUserGestureToken().setJavascriptPrompt();
        WebString actualValue;
        bool ok = webframe->client()->runModalPromptDialog(message, defaultValue, &actualValue);
        if (ok)
            result = actualValue;
        return ok;
    }
    return false;
}

void ChromeClientImpl::setStatusbarText(const String& message)
{
    if (m_webView->client())
        m_webView->client()->setStatusText(message);
}

bool ChromeClientImpl::tabsToLinks()
{
    return m_webView->tabsToLinks();
}

IntRect ChromeClientImpl::windowResizerRect() const
{
    if (m_webView->client())
        return m_webView->client()->windowResizerRect();
    return IntRect();
}

void ChromeClientImpl::invalidateRect(const IntRect& updateRect)
{
    if (!updateRect.isEmpty())
        m_webView->invalidateRect(updateRect);
}

void ChromeClientImpl::scheduleAnimation(Widget* widget)
{
    DCHECK(widget->isFrameView());
    FrameView* view = toFrameView(widget);
    LocalFrame* frame = view->frame().localFrameRoot();

    // If the frame is still being created, it might not yet have a WebWidget.
    // FIXME: Is this the right thing to do? Is there a way to avoid having
    // a local frame root that doesn't have a WebWidget? During initialization
    // there is no content to draw so this call serves no purpose.
    if (WebLocalFrameImpl::fromFrame(frame) && WebLocalFrameImpl::fromFrame(frame)->frameWidget())
        WebLocalFrameImpl::fromFrame(frame)->frameWidget()->scheduleAnimation();
}

IntRect ChromeClientImpl::viewportToScreen(const IntRect& rectInViewport, const Widget* widget) const
{
    WebRect screenRect(rectInViewport);

    DCHECK(widget->isFrameView());
    const FrameView* view = toFrameView(widget);
    LocalFrame* frame = view->frame().localFrameRoot();

    WebWidgetClient* client = WebLocalFrameImpl::fromFrame(frame)->frameWidget()->client();

    if (client) {
        client->convertViewportToWindow(&screenRect);
        WebRect windowRect = client->windowRect();
        screenRect.x += windowRect.x;
        screenRect.y += windowRect.y;
    }

    return screenRect;
}

float ChromeClientImpl::windowToViewportScalar(const float scalarValue) const
{
    if (!m_webView->client())
        return scalarValue;
    WebFloatRect viewportRect(0, 0, scalarValue, 0);
    m_webView->client()->convertWindowToViewport(&viewportRect);
    return viewportRect.width;
}

WebScreenInfo ChromeClientImpl::screenInfo() const
{
    return m_webView->client() ? m_webView->client()->screenInfo() : WebScreenInfo();
}

void ChromeClientImpl::contentsSizeChanged(LocalFrame* frame, const IntSize& size) const
{
    m_webView->didChangeContentsSize();

    WebLocalFrameImpl* webframe = WebLocalFrameImpl::fromFrame(frame);
    webframe->didChangeContentsSize(size);
}

void ChromeClientImpl::pageScaleFactorChanged() const
{
    m_webView->pageScaleFactorChanged();
}

float ChromeClientImpl::clampPageScaleFactorToLimits(float scale) const
{
    return m_webView->clampPageScaleFactorToLimits(scale);
}

void ChromeClientImpl::layoutUpdated(LocalFrame* frame) const
{
    m_webView->layoutUpdated(WebLocalFrameImpl::fromFrame(frame));
}

void ChromeClientImpl::showMouseOverURL(const HitTestResult& result)
{
    if (!m_webView->client())
        return;

    WebURL url;
    // Find out if the mouse is over a link, and if so, let our UI know...
    if (result.isLiveLink() && !result.absoluteLinkURL().getString().isEmpty()) {
        url = result.absoluteLinkURL();
    } else if (result.innerNode()
        && (isHTMLObjectElement(*result.innerNode())
            || isHTMLEmbedElement(*result.innerNode()))) {
        LayoutObject* object = result.innerNode()->layoutObject();
        if (object && object->isLayoutPart()) {
            Widget* widget = toLayoutPart(object)->widget();
            if (widget && widget->isPluginContainer()) {
                WebPluginContainerImpl* plugin = toWebPluginContainerImpl(widget);
                url = plugin->plugin()->linkAtPosition(result.roundedPointInInnerNodeFrame());
            }
        }
    }

    m_webView->client()->setMouseOverURL(url);
}

void ChromeClientImpl::setToolTip(const String& tooltipText, TextDirection dir)
{
    if (!m_webView->client())
        return;
    if (!tooltipText.isEmpty()) {
        m_webView->client()->setToolTipText(tooltipText, toWebTextDirection(dir));
        m_didRequestNonEmptyToolTip = true;
    } else if (m_didRequestNonEmptyToolTip) {
        // WebViewClient::setToolTipText will send an IPC message.  We'd like to
        // reduce the number of setToolTipText calls.
        m_webView->client()->setToolTipText(tooltipText, toWebTextDirection(dir));
        m_didRequestNonEmptyToolTip = false;
    }
}

void ChromeClientImpl::dispatchViewportPropertiesDidChange(const ViewportDescription& description) const
{
    m_webView->updatePageDefinedViewportConstraints(description);
}

void ChromeClientImpl::printDelegate(LocalFrame* frame)
{
    if (m_webView->client())
        m_webView->client()->printPage(WebLocalFrameImpl::fromFrame(frame));
}

ColorChooser* ChromeClientImpl::openColorChooser(LocalFrame* frame, ColorChooserClient* chooserClient, const Color&)
{
    notifyPopupOpeningObservers();
    ColorChooserUIController* controller = nullptr;
    if (RuntimeEnabledFeatures::pagePopupEnabled())
        controller = ColorChooserPopupUIController::create(frame, this, chooserClient);
    else
        controller = ColorChooserUIController::create(frame, chooserClient);
    controller->openUI();
    return controller;
}

DateTimeChooser* ChromeClientImpl::openDateTimeChooser(DateTimeChooserClient* pickerClient, const DateTimeChooserParameters& parameters)
{
    notifyPopupOpeningObservers();
    if (RuntimeEnabledFeatures::inputMultipleFieldsUIEnabled())
        return DateTimeChooserImpl::create(this, pickerClient, parameters);
    return ExternalDateTimeChooser::create(this, m_webView->client(), pickerClient, parameters);
}

void ChromeClientImpl::openFileChooser(LocalFrame* frame, PassRefPtr<FileChooser> fileChooser)
{
    notifyPopupOpeningObservers();
    WebFrameClient* client = WebLocalFrameImpl::fromFrame(frame)->client();
    if (!client)
        return;

    WebFileChooserParams params;
    params.multiSelect = fileChooser->settings().allowsMultipleFiles;
    params.directory = fileChooser->settings().allowsDirectoryUpload;
    params.acceptTypes = fileChooser->settings().acceptTypes();
    params.selectedFiles = fileChooser->settings().selectedFiles;
    if (params.selectedFiles.size() > 0)
        params.initialValue = params.selectedFiles[0];
    params.useMediaCapture = fileChooser->settings().useMediaCapture;
    params.needLocalPath = fileChooser->settings().allowsDirectoryUpload;
    params.requestor = frame->document()->url();

    WebFileChooserCompletionImpl* chooserCompletion = new WebFileChooserCompletionImpl(fileChooser);
    if (client->runFileChooser(params, chooserCompletion))
        return;
    // Choosing failed, so do callback with an empty list.
    chooserCompletion->didChooseFile(WebVector<WebString>());
}

void ChromeClientImpl::enumerateChosenDirectory(FileChooser* fileChooser)
{
    WebViewClient* client = m_webView->client();
    if (!client)
        return;

    WebFileChooserCompletionImpl* chooserCompletion = new WebFileChooserCompletionImpl(fileChooser);

    DCHECK(fileChooser);
    DCHECK(fileChooser->settings().selectedFiles.size());

    // If the enumeration can't happen, call the callback with an empty list.
    if (!client->enumerateChosenDirectory(fileChooser->settings().selectedFiles[0], chooserCompletion))
        chooserCompletion->didChooseFile(WebVector<WebString>());
}

Cursor ChromeClientImpl::lastSetCursorForTesting() const
{
    return m_lastSetMouseCursorForTesting;
}

void ChromeClientImpl::setCursor(const Cursor& cursor, LocalFrame* localRoot)
{
    m_lastSetMouseCursorForTesting = cursor;
    setCursor(WebCursorInfo(cursor), localRoot);
}

void ChromeClientImpl::setCursor(const WebCursorInfo& cursor, LocalFrame* localRoot)
{
    if (m_cursorOverridden)
        return;

#if OS(MACOSX)
    // On Mac the mousemove event propagates to both the popup and main window.
    // If a popup is open we don't want the main window to change the cursor.
    if (m_webView->hasOpenedPopup())
        return;
#endif
    if (!m_webView->client())
        return;
    // TODO(kenrb, dcheng): For top-level frames we still use the WebView as
    // a WebWidget. This special case will be removed when top-level frames
    // get WebFrameWidgets.
    if (localRoot->isMainFrame()) {
        m_webView->client()->didChangeCursor(cursor);
    } else {
        WebLocalFrameImpl* webFrame = WebLocalFrameImpl::fromFrame(localRoot);
        DCHECK(webFrame);
        DCHECK(webFrame->frameWidget());
        if (toWebFrameWidgetImpl(webFrame->frameWidget())->client())
            toWebFrameWidgetImpl(webFrame->frameWidget())->client()->didChangeCursor(cursor);
    }
}

void ChromeClientImpl::setCursorForPlugin(const WebCursorInfo& cursor, LocalFrame* localRoot)
{
    setCursor(cursor, localRoot);
}

void ChromeClientImpl::setCursorOverridden(bool overridden)
{
    m_cursorOverridden = overridden;
}

void ChromeClientImpl::postAccessibilityNotification(AXObject* obj, AXObjectCache::AXNotification notification)
{
    // Alert assistive technology about the accessibility object notification.
    if (!obj || !obj->getDocument())
        return;

    WebLocalFrameImpl* webframe = WebLocalFrameImpl::fromFrame(obj->getDocument()->axObjectCacheOwner().frame());
    if (webframe && webframe->client())
        webframe->client()->postAccessibilityEvent(WebAXObject(obj), toWebAXEvent(notification));
}

String ChromeClientImpl::acceptLanguages()
{
    return m_webView->client()->acceptLanguages();
}

void ChromeClientImpl::attachRootGraphicsLayer(GraphicsLayer* rootLayer, LocalFrame* localRoot)
{
    // FIXME: For top-level frames we still use the WebView as a WebWidget. This
    // special case will be removed when top-level frames get WebFrameWidgets.
    if (localRoot->isMainFrame()) {
        m_webView->setRootGraphicsLayer(rootLayer);
    } else {
        WebLocalFrameImpl* webFrame = WebLocalFrameImpl::fromFrame(localRoot);
        // FIXME: The following conditional is only needed for staging until the
        // Chromium patch lands that instantiates a WebFrameWidget.
        if (!webFrame->frameWidget()) {
            m_webView->setRootGraphicsLayer(rootLayer);
            return;
        }
        DCHECK(webFrame);
        DCHECK(webFrame->frameWidget());
        toWebFrameWidgetImpl(webFrame->frameWidget())->setRootGraphicsLayer(rootLayer);
    }
}

void ChromeClientImpl::didPaint(const PaintArtifact& paintArtifact)
{
    // TODO(jbroman): This doesn't handle OOPIF correctly. We probably need a
    // branch for WebFrameWidget, like attachRootGraphicsLayer.
    m_webView->getPaintArtifactCompositor().update(paintArtifact);
}

void ChromeClientImpl::attachCompositorAnimationTimeline(CompositorAnimationTimeline* compositorTimeline, LocalFrame* localRoot)
{
    // FIXME: For top-level frames we still use the WebView as a WebWidget. This
    // special case will be removed when top-level frames get WebFrameWidgets.
    if (localRoot->isMainFrame()) {
        m_webView->attachCompositorAnimationTimeline(compositorTimeline);
    } else {
        WebLocalFrameImpl* webFrame = WebLocalFrameImpl::fromFrame(localRoot);
        // FIXME: The following conditional is only needed for staging until the
        // Chromium patch lands that instantiates a WebFrameWidget.
        if (!webFrame->frameWidget()) {
            m_webView->attachCompositorAnimationTimeline(compositorTimeline);
            return;
        }
        DCHECK(webFrame);
        DCHECK(webFrame->frameWidget());
        toWebFrameWidgetImpl(webFrame->frameWidget())->attachCompositorAnimationTimeline(compositorTimeline);
    }
}

void ChromeClientImpl::detachCompositorAnimationTimeline(CompositorAnimationTimeline* compositorTimeline, LocalFrame* localRoot)
{
    // FIXME: For top-level frames we still use the WebView as a WebWidget. This
    // special case will be removed when top-level frames get WebFrameWidgets.
    if (localRoot->isMainFrame()) {
        m_webView->detachCompositorAnimationTimeline(compositorTimeline);
    } else {
        WebLocalFrameImpl* webFrame = WebLocalFrameImpl::fromFrame(localRoot);
        // FIXME: The following conditional is only needed for staging until the
        // Chromium patch lands that instantiates a WebFrameWidget.
        if (!webFrame->frameWidget()) {
            m_webView->detachCompositorAnimationTimeline(compositorTimeline);
            return;
        }
        DCHECK(webFrame);
        DCHECK(webFrame->frameWidget());
        toWebFrameWidgetImpl(webFrame->frameWidget())->detachCompositorAnimationTimeline(compositorTimeline);
    }
}

void ChromeClientImpl::enterFullScreenForElement(Element* element)
{
    m_webView->enterFullScreenForElement(element);
}

void ChromeClientImpl::exitFullScreenForElement(Element* element)
{
    m_webView->exitFullScreenForElement(element);
}

void ChromeClientImpl::clearCompositedSelection()
{
    m_webView->clearCompositedSelection();
}

void ChromeClientImpl::updateCompositedSelection(const CompositedSelection& selection)
{
    m_webView->updateCompositedSelection(WebSelection(selection));
}

bool ChromeClientImpl::hasOpenedPopup() const
{
    return m_webView->hasOpenedPopup();
}

PopupMenu* ChromeClientImpl::openPopupMenu(LocalFrame& frame, HTMLSelectElement& select)
{
    notifyPopupOpeningObservers();
    if (WebViewImpl::useExternalPopupMenus())
        return new ExternalPopupMenu(frame, select, *m_webView);

    DCHECK(RuntimeEnabledFeatures::pagePopupEnabled());
    return PopupMenuImpl::create(this, select);
}

PagePopup* ChromeClientImpl::openPagePopup(PagePopupClient* client)
{
    return m_webView->openPagePopup(client);
}

void ChromeClientImpl::closePagePopup(PagePopup* popup)
{
    m_webView->closePagePopup(popup);
}

DOMWindow* ChromeClientImpl::pagePopupWindowForTesting() const
{
    return m_webView->pagePopupWindow();
}

bool ChromeClientImpl::shouldOpenModalDialogDuringPageDismissal(const DialogType& dialogType, const String& dialogMessage, Document::PageDismissalType dismissalType) const
{
    String message = String("Blocked ") + dialogTypeToString(dialogType) + "('" + dialogMessage + "') during " + dismissalTypeToString(dismissalType) + ".";
    m_webView->mainFrame()->addMessageToConsole(WebConsoleMessage(WebConsoleMessage::LevelError, message));

    return false;
}

void ChromeClientImpl::setEventListenerProperties(WebEventListenerClass eventClass, WebEventListenerProperties properties)
{
    if (WebLayerTreeView* treeView = m_webView->layerTreeView()) {
        treeView->setEventListenerProperties(eventClass, properties);
        if (eventClass == WebEventListenerClass::TouchStartOrMove) {
            m_webView->hasTouchEventHandlers(properties != WebEventListenerProperties::Nothing || eventListenerProperties(WebEventListenerClass::TouchEndOrCancel) != WebEventListenerProperties::Nothing);
        } else if (eventClass == WebEventListenerClass::TouchEndOrCancel) {
            m_webView->hasTouchEventHandlers(properties != WebEventListenerProperties::Nothing || eventListenerProperties(WebEventListenerClass::TouchStartOrMove) != WebEventListenerProperties::Nothing);
        }
    } else {
        m_webView->hasTouchEventHandlers(true);
    }
}

void ChromeClientImpl::beginLifecycleUpdates()
{
    if (WebLayerTreeView* treeView = m_webView->layerTreeView()) {
        treeView->setDeferCommits(false);
        treeView->setNeedsBeginFrame();
    }
}

WebEventListenerProperties ChromeClientImpl::eventListenerProperties(WebEventListenerClass eventClass) const
{
    if (WebLayerTreeView* treeView = m_webView->layerTreeView())
        return treeView->eventListenerProperties(eventClass);
    return WebEventListenerProperties::Nothing;
}

void ChromeClientImpl::setHasScrollEventHandlers(bool hasEventHandlers)
{
    if (WebLayerTreeView* treeView = m_webView->layerTreeView())
        treeView->setHaveScrollEventHandlers(hasEventHandlers);
}

bool ChromeClientImpl::hasScrollEventHandlers() const
{
    if (WebLayerTreeView* treeView = m_webView->layerTreeView())
        return treeView->haveScrollEventHandlers();
    return false;
}

void ChromeClientImpl::setTouchAction(TouchAction touchAction)
{
    if (WebViewClient* client = m_webView->client())
        client->setTouchAction(static_cast<WebTouchAction>(touchAction));
}

bool ChromeClientImpl::requestPointerLock(LocalFrame* frame)
{
    LocalFrame* localRoot = frame->localFrameRoot();
    return WebLocalFrameImpl::fromFrame(localRoot)->frameWidget()->client()->requestPointerLock();
}

void ChromeClientImpl::requestPointerUnlock(LocalFrame* frame)
{
    LocalFrame* localRoot = frame->localFrameRoot();
    return WebLocalFrameImpl::fromFrame(localRoot)->frameWidget()->client()->requestPointerUnlock();
}

void ChromeClientImpl::annotatedRegionsChanged()
{
    if (WebViewClient* client = m_webView->client())
        client->draggableRegionsChanged();
}

void ChromeClientImpl::didAssociateFormControls(const HeapVector<Member<Element>>& elements, LocalFrame* frame)
{
    WebLocalFrameImpl* webframe = WebLocalFrameImpl::fromFrame(frame);
    if (webframe->autofillClient())
        webframe->autofillClient()->didAssociateFormControls(elements);
}

void ChromeClientImpl::didCancelCompositionOnSelectionChange()
{
    if (m_webView->client())
        m_webView->client()->didCancelCompositionOnSelectionChange();
}

void ChromeClientImpl::willSetInputMethodState()
{
    if (m_webView->client())
        m_webView->client()->resetInputMethod();
}

void ChromeClientImpl::didUpdateTextOfFocusedElementByNonUserInput()
{
    if (m_webView->client())
        m_webView->client()->didUpdateTextOfFocusedElementByNonUserInput();
}

void ChromeClientImpl::showImeIfNeeded()
{
    if (m_webView->client())
        m_webView->client()->showImeIfNeeded();
}

void ChromeClientImpl::showUnhandledTapUIIfNeeded(IntPoint tappedPositionInViewport, Node* tappedNode, bool pageChanged)
{
    if (m_webView->client())
        m_webView->client()->showUnhandledTapUIIfNeeded(WebPoint(tappedPositionInViewport), WebNode(tappedNode), pageChanged);
}

void ChromeClientImpl::onMouseDown(Node* mouseDownNode)
{
    if (m_webView->client())
        m_webView->client()->onMouseDown(WebNode(mouseDownNode));
}

void ChromeClientImpl::handleKeyboardEventOnTextField(HTMLInputElement& inputElement, KeyboardEvent& event)
{
    WebLocalFrameImpl* webframe = WebLocalFrameImpl::fromFrame(inputElement.document().frame());
    if (webframe->autofillClient())
        webframe->autofillClient()->textFieldDidReceiveKeyDown(WebInputElement(&inputElement), WebKeyboardEventBuilder(event));
}

void ChromeClientImpl::didChangeValueInTextField(HTMLFormControlElement& element)
{
    WebLocalFrameImpl* webframe = WebLocalFrameImpl::fromFrame(element.document().frame());
    if (webframe->autofillClient())
        webframe->autofillClient()->textFieldDidChange(WebFormControlElement(&element));

    m_webView->pageImportanceSignals()->setHadFormInteraction();
}

void ChromeClientImpl::didEndEditingOnTextField(HTMLInputElement& inputElement)
{
    WebLocalFrameImpl* webframe = WebLocalFrameImpl::fromFrame(inputElement.document().frame());
    if (webframe->autofillClient())
        webframe->autofillClient()->textFieldDidEndEditing(WebInputElement(&inputElement));
}

void ChromeClientImpl::openTextDataListChooser(HTMLInputElement& input)
{
    notifyPopupOpeningObservers();
    WebLocalFrameImpl* webframe = WebLocalFrameImpl::fromFrame(input.document().frame());
    if (webframe->autofillClient())
        webframe->autofillClient()->openTextDataListChooser(WebInputElement(&input));
}

void ChromeClientImpl::textFieldDataListChanged(HTMLInputElement& input)
{
    WebLocalFrameImpl* webframe = WebLocalFrameImpl::fromFrame(input.document().frame());
    if (webframe->autofillClient())
        webframe->autofillClient()->dataListOptionsChanged(WebInputElement(&input));
}

void ChromeClientImpl::ajaxSucceeded(LocalFrame* frame)
{
    WebLocalFrameImpl* webframe = WebLocalFrameImpl::fromFrame(frame);
    if (webframe->autofillClient())
        webframe->autofillClient()->ajaxSucceeded();
}

void ChromeClientImpl::registerViewportLayers() const
{
    if (m_webView->rootGraphicsLayer() && m_webView->layerTreeView())
        m_webView->page()->frameHost().visualViewport().registerLayersWithTreeView(m_webView->layerTreeView());
}

void ChromeClientImpl::didUpdateTopControls() const
{
    m_webView->didUpdateTopControls();
}

CompositorProxyClient* ChromeClientImpl::createCompositorProxyClient(LocalFrame* frame)
{
    WebLocalFrameImpl* webFrame = WebLocalFrameImpl::fromFrame(frame);
    return webFrame->localRoot()->frameWidget()->createCompositorProxyClient();
}

void ChromeClientImpl::registerPopupOpeningObserver(PopupOpeningObserver* observer)
{
    DCHECK(observer);
    m_popupOpeningObservers.append(observer);
}

void ChromeClientImpl::unregisterPopupOpeningObserver(PopupOpeningObserver* observer)
{
    size_t index = m_popupOpeningObservers.find(observer);
    DCHECK_NE(index, kNotFound);
    m_popupOpeningObservers.remove(index);
}

void ChromeClientImpl::notifyPopupOpeningObservers() const
{
    const Vector<PopupOpeningObserver*> observers(m_popupOpeningObservers);
    for (const auto& observer : observers)
        observer->willOpenPopup();
}

FloatSize ChromeClientImpl::elasticOverscroll() const
{
    return m_webView->elasticOverscroll();
}

void ChromeClientImpl::didObserveNonGetFetchFromScript() const
{
    if (m_webView->pageImportanceSignals())
        m_webView->pageImportanceSignals()->setIssuedNonGetFetchFromScript();
}

std::unique_ptr<WebFrameScheduler> ChromeClientImpl::createFrameScheduler(BlameContext* blameContext)
{
    return wrapUnique(m_webView->scheduler()->createFrameScheduler(blameContext).release());
}

double ChromeClientImpl::lastFrameTimeMonotonic() const
{
    return m_webView->lastFrameTimeMonotonic();
}

} // namespace blink
