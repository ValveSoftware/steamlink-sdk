/*
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
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
#include "web/WebViewImpl.h"

#include "core/CSSValueKeywords.h"
#include "core/HTMLNames.h"
#include "core/accessibility/AXObjectCache.h"
#include "core/clipboard/DataObject.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentMarkerController.h"
#include "core/dom/Text.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/HTMLInterchange.h"
#include "core/editing/InputMethodController.h"
#include "core/editing/TextIterator.h"
#include "core/editing/markup.h"
#include "core/events/KeyboardEvent.h"
#include "core/events/WheelEvent.h"
#include "core/frame/EventHandlerRegistry.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/PinchViewport.h"
#include "core/frame/Settings.h"
#include "core/frame/SmartClip.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/HTMLPlugInElement.h"
#include "core/html/HTMLTextAreaElement.h"
#include "core/html/ime/InputMethodContext.h"
#include "core/inspector/InspectorController.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/UniqueIdentifier.h"
#include "core/page/Chrome.h"
#include "core/page/ContextMenuController.h"
#include "core/page/DragController.h"
#include "core/page/DragData.h"
#include "core/page/DragSession.h"
#include "core/page/EventHandler.h"
#include "core/page/FocusController.h"
#include "core/page/FrameTree.h"
#include "core/page/InjectedStyleSheets.h"
#include "core/page/Page.h"
#include "core/page/PagePopupClient.h"
#include "core/page/PointerLockController.h"
#include "core/page/ScopedPageLoadDeferrer.h"
#include "core/page/TouchDisambiguation.h"
#include "core/rendering/FastTextAutosizer.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/RenderWidget.h"
#include "core/rendering/TextAutosizer.h"
#include "core/rendering/compositing/RenderLayerCompositor.h"
#include "modules/device_orientation/DeviceOrientationInspectorAgent.h"
#include "modules/encryptedmedia/MediaKeysController.h"
#include "modules/filesystem/InspectorFileSystemAgent.h"
#include "modules/indexeddb/InspectorIndexedDBAgent.h"
#include "modules/push_messaging/PushController.h"
#include "platform/ContextMenu.h"
#include "platform/ContextMenuItem.h"
#include "platform/Cursor.h"
#include "platform/KeyboardCodes.h"
#include "platform/NotImplemented.h"
#include "platform/OverscrollTheme.h"
#include "platform/PlatformGestureEvent.h"
#include "platform/PlatformKeyboardEvent.h"
#include "platform/PlatformMouseEvent.h"
#include "platform/PlatformWheelEvent.h"
#include "platform/PopupMenuClient.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/TraceEvent.h"
#include "platform/UserGestureIndicator.h"
#include "platform/exported/WebActiveGestureAnimation.h"
#include "platform/fonts/FontCache.h"
#include "platform/graphics/Color.h"
#include "platform/graphics/Image.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "public/platform/Platform.h"
#include "public/platform/WebDragData.h"
#include "public/platform/WebFloatPoint.h"
#include "public/platform/WebGestureCurve.h"
#include "public/platform/WebImage.h"
#include "public/platform/WebLayerTreeView.h"
#include "public/platform/WebVector.h"
#include "public/web/WebAXObject.h"
#include "public/web/WebActiveWheelFlingParameters.h"
#include "public/web/WebAutofillClient.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebHitTestResult.h"
#include "public/web/WebInputElement.h"
#include "public/web/WebMediaPlayerAction.h"
#include "public/web/WebNode.h"
#include "public/web/WebPlugin.h"
#include "public/web/WebPluginAction.h"
#include "public/web/WebRange.h"
#include "public/web/WebTextInputInfo.h"
#include "public/web/WebViewClient.h"
#include "public/web/WebWindowFeatures.h"
#include "web/CompositionUnderlineVectorBuilder.h"
#include "web/ContextFeaturesClientImpl.h"
#include "web/DatabaseClientImpl.h"
#include "web/FullscreenController.h"
#include "web/GraphicsLayerFactoryChromium.h"
#include "web/LinkHighlight.h"
#include "web/NavigatorContentUtilsClientImpl.h"
#include "web/PopupContainer.h"
#include "web/PrerendererClientImpl.h"
#include "web/SpeechRecognitionClientProxy.h"
#include "web/StorageQuotaClientImpl.h"
#include "web/ValidationMessageClientImpl.h"
#include "web/ViewportAnchor.h"
#include "web/WebDevToolsAgentImpl.h"
#include "web/WebDevToolsAgentPrivate.h"
#include "web/WebInputEventConversion.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebPagePopupImpl.h"
#include "web/WebPluginContainerImpl.h"
#include "web/WebPopupMenuImpl.h"
#include "web/WebRemoteFrameImpl.h"
#include "web/WebSettingsImpl.h"
#include "web/WorkerGlobalScopeProxyProviderImpl.h"
#include "web/painting/ContinuousPainter.h"
#include "wtf/CurrentTime.h"
#include "wtf/RefPtr.h"
#include "wtf/TemporaryChange.h"

#if USE(DEFAULT_RENDER_THEME)
#include "core/rendering/RenderThemeChromiumDefault.h"
#endif

// Get rid of WTF's pow define so we can use std::pow.
#undef pow
#include <cmath> // for std::pow

using namespace WebCore;

// The following constants control parameters for automated scaling of webpages
// (such as due to a double tap gesture or find in page etc.). These are
// experimentally determined.
static const int touchPointPadding = 32;
static const int nonUserInitiatedPointPadding = 11;
static const float minScaleDifference = 0.01f;
static const float doubleTapZoomContentDefaultMargin = 5;
static const float doubleTapZoomContentMinimumMargin = 2;
static const double doubleTapZoomAnimationDurationInSeconds = 0.25;
static const float doubleTapZoomAlreadyLegibleRatio = 1.2f;

static const double multipleTargetsZoomAnimationDurationInSeconds = 0.25;
static const double findInPageAnimationDurationInSeconds = 0;

// Constants for viewport anchoring on resize.
static const float viewportAnchorXCoord = 0.5f;
static const float viewportAnchorYCoord = 0;

// Constants for zooming in on a focused text field.
static const double scrollAndScaleAnimationDurationInSeconds = 0.2;
static const int minReadableCaretHeight = 18;
static const float minScaleChangeToTriggerZoom = 1.05f;
static const float leftBoxRatio = 0.3f;
static const int caretPadding = 10;

namespace blink {

// Change the text zoom level by kTextSizeMultiplierRatio each time the user
// zooms text in or out (ie., change by 20%).  The min and max values limit
// text zoom to half and 3x the original text size.  These three values match
// those in Apple's port in WebKit/WebKit/WebView/WebView.mm
const double WebView::textSizeMultiplierRatio = 1.2;
const double WebView::minTextSizeMultiplier = 0.5;
const double WebView::maxTextSizeMultiplier = 3.0;

// Used to defer all page activity in cases where the embedder wishes to run
// a nested event loop. Using a stack enables nesting of message loop invocations.
static Vector<ScopedPageLoadDeferrer*>& pageLoadDeferrerStack()
{
    DEFINE_STATIC_LOCAL(Vector<ScopedPageLoadDeferrer*>, deferrerStack, ());
    return deferrerStack;
}

// Ensure that the WebDragOperation enum values stay in sync with the original
// DragOperation constants.
#define COMPILE_ASSERT_MATCHING_ENUM(coreName) \
    COMPILE_ASSERT(int(coreName) == int(Web##coreName), dummy##coreName)
COMPILE_ASSERT_MATCHING_ENUM(DragOperationNone);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationCopy);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationLink);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationGeneric);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationPrivate);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationMove);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationDelete);
COMPILE_ASSERT_MATCHING_ENUM(DragOperationEvery);

static bool shouldUseExternalPopupMenus = false;

static int webInputEventKeyStateToPlatformEventKeyState(int webInputEventKeyState)
{
    int platformEventKeyState = 0;
    if (webInputEventKeyState & WebInputEvent::ShiftKey)
        platformEventKeyState = platformEventKeyState | WebCore::PlatformEvent::ShiftKey;
    if (webInputEventKeyState & WebInputEvent::ControlKey)
        platformEventKeyState = platformEventKeyState | WebCore::PlatformEvent::CtrlKey;
    if (webInputEventKeyState & WebInputEvent::AltKey)
        platformEventKeyState = platformEventKeyState | WebCore::PlatformEvent::AltKey;
    if (webInputEventKeyState & WebInputEvent::MetaKey)
        platformEventKeyState = platformEventKeyState | WebCore::PlatformEvent::MetaKey;
    return platformEventKeyState;
}

namespace {

class UserGestureNotifier {
public:
    // If a UserGestureIndicator is created for a user gesture during the
    // lifetime of a UserGestureNotifier and *userGestureObserved is false,
    // the object will notify the client and set *userGestureObserved to true.
    UserGestureNotifier(WebAutofillClient*, bool* userGestureObserved);
    ~UserGestureNotifier();

private:
    WebAutofillClient* const m_client;
    bool* const m_userGestureObserved;
};

UserGestureNotifier::UserGestureNotifier(WebAutofillClient* client, bool* userGestureObserved)
    : m_client(client)
    , m_userGestureObserved(userGestureObserved)
{
    ASSERT(m_userGestureObserved);
    if (m_client)
        UserGestureIndicator::clearProcessedUserGestureInPast();
}

UserGestureNotifier::~UserGestureNotifier()
{
    if (!*m_userGestureObserved && UserGestureIndicator::processedUserGestureInPast()) {
        *m_userGestureObserved = true;
        if (m_client)
            m_client->firstUserGestureObserved();
    }
}

} // namespace

// WebView ----------------------------------------------------------------

WebView* WebView::create(WebViewClient* client)
{
    // Pass the WebViewImpl's self-reference to the caller.
    return WebViewImpl::create(client);
}

WebViewImpl* WebViewImpl::create(WebViewClient* client)
{
    // Pass the WebViewImpl's self-reference to the caller.
    return adoptRef(new WebViewImpl(client)).leakRef();
}

void WebView::setUseExternalPopupMenus(bool useExternalPopupMenus)
{
    shouldUseExternalPopupMenus = useExternalPopupMenus;
}

void WebView::updateVisitedLinkState(unsigned long long linkHash)
{
    Page::visitedStateChanged(linkHash);
}

void WebView::resetVisitedLinkState()
{
    Page::allVisitedStateChanged();
}

void WebView::willEnterModalLoop()
{
    pageLoadDeferrerStack().append(new ScopedPageLoadDeferrer());
}

void WebView::didExitModalLoop()
{
    ASSERT(pageLoadDeferrerStack().size());

    delete pageLoadDeferrerStack().last();
    pageLoadDeferrerStack().removeLast();
}

void WebViewImpl::setMainFrame(WebFrame* frame)
{
    if (frame->isWebLocalFrame())
        toWebLocalFrameImpl(frame)->initializeAsMainFrame(page());
    else
        toWebRemoteFrameImpl(frame)->initializeAsMainFrame(page());
}

void WebViewImpl::setAutofillClient(WebAutofillClient* autofillClient)
{
    m_autofillClient = autofillClient;
}

void WebViewImpl::setDevToolsAgentClient(WebDevToolsAgentClient* devToolsClient)
{
    if (devToolsClient)
        m_devToolsAgent = adoptPtr(new WebDevToolsAgentImpl(this, devToolsClient));
    else
        m_devToolsAgent.clear();
}

void WebViewImpl::setPrerendererClient(WebPrerendererClient* prerendererClient)
{
    ASSERT(m_page);
    providePrerendererClientTo(*m_page, new PrerendererClientImpl(prerendererClient));
}

void WebViewImpl::setSpellCheckClient(WebSpellCheckClient* spellCheckClient)
{
    m_spellCheckClient = spellCheckClient;
}

WebViewImpl::WebViewImpl(WebViewClient* client)
    : m_client(client)
    , m_autofillClient(0)
    , m_spellCheckClient(0)
    , m_chromeClientImpl(this)
    , m_contextMenuClientImpl(this)
    , m_dragClientImpl(this)
    , m_editorClientImpl(this)
    , m_inspectorClientImpl(this)
    , m_backForwardClientImpl(this)
    , m_spellCheckerClientImpl(this)
    , m_storageClientImpl(this)
    , m_fixedLayoutSizeLock(false)
    , m_shouldAutoResize(false)
    , m_zoomLevel(0)
    , m_minimumZoomLevel(zoomFactorToZoomLevel(minTextSizeMultiplier))
    , m_maximumZoomLevel(zoomFactorToZoomLevel(maxTextSizeMultiplier))
    , m_doubleTapZoomPageScaleFactor(0)
    , m_doubleTapZoomPending(false)
    , m_enableFakePageScaleAnimationForTesting(false)
    , m_fakePageScaleAnimationPageScaleFactor(0)
    , m_fakePageScaleAnimationUseAnchor(false)
    , m_contextMenuAllowed(false)
    , m_doingDragAndDrop(false)
    , m_ignoreInputEvents(false)
    , m_compositorDeviceScaleFactorOverride(0)
    , m_rootLayerScale(1)
    , m_suppressNextKeypressEvent(false)
    , m_imeAcceptEvents(true)
    , m_operationsAllowed(WebDragOperationNone)
    , m_dragOperation(WebDragOperationNone)
    , m_isTransparent(false)
    , m_tabsToLinks(false)
    , m_layerTreeView(0)
    , m_rootLayer(0)
    , m_rootGraphicsLayer(0)
    , m_rootTransformLayer(0)
    , m_graphicsLayerFactory(adoptPtr(new GraphicsLayerFactoryChromium(this)))
    , m_isAcceleratedCompositingActive(false)
    , m_layerTreeViewCommitsDeferred(false)
    , m_matchesHeuristicsForGpuRasterization(false)
    , m_recreatingGraphicsContext(false)
    , m_flingModifier(0)
    , m_flingSourceDevice(false)
    , m_fullscreenController(FullscreenController::create(this))
    , m_showFPSCounter(false)
    , m_showPaintRects(false)
    , m_showDebugBorders(false)
    , m_continuousPaintingEnabled(false)
    , m_showScrollBottleneckRects(false)
    , m_baseBackgroundColor(Color::white)
    , m_backgroundColorOverride(Color::transparent)
    , m_zoomFactorOverride(0)
    , m_userGestureObserved(false)
{
    Page::PageClients pageClients;
    pageClients.chromeClient = &m_chromeClientImpl;
    pageClients.contextMenuClient = &m_contextMenuClientImpl;
    pageClients.editorClient = &m_editorClientImpl;
    pageClients.dragClient = &m_dragClientImpl;
    pageClients.inspectorClient = &m_inspectorClientImpl;
    pageClients.backForwardClient = &m_backForwardClientImpl;
    pageClients.spellCheckerClient = &m_spellCheckerClientImpl;
    pageClients.storageClient = &m_storageClientImpl;

    m_page = adoptPtrWillBeNoop(new Page(pageClients));
    MediaKeysController::provideMediaKeysTo(*m_page, &m_mediaKeysClientImpl);
    provideSpeechRecognitionTo(*m_page, SpeechRecognitionClientProxy::create(client ? client->speechRecognizer() : 0));
    provideNavigatorContentUtilsTo(*m_page, NavigatorContentUtilsClientImpl::create(this));

    provideContextFeaturesTo(*m_page, ContextFeaturesClientImpl::create());
    DeviceOrientationInspectorAgent::provideTo(*m_page);

    m_page->inspectorController().registerModuleAgent(InspectorFileSystemAgent::create(m_page.get()));
    provideDatabaseClientTo(*m_page, DatabaseClientImpl::create());
    InspectorIndexedDBAgent::provideTo(m_page.get());
    provideStorageQuotaClientTo(*m_page, StorageQuotaClientImpl::create());
    m_page->setValidationMessageClient(ValidationMessageClientImpl::create(*this));
    provideWorkerGlobalScopeProxyProviderTo(*m_page, WorkerGlobalScopeProxyProviderImpl::create());

    m_page->makeOrdinary();

    if (m_client) {
        providePushControllerTo(*m_page, m_client->webPushClient());
        setDeviceScaleFactor(m_client->screenInfo().deviceScaleFactor);
        setVisibilityState(m_client->visibilityState(), true);
    }

    m_inspectorSettingsMap = adoptPtr(new SettingsMap);
}

WebViewImpl::~WebViewImpl()
{
    ASSERT(!m_page);
}

WebLocalFrameImpl* WebViewImpl::mainFrameImpl()
{
    return m_page && m_page->mainFrame() && m_page->mainFrame()->isLocalFrame() ? WebLocalFrameImpl::fromFrame(m_page->deprecatedLocalMainFrame()) : 0;
}

bool WebViewImpl::tabKeyCyclesThroughElements() const
{
    ASSERT(m_page);
    return m_page->tabKeyCyclesThroughElements();
}

void WebViewImpl::setTabKeyCyclesThroughElements(bool value)
{
    if (m_page)
        m_page->setTabKeyCyclesThroughElements(value);
}

void WebViewImpl::handleMouseLeave(LocalFrame& mainFrame, const WebMouseEvent& event)
{
    m_client->setMouseOverURL(WebURL());
    PageWidgetEventHandler::handleMouseLeave(mainFrame, event);
}

void WebViewImpl::handleMouseDown(LocalFrame& mainFrame, const WebMouseEvent& event)
{
    // If there is a popup open, close it as the user is clicking on the page (outside of the
    // popup). We also save it so we can prevent a click on an element from immediately
    // reopening the same popup.
    RefPtr<PopupContainer> selectPopup;
    RefPtr<WebPagePopupImpl> pagePopup;
    if (event.button == WebMouseEvent::ButtonLeft) {
        selectPopup = m_selectPopup;
        pagePopup = m_pagePopup;
        hidePopups();
        ASSERT(!m_selectPopup);
        ASSERT(!m_pagePopup);
    }

    m_lastMouseDownPoint = WebPoint(event.x, event.y);

    // Take capture on a mouse down on a plugin so we can send it mouse events.
    // If the hit node is a plugin but a scrollbar is over it don't start mouse
    // capture because it will interfere with the scrollbar receiving events.
    IntPoint point(event.x, event.y);
    if (event.button == WebMouseEvent::ButtonLeft && m_page->mainFrame()->isLocalFrame() && !m_page->deprecatedLocalMainFrame()->view()->scrollbarAtPoint(point)) {
        point = m_page->deprecatedLocalMainFrame()->view()->windowToContents(point);
        HitTestResult result(m_page->deprecatedLocalMainFrame()->eventHandler().hitTestResultAtPoint(point));
        Node* hitNode = result.innerNonSharedNode();

        if (!result.scrollbar() && hitNode && hitNode->renderer() && hitNode->renderer()->isEmbeddedObject()) {
            m_mouseCaptureNode = hitNode;
            TRACE_EVENT_ASYNC_BEGIN0("input", "capturing mouse", this);
        }
    }

    PageWidgetEventHandler::handleMouseDown(mainFrame, event);

    if (event.button == WebMouseEvent::ButtonLeft && m_mouseCaptureNode)
        m_mouseCaptureGestureToken = mainFrame.eventHandler().takeLastMouseDownGestureToken();

    if (m_selectPopup && m_selectPopup == selectPopup) {
        // That click triggered a select popup which is the same as the one that
        // was showing before the click.  It means the user clicked the select
        // while the popup was showing, and as a result we first closed then
        // immediately reopened the select popup.  It needs to be closed.
        hideSelectPopup();
    }

    if (m_pagePopup && pagePopup && m_pagePopup->hasSamePopupClient(pagePopup.get())) {
        // That click triggered a page popup that is the same as the one we just closed.
        // It needs to be closed.
        closePagePopup(m_pagePopup.get());
    }

    // Dispatch the contextmenu event regardless of if the click was swallowed.
#if OS(WIN)
    // On Windows, we handle it on mouse up, not down.
#elif OS(MACOSX)
    if (event.button == WebMouseEvent::ButtonRight
        || (event.button == WebMouseEvent::ButtonLeft
            && event.modifiers & WebMouseEvent::ControlKey))
        mouseContextMenu(event);
#else
    if (event.button == WebMouseEvent::ButtonRight)
        mouseContextMenu(event);
#endif
}

void WebViewImpl::mouseContextMenu(const WebMouseEvent& event)
{
    if (!mainFrameImpl() || !mainFrameImpl()->frameView())
        return;

    m_page->contextMenuController().clearContextMenu();

    PlatformMouseEventBuilder pme(mainFrameImpl()->frameView(), event);

    // Find the right target frame. See issue 1186900.
    HitTestResult result = hitTestResultForWindowPos(pme.position());
    Frame* targetFrame;
    if (result.innerNonSharedNode())
        targetFrame = result.innerNonSharedNode()->document().frame();
    else
        targetFrame = m_page->focusController().focusedOrMainFrame();

    if (!targetFrame->isLocalFrame())
        return;

    LocalFrame* targetLocalFrame = toLocalFrame(targetFrame);

#if OS(WIN)
    targetLocalFrame->view()->setCursor(pointerCursor());
#endif

    m_contextMenuAllowed = true;
    targetLocalFrame->eventHandler().sendContextMenuEvent(pme);
    m_contextMenuAllowed = false;
    // Actually showing the context menu is handled by the ContextMenuClient
    // implementation...
}

void WebViewImpl::handleMouseUp(LocalFrame& mainFrame, const WebMouseEvent& event)
{
    PageWidgetEventHandler::handleMouseUp(mainFrame, event);

#if OS(WIN)
    // Dispatch the contextmenu event regardless of if the click was swallowed.
    // On Mac/Linux, we handle it on mouse down, not up.
    if (event.button == WebMouseEvent::ButtonRight)
        mouseContextMenu(event);
#endif
}

bool WebViewImpl::handleMouseWheel(LocalFrame& mainFrame, const WebMouseWheelEvent& event)
{
    hidePopups();
    return PageWidgetEventHandler::handleMouseWheel(mainFrame, event);
}

bool WebViewImpl::scrollBy(const WebFloatSize& delta, const WebFloatSize& velocity)
{
    if (m_flingSourceDevice == WebGestureDeviceTouchpad) {
        WebMouseWheelEvent syntheticWheel;
        const float tickDivisor = WebCore::WheelEvent::TickMultiplier;

        syntheticWheel.deltaX = delta.width;
        syntheticWheel.deltaY = delta.height;
        syntheticWheel.wheelTicksX = delta.width / tickDivisor;
        syntheticWheel.wheelTicksY = delta.height / tickDivisor;
        syntheticWheel.hasPreciseScrollingDeltas = true;
        syntheticWheel.x = m_positionOnFlingStart.x;
        syntheticWheel.y = m_positionOnFlingStart.y;
        syntheticWheel.globalX = m_globalPositionOnFlingStart.x;
        syntheticWheel.globalY = m_globalPositionOnFlingStart.y;
        syntheticWheel.modifiers = m_flingModifier;

        if (m_page && m_page->mainFrame() && m_page->mainFrame()->isLocalFrame() && m_page->deprecatedLocalMainFrame()->view())
            return handleMouseWheel(*m_page->deprecatedLocalMainFrame(), syntheticWheel);
    } else {
        WebGestureEvent syntheticGestureEvent;

        syntheticGestureEvent.type = WebInputEvent::GestureScrollUpdateWithoutPropagation;
        syntheticGestureEvent.data.scrollUpdate.deltaX = delta.width;
        syntheticGestureEvent.data.scrollUpdate.deltaY = delta.height;
        syntheticGestureEvent.x = m_positionOnFlingStart.x;
        syntheticGestureEvent.y = m_positionOnFlingStart.y;
        syntheticGestureEvent.globalX = m_globalPositionOnFlingStart.x;
        syntheticGestureEvent.globalY = m_globalPositionOnFlingStart.y;
        syntheticGestureEvent.modifiers = m_flingModifier;
        syntheticGestureEvent.sourceDevice = WebGestureDeviceTouchscreen;

        if (m_page && m_page->mainFrame() && m_page->mainFrame()->isLocalFrame() && m_page->deprecatedLocalMainFrame()->view())
            return handleGestureEvent(syntheticGestureEvent);
    }
    return false;
}

bool WebViewImpl::handleGestureEvent(const WebGestureEvent& event)
{
    bool eventSwallowed = false;
    bool eventCancelled = false; // for disambiguation

    // Special handling for slow-path fling gestures.
    switch (event.type) {
    case WebInputEvent::GestureFlingStart: {
        if (mainFrameImpl()->frame()->eventHandler().isScrollbarHandlingGestures())
            break;
        m_client->cancelScheduledContentIntents();
        m_positionOnFlingStart = WebPoint(event.x / pageScaleFactor(), event.y / pageScaleFactor());
        m_globalPositionOnFlingStart = WebPoint(event.globalX, event.globalY);
        m_flingModifier = event.modifiers;
        m_flingSourceDevice = event.sourceDevice;
        OwnPtr<WebGestureCurve> flingCurve = adoptPtr(Platform::current()->createFlingAnimationCurve(event.sourceDevice, WebFloatPoint(event.data.flingStart.velocityX, event.data.flingStart.velocityY), WebSize()));
        ASSERT(flingCurve);
        m_gestureAnimation = WebActiveGestureAnimation::createAtAnimationStart(flingCurve.release(), this);
        scheduleAnimation();
        eventSwallowed = true;

        m_client->didHandleGestureEvent(event, eventCancelled);
        return eventSwallowed;
    }
    case WebInputEvent::GestureFlingCancel:
        if (endActiveFlingAnimation())
            eventSwallowed = true;

        m_client->didHandleGestureEvent(event, eventCancelled);
        return eventSwallowed;
    default:
        break;
    }

    PlatformGestureEventBuilder platformEvent(mainFrameImpl()->frameView(), event);

    // Handle link highlighting outside the main switch to avoid getting lost in the
    // complicated set of cases handled below.
    switch (event.type) {
    case WebInputEvent::GestureShowPress:
        // Queue a highlight animation, then hand off to regular handler.
        if (settingsImpl()->gestureTapHighlightEnabled())
            enableTapHighlightAtPoint(platformEvent);
        break;
    case WebInputEvent::GestureTapCancel:
    case WebInputEvent::GestureTap:
    case WebInputEvent::GestureLongPress:
        for (size_t i = 0; i < m_linkHighlights.size(); ++i)
            m_linkHighlights[i]->startHighlightAnimationIfNeeded();
        break;
    default:
        break;
    }

    switch (event.type) {
    case WebInputEvent::GestureTap: {
        m_client->cancelScheduledContentIntents();
        if (detectContentOnTouch(platformEvent.position())) {
            eventSwallowed = true;
            break;
        }

        RefPtr<PopupContainer> selectPopup;
        selectPopup = m_selectPopup;
        hideSelectPopup();
        ASSERT(!m_selectPopup);

        // Don't trigger a disambiguation popup on sites designed for mobile devices.
        // Instead, assume that the page has been designed with big enough buttons and links.
        if (event.data.tap.width > 0 && !shouldDisableDesktopWorkarounds()) {
            // FIXME: didTapMultipleTargets should just take a rect instead of
            // an event.
            WebGestureEvent scaledEvent = event;
            scaledEvent.x = event.x / pageScaleFactor();
            scaledEvent.y = event.y / pageScaleFactor();
            scaledEvent.data.tap.width = event.data.tap.width / pageScaleFactor();
            scaledEvent.data.tap.height = event.data.tap.height / pageScaleFactor();
            IntRect boundingBox(scaledEvent.x - scaledEvent.data.tap.width / 2, scaledEvent.y - scaledEvent.data.tap.height / 2, scaledEvent.data.tap.width, scaledEvent.data.tap.height);
            Vector<IntRect> goodTargets;
            WillBeHeapVector<RawPtrWillBeMember<Node> > highlightNodes;
            findGoodTouchTargets(boundingBox, mainFrameImpl()->frame(), goodTargets, highlightNodes);
            // FIXME: replace touch adjustment code when numberOfGoodTargets == 1?
            // Single candidate case is currently handled by: https://bugs.webkit.org/show_bug.cgi?id=85101
            if (goodTargets.size() >= 2 && m_client && m_client->didTapMultipleTargets(scaledEvent, goodTargets)) {
                if (settingsImpl()->gestureTapHighlightEnabled())
                    enableTapHighlights(highlightNodes);
                for (size_t i = 0; i < m_linkHighlights.size(); ++i)
                    m_linkHighlights[i]->startHighlightAnimationIfNeeded();
                eventSwallowed = true;
                eventCancelled = true;
                break;
            }
        }

        eventSwallowed = mainFrameImpl()->frame()->eventHandler().handleGestureEvent(platformEvent);

        if (m_selectPopup && m_selectPopup == selectPopup) {
            // That tap triggered a select popup which is the same as the one that
            // was showing before the tap. It means the user tapped the select
            // while the popup was showing, and as a result we first closed then
            // immediately reopened the select popup. It needs to be closed.
            hideSelectPopup();
        }

        break;
    }
    case WebInputEvent::GestureTwoFingerTap:
    case WebInputEvent::GestureLongPress:
    case WebInputEvent::GestureLongTap: {
        if (!mainFrameImpl() || !mainFrameImpl()->frameView())
            break;

        m_client->cancelScheduledContentIntents();
        m_page->contextMenuController().clearContextMenu();
        m_contextMenuAllowed = true;
        eventSwallowed = mainFrameImpl()->frame()->eventHandler().handleGestureEvent(platformEvent);
        m_contextMenuAllowed = false;

        break;
    }
    case WebInputEvent::GestureShowPress: {
        m_client->cancelScheduledContentIntents();
        eventSwallowed = mainFrameImpl()->frame()->eventHandler().handleGestureEvent(platformEvent);
        break;
    }
    case WebInputEvent::GestureDoubleTap:
        if (m_webSettings->doubleTapToZoomEnabled() && minimumPageScaleFactor() != maximumPageScaleFactor()) {
            m_client->cancelScheduledContentIntents();
            animateDoubleTapZoom(platformEvent.position());
        }
        // GestureDoubleTap is currently only used by Android for zooming. For WebCore,
        // GestureTap with tap count = 2 is used instead. So we drop GestureDoubleTap here.
        eventSwallowed = true;
        break;
    case WebInputEvent::GestureScrollBegin:
    case WebInputEvent::GesturePinchBegin:
        m_client->cancelScheduledContentIntents();
    case WebInputEvent::GestureTapDown:
    case WebInputEvent::GestureScrollEnd:
    case WebInputEvent::GestureScrollUpdate:
    case WebInputEvent::GestureScrollUpdateWithoutPropagation:
    case WebInputEvent::GestureTapCancel:
    case WebInputEvent::GestureTapUnconfirmed:
    case WebInputEvent::GesturePinchEnd:
    case WebInputEvent::GesturePinchUpdate:
    case WebInputEvent::GestureFlingStart: {
        eventSwallowed = mainFrameImpl()->frame()->eventHandler().handleGestureEvent(platformEvent);
        break;
    }
    default:
        ASSERT_NOT_REACHED();
    }
    m_client->didHandleGestureEvent(event, eventCancelled);
    return eventSwallowed;
}

void WebViewImpl::transferActiveWheelFlingAnimation(const WebActiveWheelFlingParameters& parameters)
{
    TRACE_EVENT0("webkit", "WebViewImpl::transferActiveWheelFlingAnimation");
    ASSERT(!m_gestureAnimation);
    m_positionOnFlingStart = parameters.point;
    m_globalPositionOnFlingStart = parameters.globalPoint;
    m_flingModifier = parameters.modifiers;
    OwnPtr<WebGestureCurve> curve = adoptPtr(Platform::current()->createFlingAnimationCurve(parameters.sourceDevice, WebFloatPoint(parameters.delta), parameters.cumulativeScroll));
    ASSERT(curve);
    m_gestureAnimation = WebActiveGestureAnimation::createWithTimeOffset(curve.release(), this, parameters.startTime);
    scheduleAnimation();
}

bool WebViewImpl::endActiveFlingAnimation()
{
    if (m_gestureAnimation) {
        m_gestureAnimation.clear();
        if (m_layerTreeView)
            m_layerTreeView->didStopFlinging();
        return true;
    }
    return false;
}

bool WebViewImpl::startPageScaleAnimation(const IntPoint& targetPosition, bool useAnchor, float newScale, double durationInSeconds)
{
    WebPoint clampedPoint = targetPosition;
    if (!useAnchor) {
        clampedPoint = clampOffsetAtScale(targetPosition, newScale);
        if (!durationInSeconds) {
            setPageScaleFactor(newScale, clampedPoint);
            return false;
        }
    }
    if (useAnchor && newScale == pageScaleFactor())
        return false;

    if (m_enableFakePageScaleAnimationForTesting) {
        m_fakePageScaleAnimationTargetPosition = targetPosition;
        m_fakePageScaleAnimationUseAnchor = useAnchor;
        m_fakePageScaleAnimationPageScaleFactor = newScale;
    } else {
        if (!m_layerTreeView)
            return false;
        m_layerTreeView->startPageScaleAnimation(targetPosition, useAnchor, newScale, durationInSeconds);
    }
    return true;
}

void WebViewImpl::enableFakePageScaleAnimationForTesting(bool enable)
{
    m_enableFakePageScaleAnimationForTesting = enable;
}

void WebViewImpl::setShowFPSCounter(bool show)
{
    if (m_layerTreeView) {
        TRACE_EVENT0("webkit", "WebViewImpl::setShowFPSCounter");
        m_layerTreeView->setShowFPSCounter(show);
    }
    m_showFPSCounter = show;
}

void WebViewImpl::setShowPaintRects(bool show)
{
    if (m_layerTreeView) {
        TRACE_EVENT0("webkit", "WebViewImpl::setShowPaintRects");
        m_layerTreeView->setShowPaintRects(show);
    }
    m_showPaintRects = show;
}

void WebViewImpl::setShowDebugBorders(bool show)
{
    if (m_layerTreeView)
        m_layerTreeView->setShowDebugBorders(show);
    m_showDebugBorders = show;
}

void WebViewImpl::setContinuousPaintingEnabled(bool enabled)
{
    if (m_layerTreeView) {
        TRACE_EVENT0("webkit", "WebViewImpl::setContinuousPaintingEnabled");
        m_layerTreeView->setContinuousPaintingEnabled(enabled);
    }
    m_continuousPaintingEnabled = enabled;
    m_client->scheduleAnimation();
}

void WebViewImpl::setShowScrollBottleneckRects(bool show)
{
    if (m_layerTreeView)
        m_layerTreeView->setShowScrollBottleneckRects(show);
    m_showScrollBottleneckRects = show;
}

void WebViewImpl::getSelectionRootBounds(WebRect& bounds) const
{
    const Frame* frame = focusedWebCoreFrame();
    if (!frame || !frame->isLocalFrame())
        return;

    Element* root = toLocalFrame(frame)->selection().rootEditableElementOrDocumentElement();
    if (!root)
        return;

    // If the selection is inside a form control, the root will be a <div> that
    // behaves as the editor but we want to return the actual element's bounds.
    // In practice, that means <textarea> and <input> controls that behave like
    // a text field.
    Element* shadowHost = root->shadowHost();
    if (shadowHost
        && (isHTMLTextAreaElement(*shadowHost)
            || (isHTMLInputElement(*shadowHost)
                && toHTMLInputElement(*shadowHost).isText())))
        root = shadowHost;

    IntRect boundingBox = isHTMLHtmlElement(root)
        ? IntRect(IntPoint(0, 0), root->document().frame()->view()->contentsSize())
        : root->pixelSnappedBoundingBox();

    boundingBox = root->document().frame()->view()->contentsToWindow(boundingBox);
    boundingBox.scale(pageScaleFactor());
    bounds = boundingBox;
}

void WebViewImpl::acceptLanguagesChanged()
{
    if (!page())
        return;

    page()->acceptLanguagesChanged();
}

bool WebViewImpl::handleKeyEvent(const WebKeyboardEvent& event)
{
    ASSERT((event.type == WebInputEvent::RawKeyDown)
        || (event.type == WebInputEvent::KeyDown)
        || (event.type == WebInputEvent::KeyUp));

    // Halt an in-progress fling on a key event.
    endActiveFlingAnimation();

    // Please refer to the comments explaining the m_suppressNextKeypressEvent
    // member.
    // The m_suppressNextKeypressEvent is set if the KeyDown is handled by
    // Webkit. A keyDown event is typically associated with a keyPress(char)
    // event and a keyUp event. We reset this flag here as this is a new keyDown
    // event.
    m_suppressNextKeypressEvent = false;

    // If there is a select popup, it should be the one processing the event,
    // not the page.
    if (m_selectPopup)
        return m_selectPopup->handleKeyEvent(PlatformKeyboardEventBuilder(event));
    if (m_pagePopup) {
        m_pagePopup->handleKeyEvent(PlatformKeyboardEventBuilder(event));
        // We need to ignore the next Char event after this otherwise pressing
        // enter when selecting an item in the popup will go to the page.
        if (WebInputEvent::RawKeyDown == event.type)
            m_suppressNextKeypressEvent = true;
        return true;
    }

    RefPtr<Frame> focusedFrame = focusedWebCoreFrame();
    if (focusedFrame && focusedFrame->isRemoteFrameTemporary()) {
        WebLocalFrameImpl* webFrame = WebLocalFrameImpl::fromFrame(toLocalFrameTemporary(focusedFrame.get()));
        webFrame->client()->forwardInputEvent(&event);
        return true;
    }

    if (!focusedFrame || !focusedFrame->isLocalFrame())
        return false;

    RefPtr<LocalFrame> frame = toLocalFrame(focusedFrame.get());

    PlatformKeyboardEventBuilder evt(event);

    if (frame->eventHandler().keyEvent(evt)) {
        if (WebInputEvent::RawKeyDown == event.type) {
            // Suppress the next keypress event unless the focused node is a plug-in node.
            // (Flash needs these keypress events to handle non-US keyboards.)
            Element* element = focusedElement();
            if (!element || !element->renderer() || !element->renderer()->isEmbeddedObject())
                m_suppressNextKeypressEvent = true;
        }
        return true;
    }

#if !OS(MACOSX)
    const WebInputEvent::Type contextMenuTriggeringEventType =
#if OS(WIN)
        WebInputEvent::KeyUp;
#else
        WebInputEvent::RawKeyDown;
#endif

    bool isUnmodifiedMenuKey = !(event.modifiers & WebInputEvent::InputModifiers) && event.windowsKeyCode == VKEY_APPS;
    bool isShiftF10 = event.modifiers == WebInputEvent::ShiftKey && event.windowsKeyCode == VKEY_F10;
    if ((isUnmodifiedMenuKey || isShiftF10) && event.type == contextMenuTriggeringEventType) {
        sendContextMenuEvent(event);
        return true;
    }
#endif // !OS(MACOSX)

    return keyEventDefault(event);
}

bool WebViewImpl::handleCharEvent(const WebKeyboardEvent& event)
{
    ASSERT(event.type == WebInputEvent::Char);

    // Please refer to the comments explaining the m_suppressNextKeypressEvent
    // member.  The m_suppressNextKeypressEvent is set if the KeyDown is
    // handled by Webkit. A keyDown event is typically associated with a
    // keyPress(char) event and a keyUp event. We reset this flag here as it
    // only applies to the current keyPress event.
    bool suppress = m_suppressNextKeypressEvent;
    m_suppressNextKeypressEvent = false;

    // If there is a select popup, it should be the one processing the event,
    // not the page.
    if (m_selectPopup)
        return m_selectPopup->handleKeyEvent(PlatformKeyboardEventBuilder(event));
    if (m_pagePopup)
        return m_pagePopup->handleKeyEvent(PlatformKeyboardEventBuilder(event));

    LocalFrame* frame = toLocalFrame(focusedWebCoreFrame());
    if (!frame)
        return suppress;

    EventHandler& handler = frame->eventHandler();

    PlatformKeyboardEventBuilder evt(event);
    if (!evt.isCharacterKey())
        return true;

    // Accesskeys are triggered by char events and can't be suppressed.
    if (handler.handleAccessKey(evt))
        return true;

    // Safari 3.1 does not pass off windows system key messages (WM_SYSCHAR) to
    // the eventHandler::keyEvent. We mimic this behavior on all platforms since
    // for now we are converting other platform's key events to windows key
    // events.
    if (evt.isSystemKey())
        return false;

    if (!suppress && !handler.keyEvent(evt))
        return keyEventDefault(event);

    return true;
}

WebRect WebViewImpl::computeBlockBounds(const WebRect& rect, bool ignoreClipping)
{
    if (!mainFrameImpl())
        return WebRect();

    // Use the rect-based hit test to find the node.
    IntPoint point = mainFrameImpl()->frameView()->windowToContents(IntPoint(rect.x, rect.y));
    HitTestRequest::HitTestRequestType hitType = HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::ConfusingAndOftenMisusedDisallowShadowContent | (ignoreClipping ? HitTestRequest::IgnoreClipping : 0);
    HitTestResult result = mainFrameImpl()->frame()->eventHandler().hitTestResultAtPoint(point, hitType, IntSize(rect.width, rect.height));

    Node* node = result.innerNonSharedNode();
    if (!node)
        return WebRect();

    // Find the block type node based on the hit node.
    while (node && (!node->renderer() || node->renderer()->isInline()))
        node = node->parentNode();

    // Return the bounding box in the window coordinate system.
    if (node) {
        IntRect rect = node->Node::pixelSnappedBoundingBox();
        LocalFrame* frame = node->document().frame();
        return frame->view()->contentsToWindow(rect);
    }
    return WebRect();
}

WebRect WebViewImpl::widenRectWithinPageBounds(const WebRect& source, int targetMargin, int minimumMargin)
{
    WebSize maxSize;
    if (mainFrame())
        maxSize = mainFrame()->contentsSize();
    IntSize scrollOffset;
    if (mainFrame())
        scrollOffset = mainFrame()->scrollOffset();
    int leftMargin = targetMargin;
    int rightMargin = targetMargin;

    const int absoluteSourceX = source.x + scrollOffset.width();
    if (leftMargin > absoluteSourceX) {
        leftMargin = absoluteSourceX;
        rightMargin = std::max(leftMargin, minimumMargin);
    }

    const int maximumRightMargin = maxSize.width - (source.width + absoluteSourceX);
    if (rightMargin > maximumRightMargin) {
        rightMargin = maximumRightMargin;
        leftMargin = std::min(leftMargin, std::max(rightMargin, minimumMargin));
    }

    const int newWidth = source.width + leftMargin + rightMargin;
    const int newX = source.x - leftMargin;

    ASSERT(newWidth >= 0);
    ASSERT(scrollOffset.width() + newX + newWidth <= maxSize.width);

    return WebRect(newX, source.y, newWidth, source.height);
}

float WebViewImpl::legibleScale() const
{
    // Pages should be as legible as on desktop when at dpi scale, so no
    // need to zoom in further when automatically determining zoom level
    // (after double tap, find in page, etc), though the user should still
    // be allowed to manually pinch zoom in further if they desire.
    float legibleScale = 1;
    if (page())
        legibleScale *= page()->settings().accessibilityFontScaleFactor();
    return legibleScale;
}

void WebViewImpl::computeScaleAndScrollForBlockRect(const WebPoint& hitPoint, const WebRect& blockRect, float padding, float defaultScaleWhenAlreadyLegible, float& scale, WebPoint& scroll)
{
    scale = pageScaleFactor();
    scroll.x = scroll.y = 0;

    WebRect rect = blockRect;

    if (!rect.isEmpty()) {
        float defaultMargin = doubleTapZoomContentDefaultMargin;
        float minimumMargin = doubleTapZoomContentMinimumMargin;
        // We want the margins to have the same physical size, which means we
        // need to express them in post-scale size. To do that we'd need to know
        // the scale we're scaling to, but that depends on the margins. Instead
        // we express them as a fraction of the target rectangle: this will be
        // correct if we end up fully zooming to it, and won't matter if we
        // don't.
        rect = widenRectWithinPageBounds(rect,
                static_cast<int>(defaultMargin * rect.width / m_size.width),
                static_cast<int>(minimumMargin * rect.width / m_size.width));
        // Fit block to screen, respecting limits.
        scale = static_cast<float>(m_size.width) / rect.width;
        scale = std::min(scale, legibleScale());
        if (pageScaleFactor() < defaultScaleWhenAlreadyLegible)
            scale = std::max(scale, defaultScaleWhenAlreadyLegible);
        scale = clampPageScaleFactorToLimits(scale);
    }

    // FIXME: If this is being called for auto zoom during find in page,
    // then if the user manually zooms in it'd be nice to preserve the
    // relative increase in zoom they caused (if they zoom out then it's ok
    // to zoom them back in again). This isn't compatible with our current
    // double-tap zoom strategy (fitting the containing block to the screen)
    // though.

    float screenWidth = m_size.width / scale;
    float screenHeight = m_size.height / scale;

    // Scroll to vertically align the block.
    if (rect.height < screenHeight) {
        // Vertically center short blocks.
        rect.y -= 0.5 * (screenHeight - rect.height);
    } else {
        // Ensure position we're zooming to (+ padding) isn't off the bottom of
        // the screen.
        rect.y = std::max<float>(rect.y, hitPoint.y + padding - screenHeight);
    } // Otherwise top align the block.

    // Do the same thing for horizontal alignment.
    if (rect.width < screenWidth)
        rect.x -= 0.5 * (screenWidth - rect.width);
    else
        rect.x = std::max<float>(rect.x, hitPoint.x + padding - screenWidth);
    scroll.x = rect.x;
    scroll.y = rect.y;

    scale = clampPageScaleFactorToLimits(scale);
    scroll = mainFrameImpl()->frameView()->windowToContents(scroll);
    scroll = clampOffsetAtScale(scroll, scale);
}

static bool invokesHandCursor(Node* node, LocalFrame* frame)
{
    if (!node || !node->renderer())
        return false;

    ECursor cursor = node->renderer()->style()->cursor();
    return cursor == CURSOR_POINTER
        || (cursor == CURSOR_AUTO && frame->eventHandler().useHandCursor(node, node->isLink()));
}

Node* WebViewImpl::bestTapNode(const PlatformGestureEvent& tapEvent)
{
    if (!m_page || !m_page->mainFrame() || !m_page->mainFrame()->isLocalFrame())
        return 0;

    Node* bestTouchNode = 0;

    IntPoint touchEventLocation(tapEvent.position());
    m_page->deprecatedLocalMainFrame()->eventHandler().adjustGesturePosition(tapEvent, touchEventLocation);

    IntPoint hitTestPoint = m_page->deprecatedLocalMainFrame()->view()->windowToContents(touchEventLocation);
    HitTestResult result = m_page->deprecatedLocalMainFrame()->eventHandler().hitTestResultAtPoint(hitTestPoint, HitTestRequest::TouchEvent | HitTestRequest::ConfusingAndOftenMisusedDisallowShadowContent);
    bestTouchNode = result.targetNode();

    // We might hit something like an image map that has no renderer on it
    // Walk up the tree until we have a node with an attached renderer
    while (bestTouchNode && !bestTouchNode->renderer())
        bestTouchNode = bestTouchNode->parentNode();

    // Check if we're in the subtree of a node with a hand cursor
    // this is the heuristic we use to determine if we show a highlight on tap
    while (bestTouchNode && !invokesHandCursor(bestTouchNode, m_page->deprecatedLocalMainFrame()))
        bestTouchNode = bestTouchNode->parentNode();

    if (!bestTouchNode)
        return 0;

    // We should pick the largest enclosing node with hand cursor set.
    while (bestTouchNode->parentNode() && invokesHandCursor(bestTouchNode->parentNode(), toLocalFrame(m_page->mainFrame())))
        bestTouchNode = bestTouchNode->parentNode();

    return bestTouchNode;
}

void WebViewImpl::enableTapHighlightAtPoint(const PlatformGestureEvent& tapEvent)
{
    Node* touchNode = bestTapNode(tapEvent);

    WillBeHeapVector<RawPtrWillBeMember<Node> > highlightNodes;
    highlightNodes.append(touchNode);

    enableTapHighlights(highlightNodes);
}

void WebViewImpl::enableTapHighlights(WillBeHeapVector<RawPtrWillBeMember<Node> >& highlightNodes)
{
    if (highlightNodes.isEmpty())
        return;

    // Always clear any existing highlight when this is invoked, even if we
    // don't get a new target to highlight.
    m_linkHighlights.clear();

    // LinkHighlight reads out layout and compositing state, so we need to make sure that's all up to date.
    layout();

    for (size_t i = 0; i < highlightNodes.size(); ++i) {
        Node* node = highlightNodes[i];

        if (!node || !node->renderer())
            continue;

        Color highlightColor = node->renderer()->style()->tapHighlightColor();
        // Safari documentation for -webkit-tap-highlight-color says if the specified color has 0 alpha,
        // then tap highlighting is disabled.
        // http://developer.apple.com/library/safari/#documentation/appleapplications/reference/safaricssref/articles/standardcssproperties.html
        if (!highlightColor.alpha())
            continue;

        m_linkHighlights.append(LinkHighlight::create(node, this));
    }
}

void WebViewImpl::animateDoubleTapZoom(const IntPoint& point)
{
    if (!mainFrameImpl())
        return;

    WebRect rect(point.x(), point.y(), touchPointPadding, touchPointPadding);
    WebRect blockBounds = computeBlockBounds(rect, false);

    float scale;
    WebPoint scroll;

    computeScaleAndScrollForBlockRect(point, blockBounds, touchPointPadding, minimumPageScaleFactor() * doubleTapZoomAlreadyLegibleRatio, scale, scroll);

    bool stillAtPreviousDoubleTapScale = (pageScaleFactor() == m_doubleTapZoomPageScaleFactor
        && m_doubleTapZoomPageScaleFactor != minimumPageScaleFactor())
        || m_doubleTapZoomPending;

    bool scaleUnchanged = fabs(pageScaleFactor() - scale) < minScaleDifference;
    bool shouldZoomOut = blockBounds.isEmpty() || scaleUnchanged || stillAtPreviousDoubleTapScale;

    bool isAnimating;

    if (shouldZoomOut) {
        scale = minimumPageScaleFactor();
        isAnimating = startPageScaleAnimation(mainFrameImpl()->frameView()->windowToContents(point), true, scale, doubleTapZoomAnimationDurationInSeconds);
    } else {
        isAnimating = startPageScaleAnimation(scroll, false, scale, doubleTapZoomAnimationDurationInSeconds);
    }

    if (isAnimating) {
        m_doubleTapZoomPageScaleFactor = scale;
        m_doubleTapZoomPending = true;
    }
}

void WebViewImpl::zoomToFindInPageRect(const WebRect& rect)
{
    if (!mainFrameImpl())
        return;

    WebRect blockBounds = computeBlockBounds(rect, true);

    if (blockBounds.isEmpty()) {
        // Keep current scale (no need to scroll as x,y will normally already
        // be visible). FIXME: Revisit this if it isn't always true.
        return;
    }

    float scale;
    WebPoint scroll;

    computeScaleAndScrollForBlockRect(WebPoint(rect.x, rect.y), blockBounds, nonUserInitiatedPointPadding, minimumPageScaleFactor(), scale, scroll);

    startPageScaleAnimation(scroll, false, scale, findInPageAnimationDurationInSeconds);
}

bool WebViewImpl::zoomToMultipleTargetsRect(const WebRect& rect)
{
    if (!mainFrameImpl())
        return false;

    float scale;
    WebPoint scroll;

    computeScaleAndScrollForBlockRect(WebPoint(rect.x, rect.y), rect, nonUserInitiatedPointPadding, minimumPageScaleFactor(), scale, scroll);

    if (scale <= pageScaleFactor())
        return false;

    startPageScaleAnimation(scroll, false, scale, multipleTargetsZoomAnimationDurationInSeconds);
    return true;
}

void WebViewImpl::hasTouchEventHandlers(bool hasTouchHandlers)
{
    if (m_client)
        m_client->hasTouchEventHandlers(hasTouchHandlers);
}

bool WebViewImpl::hasTouchEventHandlersAt(const WebPoint& point)
{
    // FIXME: Implement this. Note that the point must be divided by pageScaleFactor.
    return true;
}

#if !OS(MACOSX)
// Mac has no way to open a context menu based on a keyboard event.
bool WebViewImpl::sendContextMenuEvent(const WebKeyboardEvent& event)
{
    // The contextMenuController() holds onto the last context menu that was
    // popped up on the page until a new one is created. We need to clear
    // this menu before propagating the event through the DOM so that we can
    // detect if we create a new menu for this event, since we won't create
    // a new menu if the DOM swallows the event and the defaultEventHandler does
    // not run.
    page()->contextMenuController().clearContextMenu();

    m_contextMenuAllowed = true;
    Frame* focusedFrame = page()->focusController().focusedOrMainFrame();
    if (!focusedFrame->isLocalFrame())
        return false;
    bool handled = toLocalFrame(focusedFrame)->eventHandler().sendContextMenuEventForKey();
    m_contextMenuAllowed = false;
    return handled;
}
#endif

bool WebViewImpl::keyEventDefault(const WebKeyboardEvent& event)
{
    LocalFrame* frame = toLocalFrame(focusedWebCoreFrame());
    if (!frame)
        return false;

    switch (event.type) {
    case WebInputEvent::Char:
        if (event.windowsKeyCode == VKEY_SPACE) {
            int keyCode = ((event.modifiers & WebInputEvent::ShiftKey) ? VKEY_PRIOR : VKEY_NEXT);
            return scrollViewWithKeyboard(keyCode, event.modifiers);
        }
        break;
    case WebInputEvent::RawKeyDown:
        if (event.modifiers == WebInputEvent::ControlKey) {
            switch (event.windowsKeyCode) {
#if !OS(MACOSX)
            case 'A':
                focusedFrame()->executeCommand(WebString::fromUTF8("SelectAll"));
                return true;
            case VKEY_INSERT:
            case 'C':
                focusedFrame()->executeCommand(WebString::fromUTF8("Copy"));
                return true;
#endif
            // Match FF behavior in the sense that Ctrl+home/end are the only Ctrl
            // key combinations which affect scrolling. Safari is buggy in the
            // sense that it scrolls the page for all Ctrl+scrolling key
            // combinations. For e.g. Ctrl+pgup/pgdn/up/down, etc.
            case VKEY_HOME:
            case VKEY_END:
                break;
            default:
                return false;
            }
        }
        if (!event.isSystemKey && !(event.modifiers & WebInputEvent::ShiftKey))
            return scrollViewWithKeyboard(event.windowsKeyCode, event.modifiers);
        break;
    default:
        break;
    }
    return false;
}

bool WebViewImpl::scrollViewWithKeyboard(int keyCode, int modifiers)
{
    ScrollDirection scrollDirection;
    ScrollGranularity scrollGranularity;
#if OS(MACOSX)
    // Control-Up/Down should be PageUp/Down on Mac.
    if (modifiers & WebMouseEvent::ControlKey) {
      if (keyCode == VKEY_UP)
        keyCode = VKEY_PRIOR;
      else if (keyCode == VKEY_DOWN)
        keyCode = VKEY_NEXT;
    }
#endif
    if (!mapKeyCodeForScroll(keyCode, &scrollDirection, &scrollGranularity))
        return false;
    return bubblingScroll(scrollDirection, scrollGranularity);
}

bool WebViewImpl::mapKeyCodeForScroll(int keyCode,
                                      WebCore::ScrollDirection* scrollDirection,
                                      WebCore::ScrollGranularity* scrollGranularity)
{
    switch (keyCode) {
    case VKEY_LEFT:
        *scrollDirection = ScrollLeft;
        *scrollGranularity = ScrollByLine;
        break;
    case VKEY_RIGHT:
        *scrollDirection = ScrollRight;
        *scrollGranularity = ScrollByLine;
        break;
    case VKEY_UP:
        *scrollDirection = ScrollUp;
        *scrollGranularity = ScrollByLine;
        break;
    case VKEY_DOWN:
        *scrollDirection = ScrollDown;
        *scrollGranularity = ScrollByLine;
        break;
    case VKEY_HOME:
        *scrollDirection = ScrollUp;
        *scrollGranularity = ScrollByDocument;
        break;
    case VKEY_END:
        *scrollDirection = ScrollDown;
        *scrollGranularity = ScrollByDocument;
        break;
    case VKEY_PRIOR:  // page up
        *scrollDirection = ScrollUp;
        *scrollGranularity = ScrollByPage;
        break;
    case VKEY_NEXT:  // page down
        *scrollDirection = ScrollDown;
        *scrollGranularity = ScrollByPage;
        break;
    default:
        return false;
    }

    return true;
}

void WebViewImpl::hideSelectPopup()
{
    if (m_selectPopup)
        m_selectPopup->hidePopup();
}

bool WebViewImpl::bubblingScroll(ScrollDirection scrollDirection, ScrollGranularity scrollGranularity)
{
    LocalFrame* frame = toLocalFrame(focusedWebCoreFrame());
    if (!frame)
        return false;

    return frame->eventHandler().bubblingScroll(scrollDirection, scrollGranularity);
}

void WebViewImpl::popupOpened(PopupContainer* popupContainer)
{
    ASSERT(!m_selectPopup);
    m_selectPopup = popupContainer;
    ASSERT(mainFrameImpl()->frame()->document());
    Document& document = *mainFrameImpl()->frame()->document();
    page()->frameHost().eventHandlerRegistry().didAddEventHandler(document, EventHandlerRegistry::WheelEvent);
}

void WebViewImpl::popupClosed(PopupContainer* popupContainer)
{
    ASSERT(m_selectPopup);
    m_selectPopup = nullptr;
    ASSERT(mainFrameImpl()->frame()->document());
    Document& document = *mainFrameImpl()->frame()->document();
    // Remove the handler we added in |popupOpened| conditionally, because the
    // Document may have already removed it, for instance, due to a navigation.
    EventHandlerRegistry* registry = &document.frameHost()->eventHandlerRegistry();
    if (registry->eventHandlerTargets(EventHandlerRegistry::WheelEvent)->contains(&document))
        registry->didRemoveEventHandler(document, EventHandlerRegistry::WheelEvent);
}

PagePopup* WebViewImpl::openPagePopup(PagePopupClient* client, const IntRect& originBoundsInRootView)
{
    ASSERT(client);
    if (hasOpenedPopup())
        hidePopups();
    ASSERT(!m_pagePopup);

    WebWidget* popupWidget = m_client->createPopupMenu(WebPopupTypePage);
    ASSERT(popupWidget);
    m_pagePopup = toWebPagePopupImpl(popupWidget);
    if (!m_pagePopup->initialize(this, client, originBoundsInRootView)) {
        m_pagePopup->closePopup();
        m_pagePopup = nullptr;
    }
    return m_pagePopup.get();
}

void WebViewImpl::closePagePopup(PagePopup* popup)
{
    ASSERT(popup);
    WebPagePopupImpl* popupImpl = toWebPagePopupImpl(popup);
    ASSERT(m_pagePopup.get() == popupImpl);
    if (m_pagePopup.get() != popupImpl)
        return;
    m_pagePopup->closePopup();
    m_pagePopup = nullptr;
}

Frame* WebViewImpl::focusedWebCoreFrame() const
{
    return m_page ? m_page->focusController().focusedOrMainFrame() : 0;
}

WebViewImpl* WebViewImpl::fromPage(Page* page)
{
    if (!page)
        return 0;
    return static_cast<WebViewImpl*>(page->chrome().client().webView());
}

// WebWidget ------------------------------------------------------------------

void WebViewImpl::close()
{
    if (m_page) {
        // Initiate shutdown for the entire frameset.  This will cause a lot of
        // notifications to be sent.
        m_page->willBeDestroyed();
        m_page.clear();
    }

    // Should happen after m_page.clear().
    if (m_devToolsAgent)
        m_devToolsAgent.clear();

    // Reset the delegate to prevent notifications being sent as we're being
    // deleted.
    m_client = 0;

    deref();  // Balances ref() acquired in WebView::create
}

void WebViewImpl::willStartLiveResize()
{
    if (mainFrameImpl() && mainFrameImpl()->frameView())
        mainFrameImpl()->frameView()->willStartLiveResize();

    LocalFrame* frame = mainFrameImpl()->frame();
    WebPluginContainerImpl* pluginContainer = WebLocalFrameImpl::pluginContainerFromFrame(frame);
    if (pluginContainer)
        pluginContainer->willStartLiveResize();
}

WebSize WebViewImpl::size()
{
    return m_size;
}

void WebViewImpl::resizePinchViewport(const WebSize& newSize)
{
    if (!pinchVirtualViewportEnabled())
        return;

    page()->frameHost().pinchViewport().setSize(newSize);
}

void WebViewImpl::resize(const WebSize& newSize)
{
    if (m_shouldAutoResize || m_size == newSize)
        return;

    FrameView* view = mainFrameImpl()->frameView();
    if (!view)
        return;

    WebSize oldSize = m_size;
    float oldPageScaleFactor = pageScaleFactor();
    int oldContentsWidth = contentsSize().width();

    m_size = newSize;

    bool shouldAnchorAndRescaleViewport = settings()->mainFrameResizesAreOrientationChanges()
        && oldSize.width && oldContentsWidth && newSize.width != oldSize.width && !m_fullscreenController->isFullscreen();

    ViewportAnchor viewportAnchor(&mainFrameImpl()->frame()->eventHandler());
    if (shouldAnchorAndRescaleViewport) {
        viewportAnchor.setAnchor(view->visibleContentRect(),
                                 FloatSize(viewportAnchorXCoord, viewportAnchorYCoord));
    }

    {
        // Avoids unnecessary invalidations while various bits of state in FastTextAutosizer are updated.
        FastTextAutosizer::DeferUpdatePageInfo deferUpdatePageInfo(page());

        m_pageScaleConstraintsSet.didChangeViewSize(m_size);

        updatePageDefinedViewportConstraints(mainFrameImpl()->frame()->document()->viewportDescription());
        updateMainFrameLayoutSize();

        // If the virtual viewport pinch mode is enabled, the main frame will be resized
        // after layout so it can be sized to the contentsSize.
        if (!pinchVirtualViewportEnabled() && mainFrameImpl()->frameView())
            mainFrameImpl()->frameView()->resize(m_size);

        if (pinchVirtualViewportEnabled())
            page()->frameHost().pinchViewport().setSize(m_size);

        // When device emulation is enabled, device size values may change - they are
        // usually set equal to the view size. These values are not considered viewport-dependent
        // (see MediaQueryExp::isViewportDependent), since they are only viewport-dependent in emulation mode,
        // and thus will not be invalidated in |FrameView::performPreLayoutTasks|.
        // Therefore we should force explicit media queries invalidation here.
        if (page()->inspectorController().deviceEmulationEnabled()) {
            if (Document* document = mainFrameImpl()->frame()->document())
                document->mediaQueryAffectingValueChanged();
        }
    }

    if (settings()->viewportEnabled() && !m_fixedLayoutSizeLock) {
        // Relayout immediately to recalculate the minimum scale limit.
        if (view->needsLayout())
            view->layout();

        if (shouldAnchorAndRescaleViewport) {
            float viewportWidthRatio = static_cast<float>(newSize.width) / oldSize.width;
            float contentsWidthRatio = static_cast<float>(contentsSize().width()) / oldContentsWidth;
            float scaleMultiplier = viewportWidthRatio / contentsWidthRatio;

            IntSize viewportSize = view->visibleContentRect().size();
            if (scaleMultiplier != 1) {
                float newPageScaleFactor = oldPageScaleFactor * scaleMultiplier;
                viewportSize.scale(pageScaleFactor() / newPageScaleFactor);
                IntPoint scrollOffsetAtNewScale = viewportAnchor.computeOrigin(viewportSize);
                setPageScaleFactor(newPageScaleFactor, scrollOffsetAtNewScale);
            } else {
                IntPoint scrollOffsetAtNewScale = clampOffsetAtScale(viewportAnchor.computeOrigin(viewportSize), pageScaleFactor());
                updateMainFrameScrollPosition(scrollOffsetAtNewScale, false);
            }
        }
    }

    sendResizeEventAndRepaint();
}

void WebViewImpl::willEndLiveResize()
{
    if (mainFrameImpl() && mainFrameImpl()->frameView())
        mainFrameImpl()->frameView()->willEndLiveResize();

    LocalFrame* frame = mainFrameImpl()->frame();
    WebPluginContainerImpl* pluginContainer = WebLocalFrameImpl::pluginContainerFromFrame(frame);
    if (pluginContainer)
        pluginContainer->willEndLiveResize();
}

void WebViewImpl::willEnterFullScreen()
{
    m_fullscreenController->willEnterFullScreen();
}

void WebViewImpl::didEnterFullScreen()
{
    m_fullscreenController->didEnterFullScreen();
}

void WebViewImpl::willExitFullScreen()
{
    m_fullscreenController->willExitFullScreen();
}

void WebViewImpl::didExitFullScreen()
{
    m_fullscreenController->didExitFullScreen();
}

void WebViewImpl::animate(double monotonicFrameBeginTime)
{
    TRACE_EVENT0("webkit", "WebViewImpl::animate");

    if (!monotonicFrameBeginTime)
        monotonicFrameBeginTime = monotonicallyIncreasingTime();

    // Create synthetic wheel events as necessary for fling.
    if (m_gestureAnimation) {
        if (m_gestureAnimation->animate(monotonicFrameBeginTime))
            scheduleAnimation();
        else {
            endActiveFlingAnimation();

            PlatformGestureEvent endScrollEvent(PlatformEvent::GestureScrollEnd,
                m_positionOnFlingStart, m_globalPositionOnFlingStart,
                IntSize(), 0, false, false, false, false,
                0, 0, 0, 0);

            mainFrameImpl()->frame()->eventHandler().handleGestureScrollEnd(endScrollEvent);
        }
    }

    if (!m_page)
        return;

    PageWidgetDelegate::animate(m_page.get(), monotonicFrameBeginTime);

    if (m_continuousPaintingEnabled) {
        ContinuousPainter::setNeedsDisplayRecursive(m_rootGraphicsLayer, m_pageOverlays.get());
        m_client->scheduleAnimation();
    }
}

void WebViewImpl::layout()
{
    TRACE_EVENT0("webkit", "WebViewImpl::layout");
    PageWidgetDelegate::layout(m_page.get());
    updateLayerTreeBackgroundColor();

    for (size_t i = 0; i < m_linkHighlights.size(); ++i)
        m_linkHighlights[i]->updateGeometry();
}

void WebViewImpl::paint(WebCanvas* canvas, const WebRect& rect)
{
    // This should only be used when compositing is not being used for this
    // WebView, and it is painting into the recording of its parent.
    ASSERT(!isAcceleratedCompositingActive());

    double paintStart = currentTime();
    PageWidgetDelegate::paint(m_page.get(), pageOverlays(), canvas, rect, isTransparent() ? PageWidgetDelegate::Translucent : PageWidgetDelegate::Opaque);
    double paintEnd = currentTime();
    double pixelsPerSec = (rect.width * rect.height) / (paintEnd - paintStart);
    blink::Platform::current()->histogramCustomCounts("Renderer4.SoftwarePaintDurationMS", (paintEnd - paintStart) * 1000, 0, 120, 30);
    blink::Platform::current()->histogramCustomCounts("Renderer4.SoftwarePaintMegapixPerSecond", pixelsPerSec / 1000000, 10, 210, 30);
}

#if OS(ANDROID)
void WebViewImpl::paintCompositedDeprecated(WebCanvas* canvas, const WebRect& rect)
{
    // Note: This method exists on OS(ANDROID) and will hopefully be
    //       removed once the link disambiguation feature renders using
    //       the compositor.
    ASSERT(isAcceleratedCompositingActive());

    FrameView* view = page()->deprecatedLocalMainFrame()->view();
    PaintBehavior oldPaintBehavior = view->paintBehavior();
    view->setPaintBehavior(oldPaintBehavior | PaintBehaviorFlattenCompositingLayers);

    PageWidgetDelegate::paint(m_page.get(), pageOverlays(), canvas, rect, isTransparent() ? PageWidgetDelegate::Translucent : PageWidgetDelegate::Opaque);

    view->setPaintBehavior(oldPaintBehavior);
}
#endif

void WebViewImpl::compositeAndReadbackAsync(WebCompositeAndReadbackAsyncCallback* callback)
{
    ASSERT(isAcceleratedCompositingActive());
    m_layerTreeView->compositeAndReadbackAsync(callback);
}

bool WebViewImpl::isTrackingRepaints() const
{
    if (!page())
        return false;
    if (!page()->mainFrame()->isLocalFrame())
        return false;
    FrameView* view = page()->deprecatedLocalMainFrame()->view();
    return view->isTrackingPaintInvalidations();
}

void WebViewImpl::themeChanged()
{
    if (!page())
        return;
    if (!page()->mainFrame()->isLocalFrame())
        return;
    FrameView* view = page()->deprecatedLocalMainFrame()->view();

    WebRect damagedRect(0, 0, m_size.width, m_size.height);
    view->invalidateRect(damagedRect);
}

void WebViewImpl::enterFullScreenForElement(WebCore::Element* element)
{
    m_fullscreenController->enterFullScreenForElement(element);
}

void WebViewImpl::exitFullScreenForElement(WebCore::Element* element)
{
    m_fullscreenController->exitFullScreenForElement(element);
}

bool WebViewImpl::hasHorizontalScrollbar()
{
    return mainFrameImpl()->frameView()->horizontalScrollbar();
}

bool WebViewImpl::hasVerticalScrollbar()
{
    return mainFrameImpl()->frameView()->verticalScrollbar();
}

const WebInputEvent* WebViewImpl::m_currentInputEvent = 0;

// FIXME: autogenerate this kind of code, and use it throughout Blink rather than
// the one-offs for subsets of these values.
static const AtomicString* inputTypeToName(WebInputEvent::Type type)
{
    switch (type) {
    case WebInputEvent::MouseDown:
        return &EventTypeNames::mousedown;
    case WebInputEvent::MouseUp:
        return &EventTypeNames::mouseup;
    case WebInputEvent::MouseMove:
        return &EventTypeNames::mousemove;
    case WebInputEvent::MouseEnter:
        return &EventTypeNames::mouseenter;
    case WebInputEvent::MouseLeave:
        return &EventTypeNames::mouseleave;
    case WebInputEvent::ContextMenu:
        return &EventTypeNames::contextmenu;
    case WebInputEvent::MouseWheel:
        return &EventTypeNames::mousewheel;
    case WebInputEvent::KeyDown:
        return &EventTypeNames::keydown;
    case WebInputEvent::KeyUp:
        return &EventTypeNames::keyup;
    case WebInputEvent::GestureScrollBegin:
        return &EventTypeNames::gesturescrollstart;
    case WebInputEvent::GestureScrollEnd:
        return &EventTypeNames::gesturescrollend;
    case WebInputEvent::GestureScrollUpdate:
        return &EventTypeNames::gesturescrollupdate;
    case WebInputEvent::TouchStart:
        return &EventTypeNames::touchstart;
    case WebInputEvent::TouchMove:
        return &EventTypeNames::touchmove;
    case WebInputEvent::TouchEnd:
        return &EventTypeNames::touchend;
    case WebInputEvent::TouchCancel:
        return &EventTypeNames::touchcancel;
    default:
        return 0;
    }
}

bool WebViewImpl::handleInputEvent(const WebInputEvent& inputEvent)
{
    UserGestureNotifier notifier(m_autofillClient, &m_userGestureObserved);
    // On the first input event since page load, |notifier| instructs the
    // autofill client to unblock values of password input fields of any forms
    // on the page. There is a single input event, GestureTap, which can both
    // be the first event after page load, and cause a form submission. In that
    // case, the form submission happens before the autofill client is told
    // to unblock the password values, and so the password values are not
    // submitted. To avoid that, GestureTap is handled explicitly:
    if (inputEvent.type == WebInputEvent::GestureTap && m_autofillClient) {
        m_userGestureObserved = true;
        m_autofillClient->firstUserGestureObserved();
    }

    const AtomicString* inputEventName = inputTypeToName(inputEvent.type);
    TRACE_EVENT1("input", "WebViewImpl::handleInputEvent", "type", inputEventName ? inputEventName->ascii() : "unknown");
    // If we've started a drag and drop operation, ignore input events until
    // we're done.
    if (m_doingDragAndDrop)
        return true;

    if (m_devToolsAgent && m_devToolsAgent->handleInputEvent(m_page.get(), inputEvent))
        return true;

    // Report the event to be NOT processed by WebKit, so that the browser can handle it appropriately.
    if (m_ignoreInputEvents)
        return false;

    TemporaryChange<const WebInputEvent*> currentEventChange(m_currentInputEvent, &inputEvent);

    if (isPointerLocked() && WebInputEvent::isMouseEventType(inputEvent.type)) {
      pointerLockMouseEvent(inputEvent);
      return true;
    }

    if (m_mouseCaptureNode && WebInputEvent::isMouseEventType(inputEvent.type)) {
        TRACE_EVENT1("input", "captured mouse event", "type", inputEvent.type);
        // Save m_mouseCaptureNode since mouseCaptureLost() will clear it.
        RefPtrWillBeRawPtr<Node> node = m_mouseCaptureNode;

        // Not all platforms call mouseCaptureLost() directly.
        if (inputEvent.type == WebInputEvent::MouseUp)
            mouseCaptureLost();

        OwnPtr<UserGestureIndicator> gestureIndicator;

        AtomicString eventType;
        switch (inputEvent.type) {
        case WebInputEvent::MouseMove:
            eventType = EventTypeNames::mousemove;
            break;
        case WebInputEvent::MouseLeave:
            eventType = EventTypeNames::mouseout;
            break;
        case WebInputEvent::MouseDown:
            eventType = EventTypeNames::mousedown;
            gestureIndicator = adoptPtr(new UserGestureIndicator(DefinitelyProcessingNewUserGesture));
            m_mouseCaptureGestureToken = gestureIndicator->currentToken();
            break;
        case WebInputEvent::MouseUp:
            eventType = EventTypeNames::mouseup;
            gestureIndicator = adoptPtr(new UserGestureIndicator(m_mouseCaptureGestureToken.release()));
            break;
        default:
            ASSERT_NOT_REACHED();
        }

        node->dispatchMouseEvent(
              PlatformMouseEventBuilder(mainFrameImpl()->frameView(), *static_cast<const WebMouseEvent*>(&inputEvent)),
              eventType, static_cast<const WebMouseEvent*>(&inputEvent)->clickCount);
        return true;
    }

    return PageWidgetDelegate::handleInputEvent(m_page.get(), *this, inputEvent);
}

void WebViewImpl::setCursorVisibilityState(bool isVisible)
{
    if (m_page)
        m_page->setIsCursorVisible(isVisible);
}

void WebViewImpl::mouseCaptureLost()
{
    TRACE_EVENT_ASYNC_END0("input", "capturing mouse", this);
    m_mouseCaptureNode = nullptr;
}

void WebViewImpl::setFocus(bool enable)
{
    m_page->focusController().setFocused(enable);
    if (enable) {
        m_page->focusController().setActive(true);
        RefPtr<Frame> focusedFrame = m_page->focusController().focusedFrame();
        if (focusedFrame && focusedFrame->isLocalFrame()) {
            LocalFrame* localFrame = toLocalFrame(focusedFrame.get());
            Element* element = localFrame->document()->focusedElement();
            if (element && localFrame->selection().selection().isNone()) {
                // If the selection was cleared while the WebView was not
                // focused, then the focus element shows with a focus ring but
                // no caret and does respond to keyboard inputs.
                if (element->isTextFormControl()) {
                    element->updateFocusAppearance(true);
                } else if (element->isContentEditable()) {
                    // updateFocusAppearance() selects all the text of
                    // contentseditable DIVs. So we set the selection explicitly
                    // instead. Note that this has the side effect of moving the
                    // caret back to the beginning of the text.
                    Position position(element, 0, Position::PositionIsOffsetInAnchor);
                    localFrame->selection().setSelection(VisibleSelection(position, SEL_DEFAULT_AFFINITY));
                }
            }
        }
        m_imeAcceptEvents = true;
    } else {
        hidePopups();

        // Clear focus on the currently focused frame if any.
        if (!m_page)
            return;

        LocalFrame* frame = m_page->mainFrame() && m_page->mainFrame()->isLocalFrame()
            ? m_page->deprecatedLocalMainFrame() : 0;
        if (!frame)
            return;

        RefPtr<Frame> focusedFrame = m_page->focusController().focusedFrame();
        if (focusedFrame && focusedFrame->isLocalFrame()) {
            // Finish an ongoing composition to delete the composition node.
            if (toLocalFrame(focusedFrame.get())->inputMethodController().hasComposition()) {
                if (m_autofillClient)
                    m_autofillClient->setIgnoreTextChanges(true);

                toLocalFrame(focusedFrame.get())->inputMethodController().confirmComposition();

                if (m_autofillClient)
                    m_autofillClient->setIgnoreTextChanges(false);
            }
            m_imeAcceptEvents = false;
        }
    }
}

bool WebViewImpl::setComposition(
    const WebString& text,
    const WebVector<WebCompositionUnderline>& underlines,
    int selectionStart,
    int selectionEnd)
{
    LocalFrame* focused = toLocalFrame(focusedWebCoreFrame());
    if (!focused || !m_imeAcceptEvents)
        return false;

    if (WebPlugin* plugin = focusedPluginIfInputMethodSupported(focused))
        return plugin->setComposition(text, underlines, selectionStart, selectionEnd);

    // The input focus has been moved to another WebWidget object.
    // We should use this |editor| object only to complete the ongoing
    // composition.
    InputMethodController& inputMethodController = focused->inputMethodController();
    if (!focused->editor().canEdit() && !inputMethodController.hasComposition())
        return false;

    // We should verify the parent node of this IME composition node are
    // editable because JavaScript may delete a parent node of the composition
    // node. In this case, WebKit crashes while deleting texts from the parent
    // node, which doesn't exist any longer.
    RefPtrWillBeRawPtr<Range> range = inputMethodController.compositionRange();
    if (range) {
        Node* node = range->startContainer();
        if (!node || !node->isContentEditable())
            return false;
    }

    // If we're not going to fire a keypress event, then the keydown event was
    // canceled.  In that case, cancel any existing composition.
    if (text.isEmpty() || m_suppressNextKeypressEvent) {
        // A browser process sent an IPC message which does not contain a valid
        // string, which means an ongoing composition has been canceled.
        // If the ongoing composition has been canceled, replace the ongoing
        // composition string with an empty string and complete it.
        String emptyString;
        Vector<CompositionUnderline> emptyUnderlines;
        inputMethodController.setComposition(emptyString, emptyUnderlines, 0, 0);
        return text.isEmpty();
    }

    // When the range of composition underlines overlap with the range between
    // selectionStart and selectionEnd, WebKit somehow won't paint the selection
    // at all (see InlineTextBox::paint() function in InlineTextBox.cpp).
    // But the selection range actually takes effect.
    inputMethodController.setComposition(String(text),
                           CompositionUnderlineVectorBuilder(underlines),
                           selectionStart, selectionEnd);

    return inputMethodController.hasComposition();
}

bool WebViewImpl::confirmComposition()
{
    return confirmComposition(DoNotKeepSelection);
}

bool WebViewImpl::confirmComposition(ConfirmCompositionBehavior selectionBehavior)
{
    return confirmComposition(WebString(), selectionBehavior);
}

bool WebViewImpl::confirmComposition(const WebString& text)
{
    return confirmComposition(text, DoNotKeepSelection);
}

bool WebViewImpl::confirmComposition(const WebString& text, ConfirmCompositionBehavior selectionBehavior)
{
    LocalFrame* focused = toLocalFrame(focusedWebCoreFrame());
    if (!focused || !m_imeAcceptEvents)
        return false;

    if (WebPlugin* plugin = focusedPluginIfInputMethodSupported(focused))
        return plugin->confirmComposition(text, selectionBehavior);

    return focused->inputMethodController().confirmCompositionOrInsertText(text, selectionBehavior == KeepSelection ? InputMethodController::KeepSelection : InputMethodController::DoNotKeepSelection);
}

bool WebViewImpl::compositionRange(size_t* location, size_t* length)
{
    LocalFrame* focused = toLocalFrame(focusedWebCoreFrame());
    if (!focused || !m_imeAcceptEvents)
        return false;

    RefPtrWillBeRawPtr<Range> range = focused->inputMethodController().compositionRange();
    if (!range)
        return false;

    Element* editable = focused->selection().rootEditableElementOrDocumentElement();
    ASSERT(editable);
    PlainTextRange plainTextRange(PlainTextRange::create(*editable, *range.get()));
    if (plainTextRange.isNull())
        return false;
    *location = plainTextRange.start();
    *length = plainTextRange.length();
    return true;
}

WebTextInputInfo WebViewImpl::textInputInfo()
{
    WebTextInputInfo info;

    LocalFrame* focused = toLocalFrame(focusedWebCoreFrame());
    if (!focused)
        return info;

    FrameSelection& selection = focused->selection();
    Node* node = selection.selection().rootEditableElement();
    if (!node)
        return info;

    info.inputMode = inputModeOfFocusedElement();

    info.type = textInputType();
    if (info.type == WebTextInputTypeNone)
        return info;

    if (!focused->editor().canEdit())
        return info;

    // Emits an object replacement character for each replaced element so that
    // it is exposed to IME and thus could be deleted by IME on android.
    info.value = plainText(rangeOfContents(node).get(), TextIteratorEmitsObjectReplacementCharacter);

    if (info.value.isEmpty())
        return info;

    if (RefPtrWillBeRawPtr<Range> range = selection.selection().firstRange()) {
        PlainTextRange plainTextRange(PlainTextRange::create(*node, *range.get()));
        if (plainTextRange.isNotNull()) {
            info.selectionStart = plainTextRange.start();
            info.selectionEnd = plainTextRange.end();
        }
    }

    if (RefPtrWillBeRawPtr<Range> range = focused->inputMethodController().compositionRange()) {
        PlainTextRange plainTextRange(PlainTextRange::create(*node, *range.get()));
        if (plainTextRange.isNotNull()) {
            info.compositionStart = plainTextRange.start();
            info.compositionEnd = plainTextRange.end();
        }
    }

    return info;
}

WebTextInputType WebViewImpl::textInputType()
{
    Element* element = focusedElement();
    if (!element)
        return WebTextInputTypeNone;

    if (isHTMLInputElement(*element)) {
        HTMLInputElement& input = toHTMLInputElement(*element);

        if (input.isDisabledOrReadOnly())
            return WebTextInputTypeNone;

        if (input.isPasswordField())
            return WebTextInputTypePassword;
        if (input.isSearchField())
            return WebTextInputTypeSearch;
        if (input.isEmailField())
            return WebTextInputTypeEmail;
        if (input.isNumberField())
            return WebTextInputTypeNumber;
        if (input.isTelephoneField())
            return WebTextInputTypeTelephone;
        if (input.isURLField())
            return WebTextInputTypeURL;
        if (input.isDateField())
            return WebTextInputTypeDate;
        if (input.isDateTimeLocalField())
            return WebTextInputTypeDateTimeLocal;
        if (input.isMonthField())
            return WebTextInputTypeMonth;
        if (input.isTimeField())
            return WebTextInputTypeTime;
        if (input.isWeekField())
            return WebTextInputTypeWeek;
        if (input.isTextField())
            return WebTextInputTypeText;

        return WebTextInputTypeNone;
    }

    if (isHTMLTextAreaElement(*element)) {
        if (toHTMLTextAreaElement(*element).isDisabledOrReadOnly())
            return WebTextInputTypeNone;
        return WebTextInputTypeTextArea;
    }

#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
    if (element->isHTMLElement()) {
        if (toHTMLElement(element)->isDateTimeFieldElement())
            return WebTextInputTypeDateTimeField;
    }
#endif

    if (element->shouldUseInputMethod())
        return WebTextInputTypeContentEditable;

    return WebTextInputTypeNone;
}

WebString WebViewImpl::inputModeOfFocusedElement()
{
    if (!RuntimeEnabledFeatures::inputModeAttributeEnabled())
        return WebString();

    Element* element = focusedElement();
    if (!element)
        return WebString();

    if (isHTMLInputElement(*element)) {
        const HTMLInputElement& input = toHTMLInputElement(*element);
        if (input.supportsInputModeAttribute())
            return input.fastGetAttribute(HTMLNames::inputmodeAttr).lower();
        return WebString();
    }
    if (isHTMLTextAreaElement(*element)) {
        const HTMLTextAreaElement& textarea = toHTMLTextAreaElement(*element);
        return textarea.fastGetAttribute(HTMLNames::inputmodeAttr).lower();
    }

    return WebString();
}

bool WebViewImpl::selectionBounds(WebRect& anchor, WebRect& focus) const
{
    const LocalFrame* frame = toLocalFrame(focusedWebCoreFrame());
    if (!frame)
        return false;
    FrameSelection& selection = frame->selection();

    if (selection.isCaret()) {
        anchor = focus = selection.absoluteCaretBounds();
    } else {
        RefPtrWillBeRawPtr<Range> selectedRange = selection.toNormalizedRange();
        if (!selectedRange)
            return false;

        RefPtrWillBeRawPtr<Range> range(Range::create(selectedRange->startContainer()->document(),
            selectedRange->startContainer(),
            selectedRange->startOffset(),
            selectedRange->startContainer(),
            selectedRange->startOffset()));
        anchor = frame->editor().firstRectForRange(range.get());

        range = Range::create(selectedRange->endContainer()->document(),
            selectedRange->endContainer(),
            selectedRange->endOffset(),
            selectedRange->endContainer(),
            selectedRange->endOffset());
        focus = frame->editor().firstRectForRange(range.get());
    }

    IntRect scaledAnchor(frame->view()->contentsToWindow(anchor));
    IntRect scaledFocus(frame->view()->contentsToWindow(focus));

    if (pinchVirtualViewportEnabled()) {
        // FIXME(http://crbug.com/371902) - We shouldn't have to do this
        // manually, the contentsToWindow methods above should be fixed to do
        // this.
        IntPoint pinchViewportOffset =
            roundedIntPoint(page()->frameHost().pinchViewport().visibleRect().location());
        scaledAnchor.moveBy(-pinchViewportOffset);
        scaledFocus.moveBy(-pinchViewportOffset);
    }

    scaledAnchor.scale(pageScaleFactor());
    scaledFocus.scale(pageScaleFactor());
    anchor = scaledAnchor;
    focus = scaledFocus;

    if (!selection.selection().isBaseFirst())
        std::swap(anchor, focus);
    return true;
}

InputMethodContext* WebViewImpl::inputMethodContext()
{
    if (!m_imeAcceptEvents)
        return 0;

    LocalFrame* focusedFrame = toLocalFrame(focusedWebCoreFrame());
    if (!focusedFrame)
        return 0;

    Element* target = focusedFrame->document()->focusedElement();
    if (target && target->hasInputMethodContext())
        return &target->inputMethodContext();

    return 0;
}

WebPlugin* WebViewImpl::focusedPluginIfInputMethodSupported(LocalFrame* frame)
{
    WebPluginContainerImpl* container = WebLocalFrameImpl::pluginContainerFromNode(frame, WebNode(focusedElement()));
    if (container && container->supportsInputMethod())
        return container->plugin();
    return 0;
}

void WebViewImpl::didShowCandidateWindow()
{
    if (InputMethodContext* context = inputMethodContext())
        context->dispatchCandidateWindowShowEvent();
}

void WebViewImpl::didUpdateCandidateWindow()
{
    if (InputMethodContext* context = inputMethodContext())
        context->dispatchCandidateWindowUpdateEvent();
}

void WebViewImpl::didHideCandidateWindow()
{
    if (InputMethodContext* context = inputMethodContext())
        context->dispatchCandidateWindowHideEvent();
}

bool WebViewImpl::selectionTextDirection(WebTextDirection& start, WebTextDirection& end) const
{
    const LocalFrame* frame = toLocalFrame(focusedWebCoreFrame());
    if (!frame)
        return false;
    FrameSelection& selection = frame->selection();
    if (!selection.toNormalizedRange())
        return false;
    start = selection.start().primaryDirection() == RTL ? WebTextDirectionRightToLeft : WebTextDirectionLeftToRight;
    end = selection.end().primaryDirection() == RTL ? WebTextDirectionRightToLeft : WebTextDirectionLeftToRight;
    return true;
}

bool WebViewImpl::isSelectionAnchorFirst() const
{
    if (const LocalFrame* frame = toLocalFrame(focusedWebCoreFrame()))
        return frame->selection().selection().isBaseFirst();
    return false;
}

WebVector<WebCompositionUnderline> WebViewImpl::compositionUnderlines() const
{
    const LocalFrame* focused = toLocalFrame(focusedWebCoreFrame());
    if (!focused)
        return WebVector<WebCompositionUnderline>();
    const Vector<CompositionUnderline>& underlines = focused->inputMethodController().customCompositionUnderlines();
    WebVector<WebCompositionUnderline> results(underlines.size());
    for (size_t index = 0; index < underlines.size(); ++index) {
        CompositionUnderline underline = underlines[index];
        results[index] = WebCompositionUnderline(underline.startOffset, underline.endOffset, static_cast<WebColor>(underline.color.rgb()), underline.thick, static_cast<WebColor>(underline.backgroundColor.rgb()));
    }
    return results;
}

WebColor WebViewImpl::backgroundColor() const
{
    if (isTransparent())
        return Color::transparent;
    if (!m_page)
        return m_baseBackgroundColor;
    if (!m_page->mainFrame())
        return m_baseBackgroundColor;
    if (!m_page->mainFrame()->isLocalFrame())
        return m_baseBackgroundColor;
    FrameView* view = m_page->deprecatedLocalMainFrame()->view();
    return view->documentBackgroundColor().rgb();
}

bool WebViewImpl::caretOrSelectionRange(size_t* location, size_t* length)
{
    const LocalFrame* focused = toLocalFrame(focusedWebCoreFrame());
    if (!focused)
        return false;

    PlainTextRange selectionOffsets = focused->inputMethodController().getSelectionOffsets();
    if (selectionOffsets.isNull())
        return false;

    *location = selectionOffsets.start();
    *length = selectionOffsets.length();
    return true;
}

void WebViewImpl::setTextDirection(WebTextDirection direction)
{
    // The Editor::setBaseWritingDirection() function checks if we can change
    // the text direction of the selected node and updates its DOM "dir"
    // attribute and its CSS "direction" property.
    // So, we just call the function as Safari does.
    const LocalFrame* focused = toLocalFrame(focusedWebCoreFrame());
    if (!focused)
        return;

    Editor& editor = focused->editor();
    if (!editor.canEdit())
        return;

    switch (direction) {
    case WebTextDirectionDefault:
        editor.setBaseWritingDirection(NaturalWritingDirection);
        break;

    case WebTextDirectionLeftToRight:
        editor.setBaseWritingDirection(LeftToRightWritingDirection);
        break;

    case WebTextDirectionRightToLeft:
        editor.setBaseWritingDirection(RightToLeftWritingDirection);
        break;

    default:
        notImplemented();
        break;
    }
}

bool WebViewImpl::isAcceleratedCompositingActive() const
{
    return m_isAcceleratedCompositingActive;
}

void WebViewImpl::willCloseLayerTreeView()
{
    setIsAcceleratedCompositingActive(false);
    m_layerTreeView = 0;
}

void WebViewImpl::didAcquirePointerLock()
{
    if (page())
        page()->pointerLockController().didAcquirePointerLock();
}

void WebViewImpl::didNotAcquirePointerLock()
{
    if (page())
        page()->pointerLockController().didNotAcquirePointerLock();
}

void WebViewImpl::didLosePointerLock()
{
    if (page())
        page()->pointerLockController().didLosePointerLock();
}

void WebViewImpl::didChangeWindowResizerRect()
{
    if (mainFrameImpl()->frameView())
        mainFrameImpl()->frameView()->windowResizerRectChanged();
}

// WebView --------------------------------------------------------------------

WebSettingsImpl* WebViewImpl::settingsImpl()
{
    if (!m_webSettings)
        m_webSettings = adoptPtr(new WebSettingsImpl(&m_page->settings(), &m_page->inspectorController()));
    ASSERT(m_webSettings);
    return m_webSettings.get();
}

WebSettings* WebViewImpl::settings()
{
    return settingsImpl();
}

WebString WebViewImpl::pageEncoding() const
{
    if (!m_page)
        return WebString();

    if (!m_page->mainFrame()->isLocalFrame())
        return WebString();

    // FIXME: Is this check needed?
    if (!m_page->deprecatedLocalMainFrame()->document()->loader())
        return WebString();

    return m_page->deprecatedLocalMainFrame()->document()->encodingName();
}

void WebViewImpl::setPageEncoding(const WebString& encodingName)
{
    if (!m_page)
        return;

    if (!m_page->mainFrame()->isLocalFrame())
        return;

    // Only change override encoding, don't change default encoding.
    // Note that the new encoding must be 0 if it isn't supposed to be set.
    AtomicString newEncodingName;
    if (!encodingName.isEmpty())
        newEncodingName = encodingName;
    m_page->deprecatedLocalMainFrame()->loader().reload(NormalReload, KURL(), newEncodingName);
}

WebFrame* WebViewImpl::mainFrame()
{
    return WebFrame::fromFrame(m_page ? m_page->mainFrame() : 0);
}

WebFrame* WebViewImpl::findFrameByName(
    const WebString& name, WebFrame* relativeToFrame)
{
    // FIXME: Either this should only deal with WebLocalFrames or it should move to WebFrame.
    if (!relativeToFrame)
        relativeToFrame = mainFrame();
    Frame* frame = toWebLocalFrameImpl(relativeToFrame)->frame();
    frame = frame->tree().find(name);
    if (!frame || !frame->isLocalFrame())
        return 0;
    return WebLocalFrameImpl::fromFrame(toLocalFrame(frame));
}

WebFrame* WebViewImpl::focusedFrame()
{
    return WebLocalFrameImpl::fromFrame(toLocalFrame(focusedWebCoreFrame()));
}

void WebViewImpl::setFocusedFrame(WebFrame* frame)
{
    if (!frame) {
        // Clears the focused frame if any.
        Frame* focusedFrame = focusedWebCoreFrame();
        if (focusedFrame && focusedFrame->isLocalFrame())
            toLocalFrame(focusedFrame)->selection().setFocused(false);
        return;
    }
    LocalFrame* webcoreFrame = toWebLocalFrameImpl(frame)->frame();
    webcoreFrame->page()->focusController().setFocusedFrame(webcoreFrame);
}

void WebViewImpl::setInitialFocus(bool reverse)
{
    if (!m_page)
        return;
    Frame* frame = page()->focusController().focusedOrMainFrame();
    if (frame->isLocalFrame()) {
        if (Document* document = toLocalFrame(frame)->document())
            document->setFocusedElement(nullptr);
    }
    page()->focusController().setInitialFocus(reverse ? FocusTypeBackward : FocusTypeForward);
}

void WebViewImpl::clearFocusedElement()
{
    RefPtr<Frame> frame = focusedWebCoreFrame();
    if (!frame || !frame->isLocalFrame())
        return;

    LocalFrame* localFrame = toLocalFrame(frame.get());

    RefPtrWillBeRawPtr<Document> document = localFrame->document();
    if (!document)
        return;

    RefPtrWillBeRawPtr<Element> oldFocusedElement = document->focusedElement();

    // Clear the focused node.
    document->setFocusedElement(nullptr);

    if (!oldFocusedElement)
        return;

    // If a text field has focus, we need to make sure the selection controller
    // knows to remove selection from it. Otherwise, the text field is still
    // processing keyboard events even though focus has been moved to the page and
    // keystrokes get eaten as a result.
    if (oldFocusedElement->isContentEditable() || oldFocusedElement->isTextFormControl())
        localFrame->selection().clear();
}

void WebViewImpl::scrollFocusedNodeIntoRect(const WebRect& rect)
{
    LocalFrame* frame = page()->mainFrame() && page()->mainFrame()->isLocalFrame()
        ? page()->deprecatedLocalMainFrame() : 0;
    Element* element = focusedElement();
    if (!frame || !frame->view() || !element)
        return;

    if (!m_webSettings->autoZoomFocusedNodeToLegibleScale()) {
        frame->view()->scrollElementToRect(element, IntRect(rect.x, rect.y, rect.width, rect.height));
        return;
    }

    float scale;
    IntPoint scroll;
    bool needAnimation;
    computeScaleAndScrollForFocusedNode(element, scale, scroll, needAnimation);
    if (needAnimation)
        startPageScaleAnimation(scroll, false, scale, scrollAndScaleAnimationDurationInSeconds);
}

void WebViewImpl::computeScaleAndScrollForFocusedNode(Node* focusedNode, float& newScale, IntPoint& newScroll, bool& needAnimation)
{
    focusedNode->document().updateLayoutIgnorePendingStylesheets();

    // 'caret' is rect encompassing the blinking cursor.
    IntRect textboxRect = focusedNode->document().view()->contentsToWindow(pixelSnappedIntRect(focusedNode->Node::boundingBox()));
    WebRect caret, unusedEnd;
    selectionBounds(caret, unusedEnd);
    IntRect unscaledCaret = caret;
    unscaledCaret.scale(1 / pageScaleFactor());
    caret = unscaledCaret;

    // Pick a scale which is reasonably readable. This is the scale at which
    // the caret height will become minReadableCaretHeight (adjusted for dpi
    // and font scale factor).
    newScale = clampPageScaleFactorToLimits(legibleScale() * minReadableCaretHeight / caret.height);
    const float deltaScale = newScale / pageScaleFactor();

    // Convert the rects to absolute space in the new scale.
    IntRect textboxRectInDocumentCoordinates = textboxRect;
    textboxRectInDocumentCoordinates.move(mainFrame()->scrollOffset());
    IntRect caretInDocumentCoordinates = caret;
    caretInDocumentCoordinates.move(mainFrame()->scrollOffset());

    int viewWidth = m_size.width / newScale;
    int viewHeight = m_size.height / newScale;

    if (textboxRectInDocumentCoordinates.width() <= viewWidth) {
        // Field is narrower than screen. Try to leave padding on left so field's
        // label is visible, but it's more important to ensure entire field is
        // onscreen.
        int idealLeftPadding = viewWidth * leftBoxRatio;
        int maxLeftPaddingKeepingBoxOnscreen = viewWidth - textboxRectInDocumentCoordinates.width();
        newScroll.setX(textboxRectInDocumentCoordinates.x() - std::min<int>(idealLeftPadding, maxLeftPaddingKeepingBoxOnscreen));
    } else {
        // Field is wider than screen. Try to left-align field, unless caret would
        // be offscreen, in which case right-align the caret.
        newScroll.setX(std::max<int>(textboxRectInDocumentCoordinates.x(), caretInDocumentCoordinates.x() + caretInDocumentCoordinates.width() + caretPadding - viewWidth));
    }
    if (textboxRectInDocumentCoordinates.height() <= viewHeight) {
        // Field is shorter than screen. Vertically center it.
        newScroll.setY(textboxRectInDocumentCoordinates.y() - (viewHeight - textboxRectInDocumentCoordinates.height()) / 2);
    } else {
        // Field is taller than screen. Try to top align field, unless caret would
        // be offscreen, in which case bottom-align the caret.
        newScroll.setY(std::max<int>(textboxRectInDocumentCoordinates.y(), caretInDocumentCoordinates.y() + caretInDocumentCoordinates.height() + caretPadding - viewHeight));
    }

    needAnimation = false;
    // If we are at less than the target zoom level, zoom in.
    if (deltaScale > minScaleChangeToTriggerZoom)
        needAnimation = true;
    // If the caret is offscreen, then animate.
    IntRect sizeRect(0, 0, viewWidth, viewHeight);
    if (!sizeRect.contains(caret))
        needAnimation = true;
    // If the box is partially offscreen and it's possible to bring it fully
    // onscreen, then animate.
    if (sizeRect.contains(textboxRectInDocumentCoordinates.width(), textboxRectInDocumentCoordinates.height()) && !sizeRect.contains(textboxRect))
        needAnimation = true;
}

void WebViewImpl::advanceFocus(bool reverse)
{
    page()->focusController().advanceFocus(reverse ? FocusTypeBackward : FocusTypeForward);
}

double WebViewImpl::zoomLevel()
{
    return m_zoomLevel;
}

double WebViewImpl::setZoomLevel(double zoomLevel)
{
    if (zoomLevel < m_minimumZoomLevel)
        m_zoomLevel = m_minimumZoomLevel;
    else if (zoomLevel > m_maximumZoomLevel)
        m_zoomLevel = m_maximumZoomLevel;
    else
        m_zoomLevel = zoomLevel;

    LocalFrame* frame = mainFrameImpl()->frame();
    WebPluginContainerImpl* pluginContainer = WebLocalFrameImpl::pluginContainerFromFrame(frame);
    if (pluginContainer)
        pluginContainer->plugin()->setZoomLevel(m_zoomLevel, false);
    else {
        float zoomFactor = m_zoomFactorOverride ? m_zoomFactorOverride : static_cast<float>(zoomLevelToZoomFactor(m_zoomLevel));
        frame->setPageZoomFactor(zoomFactor);
    }

    return m_zoomLevel;
}

void WebViewImpl::zoomLimitsChanged(double minimumZoomLevel,
                                    double maximumZoomLevel)
{
    m_minimumZoomLevel = minimumZoomLevel;
    m_maximumZoomLevel = maximumZoomLevel;
    m_client->zoomLimitsChanged(m_minimumZoomLevel, m_maximumZoomLevel);
}

float WebViewImpl::textZoomFactor()
{
    return mainFrameImpl()->frame()->textZoomFactor();
}

float WebViewImpl::setTextZoomFactor(float textZoomFactor)
{
    LocalFrame* frame = mainFrameImpl()->frame();
    if (WebLocalFrameImpl::pluginContainerFromFrame(frame))
        return 1;

    frame->setTextZoomFactor(textZoomFactor);

    return textZoomFactor;
}

void WebViewImpl::fullFramePluginZoomLevelChanged(double zoomLevel)
{
    if (zoomLevel == m_zoomLevel)
        return;

    m_zoomLevel = std::max(std::min(zoomLevel, m_maximumZoomLevel), m_minimumZoomLevel);
    m_client->zoomLevelChanged();
}

double WebView::zoomLevelToZoomFactor(double zoomLevel)
{
    return pow(textSizeMultiplierRatio, zoomLevel);
}

double WebView::zoomFactorToZoomLevel(double factor)
{
    // Since factor = 1.2^level, level = log(factor) / log(1.2)
    return log(factor) / log(textSizeMultiplierRatio);
}

float WebViewImpl::pageScaleFactor() const
{
    if (!page())
        return 1;

    if (!pinchVirtualViewportEnabled())
        return page()->pageScaleFactor();

    return page()->frameHost().pinchViewport().scale();
}

float WebViewImpl::clampPageScaleFactorToLimits(float scaleFactor) const
{
    return m_pageScaleConstraintsSet.finalConstraints().clampToConstraints(scaleFactor);
}

IntPoint WebViewImpl::clampOffsetAtScale(const IntPoint& offset, float scale)
{
    FrameView* view = mainFrameImpl()->frameView();
    if (!view)
        return offset;

    return view->clampOffsetAtScale(offset, scale);
}

bool WebViewImpl::pinchVirtualViewportEnabled() const
{
    ASSERT(page());
    return page()->settings().pinchVirtualViewportEnabled();
}

void WebViewImpl::setPinchViewportOffset(const WebFloatPoint& offset)
{
    ASSERT(page());

    if (!pinchVirtualViewportEnabled())
        return;

    page()->frameHost().pinchViewport().setLocation(offset);
}

WebFloatPoint WebViewImpl::pinchViewportOffset() const
{
    ASSERT(page());

    if (!pinchVirtualViewportEnabled())
        return WebFloatPoint();

    return page()->frameHost().pinchViewport().visibleRect().location();
}

void WebViewImpl::setPageScaleFactor(float scaleFactor)
{
    ASSERT(page());

    scaleFactor = clampPageScaleFactorToLimits(scaleFactor);
    if (scaleFactor == pageScaleFactor())
        return;

    // TODO(bokan): Old-style pinch path. Remove when we're migrated to
    // virtual viewport pinch.
    if (!pinchVirtualViewportEnabled()) {
        IntPoint scrollOffset(mainFrame()->scrollOffset().width, mainFrame()->scrollOffset().height);
        setPageScaleFactor(scaleFactor, scrollOffset);
        return;
    }

    page()->frameHost().pinchViewport().setScale(scaleFactor);
    deviceOrPageScaleFactorChanged();
}

void WebViewImpl::setMainFrameScrollOffset(const WebPoint& origin)
{
    updateMainFrameScrollPosition(origin, false);
}

void WebViewImpl::setPageScaleFactor(float scaleFactor, const WebPoint& origin)
{
    if (!page())
        return;

    IntPoint newScrollOffset = origin;
    scaleFactor = clampPageScaleFactorToLimits(scaleFactor);
    newScrollOffset = clampOffsetAtScale(newScrollOffset, scaleFactor);

    if (pinchVirtualViewportEnabled())
        setPageScaleFactor(scaleFactor);
        // Note, we don't set the offset in the new path. This method is going
        // away for the new pinch model so that's ok.
    else
        page()->setPageScaleFactor(scaleFactor, newScrollOffset);
}


float WebViewImpl::deviceScaleFactor() const
{
    if (!page())
        return 1;

    return page()->deviceScaleFactor();
}

void WebViewImpl::setDeviceScaleFactor(float scaleFactor)
{
    if (!page())
        return;

    page()->setDeviceScaleFactor(scaleFactor);

    if (m_layerTreeView)
        updateLayerTreeDeviceScaleFactor();
}

void WebViewImpl::enableAutoResizeMode(const WebSize& minSize, const WebSize& maxSize)
{
    m_shouldAutoResize = true;
    m_minAutoSize = minSize;
    m_maxAutoSize = maxSize;
    configureAutoResizeMode();
}

void WebViewImpl::disableAutoResizeMode()
{
    m_shouldAutoResize = false;
    configureAutoResizeMode();
}

void WebViewImpl::setUserAgentPageScaleConstraints(PageScaleConstraints newConstraints)
{
    if (newConstraints == m_pageScaleConstraintsSet.userAgentConstraints())
        return;

    m_pageScaleConstraintsSet.setUserAgentConstraints(newConstraints);

    if (!mainFrameImpl() || !mainFrameImpl()->frameView())
        return;

    mainFrameImpl()->frameView()->setNeedsLayout();
}

void WebViewImpl::setInitialPageScaleOverride(float initialPageScaleFactorOverride)
{
    PageScaleConstraints constraints = m_pageScaleConstraintsSet.userAgentConstraints();
    constraints.initialScale = initialPageScaleFactorOverride;

    if (constraints == m_pageScaleConstraintsSet.userAgentConstraints())
        return;

    m_pageScaleConstraintsSet.setNeedsReset(true);
    setUserAgentPageScaleConstraints(constraints);
}

void WebViewImpl::setPageScaleFactorLimits(float minPageScale, float maxPageScale)
{
    PageScaleConstraints constraints = m_pageScaleConstraintsSet.userAgentConstraints();
    constraints.minimumScale = minPageScale;
    constraints.maximumScale = maxPageScale;
    setUserAgentPageScaleConstraints(constraints);
}

void WebViewImpl::setIgnoreViewportTagScaleLimits(bool ignore)
{
    PageScaleConstraints constraints = m_pageScaleConstraintsSet.userAgentConstraints();
    if (ignore) {
        constraints.minimumScale = m_pageScaleConstraintsSet.defaultConstraints().minimumScale;
        constraints.maximumScale = m_pageScaleConstraintsSet.defaultConstraints().maximumScale;
    } else {
        constraints.minimumScale = -1;
        constraints.maximumScale = -1;
    }
    setUserAgentPageScaleConstraints(constraints);
}

void WebViewImpl::refreshPageScaleFactorAfterLayout()
{
    if (!mainFrame() || !page() || !page()->mainFrame() || !page()->mainFrame()->isLocalFrame() || !page()->deprecatedLocalMainFrame()->view())
        return;
    FrameView* view = page()->deprecatedLocalMainFrame()->view();

    updatePageDefinedViewportConstraints(mainFrameImpl()->frame()->document()->viewportDescription());
    m_pageScaleConstraintsSet.computeFinalConstraints();

    if (settings()->shrinksViewportContentToFit() && !m_fixedLayoutSizeLock) {
        int verticalScrollbarWidth = 0;
        if (view->verticalScrollbar() && !view->verticalScrollbar()->isOverlayScrollbar())
            verticalScrollbarWidth = view->verticalScrollbar()->width();
        m_pageScaleConstraintsSet.adjustFinalConstraintsToContentsSize(contentsSize(), verticalScrollbarWidth);
    }

    if (pinchVirtualViewportEnabled())
        mainFrameImpl()->frameView()->resize(m_pageScaleConstraintsSet.mainFrameSize(contentsSize()));

    float newPageScaleFactor = pageScaleFactor();
    if (m_pageScaleConstraintsSet.needsReset() && m_pageScaleConstraintsSet.finalConstraints().initialScale != -1) {
        newPageScaleFactor = m_pageScaleConstraintsSet.finalConstraints().initialScale;
        m_pageScaleConstraintsSet.setNeedsReset(false);
    }
    setPageScaleFactor(newPageScaleFactor);

    updateLayerTreeViewport();

    // Relayout immediately to avoid violating the rule that needsLayout()
    // isn't set at the end of a layout.
    if (view->needsLayout())
        view->layout();
}

void WebViewImpl::updatePageDefinedViewportConstraints(const ViewportDescription& description)
{
    if (!settings()->viewportEnabled() || !page() || (!m_size.width && !m_size.height) || !page()->mainFrame()->isLocalFrame())
        return;

    Document* document = page()->deprecatedLocalMainFrame()->document();

    if (settingsImpl()->useExpandedHeuristicsForGpuRasterization()) {
        m_matchesHeuristicsForGpuRasterization = description.maxWidth == Length(DeviceWidth)
            && description.minZoom == 1.0
            && description.minZoomIsExplicit;
    } else {
        m_matchesHeuristicsForGpuRasterization = description.maxWidth == Length(DeviceWidth)
            && description.minZoom == 1.0
            && description.minZoomIsExplicit
            && description.zoom == 1.0
            && description.zoomIsExplicit
            && description.userZoom
            && description.userZoomIsExplicit;
    }
    if (m_layerTreeView)
        m_layerTreeView->heuristicsForGpuRasterizationUpdated(m_matchesHeuristicsForGpuRasterization);

    Length defaultMinWidth = document->viewportDefaultMinWidth();
    if (defaultMinWidth.isAuto())
        defaultMinWidth = Length(ExtendToZoom);

    ViewportDescription adjustedDescription = description;
    if (settingsImpl()->viewportMetaLayoutSizeQuirk() && adjustedDescription.type == ViewportDescription::ViewportMeta) {
        const int legacyWidthSnappingMagicNumber = 320;
        if (adjustedDescription.maxWidth.isFixed() && adjustedDescription.maxWidth.value() <= legacyWidthSnappingMagicNumber)
            adjustedDescription.maxWidth = Length(DeviceWidth);
        if (adjustedDescription.maxHeight.isFixed() && adjustedDescription.maxHeight.value() <= m_size.height)
            adjustedDescription.maxHeight = Length(DeviceHeight);
        adjustedDescription.minWidth = adjustedDescription.maxWidth;
        adjustedDescription.minHeight = adjustedDescription.maxHeight;
    }

    float oldInitialScale = m_pageScaleConstraintsSet.pageDefinedConstraints().initialScale;
    m_pageScaleConstraintsSet.updatePageDefinedConstraints(adjustedDescription, defaultMinWidth);

    if (settingsImpl()->clobberUserAgentInitialScaleQuirk()
        && m_pageScaleConstraintsSet.userAgentConstraints().initialScale != -1
        && m_pageScaleConstraintsSet.userAgentConstraints().initialScale * deviceScaleFactor() <= 1) {
        if (description.maxWidth == Length(DeviceWidth)
            || (description.maxWidth.type() == Auto && m_pageScaleConstraintsSet.pageDefinedConstraints().initialScale == 1.0f))
            setInitialPageScaleOverride(-1);
    }

    m_pageScaleConstraintsSet.adjustForAndroidWebViewQuirks(adjustedDescription, defaultMinWidth.intValue(), deviceScaleFactor(), settingsImpl()->supportDeprecatedTargetDensityDPI(), page()->settings().wideViewportQuirkEnabled(), page()->settings().useWideViewport(), page()->settings().loadWithOverviewMode(), settingsImpl()->viewportMetaNonUserScalableQuirk());
    float newInitialScale = m_pageScaleConstraintsSet.pageDefinedConstraints().initialScale;
    if (oldInitialScale != newInitialScale && newInitialScale != -1) {
        m_pageScaleConstraintsSet.setNeedsReset(true);
        if (mainFrameImpl() && mainFrameImpl()->frameView())
            mainFrameImpl()->frameView()->setNeedsLayout();
    }

    updateMainFrameLayoutSize();

    if (LocalFrame* frame = page()->deprecatedLocalMainFrame()) {
        if (FastTextAutosizer* textAutosizer = frame->document()->fastTextAutosizer())
            textAutosizer->updatePageInfoInAllFrames();
    }
}

void WebViewImpl::updateMainFrameLayoutSize()
{
    if (m_fixedLayoutSizeLock || m_shouldAutoResize || !mainFrameImpl())
        return;

    RefPtr<FrameView> view = mainFrameImpl()->frameView();
    if (!view)
        return;

    WebSize layoutSize = m_size;

    if (settings()->viewportEnabled()) {
        layoutSize = flooredIntSize(m_pageScaleConstraintsSet.pageDefinedConstraints().layoutSize);

        bool textAutosizingEnabled = page()->settings().textAutosizingEnabled();
        if (textAutosizingEnabled && layoutSize.width != view->layoutSize().width()) {
            if (TextAutosizer* textAutosizer = page()->deprecatedLocalMainFrame()->document()->textAutosizer())
                textAutosizer->recalculateMultipliers();
        }
    }

    if (page()->settings().forceZeroLayoutHeight())
        layoutSize.height = 0;

    view->setLayoutSize(layoutSize);
}

IntSize WebViewImpl::contentsSize() const
{
    if (!page()->mainFrame()->isLocalFrame())
        return IntSize();
    RenderView* root = page()->deprecatedLocalMainFrame()->contentRenderer();
    if (!root)
        return IntSize();
    return root->documentRect().size();
}

WebSize WebViewImpl::contentsPreferredMinimumSize()
{
    Document* document = m_page->mainFrame()->isLocalFrame() ? m_page->deprecatedLocalMainFrame()->document() : 0;
    if (!document || !document->renderView() || !document->documentElement())
        return WebSize();

    layout();
    FontCachePurgePreventer fontCachePurgePreventer; // Required by minPreferredLogicalWidth().
    IntSize preferredMinimumSize(document->renderView()->minPreferredLogicalWidth(), document->documentElement()->scrollHeight());
    preferredMinimumSize.scale(zoomLevelToZoomFactor(zoomLevel()));
    return preferredMinimumSize;
}

float WebViewImpl::minimumPageScaleFactor() const
{
    return m_pageScaleConstraintsSet.finalConstraints().minimumScale;
}

float WebViewImpl::maximumPageScaleFactor() const
{
    return m_pageScaleConstraintsSet.finalConstraints().maximumScale;
}

void WebViewImpl::resetScrollAndScaleState()
{
    // TODO: This is done by the pinchViewport().reset() call below and can be removed when
    // the new pinch path is the only one.
    setPageScaleFactor(1);
    updateMainFrameScrollPosition(IntPoint(), true);
    page()->frameHost().pinchViewport().reset();

    if (!page()->mainFrame()->isLocalFrame())
        return;

    // Clear out the values for the current history item. This will prevent the history item from clobbering the
    // value determined during page scale initialization, which may be less than 1.
    page()->deprecatedLocalMainFrame()->loader().clearScrollPositionAndViewState();
    m_pageScaleConstraintsSet.setNeedsReset(true);

    // Clobber saved scales and scroll offsets.
    if (FrameView* view = page()->deprecatedLocalMainFrame()->document()->view())
        view->cacheCurrentScrollPosition();
}

void WebViewImpl::setFixedLayoutSize(const WebSize& layoutSize)
{
    if (!page() || !page()->mainFrame()->isLocalFrame())
        return;

    LocalFrame* frame = page()->deprecatedLocalMainFrame();
    if (!frame)
        return;

    RefPtr<FrameView> view = frame->view();
    if (!view)
        return;

    m_fixedLayoutSizeLock = layoutSize.width || layoutSize.height;

    if (m_fixedLayoutSizeLock)
        view->setLayoutSize(layoutSize);
    else
        updateMainFrameLayoutSize();
}

void WebViewImpl::performMediaPlayerAction(const WebMediaPlayerAction& action,
                                           const WebPoint& location)
{
    HitTestResult result = hitTestResultForWindowPos(location);
    RefPtrWillBeRawPtr<Node> node = result.innerNonSharedNode();
    if (!isHTMLVideoElement(*node) && !isHTMLAudioElement(*node))
        return;

    RefPtrWillBeRawPtr<HTMLMediaElement> mediaElement = static_pointer_cast<HTMLMediaElement>(node);
    switch (action.type) {
    case WebMediaPlayerAction::Play:
        if (action.enable)
            mediaElement->play();
        else
            mediaElement->pause();
        break;
    case WebMediaPlayerAction::Mute:
        mediaElement->setMuted(action.enable);
        break;
    case WebMediaPlayerAction::Loop:
        mediaElement->setLoop(action.enable);
        break;
    case WebMediaPlayerAction::Controls:
        mediaElement->setControls(action.enable);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

void WebViewImpl::performPluginAction(const WebPluginAction& action,
                                      const WebPoint& location)
{
    HitTestResult result = hitTestResultForWindowPos(location);
    RefPtrWillBeRawPtr<Node> node = result.innerNonSharedNode();
    if (!isHTMLObjectElement(*node) && !isHTMLEmbedElement(*node))
        return;

    RenderObject* object = node->renderer();
    if (object && object->isWidget()) {
        Widget* widget = toRenderWidget(object)->widget();
        if (widget && widget->isPluginContainer()) {
            WebPluginContainerImpl* plugin = toWebPluginContainerImpl(widget);
            switch (action.type) {
            case WebPluginAction::Rotate90Clockwise:
                plugin->plugin()->rotateView(WebPlugin::RotationType90Clockwise);
                break;
            case WebPluginAction::Rotate90Counterclockwise:
                plugin->plugin()->rotateView(WebPlugin::RotationType90Counterclockwise);
                break;
            default:
                ASSERT_NOT_REACHED();
            }
        }
    }
}

WebHitTestResult WebViewImpl::hitTestResultAt(const WebPoint& point)
{
    IntPoint scaledPoint = point;
    scaledPoint.scale(1 / pageScaleFactor(), 1 / pageScaleFactor());
    return hitTestResultForWindowPos(scaledPoint);
}

void WebViewImpl::copyImageAt(const WebPoint& point)
{
    if (!m_page)
        return;

    HitTestResult result = hitTestResultForWindowPos(point);

    if (result.absoluteImageURLIncludingCanvasDataURL().isEmpty()) {
        // There isn't actually an image at these coordinates.  Might be because
        // the window scrolled while the context menu was open or because the page
        // changed itself between when we thought there was an image here and when
        // we actually tried to retreive the image.
        //
        // FIXME: implement a cache of the most recent HitTestResult to avoid having
        //        to do two hit tests.
        return;
    }

    m_page->deprecatedLocalMainFrame()->editor().copyImage(result);
}

void WebViewImpl::saveImageAt(const WebPoint& point)
{
    if (!m_page)
        return;

    KURL url = hitTestResultForWindowPos(point).absoluteImageURLIncludingCanvasDataURL();

    if (url.isEmpty())
        return;

    ResourceRequest request(url);
    m_page->deprecatedLocalMainFrame()->loader().client()->loadURLExternally(
        request, NavigationPolicyDownloadTo, WebString());
}

void WebViewImpl::dragSourceEndedAt(
    const WebPoint& clientPoint,
    const WebPoint& screenPoint,
    WebDragOperation operation)
{
    PlatformMouseEvent pme(clientPoint,
                           screenPoint,
                           LeftButton, PlatformEvent::MouseMoved, 0, false, false, false,
                           false, 0);
    m_page->deprecatedLocalMainFrame()->eventHandler().dragSourceEndedAt(pme,
        static_cast<DragOperation>(operation));
}

void WebViewImpl::dragSourceSystemDragEnded()
{
    // It's possible for us to get this callback while not doing a drag if
    // it's from a previous page that got unloaded.
    if (m_doingDragAndDrop) {
        m_page->dragController().dragEnded();
        m_doingDragAndDrop = false;
    }
}

WebDragOperation WebViewImpl::dragTargetDragEnter(
    const WebDragData& webDragData,
    const WebPoint& clientPoint,
    const WebPoint& screenPoint,
    WebDragOperationsMask operationsAllowed,
    int keyModifiers)
{
    ASSERT(!m_currentDragData);

    m_currentDragData = webDragData.getValue();
    m_operationsAllowed = operationsAllowed;

    return dragTargetDragEnterOrOver(clientPoint, screenPoint, DragEnter, keyModifiers);
}

WebDragOperation WebViewImpl::dragTargetDragOver(
    const WebPoint& clientPoint,
    const WebPoint& screenPoint,
    WebDragOperationsMask operationsAllowed,
    int keyModifiers)
{
    m_operationsAllowed = operationsAllowed;

    return dragTargetDragEnterOrOver(clientPoint, screenPoint, DragOver, keyModifiers);
}

void WebViewImpl::dragTargetDragLeave()
{
    ASSERT(m_currentDragData);

    DragData dragData(
        m_currentDragData.get(),
        IntPoint(),
        IntPoint(),
        static_cast<DragOperation>(m_operationsAllowed));

    m_page->dragController().dragExited(&dragData);

    // FIXME: why is the drag scroll timer not stopped here?

    m_dragOperation = WebDragOperationNone;
    m_currentDragData = nullptr;
}

void WebViewImpl::dragTargetDrop(const WebPoint& clientPoint,
                                 const WebPoint& screenPoint,
                                 int keyModifiers)
{
    ASSERT(m_currentDragData);

    UserGestureNotifier notifier(m_autofillClient, &m_userGestureObserved);

    // If this webview transitions from the "drop accepting" state to the "not
    // accepting" state, then our IPC message reply indicating that may be in-
    // flight, or else delayed by javascript processing in this webview.  If a
    // drop happens before our IPC reply has reached the browser process, then
    // the browser forwards the drop to this webview.  So only allow a drop to
    // proceed if our webview m_dragOperation state is not DragOperationNone.

    if (m_dragOperation == WebDragOperationNone) { // IPC RACE CONDITION: do not allow this drop.
        dragTargetDragLeave();
        return;
    }

    m_currentDragData->setModifierKeyState(webInputEventKeyStateToPlatformEventKeyState(keyModifiers));
    DragData dragData(
        m_currentDragData.get(),
        clientPoint,
        screenPoint,
        static_cast<DragOperation>(m_operationsAllowed));

    UserGestureIndicator gesture(DefinitelyProcessingNewUserGesture);
    m_page->dragController().performDrag(&dragData);

    m_dragOperation = WebDragOperationNone;
    m_currentDragData = nullptr;
}

void WebViewImpl::spellingMarkers(WebVector<uint32_t>* markers)
{
    Vector<uint32_t> result;
    for (Frame* frame = m_page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (!frame->isLocalFrame())
            continue;
        const WillBeHeapVector<DocumentMarker*>& documentMarkers = toLocalFrame(frame)->document()->markers().markers();
        for (size_t i = 0; i < documentMarkers.size(); ++i)
            result.append(documentMarkers[i]->hash());
    }
    markers->assign(result);
}

WebDragOperation WebViewImpl::dragTargetDragEnterOrOver(const WebPoint& clientPoint, const WebPoint& screenPoint, DragAction dragAction, int keyModifiers)
{
    ASSERT(m_currentDragData);

    m_currentDragData->setModifierKeyState(webInputEventKeyStateToPlatformEventKeyState(keyModifiers));
    DragData dragData(
        m_currentDragData.get(),
        clientPoint,
        screenPoint,
        static_cast<DragOperation>(m_operationsAllowed));

    DragSession dragSession;
    if (dragAction == DragEnter)
        dragSession = m_page->dragController().dragEntered(&dragData);
    else
        dragSession = m_page->dragController().dragUpdated(&dragData);

    DragOperation dropEffect = dragSession.operation;

    // Mask the drop effect operation against the drag source's allowed operations.
    if (!(dropEffect & dragData.draggingSourceOperationMask()))
        dropEffect = DragOperationNone;

     m_dragOperation = static_cast<WebDragOperation>(dropEffect);

    return m_dragOperation;
}

void WebViewImpl::sendResizeEventAndRepaint()
{
    // FIXME: This is wrong. The FrameView is responsible sending a resizeEvent
    // as part of layout. Layout is also responsible for sending invalidations
    // to the embedder. This method and all callers may be wrong. -- eseidel.
    if (mainFrameImpl()->frameView()) {
        // Enqueues the resize event.
        mainFrameImpl()->frame()->document()->enqueueResizeEvent();
    }

    if (m_client) {
        if (isAcceleratedCompositingActive()) {
            updateLayerTreeViewport();
        } else {
            WebRect damagedRect(0, 0, m_size.width, m_size.height);
            m_client->didInvalidateRect(damagedRect);
        }
    }
    if (m_pageOverlays)
        m_pageOverlays->update();
}

void WebViewImpl::configureAutoResizeMode()
{
    if (!mainFrameImpl() || !mainFrameImpl()->frame() || !mainFrameImpl()->frame()->view())
        return;

    mainFrameImpl()->frame()->view()->enableAutoSizeMode(m_shouldAutoResize, m_minAutoSize, m_maxAutoSize);
}

unsigned long WebViewImpl::createUniqueIdentifierForRequest()
{
    return createUniqueIdentifier();
}

void WebViewImpl::inspectElementAt(const WebPoint& point)
{
    if (!m_page)
        return;

    if (point.x == -1 || point.y == -1) {
        m_page->inspectorController().inspect(0);
    } else {
        HitTestRequest::HitTestRequestType hitType = HitTestRequest::Move | HitTestRequest::ReadOnly | HitTestRequest::AllowChildFrameContent;
        HitTestRequest request(hitType);

        WebMouseEvent dummyEvent;
        dummyEvent.type = WebInputEvent::MouseDown;
        dummyEvent.x = point.x;
        dummyEvent.y = point.y;
        IntPoint transformedPoint = PlatformMouseEventBuilder(m_page->deprecatedLocalMainFrame()->view(), dummyEvent).position();
        HitTestResult result(m_page->deprecatedLocalMainFrame()->view()->windowToContents(transformedPoint));
        m_page->deprecatedLocalMainFrame()->contentRenderer()->hitTest(request, result);
        Node* node = result.innerNode();
        if (!node && m_page->deprecatedLocalMainFrame()->document())
            node = m_page->deprecatedLocalMainFrame()->document()->documentElement();
        m_page->inspectorController().inspect(node);
    }
}

WebString WebViewImpl::inspectorSettings() const
{
    return m_inspectorSettings;
}

void WebViewImpl::setInspectorSettings(const WebString& settings)
{
    m_inspectorSettings = settings;
}

bool WebViewImpl::inspectorSetting(const WebString& key, WebString* value) const
{
    if (!m_inspectorSettingsMap->contains(key))
        return false;
    *value = m_inspectorSettingsMap->get(key);
    return true;
}

void WebViewImpl::setInspectorSetting(const WebString& key,
                                      const WebString& value)
{
    m_inspectorSettingsMap->set(key, value);
    client()->didUpdateInspectorSetting(key, value);
}

void WebViewImpl::setCompositorDeviceScaleFactorOverride(float deviceScaleFactor)
{
    if (m_compositorDeviceScaleFactorOverride == deviceScaleFactor)
        return;
    m_compositorDeviceScaleFactorOverride = deviceScaleFactor;
    if (page() && m_layerTreeView)
        updateLayerTreeDeviceScaleFactor();
}

void WebViewImpl::setRootLayerTransform(const WebSize& rootLayerOffset, float rootLayerScale)
{
    if (m_rootLayerScale == rootLayerScale && m_rootLayerOffset == rootLayerOffset)
        return;
    m_rootLayerScale = rootLayerScale;
    m_rootLayerOffset = rootLayerOffset;
    if (mainFrameImpl())
        mainFrameImpl()->setInputEventsTransformForEmulation(m_rootLayerOffset, m_rootLayerScale);
    updateRootLayerTransform();
}

WebDevToolsAgent* WebViewImpl::devToolsAgent()
{
    return m_devToolsAgent.get();
}

WebAXObject WebViewImpl::accessibilityObject()
{
    if (!mainFrameImpl())
        return WebAXObject();

    Document* document = mainFrameImpl()->frame()->document();
    return WebAXObject(
        document->axObjectCache()->getOrCreate(document->renderView()));
}

void WebViewImpl::performCustomContextMenuAction(unsigned action)
{
    if (!m_page)
        return;
    ContextMenu* menu = m_page->contextMenuController().contextMenu();
    if (!menu)
        return;
    const ContextMenuItem* item = menu->itemWithAction(static_cast<ContextMenuAction>(ContextMenuItemBaseCustomTag + action));
    if (item)
        m_page->contextMenuController().contextMenuItemSelected(item);
    m_page->contextMenuController().clearContextMenu();
}

void WebViewImpl::showContextMenu()
{
    if (!page())
        return;

    page()->contextMenuController().clearContextMenu();
    m_contextMenuAllowed = true;
    if (LocalFrame* focusedFrame = toLocalFrame(page()->focusController().focusedOrMainFrame()))
        focusedFrame->eventHandler().sendContextMenuEventForKey();
    m_contextMenuAllowed = false;
}

// FIXME: This should be removed when the chromium side patch lands
// http://codereview.chromium.org/260623004
WebString WebViewImpl::getSmartClipData(WebRect rect)
{
    return WebString();
}

void WebViewImpl::getSmartClipData(WebRect rect, WebString& clipText, WebRect& clipRect)
{
    LocalFrame* frame = toLocalFrame(focusedWebCoreFrame());
    if (!frame)
        return;
    SmartClipData clipData = WebCore::SmartClip(frame).dataForRect(rect);
    clipText = clipData.clipData();
    clipRect = clipData.rect();
}

void WebViewImpl::extractSmartClipData(WebRect rect, WebString& clipText, WebString& clipHtml, WebRect& clipRect)
{
    LocalFrame* localFrame = toLocalFrame(focusedWebCoreFrame());
    if (!localFrame)
        return;
    SmartClipData clipData = WebCore::SmartClip(localFrame).dataForRect(rect);
    clipText = clipData.clipData();
    clipRect = clipData.rect();

    WebLocalFrameImpl* frame = mainFrameImpl();
    if (!frame)
        return;
    WebPoint startPoint(rect.x, rect.y);
    WebPoint endPoint(rect.x + rect.width, rect.y + rect.height);
    VisiblePosition startVisiblePosition = frame->visiblePositionForWindowPoint(startPoint);
    VisiblePosition endVisiblePosition = frame->visiblePositionForWindowPoint(endPoint);

    Position startPosition = startVisiblePosition.deepEquivalent();
    Position endPosition = endVisiblePosition.deepEquivalent();

    RefPtr<Range> range = Range::create(*startPosition.document(), startPosition, endPosition);
    if (!range)
        return;

    clipHtml = createMarkup(range.get(), 0, AnnotateForInterchange, false, ResolveNonLocalURLs);
}

void WebViewImpl::hidePopups()
{
    hideSelectPopup();
    if (m_pagePopup)
        closePagePopup(m_pagePopup.get());
}

void WebViewImpl::setIsTransparent(bool isTransparent)
{
    // Set any existing frames to be transparent.
    Frame* frame = m_page->mainFrame();
    while (frame) {
        if (frame->isLocalFrame())
            toLocalFrame(frame)->view()->setTransparent(isTransparent);
        frame = frame->tree().traverseNext();
    }

    // Future frames check this to know whether to be transparent.
    m_isTransparent = isTransparent;
}

bool WebViewImpl::isTransparent() const
{
    return m_isTransparent;
}

void WebViewImpl::setBaseBackgroundColor(WebColor color)
{
    layout();

    if (m_baseBackgroundColor == color)
        return;

    m_baseBackgroundColor = color;

    if (m_page->mainFrame() && m_page->mainFrame()->isLocalFrame())
        m_page->deprecatedLocalMainFrame()->view()->setBaseBackgroundColor(color);

    updateLayerTreeBackgroundColor();
}

void WebViewImpl::setIsActive(bool active)
{
    if (page())
        page()->focusController().setActive(active);
}

bool WebViewImpl::isActive() const
{
    return page() ? page()->focusController().isActive() : false;
}

void WebViewImpl::setDomainRelaxationForbidden(bool forbidden, const WebString& scheme)
{
    SchemeRegistry::setDomainRelaxationForbiddenForURLScheme(forbidden, String(scheme));
}

void WebViewImpl::setWindowFeatures(const WebWindowFeatures& features)
{
    m_page->chrome().setWindowFeatures(features);
}

void WebViewImpl::setOpenedByDOM()
{
    m_page->setOpenedByDOM();
}

void WebViewImpl::setSelectionColors(unsigned activeBackgroundColor,
                                     unsigned activeForegroundColor,
                                     unsigned inactiveBackgroundColor,
                                     unsigned inactiveForegroundColor) {
#if USE(DEFAULT_RENDER_THEME)
    RenderThemeChromiumDefault::setSelectionColors(activeBackgroundColor, activeForegroundColor, inactiveBackgroundColor, inactiveForegroundColor);
    RenderTheme::theme().platformColorsDidChange();
#endif
}

void WebView::injectStyleSheet(const WebString& sourceCode, const WebVector<WebString>& patternsIn, WebView::StyleInjectionTarget injectIn)
{
    Vector<String> patterns;
    for (size_t i = 0; i < patternsIn.size(); ++i)
        patterns.append(patternsIn[i]);

    InjectedStyleSheets::instance().add(sourceCode, patterns, static_cast<WebCore::StyleInjectionTarget>(injectIn));
}

void WebView::removeInjectedStyleSheets()
{
    InjectedStyleSheets::instance().removeAll();
}

void WebViewImpl::didCommitLoad(bool isNewNavigation, bool isNavigationWithinPage)
{
    if (isNewNavigation && !isNavigationWithinPage)
        m_pageScaleConstraintsSet.setNeedsReset(true);

    // Make sure link highlight from previous page is cleared.
    m_linkHighlights.clear();
    endActiveFlingAnimation();
    m_userGestureObserved = false;
}

void WebViewImpl::willInsertBody(WebLocalFrameImpl* webframe)
{
    if (webframe != mainFrameImpl())
        return;

    if (!m_page->mainFrame()->isLocalFrame())
        return;

    // If we get to the <body> tag and we have no pending stylesheet and import load, we
    // can be fairly confident we'll have something sensible to paint soon and
    // can turn off deferred commits.
    if (m_page->deprecatedLocalMainFrame()->document()->isRenderingReady())
        resumeTreeViewCommits();
}

void WebViewImpl::resumeTreeViewCommits()
{
    if (m_layerTreeViewCommitsDeferred) {
        if (m_layerTreeView)
            m_layerTreeView->setDeferCommits(false);
        m_layerTreeViewCommitsDeferred = false;
    }
}

void WebViewImpl::layoutUpdated(WebLocalFrameImpl* webframe)
{
    if (!m_client || webframe != mainFrameImpl())
        return;

    // If we finished a layout while in deferred commit mode,
    // that means it's time to start producing frames again so un-defer.
    resumeTreeViewCommits();

    if (m_shouldAutoResize && mainFrameImpl()->frame() && mainFrameImpl()->frame()->view()) {
        WebSize frameSize = mainFrameImpl()->frame()->view()->frameRect().size();
        if (frameSize != m_size) {
            m_size = frameSize;

            page()->frameHost().pinchViewport().setSize(m_size);
            m_pageScaleConstraintsSet.didChangeViewSize(m_size);

            m_client->didAutoResize(m_size);
            sendResizeEventAndRepaint();
        }
    }

    if (m_pageScaleConstraintsSet.constraintsDirty())
        refreshPageScaleFactorAfterLayout();

    m_client->didUpdateLayout();
}

void WebViewImpl::didChangeContentsSize()
{
    m_pageScaleConstraintsSet.didChangeContentsSize(contentsSize(), pageScaleFactor());
}

void WebViewImpl::deviceOrPageScaleFactorChanged()
{
    m_pageScaleConstraintsSet.setNeedsReset(false);
    updateLayerTreeViewport();
}

bool WebViewImpl::useExternalPopupMenus()
{
    return shouldUseExternalPopupMenus;
}

void WebViewImpl::startDragging(LocalFrame* frame,
                                const WebDragData& dragData,
                                WebDragOperationsMask mask,
                                const WebImage& dragImage,
                                const WebPoint& dragImageOffset)
{
    if (!m_client)
        return;
    ASSERT(!m_doingDragAndDrop);
    m_doingDragAndDrop = true;
    m_client->startDragging(WebLocalFrameImpl::fromFrame(frame), dragData, mask, dragImage, dragImageOffset);
}

void WebViewImpl::setIgnoreInputEvents(bool newValue)
{
    ASSERT(m_ignoreInputEvents != newValue);
    m_ignoreInputEvents = newValue;
}

void WebViewImpl::setBackgroundColorOverride(WebColor color)
{
    m_backgroundColorOverride = color;
    updateLayerTreeBackgroundColor();
}

void WebViewImpl::setZoomFactorOverride(float zoomFactor)
{
    m_zoomFactorOverride = zoomFactor;
    setZoomLevel(zoomLevel());
}

void WebViewImpl::addPageOverlay(WebPageOverlay* overlay, int zOrder)
{
    if (!m_pageOverlays)
        m_pageOverlays = PageOverlayList::create(this);

    m_pageOverlays->add(overlay, zOrder);
}

void WebViewImpl::removePageOverlay(WebPageOverlay* overlay)
{
    if (m_pageOverlays && m_pageOverlays->remove(overlay) && m_pageOverlays->empty())
        m_pageOverlays = nullptr;
}

void WebViewImpl::setOverlayLayer(WebCore::GraphicsLayer* layer)
{
    if (!m_rootGraphicsLayer)
        return;

    if (!m_page->mainFrame()->isLocalFrame())
        return;

    if (pinchVirtualViewportEnabled()) {
        m_page->deprecatedLocalMainFrame()->view()->renderView()->compositor()->setOverlayLayer(layer);
        return;
    }

    // FIXME(bokan): This path goes away after virtual viewport pinch is enabled everywhere.
    if (!m_rootTransformLayer)
        m_rootTransformLayer = m_page->deprecatedLocalMainFrame()->view()->renderView()->compositor()->ensureRootTransformLayer();

    if (m_rootTransformLayer) {
        if (layer->parent() != m_rootTransformLayer)
            m_rootTransformLayer->addChild(layer);
    }
}

Element* WebViewImpl::focusedElement() const
{
    Frame* frame = m_page->focusController().focusedFrame();
    if (!frame || !frame->isLocalFrame())
        return 0;

    Document* document = toLocalFrame(frame)->document();
    if (!document)
        return 0;

    return document->focusedElement();
}

HitTestResult WebViewImpl::hitTestResultForWindowPos(const IntPoint& pos)
{
    if (!m_page->mainFrame()->isLocalFrame())
        return HitTestResult();
    IntPoint docPoint(m_page->deprecatedLocalMainFrame()->view()->windowToContents(pos));
    return m_page->deprecatedLocalMainFrame()->eventHandler().hitTestResultAtPoint(docPoint, HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::ConfusingAndOftenMisusedDisallowShadowContent);
}

void WebViewImpl::setTabsToLinks(bool enable)
{
    m_tabsToLinks = enable;
}

bool WebViewImpl::tabsToLinks() const
{
    return m_tabsToLinks;
}

void WebViewImpl::suppressInvalidations(bool enable)
{
    if (m_client)
        m_client->suppressCompositorScheduling(enable);
}

void WebViewImpl::setRootGraphicsLayer(GraphicsLayer* layer)
{
    suppressInvalidations(true);

    if (pinchVirtualViewportEnabled()) {
        PinchViewport& pinchViewport = page()->frameHost().pinchViewport();
        pinchViewport.attachToLayerTree(layer, graphicsLayerFactory());
        if (layer) {
            m_rootGraphicsLayer = pinchViewport.rootGraphicsLayer();
            m_rootLayer = pinchViewport.rootGraphicsLayer()->platformLayer();
            m_rootTransformLayer = pinchViewport.rootGraphicsLayer();
        } else {
            m_rootGraphicsLayer = 0;
            m_rootLayer = 0;
            m_rootTransformLayer = 0;
        }
    } else {
        m_rootGraphicsLayer = layer;
        m_rootLayer = layer ? layer->platformLayer() : 0;
        m_rootTransformLayer = 0;
    }

    setIsAcceleratedCompositingActive(layer);

    updateRootLayerTransform();

    if (m_layerTreeView) {
        if (m_rootLayer) {
            m_layerTreeView->setRootLayer(*m_rootLayer);
            // We register viewport layers here since there may not be a layer
            // tree view prior to this point.
            if (pinchVirtualViewportEnabled()) {
                page()->frameHost().pinchViewport().registerLayersWithTreeView(m_layerTreeView);
            } else {
                GraphicsLayer* rootScrollLayer = compositor()->scrollLayer();
                ASSERT(rootScrollLayer);
                WebLayer* pageScaleLayer = rootScrollLayer->parent() ? rootScrollLayer->parent()->platformLayer() : 0;
                m_layerTreeView->registerViewportLayers(pageScaleLayer, rootScrollLayer->platformLayer(), 0);
            }
        } else {
            m_layerTreeView->clearRootLayer();
            if (pinchVirtualViewportEnabled())
                page()->frameHost().pinchViewport().clearLayersForTreeView(m_layerTreeView);
            else
                m_layerTreeView->clearViewportLayers();
        }
    }

    suppressInvalidations(false);
}

void WebViewImpl::scheduleCompositingLayerSync()
{
    m_layerTreeView->setNeedsAnimate();
}

void WebViewImpl::scrollRootLayer()
{
    updateLayerTreeViewport();
}

void WebViewImpl::invalidateRect(const IntRect& rect)
{
    if (m_isAcceleratedCompositingActive) {
        ASSERT(m_layerTreeView);
        updateLayerTreeViewport();
    } else if (m_client)
        m_client->didInvalidateRect(rect);
}

WebCore::GraphicsLayerFactory* WebViewImpl::graphicsLayerFactory() const
{
    return m_graphicsLayerFactory.get();
}

WebCore::RenderLayerCompositor* WebViewImpl::compositor() const
{
    if (!page()
        || !page()->mainFrame()
        || !page()->mainFrame()->isLocalFrame()
        || !page()->deprecatedLocalMainFrame()->document()
        || !page()->deprecatedLocalMainFrame()->document()->renderView())
        return 0;
    return page()->deprecatedLocalMainFrame()->document()->renderView()->compositor();
}

void WebViewImpl::registerForAnimations(WebLayer* layer)
{
    if (m_layerTreeView)
        m_layerTreeView->registerForAnimations(layer);
}

WebCore::GraphicsLayer* WebViewImpl::rootGraphicsLayer()
{
    return m_rootGraphicsLayer;
}

void WebViewImpl::scheduleAnimation()
{
    if (isAcceleratedCompositingActive()) {
        ASSERT(m_layerTreeView);
        m_layerTreeView->setNeedsAnimate();
        return;
    }
    if (m_client)
        m_client->scheduleAnimation();
}

void WebViewImpl::setIsAcceleratedCompositingActive(bool active)
{
    blink::Platform::current()->histogramEnumeration("GPU.setIsAcceleratedCompositingActive", active * 2 + m_isAcceleratedCompositingActive, 4);

    if (m_isAcceleratedCompositingActive == active)
        return;

    if (!m_client)
        return;

    if (!active) {
        m_isAcceleratedCompositingActive = false;
        if (!m_layerTreeViewCommitsDeferred
            && blink::Platform::current()->isThreadedCompositingEnabled()) {
            ASSERT(m_layerTreeView);
            // In threaded compositing mode, force compositing mode is always on so setIsAcceleratedCompositingActive(false)
            // means that we're transitioning to a new page. Suppress commits until WebKit generates invalidations so
            // we don't attempt to paint too early in the next page load.
            m_layerTreeView->setDeferCommits(true);
            m_layerTreeViewCommitsDeferred = true;
        }
    } else if (m_layerTreeView) {
        m_isAcceleratedCompositingActive = true;
        updateLayerTreeViewport();
        if (m_pageOverlays)
            m_pageOverlays->update();
    } else {
        TRACE_EVENT0("webkit", "WebViewImpl::setIsAcceleratedCompositingActive(true)");

        m_client->initializeLayerTreeView();
        m_layerTreeView = m_client->layerTreeView();
        if (m_layerTreeView) {
            m_layerTreeView->setRootLayer(*m_rootLayer);

            bool visible = page()->visibilityState() == PageVisibilityStateVisible;
            m_layerTreeView->setVisible(visible);
            updateLayerTreeDeviceScaleFactor();
            m_layerTreeView->setPageScaleFactorAndLimits(pageScaleFactor(), minimumPageScaleFactor(), maximumPageScaleFactor());
            updateLayerTreeBackgroundColor();
            m_layerTreeView->setHasTransparentBackground(isTransparent());
#if USE(RUBBER_BANDING)
            RefPtr<Image> overhangImage = OverscrollTheme::theme()->getOverhangImage();
            if (overhangImage && overhangImage->nativeImageForCurrentFrame())
                m_layerTreeView->setOverhangBitmap(overhangImage->nativeImageForCurrentFrame()->bitmap());
#endif
            updateLayerTreeViewport();
            m_isAcceleratedCompositingActive = true;
            if (m_pageOverlays)
                m_pageOverlays->update();
            m_layerTreeView->setShowFPSCounter(m_showFPSCounter);
            m_layerTreeView->setShowPaintRects(m_showPaintRects);
            m_layerTreeView->setShowDebugBorders(m_showDebugBorders);
            m_layerTreeView->setContinuousPaintingEnabled(m_continuousPaintingEnabled);
            m_layerTreeView->setShowScrollBottleneckRects(m_showScrollBottleneckRects);
            m_layerTreeView->heuristicsForGpuRasterizationUpdated(m_matchesHeuristicsForGpuRasterization);
        } else {
            // FIXME: It appears that only unittests, <webview> and android webview
            // printing can hit this code. We should make them not hit this code and
            // then delete this else clause and allowsBrokenNullLayerTreeView.
            // crbug.com/322276 and crbug.com/364716.
            ASSERT(m_client->allowsBrokenNullLayerTreeView());
            m_isAcceleratedCompositingActive = false;
            m_page->settings().setAcceleratedCompositingEnabled(false);
            m_page->updateAcceleratedCompositingSettings();
        }
    }
    if (page() && page()->mainFrame()->isLocalFrame())
        page()->deprecatedLocalMainFrame()->view()->setClipsRepaints(!m_isAcceleratedCompositingActive);
}

void WebViewImpl::updateMainFrameScrollPosition(const IntPoint& scrollPosition, bool programmaticScroll)
{
    if (!page()->mainFrame()->isLocalFrame())
        return;

    FrameView* frameView = page()->deprecatedLocalMainFrame()->view();
    if (!frameView)
        return;

    if (frameView->scrollPosition() == scrollPosition)
        return;

    bool oldProgrammaticScroll = frameView->inProgrammaticScroll();
    frameView->setInProgrammaticScroll(programmaticScroll);
    frameView->notifyScrollPositionChanged(scrollPosition);
    frameView->setInProgrammaticScroll(oldProgrammaticScroll);
}

void WebViewImpl::applyScrollAndScale(const WebSize& scrollDelta, float pageScaleDelta)
{
    if (!mainFrameImpl() || !mainFrameImpl()->frameView())
        return;

    if (pinchVirtualViewportEnabled()) {
        if (pageScaleDelta != 1) {
            // When the virtual viewport is enabled, offsets are already set for us.
            setPageScaleFactor(pageScaleFactor() * pageScaleDelta);
            m_doubleTapZoomPending = false;
        }

        return;
    }

    // TODO(bokan): Old pinch path only - virtual viewport pinch scrolls are automatically updated via GraphicsLayer::DidScroll.
    // this should be removed once old pinch is removed.
    if (pageScaleDelta == 1) {
        TRACE_EVENT_INSTANT2("webkit", "WebViewImpl::applyScrollAndScale::scrollBy", "x", scrollDelta.width, "y", scrollDelta.height);
        WebSize webScrollOffset = mainFrame()->scrollOffset();
        IntPoint scrollOffset(webScrollOffset.width + scrollDelta.width, webScrollOffset.height + scrollDelta.height);
        updateMainFrameScrollPosition(scrollOffset, false);
    } else {
        // The page scale changed, so apply a scale and scroll in a single
        // operation.
        WebSize scrollOffset = mainFrame()->scrollOffset();
        scrollOffset.width += scrollDelta.width;
        scrollOffset.height += scrollDelta.height;

        WebPoint scrollPoint(scrollOffset.width, scrollOffset.height);
        setPageScaleFactor(pageScaleFactor() * pageScaleDelta, scrollPoint);
        m_doubleTapZoomPending = false;
    }
}

void WebViewImpl::updateLayerTreeViewport()
{
    if (!page() || !m_layerTreeView)
        return;

    m_layerTreeView->setPageScaleFactorAndLimits(pageScaleFactor(), minimumPageScaleFactor(), maximumPageScaleFactor());
}

void WebViewImpl::updateLayerTreeBackgroundColor()
{
    if (!m_layerTreeView)
        return;

    m_layerTreeView->setBackgroundColor(alphaChannel(m_backgroundColorOverride) ? m_backgroundColorOverride : backgroundColor());
}

void WebViewImpl::updateLayerTreeDeviceScaleFactor()
{
    ASSERT(page());
    ASSERT(m_layerTreeView);

    float deviceScaleFactor = m_compositorDeviceScaleFactorOverride ? m_compositorDeviceScaleFactorOverride : page()->deviceScaleFactor();
    m_layerTreeView->setDeviceScaleFactor(deviceScaleFactor);
}

void WebViewImpl::updateRootLayerTransform()
{
    // If we don't have a root graphics layer, we won't bother trying to find
    // or update the transform layer.
    if (!m_rootGraphicsLayer)
        return;

    // FIXME(bokan): m_rootTransformLayer is always set here in pinch virtual viewport. This can go away once
    // that's default everywhere.
    if (!m_rootTransformLayer && m_page->mainFrame()->isLocalFrame())
        m_rootTransformLayer = m_page->deprecatedLocalMainFrame()->view()->renderView()->compositor()->ensureRootTransformLayer();

    if (m_rootTransformLayer) {
        WebCore::TransformationMatrix transform;
        transform.translate(m_rootLayerOffset.width, m_rootLayerOffset.height);
        transform = transform.scale(m_rootLayerScale);
        m_rootTransformLayer->setTransform(transform);
    }
}

bool WebViewImpl::detectContentOnTouch(const WebPoint& position)
{
    HitTestResult touchHit = hitTestResultForWindowPos(position);

    if (touchHit.isContentEditable())
        return false;

    Node* node = touchHit.innerNode();
    if (!node || !node->isTextNode())
        return false;

    // Ignore when tapping on links or nodes listening to click events, unless the click event is on the
    // body element, in which case it's unlikely that the original node itself was intended to be clickable.
    for (; node && !isHTMLBodyElement(*node); node = node->parentNode()) {
        if (node->isLink() || node->willRespondToTouchEvents() || node->willRespondToMouseClickEvents())
            return false;
    }

    WebContentDetectionResult content = m_client->detectContentAround(touchHit);
    if (!content.isValid())
        return false;

    m_client->scheduleContentIntent(content.intent());
    return true;
}

void WebViewImpl::setVisibilityState(WebPageVisibilityState visibilityState,
                                     bool isInitialState) {
    if (!page())
        return;

    ASSERT(visibilityState == WebPageVisibilityStateVisible || visibilityState == WebPageVisibilityStateHidden || visibilityState == WebPageVisibilityStatePrerender);
    m_page->setVisibilityState(static_cast<PageVisibilityState>(static_cast<int>(visibilityState)), isInitialState);

    if (m_layerTreeView) {
        bool visible = visibilityState == WebPageVisibilityStateVisible;
        m_layerTreeView->setVisible(visible);
    }
}

bool WebViewImpl::requestPointerLock()
{
    return m_client && m_client->requestPointerLock();
}

void WebViewImpl::requestPointerUnlock()
{
    if (m_client)
        m_client->requestPointerUnlock();
}

bool WebViewImpl::isPointerLocked()
{
    return m_client && m_client->isPointerLocked();
}

void WebViewImpl::pointerLockMouseEvent(const WebInputEvent& event)
{
    AtomicString eventType;
    switch (event.type) {
    case WebInputEvent::MouseDown:
        eventType = EventTypeNames::mousedown;
        break;
    case WebInputEvent::MouseUp:
        eventType = EventTypeNames::mouseup;
        break;
    case WebInputEvent::MouseMove:
        eventType = EventTypeNames::mousemove;
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    const WebMouseEvent& mouseEvent = static_cast<const WebMouseEvent&>(event);

    if (page())
        page()->pointerLockController().dispatchLockedMouseEvent(
            PlatformMouseEventBuilder(mainFrameImpl()->frameView(), mouseEvent),
            eventType);
}

bool WebViewImpl::shouldDisableDesktopWorkarounds()
{
    if (!settings()->viewportEnabled())
        return false;

    // A document is considered adapted to small screen UAs if one of these holds:
    // 1. The author specified viewport has a constrained width that is equal to
    //    the initial viewport width.
    // 2. The author has disabled viewport zoom.

    const PageScaleConstraints& constraints = m_pageScaleConstraintsSet.pageDefinedConstraints();

    if (!mainFrameImpl() || !mainFrameImpl()->frameView())
        return false;

    return mainFrameImpl()->frameView()->layoutSize().width() == m_size.width
        || (constraints.minimumScale == constraints.maximumScale && constraints.minimumScale != -1);
}

} // namespace blink
