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

#include "web/WebViewImpl.h"

#include "core/CSSValueKeywords.h"
#include "core/HTMLNames.h"
#include "core/InputTypeNames.h"
#include "core/clipboard/DataObject.h"
#include "core/dom/Document.h"
#include "core/dom/Fullscreen.h"
#include "core/dom/LayoutTreeBuilderTraversal.h"
#include "core/dom/Text.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/InputMethodController.h"
#include "core/editing/iterators/TextIterator.h"
#include "core/editing/markers/DocumentMarkerController.h"
#include "core/editing/serializers/HTMLInterchange.h"
#include "core/editing/serializers/Serialization.h"
#include "core/events/KeyboardEvent.h"
#include "core/events/UIEventWithKeyState.h"
#include "core/events/WheelEvent.h"
#include "core/fetch/UniqueIdentifier.h"
#include "core/frame/EventHandlerRegistry.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/PageScaleConstraintsSet.h"
#include "core/frame/RemoteFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/SmartClip.h"
#include "core/frame/TopControls.h"
#include "core/frame/UseCounter.h"
#include "core/frame/VisualViewport.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/HTMLPlugInElement.h"
#include "core/html/HTMLTextAreaElement.h"
#include "core/input/EventHandler.h"
#include "core/input/TouchActionUtil.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/TextAutosizer.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/FrameLoaderStateMachine.h"
#include "core/page/ContextMenuController.h"
#include "core/page/ContextMenuProvider.h"
#include "core/page/DragController.h"
#include "core/page/DragData.h"
#include "core/page/DragSession.h"
#include "core/page/FocusController.h"
#include "core/page/FrameTree.h"
#include "core/page/Page.h"
#include "core/page/PagePopupClient.h"
#include "core/page/PointerLockController.h"
#include "core/page/ScopedPageLoadDeferrer.h"
#include "core/page/TouchDisambiguation.h"
#include "core/paint/PaintLayer.h"
#include "core/timing/DOMWindowPerformance.h"
#include "core/timing/Performance.h"
#include "modules/accessibility/AXObject.h"
#include "modules/accessibility/AXObjectCacheImpl.h"
#include "modules/credentialmanager/CredentialManagerClient.h"
#include "modules/encryptedmedia/MediaKeysController.h"
#include "modules/storage/StorageNamespaceController.h"
#include "modules/webgl/WebGLRenderingContext.h"
#include "platform/ContextMenu.h"
#include "platform/ContextMenuItem.h"
#include "platform/Cursor.h"
#include "platform/Histogram.h"
#include "platform/KeyboardCodes.h"
#include "platform/Logging.h"
#include "platform/PlatformGestureEvent.h"
#include "platform/PlatformKeyboardEvent.h"
#include "platform/PlatformMouseEvent.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/TraceEvent.h"
#include "platform/UserGestureIndicator.h"
#include "platform/exported/WebActiveGestureAnimation.h"
#include "platform/fonts/FontCache.h"
#include "platform/graphics/Color.h"
#include "platform/graphics/CompositorMutatorClient.h"
#include "platform/graphics/FirstPaintInvalidationTracking.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/Image.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/gpu/DrawingBuffer.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/image-decoders/ImageDecoder.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositeAndReadbackAsyncCallback.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebDragData.h"
#include "public/platform/WebFloatPoint.h"
#include "public/platform/WebGestureCurve.h"
#include "public/platform/WebImage.h"
#include "public/platform/WebLayerTreeView.h"
#include "public/platform/WebScheduler.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/WebVector.h"
#include "public/platform/WebViewScheduler.h"
#include "public/web/WebAXObject.h"
#include "public/web/WebActiveWheelFlingParameters.h"
#include "public/web/WebAutofillClient.h"
#include "public/web/WebElement.h"
#include "public/web/WebFrame.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebFrameWidget.h"
#include "public/web/WebHitTestResult.h"
#include "public/web/WebInputElement.h"
#include "public/web/WebMeaningfulLayout.h"
#include "public/web/WebMediaPlayerAction.h"
#include "public/web/WebNode.h"
#include "public/web/WebPlugin.h"
#include "public/web/WebPluginAction.h"
#include "public/web/WebRange.h"
#include "public/web/WebScopedUserGesture.h"
#include "public/web/WebSelection.h"
#include "public/web/WebTextInputInfo.h"
#include "public/web/WebViewClient.h"
#include "public/web/WebWindowFeatures.h"
#include "web/CompositionUnderlineVectorBuilder.h"
#include "web/CompositorMutatorImpl.h"
#include "web/CompositorProxyClientImpl.h"
#include "web/ContextFeaturesClientImpl.h"
#include "web/ContextMenuAllowedScope.h"
#include "web/DatabaseClientImpl.h"
#include "web/DedicatedWorkerGlobalScopeProxyProviderImpl.h"
#include "web/DevToolsEmulator.h"
#include "web/FullscreenController.h"
#include "web/InspectorOverlay.h"
#include "web/LinkHighlightImpl.h"
#include "web/PageOverlay.h"
#include "web/PrerendererClientImpl.h"
#include "web/ResizeViewportAnchor.h"
#include "web/RotationViewportAnchor.h"
#include "web/SpeechRecognitionClientProxy.h"
#include "web/StorageQuotaClientImpl.h"
#include "web/ValidationMessageClientImpl.h"
#include "web/ViewportAnchor.h"
#include "web/WebDevToolsAgentImpl.h"
#include "web/WebInputEventConversion.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebPagePopupImpl.h"
#include "web/WebPluginContainerImpl.h"
#include "web/WebRemoteFrameImpl.h"
#include "web/WebSettingsImpl.h"
#include "wtf/CurrentTime.h"
#include "wtf/PtrUtil.h"
#include "wtf/RefPtr.h"
#include "wtf/TemporaryChange.h"
#include <memory>

#if USE(DEFAULT_RENDER_THEME)
#include "core/layout/LayoutThemeDefault.h"
#endif

// Get rid of WTF's pow define so we can use std::pow.
#undef pow
#include <cmath> // for std::pow

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
static const float viewportAnchorCoordX = 0.5f;
static const float viewportAnchorCoordY = 0;

// Constants for zooming in on a focused text field.
static const double scrollAndScaleAnimationDurationInSeconds = 0.2;
static const int minReadableCaretHeight = 16;
static const int minReadableCaretHeightForTextArea = 13;
static const float minScaleChangeToTriggerZoom = 1.5f;
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
static Vector<std::unique_ptr<ScopedPageLoadDeferrer>>& pageLoadDeferrerStack()
{
    DEFINE_STATIC_LOCAL(Vector<std::unique_ptr<ScopedPageLoadDeferrer>>, deferrerStack, ());
    return deferrerStack;
}

// Ensure that the WebDragOperation enum values stay in sync with the original
// DragOperation constants.
#define STATIC_ASSERT_ENUM(a, b)                              \
    static_assert(static_cast<int>(a) == static_cast<int>(b), \
        "mismatching enum : " #a)
STATIC_ASSERT_ENUM(DragOperationNone, WebDragOperationNone);
STATIC_ASSERT_ENUM(DragOperationCopy, WebDragOperationCopy);
STATIC_ASSERT_ENUM(DragOperationLink, WebDragOperationLink);
STATIC_ASSERT_ENUM(DragOperationGeneric, WebDragOperationGeneric);
STATIC_ASSERT_ENUM(DragOperationPrivate, WebDragOperationPrivate);
STATIC_ASSERT_ENUM(DragOperationMove, WebDragOperationMove);
STATIC_ASSERT_ENUM(DragOperationDelete, WebDragOperationDelete);
STATIC_ASSERT_ENUM(DragOperationEvery, WebDragOperationEvery);

static bool shouldUseExternalPopupMenus = false;

namespace {

class UserGestureNotifier {
public:
    // If a UserGestureIndicator is created for a user gesture since the last
    // page load and *userGestureObserved is false, the UserGestureNotifier
    // will notify the client and set *userGestureObserved to true.
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
    DCHECK(m_userGestureObserved);
}

UserGestureNotifier::~UserGestureNotifier()
{
    if (!*m_userGestureObserved && UserGestureIndicator::processedUserGestureSinceLoad()) {
        *m_userGestureObserved = true;
        if (m_client)
            m_client->firstUserGestureObserved();
    }
}

class EmptyEventListener final : public EventListener {
public:
    static EmptyEventListener* create()
    {
        return new EmptyEventListener();
    }

    bool operator==(const EventListener& other) const override
    {
        return this == &other;
    }

private:
    EmptyEventListener()
        : EventListener(CPPEventListenerType)
    {
    }

    void handleEvent(ExecutionContext* executionContext, Event*) override
    {
    }
};

class ColorOverlay final : public PageOverlay::Delegate {
public:
    explicit ColorOverlay(WebColor color)
        : m_color(color)
    {
    }

    virtual ~ColorOverlay()
    {
    }

private:
    void paintPageOverlay(const PageOverlay& pageOverlay, GraphicsContext& graphicsContext, const WebSize& size) const override
    {
        if (DrawingRecorder::useCachedDrawingIfPossible(graphicsContext, pageOverlay, DisplayItem::PageOverlay))
            return;
        FloatRect rect(0, 0, size.width, size.height);
        DrawingRecorder drawingRecorder(graphicsContext, pageOverlay, DisplayItem::PageOverlay, rect);
        graphicsContext.fillRect(rect, m_color);
    }

    WebColor m_color;
};

} // namespace

// WebView ----------------------------------------------------------------

WebView* WebView::create(WebViewClient* client, WebPageVisibilityState visibilityState)
{
    // Pass the WebViewImpl's self-reference to the caller.
    return WebViewImpl::create(client, visibilityState);
}

WebViewImpl* WebViewImpl::create(WebViewClient* client, WebPageVisibilityState visibilityState)
{
    // Pass the WebViewImpl's self-reference to the caller.
    return adoptRef(new WebViewImpl(client, visibilityState)).leakRef();
}

void WebView::setUseExternalPopupMenus(bool useExternalPopupMenus)
{
    shouldUseExternalPopupMenus = useExternalPopupMenus;
}

void WebView::updateVisitedLinkState(unsigned long long linkHash)
{
    Page::visitedStateChanged(linkHash);
}

void WebView::resetVisitedLinkState(bool invalidateVisitedLinkHashes)
{
    Page::allVisitedStateChanged(invalidateVisitedLinkHashes);
}

void WebView::willEnterModalLoop()
{
    pageLoadDeferrerStack().append(wrapUnique(new ScopedPageLoadDeferrer()));
}

void WebView::didExitModalLoop()
{
    DCHECK(pageLoadDeferrerStack().size());
    pageLoadDeferrerStack().removeLast();
}

void WebViewImpl::setMainFrame(WebFrame* frame)
{
    frame->toImplBase()->initializeCoreFrame(&page()->frameHost(), 0, nullAtom, nullAtom);
}

void WebViewImpl::setCredentialManagerClient(WebCredentialManagerClient* webCredentialManagerClient)
{
    DCHECK(m_page);
    provideCredentialManagerClientTo(*m_page, new CredentialManagerClient(webCredentialManagerClient));
}

void WebViewImpl::setPrerendererClient(WebPrerendererClient* prerendererClient)
{
    DCHECK(m_page);
    providePrerendererClientTo(*m_page, new PrerendererClientImpl(prerendererClient));
}

void WebViewImpl::setSpellCheckClient(WebSpellCheckClient* spellCheckClient)
{
    m_spellCheckClient = spellCheckClient;
}

// static
HashSet<WebViewImpl*>& WebViewImpl::allInstances()
{
    DEFINE_STATIC_LOCAL(HashSet<WebViewImpl*>, allInstances, ());
    return allInstances;
}

WebViewImpl::WebViewImpl(WebViewClient* client, WebPageVisibilityState visibilityState)
    : m_client(client)
    , m_spellCheckClient(nullptr)
    , m_chromeClientImpl(ChromeClientImpl::create(this))
    , m_contextMenuClientImpl(this)
    , m_editorClientImpl(this)
    , m_spellCheckerClientImpl(this)
    , m_storageClientImpl(this)
    , m_shouldAutoResize(false)
    , m_zoomLevel(0)
    , m_minimumZoomLevel(zoomFactorToZoomLevel(minTextSizeMultiplier))
    , m_maximumZoomLevel(zoomFactorToZoomLevel(maxTextSizeMultiplier))
    , m_zoomFactorForDeviceScaleFactor(0.f)
    , m_maximumLegibleScale(1)
    , m_doubleTapZoomPageScaleFactor(0)
    , m_doubleTapZoomPending(false)
    , m_enableFakePageScaleAnimationForTesting(false)
    , m_fakePageScaleAnimationPageScaleFactor(0)
    , m_fakePageScaleAnimationUseAnchor(false)
    , m_doingDragAndDrop(false)
    , m_ignoreInputEvents(false)
    , m_compositorDeviceScaleFactorOverride(0)
    , m_rootLayerScale(1)
    , m_suppressNextKeypressEvent(false)
    , m_imeAcceptEvents(true)
    , m_operationsAllowed(WebDragOperationNone)
    , m_dragOperation(WebDragOperationNone)
    , m_devToolsEmulator(nullptr)
    , m_isTransparent(false)
    , m_tabsToLinks(false)
    , m_layerTreeView(nullptr)
    , m_rootLayer(nullptr)
    , m_rootGraphicsLayer(nullptr)
    , m_visualViewportContainerLayer(nullptr)
    , m_matchesHeuristicsForGpuRasterization(false)
    , m_flingModifier(0)
    , m_flingSourceDevice(WebGestureDeviceUninitialized)
    , m_fullscreenController(FullscreenController::create(this))
    , m_baseBackgroundColor(Color::white)
    , m_backgroundColorOverride(Color::transparent)
    , m_zoomFactorOverride(0)
    , m_userGestureObserved(false)
    , m_shouldDispatchFirstVisuallyNonEmptyLayout(false)
    , m_shouldDispatchFirstLayoutAfterFinishedParsing(false)
    , m_shouldDispatchFirstLayoutAfterFinishedLoading(false)
    , m_displayMode(WebDisplayModeBrowser)
    , m_elasticOverscroll(FloatSize())
    , m_mutator(nullptr)
    , m_scheduler(wrapUnique(Platform::current()->currentThread()->scheduler()->createWebViewScheduler(this).release()))
    , m_lastFrameTimeMonotonic(0)
    , m_overrideCompositorVisibility(false)
{
    Page::PageClients pageClients;
    pageClients.chromeClient = m_chromeClientImpl.get();
    pageClients.contextMenuClient = &m_contextMenuClientImpl;
    pageClients.editorClient = &m_editorClientImpl;
    pageClients.spellCheckerClient = &m_spellCheckerClientImpl;

    m_page = Page::createOrdinary(pageClients);
    MediaKeysController::provideMediaKeysTo(*m_page, &m_mediaKeysClientImpl);
    provideSpeechRecognitionTo(*m_page, SpeechRecognitionClientProxy::create(client ? client->speechRecognizer() : nullptr));
    provideContextFeaturesTo(*m_page, ContextFeaturesClientImpl::create());
    provideDatabaseClientTo(*m_page, DatabaseClientImpl::create());

    provideStorageQuotaClientTo(*m_page, StorageQuotaClientImpl::create());
    m_page->setValidationMessageClient(ValidationMessageClientImpl::create(*this));
    provideDedicatedWorkerGlobalScopeProxyProviderTo(*m_page, DedicatedWorkerGlobalScopeProxyProviderImpl::create());
    StorageNamespaceController::provideStorageNamespaceTo(*m_page, &m_storageClientImpl);

    setVisibilityState(visibilityState, true);

    initializeLayerTreeView();

    m_devToolsEmulator = DevToolsEmulator::create(this);

    allInstances().add(this);

    m_pageImportanceSignals.setObserver(client);
}

WebViewImpl::~WebViewImpl()
{
    DCHECK(!m_page);

    // Each highlight uses m_owningWebViewImpl->m_linkHighlightsTimeline
    // in destructor. m_linkHighlightsTimeline might be destroyed earlier
    // than m_linkHighlights.
    DCHECK(m_linkHighlights.isEmpty());
}

WebDevToolsAgentImpl* WebViewImpl::mainFrameDevToolsAgentImpl()
{
    WebLocalFrameImpl* mainFrame = mainFrameImpl();
    return mainFrame ? mainFrame->devToolsAgentImpl() : nullptr;
}

InspectorOverlay* WebViewImpl::inspectorOverlay()
{
    if (WebDevToolsAgentImpl* devtools = mainFrameDevToolsAgentImpl())
        return devtools->overlay();
    return nullptr;
}

WebLocalFrameImpl* WebViewImpl::mainFrameImpl() const
{
    return m_page && m_page->mainFrame() && m_page->mainFrame()->isLocalFrame()
        ? WebLocalFrameImpl::fromFrame(m_page->deprecatedLocalMainFrame()) : nullptr;
}

bool WebViewImpl::tabKeyCyclesThroughElements() const
{
    DCHECK(m_page);
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
    // If there is a popup open, close it as the user is clicking on the page
    // (outside of the popup). We also save it so we can prevent a click on an
    // element from immediately reopening the same popup.
    RefPtr<WebPagePopupImpl> pagePopup;
    if (event.button == WebMouseEvent::ButtonLeft) {
        pagePopup = m_pagePopup;
        hidePopups();
        DCHECK(!m_pagePopup);
    }

    // Take capture on a mouse down on a plugin so we can send it mouse events.
    // If the hit node is a plugin but a scrollbar is over it don't start mouse
    // capture because it will interfere with the scrollbar receiving events.
    IntPoint point(event.x, event.y);
    if (event.button == WebMouseEvent::ButtonLeft && m_page->mainFrame()->isLocalFrame()) {
        point = m_page->deprecatedLocalMainFrame()->view()->rootFrameToContents(point);
        HitTestResult result(m_page->deprecatedLocalMainFrame()->eventHandler().hitTestResultAtPoint(point));
        result.setToShadowHostIfInUserAgentShadowRoot();
        Node* hitNode = result.innerNodeOrImageMapImage();

        if (!result.scrollbar() && hitNode && hitNode->layoutObject() && hitNode->layoutObject()->isEmbeddedObject()) {
            m_mouseCaptureNode = hitNode;
            TRACE_EVENT_ASYNC_BEGIN0("input", "capturing mouse", this);
        }
    }

    PageWidgetEventHandler::handleMouseDown(mainFrame, event);

    if (event.button == WebMouseEvent::ButtonLeft && m_mouseCaptureNode)
        m_mouseCaptureGestureToken = mainFrame.eventHandler().takeLastMouseDownGestureToken();

    if (m_pagePopup && pagePopup && m_pagePopup->hasSamePopupClient(pagePopup.get())) {
        // That click triggered a page popup that is the same as the one we just closed.
        // It needs to be closed.
        cancelPagePopup();
    }

    // Dispatch the contextmenu event regardless of if the click was swallowed.
    if (!page()->settings().showContextMenuOnMouseUp()) {
#if OS(MACOSX)
        if (event.button == WebMouseEvent::ButtonRight
            || (event.button == WebMouseEvent::ButtonLeft
                && event.modifiers & WebMouseEvent::ControlKey))
            mouseContextMenu(event);
#else
        if (event.button == WebMouseEvent::ButtonRight)
            mouseContextMenu(event);
#endif
    }
}

void WebViewImpl::setDisplayMode(WebDisplayMode mode)
{
    m_displayMode = mode;
    if (!mainFrameImpl() || !mainFrameImpl()->frameView())
        return;

    mainFrameImpl()->frameView()->setDisplayMode(mode);
}

void WebViewImpl::mouseContextMenu(const WebMouseEvent& event)
{
    if (!mainFrameImpl() || !mainFrameImpl()->frameView())
        return;

    m_page->contextMenuController().clearContextMenu();

    PlatformMouseEventBuilder pme(mainFrameImpl()->frameView(), event);

    // Find the right target frame. See issue 1186900.
    HitTestResult result = hitTestResultForRootFramePos(pme.position());
    Frame* targetFrame;
    if (result.innerNodeOrImageMapImage())
        targetFrame = result.innerNodeOrImageMapImage()->document().frame();
    else
        targetFrame = m_page->focusController().focusedOrMainFrame();

    if (!targetFrame->isLocalFrame())
        return;

    LocalFrame* targetLocalFrame = toLocalFrame(targetFrame);

#if OS(WIN)
    targetLocalFrame->view()->setCursor(pointerCursor());
#endif

    {
        ContextMenuAllowedScope scope;
        targetLocalFrame->eventHandler().sendContextMenuEvent(pme, nullptr);
    }
    // Actually showing the context menu is handled by the ContextMenuClient
    // implementation...
}

void WebViewImpl::handleMouseUp(LocalFrame& mainFrame, const WebMouseEvent& event)
{
    PageWidgetEventHandler::handleMouseUp(mainFrame, event);

    if (page()->settings().showContextMenuOnMouseUp()) {
        // Dispatch the contextmenu event regardless of if the click was swallowed.
        // On Mac/Linux, we handle it on mouse down, not up.
        if (event.button == WebMouseEvent::ButtonRight)
            mouseContextMenu(event);
    }
}

WebInputEventResult WebViewImpl::handleMouseWheel(LocalFrame& mainFrame, const WebMouseWheelEvent& event)
{
    // Halt an in-progress fling on a wheel tick.
    if (!event.hasPreciseScrollingDeltas)
        endActiveFlingAnimation();

    hidePopups();
    return PageWidgetEventHandler::handleMouseWheel(mainFrame, event);
}

WebGestureEvent WebViewImpl::createGestureScrollEventFromFling(WebInputEvent::Type type, WebGestureDevice sourceDevice) const
{
    WebGestureEvent gestureEvent;
    gestureEvent.type = type;
    gestureEvent.sourceDevice = sourceDevice;
    gestureEvent.timeStampSeconds = WTF::monotonicallyIncreasingTime();
    gestureEvent.x = m_positionOnFlingStart.x;
    gestureEvent.y = m_positionOnFlingStart.y;
    gestureEvent.globalX = m_globalPositionOnFlingStart.x;
    gestureEvent.globalY = m_globalPositionOnFlingStart.y;
    gestureEvent.modifiers = m_flingModifier;
    return gestureEvent;
}

bool WebViewImpl::scrollBy(const WebFloatSize& delta, const WebFloatSize& velocity)
{
    DCHECK_NE(m_flingSourceDevice, WebGestureDeviceUninitialized);
    if (!m_page || !m_page->mainFrame() || !m_page->mainFrame()->isLocalFrame() || !m_page->deprecatedLocalMainFrame()->view())
        return false;

    if (m_flingSourceDevice == WebGestureDeviceTouchpad) {
        WebMouseWheelEvent syntheticWheel;
        const float tickDivisor = WheelEvent::TickMultiplier;

        syntheticWheel.type = WebInputEvent::MouseWheel;
        syntheticWheel.timeStampSeconds = WTF::monotonicallyIncreasingTime();
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

        if (handleMouseWheel(*m_page->deprecatedLocalMainFrame(), syntheticWheel) != WebInputEventResult::NotHandled)
            return true;

        // TODO(dtapuska): Remove these GSB/GSE sequences when trackpad latching is implemented; see crbug.com/526463.
        WebGestureEvent syntheticScrollBegin = createGestureScrollEventFromFling(WebInputEvent::GestureScrollBegin, WebGestureDeviceTouchpad);
        syntheticScrollBegin.data.scrollBegin.deltaXHint = delta.width;
        syntheticScrollBegin.data.scrollBegin.deltaYHint = delta.height;
        syntheticScrollBegin.data.scrollBegin.inertialPhase = WebGestureEvent::MomentumPhase;
        handleGestureEvent(syntheticScrollBegin);

        WebGestureEvent syntheticScrollUpdate = createGestureScrollEventFromFling(WebInputEvent::GestureScrollUpdate, WebGestureDeviceTouchpad);
        syntheticScrollUpdate.data.scrollUpdate.deltaX = delta.width;
        syntheticScrollUpdate.data.scrollUpdate.deltaY = delta.height;
        syntheticScrollUpdate.data.scrollUpdate.velocityX = velocity.width;
        syntheticScrollUpdate.data.scrollUpdate.velocityY = velocity.height;
        syntheticScrollUpdate.data.scrollUpdate.inertialPhase = WebGestureEvent::MomentumPhase;
        bool scrollUpdateHandled = handleGestureEvent(syntheticScrollUpdate) != WebInputEventResult::NotHandled;

        WebGestureEvent syntheticScrollEnd = createGestureScrollEventFromFling(WebInputEvent::GestureScrollEnd, WebGestureDeviceTouchpad);
        syntheticScrollEnd.data.scrollEnd.inertialPhase = WebGestureEvent::MomentumPhase;
        handleGestureEvent(syntheticScrollEnd);
        return scrollUpdateHandled;
    } else {
        WebGestureEvent syntheticGestureEvent = createGestureScrollEventFromFling(WebInputEvent::GestureScrollUpdate, WebGestureDeviceTouchscreen);
        syntheticGestureEvent.data.scrollUpdate.preventPropagation = true;
        syntheticGestureEvent.data.scrollUpdate.deltaX = delta.width;
        syntheticGestureEvent.data.scrollUpdate.deltaY = delta.height;
        syntheticGestureEvent.data.scrollUpdate.velocityX = velocity.width;
        syntheticGestureEvent.data.scrollUpdate.velocityY = velocity.height;
        syntheticGestureEvent.data.scrollUpdate.inertialPhase = WebGestureEvent::MomentumPhase;

        return handleGestureEvent(syntheticGestureEvent) != WebInputEventResult::NotHandled;
    }
}

WebInputEventResult WebViewImpl::handleGestureEvent(const WebGestureEvent& event)
{
    if (!m_client)
        return WebInputEventResult::NotHandled;

    WebInputEventResult eventResult = WebInputEventResult::NotHandled;
    bool eventCancelled = false; // for disambiguation

    // Special handling for slow-path fling gestures.
    switch (event.type) {
    case WebInputEvent::GestureFlingStart: {
        if (mainFrameImpl()->frame()->eventHandler().isScrollbarHandlingGestures())
            break;
        endActiveFlingAnimation();
        m_client->cancelScheduledContentIntents();
        m_positionOnFlingStart = WebPoint(event.x, event.y);
        m_globalPositionOnFlingStart = WebPoint(event.globalX, event.globalY);
        m_flingModifier = event.modifiers;
        m_flingSourceDevice = event.sourceDevice;
        DCHECK_NE(m_flingSourceDevice, WebGestureDeviceUninitialized);
        std::unique_ptr<WebGestureCurve> flingCurve = wrapUnique(Platform::current()->createFlingAnimationCurve(event.sourceDevice, WebFloatPoint(event.data.flingStart.velocityX, event.data.flingStart.velocityY), WebSize()));
        DCHECK(flingCurve);
        m_gestureAnimation = WebActiveGestureAnimation::createAtAnimationStart(std::move(flingCurve), this);
        scheduleAnimation();
        eventResult = WebInputEventResult::HandledSystem;

        // Plugins may need to see GestureFlingStart to balance
        // GestureScrollBegin (since the former replaces GestureScrollEnd when
        // transitioning to a fling).
        PlatformGestureEventBuilder platformEvent(mainFrameImpl()->frameView(), event);
        // TODO(dtapuska): Why isn't the response used?
        mainFrameImpl()->frame()->eventHandler().handleGestureScrollEvent(platformEvent);

        m_client->didHandleGestureEvent(event, eventCancelled);
        return WebInputEventResult::HandledSystem;
    }
    case WebInputEvent::GestureFlingCancel:
        if (endActiveFlingAnimation())
            eventResult = WebInputEventResult::HandledSuppressed;

        m_client->didHandleGestureEvent(event, eventCancelled);
        return eventResult;
    default:
        break;
    }

    PlatformGestureEventBuilder platformEvent(mainFrameImpl()->frameView(), event);

    // Special handling for double tap and scroll events as we don't want to
    // hit test for them.
    switch (event.type) {
    case WebInputEvent::GestureDoubleTap:
        if (m_webSettings->doubleTapToZoomEnabled() && minimumPageScaleFactor() != maximumPageScaleFactor()) {
            m_client->cancelScheduledContentIntents();
            animateDoubleTapZoom(platformEvent.position());
        }
        // GestureDoubleTap is currently only used by Android for zooming. For WebCore,
        // GestureTap with tap count = 2 is used instead. So we drop GestureDoubleTap here.
        eventResult = WebInputEventResult::HandledSystem;
        m_client->didHandleGestureEvent(event, eventCancelled);
        return eventResult;
    case WebInputEvent::GestureScrollBegin:
        m_client->cancelScheduledContentIntents();
    case WebInputEvent::GestureScrollEnd:
    case WebInputEvent::GestureScrollUpdate:
    case WebInputEvent::GestureFlingStart:
        // Scrolling-related gesture events invoke EventHandler recursively for each frame down
        // the chain, doing a single-frame hit-test per frame. This matches handleWheelEvent.
        // Perhaps we could simplify things by rewriting scroll handling to work inner frame
        // out, and then unify with other gesture events.
        eventResult = mainFrameImpl()->frame()->eventHandler().handleGestureScrollEvent(platformEvent);
        m_client->didHandleGestureEvent(event, eventCancelled);
        return eventResult;
    case WebInputEvent::GesturePinchBegin:
    case WebInputEvent::GesturePinchEnd:
    case WebInputEvent::GesturePinchUpdate:
        return WebInputEventResult::NotHandled;
    default:
        break;
    }

    // Hit test across all frames and do touch adjustment as necessary for the event type.
    GestureEventWithHitTestResults targetedEvent =
        m_page->deprecatedLocalMainFrame()->eventHandler().targetGestureEvent(platformEvent);

    // Handle link highlighting outside the main switch to avoid getting lost in the
    // complicated set of cases handled below.
    switch (event.type) {
    case WebInputEvent::GestureShowPress:
        // Queue a highlight animation, then hand off to regular handler.
        enableTapHighlightAtPoint(targetedEvent);
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
        // If there is a popup open, close it as the user is clicking on the page
        // (outside of the popup). We also save it so we can prevent a tap on an
        // element from immediately reopening the same popup.
        RefPtr<WebPagePopupImpl> pagePopup = m_pagePopup;
        hidePopups();
        DCHECK(!m_pagePopup);

        m_client->cancelScheduledContentIntents();
        if (detectContentOnTouch(targetedEvent)) {
            eventResult = WebInputEventResult::HandledSystem;
            break;
        }

        // Don't trigger a disambiguation popup on sites designed for mobile devices.
        // Instead, assume that the page has been designed with big enough buttons and links.
        // Don't trigger a disambiguation popup when screencasting, since it's implemented outside of
        // compositor pipeline and is not being screencasted itself. This leads to bad user experience.
        WebDevToolsAgentImpl* devTools = mainFrameDevToolsAgentImpl();
        VisualViewport& visualViewport = page()->frameHost().visualViewport();
        bool screencastEnabled = devTools && devTools->screencastEnabled();
        if (event.data.tap.width > 0 && !visualViewport.shouldDisableDesktopWorkarounds() && !screencastEnabled) {
            IntRect boundingBox(visualViewport.viewportToRootFrame(IntRect(
                event.x - event.data.tap.width / 2,
                event.y - event.data.tap.height / 2,
                event.data.tap.width,
                event.data.tap.height)));

            // TODO(bokan): We shouldn't pass details of the VisualViewport offset to render_view_impl.
            //              crbug.com/459591
            WebSize visualViewportOffset = flooredIntSize(visualViewport.location());

            if (m_webSettings->multiTargetTapNotificationEnabled()) {
                Vector<IntRect> goodTargets;
                HeapVector<Member<Node>> highlightNodes;
                findGoodTouchTargets(boundingBox, mainFrameImpl()->frame(), goodTargets, highlightNodes);
                // FIXME: replace touch adjustment code when numberOfGoodTargets == 1?
                // Single candidate case is currently handled by: https://bugs.webkit.org/show_bug.cgi?id=85101
                if (goodTargets.size() >= 2 && m_client
                    && m_client->didTapMultipleTargets(visualViewportOffset, boundingBox, goodTargets)) {

                    enableTapHighlights(highlightNodes);
                    for (size_t i = 0; i < m_linkHighlights.size(); ++i)
                        m_linkHighlights[i]->startHighlightAnimationIfNeeded();
                    eventResult = WebInputEventResult::HandledSystem;
                    eventCancelled = true;
                    break;
                }
            }
        }

        eventResult = mainFrameImpl()->frame()->eventHandler().handleGestureEvent(targetedEvent);

        if (m_pagePopup && pagePopup && m_pagePopup->hasSamePopupClient(pagePopup.get())) {
            // The tap triggered a page popup that is the same as the one we just closed.
            // It needs to be closed.
            cancelPagePopup();
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
        {
            ContextMenuAllowedScope scope;
            eventResult = mainFrameImpl()->frame()->eventHandler().handleGestureEvent(targetedEvent);
        }

        break;
    }
    case WebInputEvent::GestureShowPress:
        m_client->cancelScheduledContentIntents();
    case WebInputEvent::GestureTapDown:
    case WebInputEvent::GestureTapCancel:
    case WebInputEvent::GestureTapUnconfirmed: {
        eventResult = mainFrameImpl()->frame()->eventHandler().handleGestureEvent(targetedEvent);
        break;
    }
    default:
        NOTREACHED();
    }
    m_client->didHandleGestureEvent(event, eventCancelled);
    return eventResult;
}

WebInputEventResult WebViewImpl::handleSyntheticWheelFromTouchpadPinchEvent(const WebGestureEvent& pinchEvent)
{
    DCHECK_EQ(pinchEvent.type, WebInputEvent::GesturePinchUpdate);

    // Touchscreen pinch events should not reach Blink.
    DCHECK_EQ(pinchEvent.sourceDevice, WebGestureDeviceTouchpad);

    // For pinch gesture events, match typical trackpad behavior on Windows by sending fake
    // wheel events with the ctrl modifier set when we see trackpad pinch gestures.  Ideally
    // we'd someday get a platform 'pinch' event and send that instead.
    WebMouseWheelEvent wheelEvent;
    wheelEvent.type = WebInputEvent::MouseWheel;
    wheelEvent.timeStampSeconds = pinchEvent.timeStampSeconds;
    wheelEvent.windowX = wheelEvent.x = pinchEvent.x;
    wheelEvent.windowY = wheelEvent.y = pinchEvent.y;
    wheelEvent.globalX = pinchEvent.globalX;
    wheelEvent.globalY = pinchEvent.globalY;
    wheelEvent.modifiers =
        pinchEvent.modifiers | WebInputEvent::ControlKey;
    wheelEvent.deltaX = 0;

    // The function to convert scales to deltaY values is designed to be
    // compatible with websites existing use of wheel events, and with existing
    // Windows trackpad behavior.  In particular, we want:
    //  - deltas should accumulate via addition: f(s1*s2)==f(s1)+f(s2)
    //  - deltas should invert via negation: f(1/s) == -f(s)
    //  - zoom in should be positive: f(s) > 0 iff s > 1
    //  - magnitude roughly matches wheels: f(2) > 25 && f(2) < 100
    //  - a formula that's relatively easy to use from JavaScript
    // Note that 'wheel' event deltaY values have their sign inverted.  So to
    // convert a wheel deltaY back to a scale use Math.exp(-deltaY/100).
    DCHECK_GT(pinchEvent.data.pinchUpdate.scale, 0);
    wheelEvent.deltaY = 100.0f * log(pinchEvent.data.pinchUpdate.scale);
    wheelEvent.hasPreciseScrollingDeltas = true;
    wheelEvent.wheelTicksX = 0;
    wheelEvent.wheelTicksY =
        pinchEvent.data.pinchUpdate.scale > 1 ? 1 : -1;

    return handleInputEvent(wheelEvent);
}

void WebViewImpl::transferActiveWheelFlingAnimation(const WebActiveWheelFlingParameters& parameters)
{
    TRACE_EVENT0("blink", "WebViewImpl::transferActiveWheelFlingAnimation");
    DCHECK(!m_gestureAnimation);
    m_positionOnFlingStart = parameters.point;
    m_globalPositionOnFlingStart = parameters.globalPoint;
    m_flingModifier = parameters.modifiers;
    std::unique_ptr<WebGestureCurve> curve = wrapUnique(Platform::current()->createFlingAnimationCurve(parameters.sourceDevice, WebFloatPoint(parameters.delta), parameters.cumulativeScroll));
    DCHECK(curve);
    m_gestureAnimation = WebActiveGestureAnimation::createWithTimeOffset(std::move(curve), this, parameters.startTime);
    DCHECK_NE(parameters.sourceDevice, WebGestureDeviceUninitialized);
    m_flingSourceDevice = parameters.sourceDevice;
    scheduleAnimation();
}

bool WebViewImpl::endActiveFlingAnimation()
{
    if (m_gestureAnimation) {
        m_gestureAnimation.reset();
        m_flingSourceDevice = WebGestureDeviceUninitialized;
        if (m_layerTreeView)
            m_layerTreeView->didStopFlinging();
        return true;
    }
    return false;
}

bool WebViewImpl::startPageScaleAnimation(const IntPoint& targetPosition, bool useAnchor, float newScale, double durationInSeconds)
{
    VisualViewport& visualViewport = page()->frameHost().visualViewport();
    WebPoint clampedPoint = targetPosition;
    if (!useAnchor) {
        clampedPoint = visualViewport.clampDocumentOffsetAtScale(targetPosition, newScale);
        if (!durationInSeconds) {
            setPageScaleFactor(newScale);

            FrameView* view = mainFrameImpl()->frameView();
            if (view && view->getScrollableArea())
                view->getScrollableArea()->setScrollPosition(DoublePoint(clampedPoint.x, clampedPoint.y), ProgrammaticScroll);

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
        TRACE_EVENT0("blink", "WebViewImpl::setShowFPSCounter");
        m_layerTreeView->setShowFPSCounter(show);
    }
}

void WebViewImpl::setShowPaintRects(bool show)
{
    if (m_layerTreeView) {
        TRACE_EVENT0("blink", "WebViewImpl::setShowPaintRects");
        m_layerTreeView->setShowPaintRects(show);
    }
    setFirstPaintInvalidationTrackingEnabledForShowPaintRects(show);
}

void WebViewImpl::setShowDebugBorders(bool show)
{
    if (m_layerTreeView)
        m_layerTreeView->setShowDebugBorders(show);
}

void WebViewImpl::setShowScrollBottleneckRects(bool show)
{
    if (m_layerTreeView)
        m_layerTreeView->setShowScrollBottleneckRects(show);
}

void WebViewImpl::acceptLanguagesChanged()
{
    if (m_client)
        FontCache::acceptLanguagesChanged(m_client->acceptLanguages());

    if (!page())
        return;

    page()->acceptLanguagesChanged();
}

WebInputEventResult WebViewImpl::handleKeyEvent(const WebKeyboardEvent& event)
{
    DCHECK((event.type == WebInputEvent::RawKeyDown)
        || (event.type == WebInputEvent::KeyDown)
        || (event.type == WebInputEvent::KeyUp));
    TRACE_EVENT2("input", "WebViewImpl::handleKeyEvent",
        "type", inputTypeToName(event.type),
        "text", String(event.text).utf8());

    // Halt an in-progress fling on a key event.
    endActiveFlingAnimation();

    // Please refer to the comments explaining the m_suppressNextKeypressEvent
    // member.
    // The m_suppressNextKeypressEvent is set if the KeyDown is handled by
    // Webkit. A keyDown event is typically associated with a keyPress(char)
    // event and a keyUp event. We reset this flag here as this is a new keyDown
    // event.
    m_suppressNextKeypressEvent = false;

    // If there is a popup, it should be the one processing the event, not the
    // page.
    if (m_pagePopup) {
        m_pagePopup->handleKeyEvent(PlatformKeyboardEventBuilder(event));
        // We need to ignore the next Char event after this otherwise pressing
        // enter when selecting an item in the popup will go to the page.
        if (WebInputEvent::RawKeyDown == event.type)
            m_suppressNextKeypressEvent = true;
        return WebInputEventResult::HandledSystem;
    }

    Frame* focusedFrame = focusedCoreFrame();
    if (focusedFrame && focusedFrame->isRemoteFrame()) {
        WebRemoteFrameImpl* webFrame = WebRemoteFrameImpl::fromFrame(*toRemoteFrame(focusedFrame));
        webFrame->client()->forwardInputEvent(&event);
        return WebInputEventResult::HandledSystem;
    }

    if (!focusedFrame || !focusedFrame->isLocalFrame())
        return WebInputEventResult::NotHandled;

    LocalFrame* frame = toLocalFrame(focusedFrame);

    PlatformKeyboardEventBuilder evt(event);

    WebInputEventResult result = frame->eventHandler().keyEvent(evt);
    if (result != WebInputEventResult::NotHandled) {
        if (WebInputEvent::RawKeyDown == event.type) {
            // Suppress the next keypress event unless the focused node is a plugin node.
            // (Flash needs these keypress events to handle non-US keyboards.)
            Element* element = focusedElement();
            if (element && element->layoutObject() && element->layoutObject()->isEmbeddedObject()) {
                if (event.windowsKeyCode == VKEY_TAB) {
                    // If the plugin supports keyboard focus then we should not send a tab keypress event.
                    Widget* widget = toLayoutPart(element->layoutObject())->widget();
                    if (widget && widget->isPluginContainer()) {
                        WebPluginContainerImpl* plugin = toWebPluginContainerImpl(widget);
                        if (plugin && plugin->supportsKeyboardFocus())
                            m_suppressNextKeypressEvent = true;
                    }
                }
            } else {
                m_suppressNextKeypressEvent = true;
            }
        }
        return result;
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
        return WebInputEventResult::HandledSystem;
    }
#endif // !OS(MACOSX)

    if (keyEventDefault(event))
        return WebInputEventResult::HandledSystem;
    return WebInputEventResult::NotHandled;
}

WebInputEventResult WebViewImpl::handleCharEvent(const WebKeyboardEvent& event)
{
    DCHECK_EQ(event.type, WebInputEvent::Char);
    TRACE_EVENT1("input", "WebViewImpl::handleCharEvent",
        "text", String(event.text).utf8());

    // Please refer to the comments explaining the m_suppressNextKeypressEvent
    // member.  The m_suppressNextKeypressEvent is set if the KeyDown is
    // handled by Webkit. A keyDown event is typically associated with a
    // keyPress(char) event and a keyUp event. We reset this flag here as it
    // only applies to the current keyPress event.
    bool suppress = m_suppressNextKeypressEvent;
    m_suppressNextKeypressEvent = false;

    // If there is a popup, it should be the one processing the event, not the
    // page.
    if (m_pagePopup)
        return m_pagePopup->handleKeyEvent(PlatformKeyboardEventBuilder(event));

    LocalFrame* frame = toLocalFrame(focusedCoreFrame());
    if (!frame)
        return suppress ? WebInputEventResult::HandledSuppressed : WebInputEventResult::NotHandled;

    EventHandler& handler = frame->eventHandler();

    PlatformKeyboardEventBuilder evt(event);
    if (!evt.isCharacterKey())
        return WebInputEventResult::HandledSuppressed;

    // Accesskeys are triggered by char events and can't be suppressed.
    if (handler.handleAccessKey(evt))
        return WebInputEventResult::HandledSystem;

    // Safari 3.1 does not pass off windows system key messages (WM_SYSCHAR) to
    // the eventHandler::keyEvent. We mimic this behavior on all platforms since
    // for now we are converting other platform's key events to windows key
    // events.
    if (evt.isSystemKey())
        return WebInputEventResult::NotHandled;

    if (suppress)
        return WebInputEventResult::HandledSuppressed;

    WebInputEventResult result = handler.keyEvent(evt);
    if (result != WebInputEventResult::NotHandled)
        return result;
    if (keyEventDefault(event))
        return WebInputEventResult::HandledSystem;

    return WebInputEventResult::NotHandled;
}

WebRect WebViewImpl::computeBlockBound(const WebPoint& pointInRootFrame, bool ignoreClipping)
{
    if (!mainFrameImpl())
        return WebRect();

    // Use the point-based hit test to find the node.
    IntPoint point = mainFrameImpl()->frameView()->rootFrameToContents(IntPoint(pointInRootFrame.x, pointInRootFrame.y));
    HitTestRequest::HitTestRequestType hitType = HitTestRequest::ReadOnly | HitTestRequest::Active | (ignoreClipping ? HitTestRequest::IgnoreClipping : 0);
    HitTestResult result = mainFrameImpl()->frame()->eventHandler().hitTestResultAtPoint(point, hitType);
    result.setToShadowHostIfInUserAgentShadowRoot();

    Node* node = result.innerNodeOrImageMapImage();
    if (!node)
        return WebRect();

    // Find the block type node based on the hit node.
    // FIXME: This wants to walk flat tree with LayoutTreeBuilderTraversal::parent().
    while (node && (!node->layoutObject() || node->layoutObject()->isInline()))
        node = LayoutTreeBuilderTraversal::parent(*node);

    // Return the bounding box in the root frame's coordinate space.
    if (node) {
        IntRect pointInRootFrame = node->Node::pixelSnappedBoundingBox();
        LocalFrame* frame = node->document().frame();
        return frame->view()->contentsToRootFrame(pointInRootFrame);
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

    DCHECK_GE(newWidth, 0);
    DCHECK_LE(scrollOffset.width() + newX + newWidth, maxSize.width);

    return WebRect(newX, source.y, newWidth, source.height);
}

float WebViewImpl::maximumLegiblePageScale() const
{
    // Pages should be as legible as on desktop when at dpi scale, so no
    // need to zoom in further when automatically determining zoom level
    // (after double tap, find in page, etc), though the user should still
    // be allowed to manually pinch zoom in further if they desire.
    if (page())
        return m_maximumLegibleScale * page()->settings().accessibilityFontScaleFactor();
    return m_maximumLegibleScale;
}

void WebViewImpl::computeScaleAndScrollForBlockRect(const WebPoint& hitPointInRootFrame, const WebRect& blockRectInRootFrame, float padding, float defaultScaleWhenAlreadyLegible, float& scale, WebPoint& scroll)
{
    scale = pageScaleFactor();
    scroll.x = scroll.y = 0;

    WebRect rect = blockRectInRootFrame;

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
        scale = std::min(scale, maximumLegiblePageScale());
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
        rect.y = std::max<float>(rect.y, hitPointInRootFrame.y + padding - screenHeight);
    } // Otherwise top align the block.

    // Do the same thing for horizontal alignment.
    if (rect.width < screenWidth)
        rect.x -= 0.5 * (screenWidth - rect.width);
    else
        rect.x = std::max<float>(rect.x, hitPointInRootFrame.x + padding - screenWidth);
    scroll.x = rect.x;
    scroll.y = rect.y;

    scale = clampPageScaleFactorToLimits(scale);
    scroll = mainFrameImpl()->frameView()->rootFrameToContents(scroll);
    scroll = page()->frameHost().visualViewport().clampDocumentOffsetAtScale(scroll, scale);
}

static Node* findCursorDefiningAncestor(Node* node, LocalFrame* frame)
{
    // Go up the tree to find the node that defines a mouse cursor style
    while (node) {
        if (node->layoutObject()) {
            ECursor cursor = node->layoutObject()->style()->cursor();
            if (cursor != CURSOR_AUTO || frame->eventHandler().useHandCursor(node, node->isLink()))
                break;
        }
        node = LayoutTreeBuilderTraversal::parent(*node);
    }

    return node;
}

static bool showsHandCursor(Node* node, LocalFrame* frame)
{
    if (!node || !node->layoutObject())
        return false;

    ECursor cursor = node->layoutObject()->style()->cursor();
    return cursor == CURSOR_POINTER
        || (cursor == CURSOR_AUTO && frame->eventHandler().useHandCursor(node, node->isLink()));
}

Node* WebViewImpl::bestTapNode(const GestureEventWithHitTestResults& targetedTapEvent)
{
    TRACE_EVENT0("input", "WebViewImpl::bestTapNode");

    if (!m_page || !m_page->mainFrame())
        return nullptr;

    Node* bestTouchNode = targetedTapEvent.hitTestResult().innerNode();
    if (!bestTouchNode)
        return nullptr;

    // We might hit something like an image map that has no layoutObject on it
    // Walk up the tree until we have a node with an attached layoutObject
    while (!bestTouchNode->layoutObject()) {
        bestTouchNode = LayoutTreeBuilderTraversal::parent(*bestTouchNode);
        if (!bestTouchNode)
            return nullptr;
    }

    // Editable nodes should not be highlighted (e.g., <input>)
    if (bestTouchNode->hasEditableStyle())
        return nullptr;

    Node* cursorDefiningAncestor =
        findCursorDefiningAncestor(bestTouchNode, m_page->deprecatedLocalMainFrame());
    // We show a highlight on tap only when the current node shows a hand cursor
    if (!cursorDefiningAncestor || !showsHandCursor(cursorDefiningAncestor, m_page->deprecatedLocalMainFrame())) {
        return nullptr;
    }

    // We should pick the largest enclosing node with hand cursor set. We do this by first jumping
    // up to cursorDefiningAncestor (which is already known to have hand cursor set). Then we locate
    // the next cursor-defining ancestor up in the the tree and repeat the jumps as long as the node
    // has hand cursor set.
    do {
        bestTouchNode = cursorDefiningAncestor;
        cursorDefiningAncestor = findCursorDefiningAncestor(LayoutTreeBuilderTraversal::parent(*bestTouchNode),
            m_page->deprecatedLocalMainFrame());
    } while (cursorDefiningAncestor && showsHandCursor(cursorDefiningAncestor, m_page->deprecatedLocalMainFrame()));

    return bestTouchNode;
}

void WebViewImpl::enableTapHighlightAtPoint(const GestureEventWithHitTestResults& targetedTapEvent)
{
    Node* touchNode = bestTapNode(targetedTapEvent);

    HeapVector<Member<Node>> highlightNodes;
    highlightNodes.append(touchNode);

    enableTapHighlights(highlightNodes);
}

void WebViewImpl::enableTapHighlights(HeapVector<Member<Node>>& highlightNodes)
{
    if (highlightNodes.isEmpty())
        return;

    // Always clear any existing highlight when this is invoked, even if we
    // don't get a new target to highlight.
    m_linkHighlights.clear();

    for (size_t i = 0; i < highlightNodes.size(); ++i) {
        Node* node = highlightNodes[i];

        if (!node || !node->layoutObject())
            continue;

        Color highlightColor = node->layoutObject()->style()->tapHighlightColor();
        // Safari documentation for -webkit-tap-highlight-color says if the specified color has 0 alpha,
        // then tap highlighting is disabled.
        // http://developer.apple.com/library/safari/#documentation/appleapplications/reference/safaricssref/articles/standardcssproperties.html
        if (!highlightColor.alpha())
            continue;

        m_linkHighlights.append(LinkHighlightImpl::create(node, this));
    }

    updateAllLifecyclePhases();
}

void WebViewImpl::animateDoubleTapZoom(const IntPoint& pointInRootFrame)
{
    if (!mainFrameImpl())
        return;

    WebRect blockBounds = computeBlockBound(pointInRootFrame, false);
    float scale;
    WebPoint scroll;

    computeScaleAndScrollForBlockRect(pointInRootFrame, blockBounds, touchPointPadding, minimumPageScaleFactor() * doubleTapZoomAlreadyLegibleRatio, scale, scroll);

    bool stillAtPreviousDoubleTapScale = (pageScaleFactor() == m_doubleTapZoomPageScaleFactor
        && m_doubleTapZoomPageScaleFactor != minimumPageScaleFactor())
        || m_doubleTapZoomPending;

    bool scaleUnchanged = fabs(pageScaleFactor() - scale) < minScaleDifference;
    bool shouldZoomOut = blockBounds.isEmpty() || scaleUnchanged || stillAtPreviousDoubleTapScale;

    bool isAnimating;

    if (shouldZoomOut) {
        scale = minimumPageScaleFactor();
        IntPoint targetPosition = mainFrameImpl()->frameView()->rootFrameToContents(pointInRootFrame);
        isAnimating = startPageScaleAnimation(targetPosition, true, scale, doubleTapZoomAnimationDurationInSeconds);
    } else {
        isAnimating = startPageScaleAnimation(scroll, false, scale, doubleTapZoomAnimationDurationInSeconds);
    }

    // TODO(dglazkov): The only reason why we're using isAnimating and not just checking for
    // m_layerTreeView->hasPendingPageScaleAnimation() is because of fake page scale animation plumbing
    // for testing, which doesn't actually initiate a page scale animation.
    if (isAnimating) {
        m_doubleTapZoomPageScaleFactor = scale;
        m_doubleTapZoomPending = true;
    }
}

void WebViewImpl::zoomToFindInPageRect(const WebRect& rectInRootFrame)
{
    if (!mainFrameImpl())
        return;

    WebRect blockBounds = computeBlockBound(WebPoint(rectInRootFrame.x + rectInRootFrame.width / 2, rectInRootFrame.y + rectInRootFrame.height / 2), true);

    if (blockBounds.isEmpty()) {
        // Keep current scale (no need to scroll as x,y will normally already
        // be visible). FIXME: Revisit this if it isn't always true.
        return;
    }

    float scale;
    WebPoint scroll;

    computeScaleAndScrollForBlockRect(WebPoint(rectInRootFrame.x, rectInRootFrame.y), blockBounds, nonUserInitiatedPointPadding, minimumPageScaleFactor(), scale, scroll);

    startPageScaleAnimation(scroll, false, scale, findInPageAnimationDurationInSeconds);
}

bool WebViewImpl::zoomToMultipleTargetsRect(const WebRect& rectInRootFrame)
{
    if (!mainFrameImpl())
        return false;

    float scale;
    WebPoint scroll;

    computeScaleAndScrollForBlockRect(WebPoint(rectInRootFrame.x, rectInRootFrame.y), rectInRootFrame, nonUserInitiatedPointPadding, minimumPageScaleFactor(), scale, scroll);

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
WebInputEventResult WebViewImpl::sendContextMenuEvent(const WebKeyboardEvent& event)
{
    // The contextMenuController() holds onto the last context menu that was
    // popped up on the page until a new one is created. We need to clear
    // this menu before propagating the event through the DOM so that we can
    // detect if we create a new menu for this event, since we won't create
    // a new menu if the DOM swallows the event and the defaultEventHandler does
    // not run.
    page()->contextMenuController().clearContextMenu();

    {
        ContextMenuAllowedScope scope;
        Frame* focusedFrame = page()->focusController().focusedOrMainFrame();
        if (!focusedFrame->isLocalFrame())
            return WebInputEventResult::NotHandled;
        // Firefox reveal focus based on "keydown" event but not "contextmenu" event, we match FF.
        if (Element* focusedElement = toLocalFrame(focusedFrame)->document()->focusedElement())
            focusedElement->scrollIntoViewIfNeeded();
        return toLocalFrame(focusedFrame)->eventHandler().sendContextMenuEventForKey(nullptr);
    }
}
#endif

void WebViewImpl::showContextMenuAtPoint(float x, float y, ContextMenuProvider* menuProvider)
{
    if (!page()->mainFrame()->isLocalFrame())
        return;
    {
        ContextMenuAllowedScope scope;
        page()->contextMenuController().clearContextMenu();
        page()->contextMenuController().showContextMenuAtPoint(page()->deprecatedLocalMainFrame(), x, y, menuProvider);
    }
}

void WebViewImpl::showContextMenuForElement(WebElement element)
{
    if (!page())
        return;

    page()->contextMenuController().clearContextMenu();
    {
        ContextMenuAllowedScope scope;
        if (LocalFrame* focusedFrame = toLocalFrame(page()->focusController().focusedOrMainFrame()))
            focusedFrame->eventHandler().sendContextMenuEventForKey(element.unwrap<Element>());
    }
}

bool WebViewImpl::keyEventDefault(const WebKeyboardEvent& event)
{
    LocalFrame* frame = toLocalFrame(focusedCoreFrame());
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
    ScrollDirectionPhysical scrollDirectionPhysical;
    ScrollGranularity scrollGranularity;
#if OS(MACOSX)
    // Alt-Up/Down should be PageUp/Down on Mac.
    if (modifiers & WebMouseEvent::AltKey) {
      if (keyCode == VKEY_UP)
        keyCode = VKEY_PRIOR;
      else if (keyCode == VKEY_DOWN)
        keyCode = VKEY_NEXT;
    }
#endif
    if (!mapKeyCodeForScroll(keyCode, &scrollDirectionPhysical, &scrollGranularity))
        return false;

    if (LocalFrame* frame = toLocalFrame(focusedCoreFrame()))
        return frame->eventHandler().bubblingScroll(toScrollDirection(scrollDirectionPhysical), scrollGranularity);
    return false;
}

bool WebViewImpl::mapKeyCodeForScroll(
    int keyCode,
    ScrollDirectionPhysical* scrollDirection,
    ScrollGranularity* scrollGranularity)
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

PagePopup* WebViewImpl::openPagePopup(PagePopupClient* client)
{
    DCHECK(client);
    if (hasOpenedPopup())
        hidePopups();
    DCHECK(!m_pagePopup);

    WebWidget* popupWidget = m_client->createPopupMenu(WebPopupTypePage);
    // createPopupMenu returns nullptr if this renderer process is about to die.
    if (!popupWidget)
        return nullptr;
    m_pagePopup = toWebPagePopupImpl(popupWidget);
    if (!m_pagePopup->initialize(this, client)) {
        m_pagePopup->closePopup();
        m_pagePopup = nullptr;
    }
    enablePopupMouseWheelEventListener();
    return m_pagePopup.get();
}

void WebViewImpl::closePagePopup(PagePopup* popup)
{
    DCHECK(popup);
    WebPagePopupImpl* popupImpl = toWebPagePopupImpl(popup);
    DCHECK_EQ(m_pagePopup.get(), popupImpl);
    if (m_pagePopup.get() != popupImpl)
        return;
    m_pagePopup->closePopup();
}

void WebViewImpl::cleanupPagePopup()
{
    m_pagePopup = nullptr;
    disablePopupMouseWheelEventListener();
}

void WebViewImpl::cancelPagePopup()
{
    if (m_pagePopup)
        m_pagePopup->cancel();
}

void WebViewImpl::enablePopupMouseWheelEventListener()
{
    // TODO(kenrb): Popup coordination for out-of-process iframes needs to be
    // added. Because of the early return here a select element
    // popup can remain visible even when the element underneath it is
    // scrolled to a new position. This is part of a larger set of issues with
    // popups.
    // See https://crbug.com/566130
    if (!mainFrameImpl())
        return;
    DCHECK(!m_popupMouseWheelEventListener);
    Document* document = mainFrameImpl()->frame()->document();
    DCHECK(document);
    // We register an empty event listener, EmptyEventListener, so that mouse
    // wheel events get sent to the WebView.
    m_popupMouseWheelEventListener = EmptyEventListener::create();
    document->addEventListener(EventTypeNames::mousewheel, m_popupMouseWheelEventListener, false);
}

void WebViewImpl::disablePopupMouseWheelEventListener()
{
    // TODO(kenrb): Concerns the same as in enablePopupMouseWheelEventListener.
    // See https://crbug.com/566130
    if (!mainFrameImpl())
        return;
    DCHECK(m_popupMouseWheelEventListener);
    Document* document = mainFrameImpl()->frame()->document();
    DCHECK(document);
    // Document may have already removed the event listener, for instance, due
    // to a navigation, but remove it anyway.
    document->removeEventListener(EventTypeNames::mousewheel, m_popupMouseWheelEventListener.release(), false);
}

LocalDOMWindow* WebViewImpl::pagePopupWindow() const
{
    return m_pagePopup ? m_pagePopup->window() : nullptr;
}

Frame* WebViewImpl::focusedCoreFrame() const
{
    return m_page ? m_page->focusController().focusedOrMainFrame() : nullptr;
}

WebViewImpl* WebViewImpl::fromPage(Page* page)
{
    return page ? static_cast<WebViewImpl*>(page->chromeClient().webView()) : nullptr;
}

// WebWidget ------------------------------------------------------------------

void WebViewImpl::close()
{
    WebDevToolsAgentImpl::webViewImplClosed(this);
    DCHECK(allInstances().contains(this));
    allInstances().remove(this);

    if (m_page) {
        // Initiate shutdown for the entire frameset.  This will cause a lot of
        // notifications to be sent.
        m_page->willBeDestroyed();
        m_page.clear();
    }

    // Reset the delegate to prevent notifications being sent as we're being
    // deleted.
    m_client = nullptr;

    deref();  // Balances ref() acquired in WebView::create
}

WebSize WebViewImpl::size()
{
    return m_size;
}

void WebViewImpl::resizeVisualViewport(const WebSize& newSize)
{
    page()->frameHost().visualViewport().setSize(newSize);
    page()->frameHost().visualViewport().clampToBoundaries();
}

void WebViewImpl::performResize()
{
    // We'll keep the initial containing block size from changing when the top
    // controls hide so that the ICB will always be the same size as the
    // viewport with the top controls shown.
    IntSize ICBSize = m_size;
    if (RuntimeEnabledFeatures::inertTopControlsEnabled() && !topControls().shrinkViewport())
        ICBSize.expand(0, -topControls().height());

    pageScaleConstraintsSet().didChangeInitialContainingBlockSize(ICBSize);

    updatePageDefinedViewportConstraints(mainFrameImpl()->frame()->document()->viewportDescription());
    updateMainFrameLayoutSize();

    page()->frameHost().visualViewport().setSize(m_size);

    if (mainFrameImpl()->frameView()) {
        if (!mainFrameImpl()->frameView()->needsLayout())
            postLayoutResize(mainFrameImpl());
    }
}

void WebViewImpl::updateTopControlsState(WebTopControlsState constraint, WebTopControlsState current, bool animate)
{
    topControls().updateConstraintsAndState(constraint, current, animate);

    if (m_layerTreeView)
        m_layerTreeView->updateTopControlsState(constraint, current, animate);
}

void WebViewImpl::didUpdateTopControls()
{
    if (m_layerTreeView) {
        m_layerTreeView->setTopControlsShownRatio(topControls().shownRatio());
        m_layerTreeView->setTopControlsHeight(topControls().height(), topControls().shrinkViewport());
    }

    WebLocalFrameImpl* mainFrame = mainFrameImpl();
    if (!mainFrame)
        return;

    FrameView* view = mainFrame->frameView();
    if (!view)
        return;

    VisualViewport& visualViewport = page()->frameHost().visualViewport();

    {
        // This object will save the current visual viewport offset w.r.t. the
        // document and restore it when the object goes out of scope. It's
        // needed since the top controls adjustment will change the maximum
        // scroll offset and we may need to reposition them to keep the user's
        // apparent position unchanged.
        ResizeViewportAnchor anchor(*view, visualViewport);

        float topControlsViewportAdjustment =
            topControls().layoutHeight() - topControls().contentOffset();
        visualViewport.setTopControlsAdjustment(topControlsViewportAdjustment);

        // Since the FrameView is sized to be the visual viewport at minimum
        // scale, its adjustment must also be scaled by the minimum scale.
        view->setTopControlsViewportAdjustment(
            topControlsViewportAdjustment / minimumPageScaleFactor());
    }
}

TopControls& WebViewImpl::topControls()
{
    return page()->frameHost().topControls();
}

void WebViewImpl::resizeViewWhileAnchored(
    FrameView* view, float topControlsHeight, bool topControlsShrinkLayout)
{
    DCHECK(mainFrameImpl());

    topControls().setHeight(topControlsHeight, topControlsShrinkLayout);

    {
        // Avoids unnecessary invalidations while various bits of state in TextAutosizer are updated.
        TextAutosizer::DeferUpdatePageInfo deferUpdatePageInfo(page());
        performResize();
    }

    m_fullscreenController->updateSize();

    // Update lifecyle phases immediately to recalculate the minimum scale limit for rotation anchoring,
    // and to make sure that no lifecycle states are stale if this WebView is embedded in another one.
    updateAllLifecyclePhases();
}

void WebViewImpl::resizeWithTopControls(const WebSize& newSize, float topControlsHeight, bool topControlsShrinkLayout)
{
    if (m_shouldAutoResize)
        return;

    if (m_size == newSize
        && topControls().height() == topControlsHeight
        && topControls().shrinkViewport() == topControlsShrinkLayout)
        return;

    if (page()->mainFrame() && !page()->mainFrame()->isLocalFrame()) {
        // Viewport resize for a remote main frame does not require any
        // particular action, but the state needs to reflect the correct size
        // so that it can be used for initalization if the main frame gets
        // swapped to a LocalFrame at a later time.
        m_size = newSize;
        pageScaleConstraintsSet().didChangeInitialContainingBlockSize(m_size);
        page()->frameHost().visualViewport().setSize(m_size);
        return;
    }

    WebLocalFrameImpl* mainFrame = mainFrameImpl();
    if (!mainFrame)
        return;

    FrameView* view = mainFrame->frameView();
    if (!view)
        return;

    VisualViewport& visualViewport = page()->frameHost().visualViewport();

    bool isRotation = settings()->mainFrameResizesAreOrientationChanges()
        && m_size.width && contentsSize().width() && newSize.width != m_size.width && !m_fullscreenController->isFullscreen();
    m_size = newSize;

    FloatSize viewportAnchorCoords(viewportAnchorCoordX, viewportAnchorCoordY);
    if (isRotation) {
        RotationViewportAnchor anchor(*view, visualViewport, viewportAnchorCoords, pageScaleConstraintsSet());
        resizeViewWhileAnchored(view, topControlsHeight, topControlsShrinkLayout);
    } else {
        ResizeViewportAnchor anchor(*view, visualViewport);
        resizeViewWhileAnchored(view, topControlsHeight, topControlsShrinkLayout);
    }
    sendResizeEventAndRepaint();
}

void WebViewImpl::resize(const WebSize& newSize)
{
    if (m_shouldAutoResize || m_size == newSize)
        return;

    resizeWithTopControls(
        newSize, topControls().height(), topControls().shrinkViewport());
}

void WebViewImpl::didEnterFullScreen()
{
    m_fullscreenController->didEnterFullScreen();
}

void WebViewImpl::didExitFullScreen()
{
    m_fullscreenController->didExitFullScreen();
}

void WebViewImpl::didUpdateFullScreenSize()
{
    m_fullscreenController->updateSize();
}

void WebViewImpl::beginFrame(double lastFrameTimeMonotonic)
{
    TRACE_EVENT1("blink", "WebViewImpl::beginFrame", "frameTime", lastFrameTimeMonotonic);
    DCHECK(lastFrameTimeMonotonic);

    // Create synthetic wheel events as necessary for fling.
    if (m_gestureAnimation) {
        if (m_gestureAnimation->animate(lastFrameTimeMonotonic))
            scheduleAnimation();
        else {
            DCHECK_NE(m_flingSourceDevice, WebGestureDeviceUninitialized);
            WebGestureDevice lastFlingSourceDevice = m_flingSourceDevice;
            endActiveFlingAnimation();

            PlatformGestureEvent endScrollEvent(PlatformEvent::GestureScrollEnd,
                m_positionOnFlingStart, m_globalPositionOnFlingStart,
                IntSize(), 0, PlatformEvent::NoModifiers, lastFlingSourceDevice == WebGestureDeviceTouchpad ? PlatformGestureSourceTouchpad : PlatformGestureSourceTouchscreen);
            endScrollEvent.setScrollGestureData(0, 0, ScrollByPrecisePixel, 0, 0, ScrollInertialPhaseMomentum, false, -1 /* null plugin id */);

            mainFrameImpl()->frame()->eventHandler().handleGestureScrollEnd(endScrollEvent);
        }
    }

    if (!mainFrameImpl())
        return;

    m_lastFrameTimeMonotonic = lastFrameTimeMonotonic;

    DocumentLifecycle::AllowThrottlingScope throttlingScope(mainFrameImpl()->frame()->document()->lifecycle());
    PageWidgetDelegate::animate(*m_page, lastFrameTimeMonotonic);
}

void WebViewImpl::updateAllLifecyclePhases()
{
    TRACE_EVENT0("blink", "WebViewImpl::updateAllLifecyclePhases");
    if (!mainFrameImpl())
        return;

    DocumentLifecycle::AllowThrottlingScope throttlingScope(mainFrameImpl()->frame()->document()->lifecycle());
    updateLayerTreeBackgroundColor();

    PageWidgetDelegate::updateAllLifecyclePhases(*m_page, *mainFrameImpl()->frame());

    if (InspectorOverlay* overlay = inspectorOverlay()) {
        overlay->updateAllLifecyclePhases();
        // TODO(chrishtr): integrate paint into the overlay's lifecycle.
        if (overlay->pageOverlay() && overlay->pageOverlay()->graphicsLayer())
            overlay->pageOverlay()->graphicsLayer()->paint(nullptr);
    }
    if (m_pageColorOverlay)
        m_pageColorOverlay->graphicsLayer()->paint(nullptr);

    // TODO(chrishtr): link highlights don't currently paint themselves, it's still driven by cc.
    // Fix this.
    for (size_t i = 0; i < m_linkHighlights.size(); ++i)
        m_linkHighlights[i]->updateGeometry();

    if (FrameView* view = mainFrameImpl()->frameView()) {
        LocalFrame* frame = mainFrameImpl()->frame();

        if (m_shouldDispatchFirstVisuallyNonEmptyLayout && view->isVisuallyNonEmpty()) {
            m_shouldDispatchFirstVisuallyNonEmptyLayout = false;
            // TODO(esprehn): Move users of this callback to something
            // better, the heuristic for "visually non-empty" is bad.
            client()->didMeaningfulLayout(WebMeaningfulLayout::VisuallyNonEmpty);
        }

        if (m_shouldDispatchFirstLayoutAfterFinishedParsing && frame->document()->hasFinishedParsing())  {
            m_shouldDispatchFirstLayoutAfterFinishedParsing = false;
            client()->didMeaningfulLayout(WebMeaningfulLayout::FinishedParsing);
        }

        if (m_shouldDispatchFirstLayoutAfterFinishedLoading && frame->document()->isLoadCompleted()) {
            m_shouldDispatchFirstLayoutAfterFinishedLoading = false;
            client()->didMeaningfulLayout(WebMeaningfulLayout::FinishedLoading);
        }
    }
}

void WebViewImpl::paint(WebCanvas* canvas, const WebRect& rect)
{
    // This should only be used when compositing is not being used for this
    // WebView, and it is painting into the recording of its parent.
    DCHECK(!isAcceleratedCompositingActive());

    double paintStart = currentTime();
    PageWidgetDelegate::paint(*m_page, canvas, rect, *m_page->deprecatedLocalMainFrame());
    double paintEnd = currentTime();
    double pixelsPerSec = (rect.width * rect.height) / (paintEnd - paintStart);
    DEFINE_STATIC_LOCAL(CustomCountHistogram, softwarePaintDurationHistogram, ("Renderer4.SoftwarePaintDurationMS", 0, 120, 30));
    softwarePaintDurationHistogram.count((paintEnd - paintStart) * 1000);
    DEFINE_STATIC_LOCAL(CustomCountHistogram, softwarePaintRateHistogram, ("Renderer4.SoftwarePaintMegapixPerSecond", 10, 210, 30));
    softwarePaintRateHistogram.count(pixelsPerSec / 1000000);
}

#if OS(ANDROID)
void WebViewImpl::paintIgnoringCompositing(WebCanvas* canvas, const WebRect& rect)
{
    // This is called on a composited WebViewImpl, but we will ignore it,
    // producing all possible content of the WebViewImpl into the WebCanvas.
    DCHECK(isAcceleratedCompositingActive());
    PageWidgetDelegate::paintIgnoringCompositing(*m_page, canvas, rect, *m_page->deprecatedLocalMainFrame());
}
#endif

void WebViewImpl::layoutAndPaintAsync(WebLayoutAndPaintAsyncCallback* callback)
{
    m_layerTreeView->layoutAndPaintAsync(callback);
}

void WebViewImpl::compositeAndReadbackAsync(WebCompositeAndReadbackAsyncCallback* callback)
{
    m_layerTreeView->compositeAndReadbackAsync(callback);
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

void WebViewImpl::enterFullScreenForElement(Element* element)
{
    m_fullscreenController->enterFullScreenForElement(element);
}

void WebViewImpl::exitFullScreenForElement(Element* element)
{
    m_fullscreenController->exitFullScreenForElement(element);
}

void WebViewImpl::clearCompositedSelection()
{
    if (m_layerTreeView)
        m_layerTreeView->clearSelection();
}

void WebViewImpl::updateCompositedSelection(const WebSelection& selection)
{
    if (m_layerTreeView)
        m_layerTreeView->registerSelection(selection);
}

bool WebViewImpl::hasHorizontalScrollbar()
{
    return mainFrameImpl()->frameView()->horizontalScrollbar();
}

bool WebViewImpl::hasVerticalScrollbar()
{
    return mainFrameImpl()->frameView()->verticalScrollbar();
}

const WebInputEvent* WebViewImpl::m_currentInputEvent = nullptr;

WebInputEventResult WebViewImpl::handleInputEvent(const WebInputEvent& inputEvent)
{
    // TODO(dcheng): The fact that this is getting called when there is no local
    // main frame is problematic and probably indicates a bug in the input event
    // routing code.
    if (!mainFrameImpl())
        return WebInputEventResult::NotHandled;

    WebAutofillClient* autofillClient = mainFrameImpl()->autofillClient();
    UserGestureNotifier notifier(autofillClient, &m_userGestureObserved);
    // On the first input event since page load, |notifier| instructs the
    // autofill client to unblock values of password input fields of any forms
    // on the page. There is a single input event, GestureTap, which can both
    // be the first event after page load, and cause a form submission. In that
    // case, the form submission happens before the autofill client is told
    // to unblock the password values, and so the password values are not
    // submitted. To avoid that, GestureTap is handled explicitly:
    if (inputEvent.type == WebInputEvent::GestureTap && autofillClient) {
        m_userGestureObserved = true;
        autofillClient->firstUserGestureObserved();
    }

    page()->frameHost().visualViewport().startTrackingPinchStats();

    TRACE_EVENT1("input", "WebViewImpl::handleInputEvent", "type", inputTypeToName(inputEvent.type));
    // If we've started a drag and drop operation, ignore input events until
    // we're done.
    if (m_doingDragAndDrop)
        return WebInputEventResult::HandledSuppressed;

    if (m_devToolsEmulator->handleInputEvent(inputEvent))
        return WebInputEventResult::HandledSuppressed;

    if (InspectorOverlay* overlay = inspectorOverlay()) {
        if (overlay->handleInputEvent(inputEvent))
            return WebInputEventResult::HandledSuppressed;
    }

    // Report the event to be NOT processed by WebKit, so that the browser can handle it appropriately.
    if (m_ignoreInputEvents)
        return WebInputEventResult::NotHandled;

    TemporaryChange<const WebInputEvent*> currentEventChange(m_currentInputEvent, &inputEvent);
    UIEventWithKeyState::clearNewTabModifierSetFromIsolatedWorld();

    bool isPointerLocked = false;
    if (WebFrameWidget* widget = mainFrameImpl()->frameWidget()) {
        if (WebWidgetClient* client = widget->client())
            isPointerLocked = client->isPointerLocked();
    }

    if (isPointerLocked && WebInputEvent::isMouseEventType(inputEvent.type)) {
        pointerLockMouseEvent(inputEvent);
        return WebInputEventResult::HandledSystem;
    }

    if (m_mouseCaptureNode && WebInputEvent::isMouseEventType(inputEvent.type)) {
        TRACE_EVENT1("input", "captured mouse event", "type", inputEvent.type);
        // Save m_mouseCaptureNode since mouseCaptureLost() will clear it.
        Node* node = m_mouseCaptureNode;

        // Not all platforms call mouseCaptureLost() directly.
        if (inputEvent.type == WebInputEvent::MouseUp)
            mouseCaptureLost();

        std::unique_ptr<UserGestureIndicator> gestureIndicator;

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
            gestureIndicator = wrapUnique(new UserGestureIndicator(DefinitelyProcessingNewUserGesture));
            m_mouseCaptureGestureToken = gestureIndicator->currentToken();
            break;
        case WebInputEvent::MouseUp:
            eventType = EventTypeNames::mouseup;
            gestureIndicator = wrapUnique(new UserGestureIndicator(m_mouseCaptureGestureToken.release()));
            break;
        default:
            NOTREACHED();
        }

        node->dispatchMouseEvent(
            PlatformMouseEventBuilder(mainFrameImpl()->frameView(), static_cast<const WebMouseEvent&>(inputEvent)),
            eventType, static_cast<const WebMouseEvent&>(inputEvent).clickCount);
        return WebInputEventResult::HandledSystem;
    }

    // FIXME: This should take in the intended frame, not the local frame root.
    WebInputEventResult result = PageWidgetDelegate::handleInputEvent(*this, inputEvent, mainFrameImpl()->frame());
    if (result != WebInputEventResult::NotHandled)
        return result;

    // Unhandled touchpad gesture pinch events synthesize mouse wheel events.
    if (inputEvent.type == WebInputEvent::GesturePinchUpdate) {
        const WebGestureEvent& pinchEvent = static_cast<const WebGestureEvent&>(inputEvent);

        // First, synthesize a Windows-like wheel event to send to any handlers that may exist.
        result = handleSyntheticWheelFromTouchpadPinchEvent(pinchEvent);
        if (result != WebInputEventResult::NotHandled)
            return result;

        if (pinchEvent.data.pinchUpdate.zoomDisabled)
            return WebInputEventResult::NotHandled;

        if (page()->frameHost().visualViewport().magnifyScaleAroundAnchor(pinchEvent.data.pinchUpdate.scale, FloatPoint(pinchEvent.x, pinchEvent.y)))
            return WebInputEventResult::HandledSystem;
    }

    return WebInputEventResult::NotHandled;
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
        LocalFrame* focusedFrame = m_page->focusController().focusedFrame();
        if (focusedFrame) {
            Element* element = focusedFrame->document()->focusedElement();
            if (element && focusedFrame->selection().selection().isNone()) {
                // If the selection was cleared while the WebView was not
                // focused, then the focus element shows with a focus ring but
                // no caret and does respond to keyboard inputs.
                if (element->isTextFormControl()) {
                    element->updateFocusAppearance(SelectionBehaviorOnFocus::Restore);
                } else if (element->isContentEditable()) {
                    // updateFocusAppearance() selects all the text of
                    // contentseditable DIVs. So we set the selection explicitly
                    // instead. Note that this has the side effect of moving the
                    // caret back to the beginning of the text.
                    Position position(element, 0);
                    focusedFrame->selection().setSelection(VisibleSelection(position, SelDefaultAffinity));
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
            ? m_page->deprecatedLocalMainFrame() : nullptr;
        if (!frame)
            return;

        LocalFrame* focusedFrame = m_page->focusController().focusedFrame();
        if (focusedFrame) {
            // Finish an ongoing composition to delete the composition node.
            if (focusedFrame->inputMethodController().hasComposition()) {
                WebAutofillClient* autofillClient = WebLocalFrameImpl::fromFrame(focusedFrame)->autofillClient();

                if (autofillClient)
                    autofillClient->setIgnoreTextChanges(true);

                focusedFrame->inputMethodController().confirmComposition();

                if (autofillClient)
                    autofillClient->setIgnoreTextChanges(false);
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
    LocalFrame* focused = toLocalFrame(focusedCoreFrame());
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
    const EphemeralRange range = inputMethodController.compositionEphemeralRange();
    if (range.isNotNull()) {
        Node* node = range.startPosition().computeContainerNode();
        if (!node || !node->isContentEditable())
            return false;
    }

    // A keypress event is canceled. If an ongoing composition exists, then the
    // keydown event should have arisen from a handled key (e.g., backspace).
    // In this case we ignore the cancellation and continue; otherwise (no
    // ongoing composition) we exit and signal success only for attempts to
    // clear the composition.
    if (m_suppressNextKeypressEvent && !inputMethodController.hasComposition())
        return text.isEmpty();

    UserGestureIndicator gestureIndicator(DefinitelyProcessingNewUserGesture);

    // When the range of composition underlines overlap with the range between
    // selectionStart and selectionEnd, WebKit somehow won't paint the selection
    // at all (see InlineTextBox::paint() function in InlineTextBox.cpp).
    // But the selection range actually takes effect.
    inputMethodController.setComposition(String(text),
                           CompositionUnderlineVectorBuilder(underlines),
                           selectionStart, selectionEnd);

    return text.isEmpty() || inputMethodController.hasComposition();
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
    UserGestureIndicator gestureIndicator(DefinitelyProcessingNewUserGesture);
    return confirmComposition(text, DoNotKeepSelection);
}

bool WebViewImpl::confirmComposition(const WebString& text, ConfirmCompositionBehavior selectionBehavior)
{
    LocalFrame* focused = toLocalFrame(focusedCoreFrame());
    if (!focused || !m_imeAcceptEvents)
        return false;

    if (WebPlugin* plugin = focusedPluginIfInputMethodSupported(focused))
        return plugin->confirmComposition(text, selectionBehavior);

    return focused->inputMethodController().confirmCompositionOrInsertText(text, selectionBehavior == KeepSelection ? InputMethodController::KeepSelection : InputMethodController::DoNotKeepSelection);
}

bool WebViewImpl::compositionRange(size_t* location, size_t* length)
{
    // FIXME: Long term, the focused frame should be a local frame. For now,
    // return early to avoid crashes.
    Frame* frame = focusedCoreFrame();
    if (!frame || frame->isRemoteFrame())
        return false;

    LocalFrame* focused = toLocalFrame(frame);
    if (!focused || !m_imeAcceptEvents)
        return false;

    const EphemeralRange range = focused->inputMethodController().compositionEphemeralRange();
    if (range.isNull())
        return false;

    Element* editable = focused->selection().rootEditableElementOrDocumentElement();
    DCHECK(editable);
    PlainTextRange plainTextRange(PlainTextRange::create(*editable, range));
    if (plainTextRange.isNull())
        return false;
    *location = plainTextRange.start();
    *length = plainTextRange.length();
    return true;
}

WebTextInputInfo WebViewImpl::textInputInfo()
{
    WebTextInputInfo info;

    Frame* focusedFrame = focusedCoreFrame();
    if (!focusedFrame->isLocalFrame())
        return info;

    LocalFrame* focused = toLocalFrame(focusedFrame);
    if (!focused)
        return info;

    FrameSelection& selection = focused->selection();
    if (!selection.isAvailable()) {
        // plugins/mouse-capture-inside-shadow.html reaches here.
        return info;
    }
    Element* element = selection.selection().rootEditableElement();
    if (!element)
        return info;

    info.inputMode = inputModeOfFocusedElement();

    info.type = textInputType();
    info.flags = textInputFlags();
    if (info.type == WebTextInputTypeNone)
        return info;

    if (!focused->editor().canEdit())
        return info;

    // TODO(dglazkov): The use of updateStyleAndLayoutIgnorePendingStylesheets needs to be audited.
    // see http://crbug.com/590369 for more details.
    focused->document()->updateStyleAndLayoutIgnorePendingStylesheets();

    // Emits an object replacement character for each replaced element so that
    // it is exposed to IME and thus could be deleted by IME on android.
    info.value = plainText(EphemeralRange::rangeOfContents(*element), TextIteratorEmitsObjectReplacementCharacter);

    if (info.value.isEmpty())
        return info;

    EphemeralRange firstRange = firstEphemeralRangeOf(selection.selection());
    if (firstRange.isNotNull()) {
        PlainTextRange plainTextRange(PlainTextRange::create(*element, firstRange));
        if (plainTextRange.isNotNull()) {
            info.selectionStart = plainTextRange.start();
            info.selectionEnd = plainTextRange.end();
        }
    }

    EphemeralRange range = focused->inputMethodController().compositionEphemeralRange();
    if (range.isNotNull()) {
        PlainTextRange plainTextRange(PlainTextRange::create(*element, range));
        if (plainTextRange.isNotNull()) {
            info.compositionStart = plainTextRange.start();
            info.compositionEnd = plainTextRange.end();
        }
    }

    return info;
}

WebTextInputType WebViewImpl::textInputType()
{
    LocalFrame* focusedFrame = m_page->focusController().focusedFrame();
    if (!focusedFrame)
        return WebTextInputTypeNone;

    if (!focusedFrame->selection().isAvailable()) {
        // "mouse-capture-inside-shadow.html" reaches here.
        return WebTextInputTypeNone;
    }

    // It's important to preserve the equivalence of textInputInfo().type and textInputType(),
    // so perform the same rootEditableElement() existence check here for consistency.
    if (!focusedFrame->selection().selection().rootEditableElement())
        return WebTextInputTypeNone;

    Document* document = focusedFrame->document();
    if (!document)
        return WebTextInputTypeNone;

    Element* element = document->focusedElement();
    if (!element)
        return WebTextInputTypeNone;

    if (isHTMLInputElement(*element)) {
        HTMLInputElement& input = toHTMLInputElement(*element);
        const AtomicString& type = input.type();

        if (input.isDisabledOrReadOnly())
            return WebTextInputTypeNone;

        if (type == InputTypeNames::password)
            return WebTextInputTypePassword;
        if (type == InputTypeNames::search)
            return WebTextInputTypeSearch;
        if (type == InputTypeNames::email)
            return WebTextInputTypeEmail;
        if (type == InputTypeNames::number)
            return WebTextInputTypeNumber;
        if (type == InputTypeNames::tel)
            return WebTextInputTypeTelephone;
        if (type == InputTypeNames::url)
            return WebTextInputTypeURL;
        if (type == InputTypeNames::date)
            return WebTextInputTypeDate;
        if (type == InputTypeNames::datetime_local)
            return WebTextInputTypeDateTimeLocal;
        if (type == InputTypeNames::month)
            return WebTextInputTypeMonth;
        if (type == InputTypeNames::time)
            return WebTextInputTypeTime;
        if (type == InputTypeNames::week)
            return WebTextInputTypeWeek;
        if (type == InputTypeNames::text)
            return WebTextInputTypeText;

        return WebTextInputTypeNone;
    }

    if (isHTMLTextAreaElement(*element)) {
        if (toHTMLTextAreaElement(*element).isDisabledOrReadOnly())
            return WebTextInputTypeNone;
        return WebTextInputTypeTextArea;
    }

    if (element->isHTMLElement()) {
        if (toHTMLElement(element)->isDateTimeFieldElement())
            return WebTextInputTypeDateTimeField;
    }

    if (element->isContentEditable(Node::UserSelectAllIsAlwaysNonEditable))
        return WebTextInputTypeContentEditable;

    return WebTextInputTypeNone;
}

int WebViewImpl::textInputFlags()
{
    Element* element = focusedElement();
    if (!element)
        return WebTextInputFlagNone;

    DEFINE_STATIC_LOCAL(AtomicString, autocompleteString, ("autocomplete"));
    DEFINE_STATIC_LOCAL(AtomicString, autocorrectString, ("autocorrect"));
    int flags = 0;

    const AtomicString& autocomplete = element->getAttribute(autocompleteString);
    if (autocomplete == "on")
        flags |= WebTextInputFlagAutocompleteOn;
    else if (autocomplete == "off")
        flags |= WebTextInputFlagAutocompleteOff;

    const AtomicString& autocorrect = element->getAttribute(autocorrectString);
    if (autocorrect == "on")
        flags |= WebTextInputFlagAutocorrectOn;
    else if (autocorrect == "off")
        flags |= WebTextInputFlagAutocorrectOff;

    SpellcheckAttributeState spellcheck = element->spellcheckAttributeState();
    if (spellcheck == SpellcheckAttributeTrue)
        flags |= WebTextInputFlagSpellcheckOn;
    else if (spellcheck == SpellcheckAttributeFalse)
        flags |= WebTextInputFlagSpellcheckOff;

    if (isHTMLTextFormControlElement(element)) {
        HTMLTextFormControlElement* formElement = static_cast<HTMLTextFormControlElement*>(element);
        if (formElement->supportsAutocapitalize()) {
            DEFINE_STATIC_LOCAL(const AtomicString, none, ("none"));
            DEFINE_STATIC_LOCAL(const AtomicString, characters, ("characters"));
            DEFINE_STATIC_LOCAL(const AtomicString, words, ("words"));
            DEFINE_STATIC_LOCAL(const AtomicString, sentences, ("sentences"));

            const AtomicString& autocapitalize = formElement->autocapitalize();
            if (autocapitalize == none)
                flags |= WebTextInputFlagAutocapitalizeNone;
            else if (autocapitalize == characters)
                flags |= WebTextInputFlagAutocapitalizeCharacters;
            else if (autocapitalize == words)
                flags |= WebTextInputFlagAutocapitalizeWords;
            else if (autocapitalize == sentences)
                flags |= WebTextInputFlagAutocapitalizeSentences;
            else
                NOTREACHED();
        }
    }

    return flags;
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
    const Frame* frame = focusedCoreFrame();
    if (!frame || !frame->isLocalFrame())
        return false;

    const LocalFrame* localFrame = toLocalFrame(frame);
    if (!localFrame)
        return false;
    FrameSelection& selection = localFrame->selection();
    if (!selection.isAvailable()) {
        // plugins/mouse-capture-inside-shadow.html reaches here.
        return false;
    }

    if (selection.isCaret()) {
        anchor = focus = selection.absoluteCaretBounds();
    } else {
        const EphemeralRange selectedRange = selection.selection().toNormalizedEphemeralRange();
        if (selectedRange.isNull())
            return false;
        anchor = localFrame->editor().firstRectForRange(EphemeralRange(selectedRange.startPosition()));
        focus = localFrame->editor().firstRectForRange(EphemeralRange(selectedRange.endPosition()));
    }

    anchor = localFrame->view()->contentsToViewport(anchor);
    focus = localFrame->view()->contentsToViewport(focus);

    if (!selection.selection().isBaseFirst())
        std::swap(anchor, focus);
    return true;
}

WebPlugin* WebViewImpl::focusedPluginIfInputMethodSupported(LocalFrame* frame)
{
    WebPluginContainerImpl* container = WebLocalFrameImpl::currentPluginContainer(frame);
    if (container && container->supportsInputMethod())
        return container->plugin();
    return nullptr;
}

bool WebViewImpl::selectionTextDirection(WebTextDirection& start, WebTextDirection& end) const
{
    const Frame* frame = focusedCoreFrame();
    if (!frame || frame->isRemoteFrame())
        return false;
    const FrameSelection& selection = toLocalFrame(frame)->selection();
    if (!selection.isAvailable()) {
        // plugins/mouse-capture-inside-shadow.html reaches here.
        return false;
    }
    if (selection.selection().toNormalizedEphemeralRange().isNull())
        return false;
    start = toWebTextDirection(primaryDirectionOf(*selection.start().anchorNode()));
    end = toWebTextDirection(primaryDirectionOf(*selection.end().anchorNode()));
    return true;
}

bool WebViewImpl::isSelectionAnchorFirst() const
{
    const Frame* frame = focusedCoreFrame();
    if (!frame || frame->isRemoteFrame())
        return false;
    FrameSelection& selection = toLocalFrame(frame)->selection();
    if (!selection.isAvailable()) {
        // plugins/mouse-capture-inside-shadow.html reaches here.
        return false;
    }
    return selection.selection().isBaseFirst();
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

WebPagePopup* WebViewImpl::pagePopup() const
{
    return m_pagePopup.get();
}

bool WebViewImpl::caretOrSelectionRange(size_t* location, size_t* length)
{
    const LocalFrame* focused = toLocalFrame(focusedCoreFrame());
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
    const LocalFrame* focused = toLocalFrame(focusedCoreFrame());
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
        NOTIMPLEMENTED();
        break;
    }
}

bool WebViewImpl::isAcceleratedCompositingActive() const
{
    // For SPv2, accelerated compositing is managed by the
    // PaintArtifactCompositor.
    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled())
        return m_paintArtifactCompositor.rootLayer();

    return m_rootLayer;
}

void WebViewImpl::willCloseLayerTreeView()
{
    if (m_linkHighlightsTimeline) {
        m_linkHighlights.clear();
        detachCompositorAnimationTimeline(m_linkHighlightsTimeline.get());
        m_linkHighlightsTimeline.reset();
    }

    if (m_layerTreeView)
        page()->willCloseLayerTreeView(*m_layerTreeView);

    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled())
        detachPaintArtifactCompositor();
    else
        setRootGraphicsLayer(nullptr);

    m_mutator = nullptr;
    m_layerTreeView = nullptr;
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
    m_pointerLockGestureToken.clear();
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
        m_webSettings = wrapUnique(new WebSettingsImpl(&m_page->settings(), m_devToolsEmulator.get()));
    DCHECK(m_webSettings);
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

    // Only change override encoding, don't change default encoding.
    // Note that the new encoding must be 0 if it isn't supposed to be set.
    AtomicString newEncodingName;
    if (!encodingName.isEmpty())
        newEncodingName = encodingName;
    m_page->frameHost().setOverrideEncoding(newEncodingName);

    if (m_page->mainFrame()->isLocalFrame()) {
        if (!m_page->deprecatedLocalMainFrame()->loader().currentItem())
            return;
        FrameLoadRequest request = FrameLoadRequest(
            nullptr,
            m_page->deprecatedLocalMainFrame()->loader().resourceRequestForReload(
                FrameLoadTypeReload, KURL(), ClientRedirectPolicy::ClientRedirect));
        request.setClientRedirect(ClientRedirectPolicy::ClientRedirect);
        m_page->deprecatedLocalMainFrame()->loader().load(request, FrameLoadTypeReload);
    }
}

WebFrame* WebViewImpl::mainFrame()
{
    return WebFrame::fromFrame(m_page ? m_page->mainFrame() : nullptr);
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
        return nullptr;
    return WebLocalFrameImpl::fromFrame(toLocalFrame(frame));
}

WebFrame* WebViewImpl::focusedFrame()
{
    return WebFrame::fromFrame(focusedCoreFrame());
}

void WebViewImpl::setFocusedFrame(WebFrame* frame)
{
    if (!frame) {
        // Clears the focused frame if any.
        Frame* focusedFrame = focusedCoreFrame();
        if (focusedFrame && focusedFrame->isLocalFrame())
            toLocalFrame(focusedFrame)->selection().setFocused(false);
        return;
    }
    LocalFrame* coreFrame = toWebLocalFrameImpl(frame)->frame();
    coreFrame->page()->focusController().setFocusedFrame(coreFrame);
}

void WebViewImpl::focusDocumentView(WebFrame* frame)
{
    // This is currently only used when replicating focus changes for
    // cross-process frames, and |notifyEmbedder| is disabled to avoid sending
    // duplicate frameFocused updates from FocusController to the browser
    // process, which already knows the latest focused frame.
    page()->focusController().focusDocumentView(frame->toImplBase()->frame(), false /* notifyEmbedder */);
}

void WebViewImpl::setInitialFocus(bool reverse)
{
    if (!m_page)
        return;
    Frame* frame = page()->focusController().focusedOrMainFrame();
    if (frame->isLocalFrame()) {
        if (Document* document = toLocalFrame(frame)->document())
            document->clearFocusedElement();
    }
    page()->focusController().setInitialFocus(reverse ? WebFocusTypeBackward : WebFocusTypeForward);
}

void WebViewImpl::clearFocusedElement()
{
    Frame* frame = focusedCoreFrame();
    if (!frame || !frame->isLocalFrame())
        return;

    LocalFrame* localFrame = toLocalFrame(frame);

    Document* document = localFrame->document();
    if (!document)
        return;

    Element* oldFocusedElement = document->focusedElement();
    document->clearFocusedElement();
    if (!oldFocusedElement)
        return;

    // If a text field has focus, we need to make sure the selection controller
    // knows to remove selection from it. Otherwise, the text field is still
    // processing keyboard events even though focus has been moved to the page and
    // keystrokes get eaten as a result.
    if (oldFocusedElement->isContentEditable() || oldFocusedElement->isTextFormControl())
        localFrame->selection().clear();
}

// TODO(dglazkov): Remove and replace with Node:hasEditableStyle.
// http://crbug.com/612560
static bool isElementEditable(const Element* element)
{
    if (element->isContentEditable())
        return true;

    if (element->isTextFormControl()) {
        const HTMLTextFormControlElement* input = toHTMLTextFormControlElement(element);
        if (!input->isDisabledOrReadOnly())
            return true;
    }

    return equalIgnoringCase(element->getAttribute(HTMLNames::roleAttr), "textbox");
}

bool WebViewImpl::scrollFocusedEditableElementIntoRect(const WebRect& rectInViewport)
{
    LocalFrame* frame = page()->mainFrame() && page()->mainFrame()->isLocalFrame()
        ? page()->deprecatedLocalMainFrame() : nullptr;
    Element* element = focusedElement();
    if (!frame || !frame->view() || !element)
        return false;

    if (!isElementEditable(element))
        return false;

    element->document().updateStyleAndLayoutIgnorePendingStylesheets();

    bool zoomInToLegibleScale = m_webSettings->autoZoomFocusedNodeToLegibleScale()
        && !page()->frameHost().visualViewport().shouldDisableDesktopWorkarounds();

    if (zoomInToLegibleScale) {
        // When deciding whether to zoom in on a focused text box, we should decide not to
        // zoom in if the user won't be able to zoom out. e.g if the textbox is within a
        // touch-action: none container the user can't zoom back out.
        TouchAction action = TouchActionUtil::computeEffectiveTouchAction(*element);
        if (!(action & TouchActionPinchZoom))
            zoomInToLegibleScale = false;
    }

    float scale;
    IntPoint scroll;
    bool needAnimation;
    computeScaleAndScrollForFocusedNode(element, zoomInToLegibleScale, scale, scroll, needAnimation);
    if (needAnimation)
        startPageScaleAnimation(scroll, false, scale, scrollAndScaleAnimationDurationInSeconds);

    return true;
}

void WebViewImpl::smoothScroll(int targetX, int targetY, long durationMs)
{
    IntPoint targetPosition(targetX, targetY);
    startPageScaleAnimation(targetPosition, false, pageScaleFactor(), (double)durationMs / 1000);
}

void WebViewImpl::computeScaleAndScrollForFocusedNode(Node* focusedNode, bool zoomInToLegibleScale, float& newScale, IntPoint& newScroll, bool& needAnimation)
{
    VisualViewport& visualViewport = page()->frameHost().visualViewport();

    WebRect caretInViewport, unusedEnd;
    selectionBounds(caretInViewport, unusedEnd);

    // 'caretInDocument' is rect encompassing the blinking cursor relative to the root document.
    IntRect caretInDocument = mainFrameImpl()->frameView()->frameToContents(visualViewport.viewportToRootFrame(caretInViewport));
    IntRect textboxRectInDocument = mainFrameImpl()->frameView()->frameToContents(
        focusedNode->document().view()->contentsToRootFrame(pixelSnappedIntRect(focusedNode->Node::boundingBox())));

    if (!zoomInToLegibleScale) {
        newScale = pageScaleFactor();
    } else {
        // Pick a scale which is reasonably readable. This is the scale at which
        // the caret height will become minReadableCaretHeightForNode (adjusted
        // for dpi and font scale factor).
        const int minReadableCaretHeightForNode = textboxRectInDocument.height() >= 2 * caretInDocument.height() ? minReadableCaretHeightForTextArea : minReadableCaretHeight;
        newScale = clampPageScaleFactorToLimits(maximumLegiblePageScale() * minReadableCaretHeightForNode / caretInDocument.height());
        newScale = std::max(newScale, pageScaleFactor());
    }
    const float deltaScale = newScale / pageScaleFactor();

    needAnimation = false;

    // If we are at less than the target zoom level, zoom in.
    if (deltaScale > minScaleChangeToTriggerZoom)
        needAnimation = true;
    else
        newScale = pageScaleFactor();

    // If the caret is offscreen, then animate.
    if (!visualViewport.visibleRectInDocument().contains(caretInDocument))
        needAnimation = true;

    // If the box is partially offscreen and it's possible to bring it fully
    // onscreen, then animate.
    if (visualViewport.visibleRect().width() >= textboxRectInDocument.width()
        && visualViewport.visibleRect().height() >= textboxRectInDocument.height()
        && !visualViewport.visibleRectInDocument().contains(textboxRectInDocument))
        needAnimation = true;

    if (!needAnimation)
        return;

    FloatSize targetViewportSize(visualViewport.size());
    targetViewportSize.scale(1 / newScale);

    if (textboxRectInDocument.width() <= targetViewportSize.width()) {
        // Field is narrower than screen. Try to leave padding on left so field's
        // label is visible, but it's more important to ensure entire field is
        // onscreen.
        int idealLeftPadding = targetViewportSize.width() * leftBoxRatio;
        int maxLeftPaddingKeepingBoxOnscreen = targetViewportSize.width() - textboxRectInDocument.width();
        newScroll.setX(textboxRectInDocument.x() - std::min<int>(idealLeftPadding, maxLeftPaddingKeepingBoxOnscreen));
    } else {
        // Field is wider than screen. Try to left-align field, unless caret would
        // be offscreen, in which case right-align the caret.
        newScroll.setX(std::max<int>(textboxRectInDocument.x(), caretInDocument.x() + caretInDocument.width() + caretPadding - targetViewportSize.width()));
    }
    if (textboxRectInDocument.height() <= targetViewportSize.height()) {
        // Field is shorter than screen. Vertically center it.
        newScroll.setY(textboxRectInDocument.y() - (targetViewportSize.height() - textboxRectInDocument.height()) / 2);
    } else {
        // Field is taller than screen. Try to top align field, unless caret would
        // be offscreen, in which case bottom-align the caret.
        newScroll.setY(std::max<int>(textboxRectInDocument.y(), caretInDocument.y() + caretInDocument.height() + caretPadding - targetViewportSize.height()));
    }
}

void WebViewImpl::advanceFocus(bool reverse)
{
    page()->focusController().advanceFocus(reverse ? WebFocusTypeBackward : WebFocusTypeForward);
}

void WebViewImpl::advanceFocusAcrossFrames(WebFocusType type, WebRemoteFrame* from, WebLocalFrame* to)
{
    // TODO(alexmos): Pass in proper with sourceCapabilities.
    page()->focusController().advanceFocusAcrossFrames(
        type, toWebRemoteFrameImpl(from)->frame(), toWebLocalFrameImpl(to)->frame());
}

double WebViewImpl::zoomLevel()
{
    return m_zoomLevel;
}

void WebViewImpl::propagateZoomFactorToLocalFrameRoots(Frame* frame, float zoomFactor)
{
    if (frame->isLocalRoot()) {
        LocalFrame* localFrame = toLocalFrame(frame);
        if (!WebLocalFrameImpl::pluginContainerFromFrame(localFrame))
            localFrame->setPageZoomFactor(zoomFactor);
    }

    for (Frame* child = frame->tree().firstChild(); child; child = child->tree().nextSibling())
        propagateZoomFactorToLocalFrameRoots(child, zoomFactor);
}

double WebViewImpl::setZoomLevel(double zoomLevel)
{
    if (zoomLevel < m_minimumZoomLevel)
        m_zoomLevel = m_minimumZoomLevel;
    else if (zoomLevel > m_maximumZoomLevel)
        m_zoomLevel = m_maximumZoomLevel;
    else
        m_zoomLevel = zoomLevel;

    float zoomFactor = m_zoomFactorOverride ? m_zoomFactorOverride : static_cast<float>(zoomLevelToZoomFactor(m_zoomLevel));
    if (m_zoomFactorForDeviceScaleFactor) {
        if (m_compositorDeviceScaleFactorOverride) {
            // Adjust the page's DSF so that DevicePixelRatio becomes m_zoomFactorForDeviceScaleFactor.
            page()->setDeviceScaleFactor(m_zoomFactorForDeviceScaleFactor / m_compositorDeviceScaleFactorOverride);
            zoomFactor *= m_compositorDeviceScaleFactorOverride;
        } else {
            page()->setDeviceScaleFactor(1.f);
            zoomFactor *= m_zoomFactorForDeviceScaleFactor;
        }
    }
    propagateZoomFactorToLocalFrameRoots(m_page->mainFrame(), zoomFactor);

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

    return page()->frameHost().visualViewport().scale();
}

float WebViewImpl::clampPageScaleFactorToLimits(float scaleFactor) const
{
    return pageScaleConstraintsSet().finalConstraints().clampToConstraints(scaleFactor);
}

void WebViewImpl::setVisualViewportOffset(const WebFloatPoint& offset)
{
    DCHECK(page());
    page()->frameHost().visualViewport().setLocation(offset);
}

WebFloatPoint WebViewImpl::visualViewportOffset() const
{
    DCHECK(page());
    return page()->frameHost().visualViewport().visibleRect().location();
}

WebFloatSize WebViewImpl::visualViewportSize() const
{
    DCHECK(page());
    return page()->frameHost().visualViewport().visibleRect().size();
}

void WebViewImpl::scrollAndRescaleViewports(float scaleFactor,
    const IntPoint& mainFrameOrigin,
    const FloatPoint& visualViewportOrigin)
{
    if (!page())
        return;

    if (!mainFrameImpl())
        return;

    FrameView * view = mainFrameImpl()->frameView();
    if (!view)
        return;

    // Order is important: visual viewport location is clamped based on
    // main frame scroll position and visual viewport scale.

    view->setScrollPosition(mainFrameOrigin, ProgrammaticScroll);

    setPageScaleFactor(scaleFactor);

    page()->frameHost().visualViewport().setLocation(visualViewportOrigin);
}

void WebViewImpl::setPageScaleFactorAndLocation(float scaleFactor, const FloatPoint& location)
{
    DCHECK(page());

    page()->frameHost().visualViewport().setScaleAndLocation(
        clampPageScaleFactorToLimits(scaleFactor),
        location);
}

void WebViewImpl::setPageScaleFactor(float scaleFactor)
{
    DCHECK(page());

    scaleFactor = clampPageScaleFactorToLimits(scaleFactor);
    if (scaleFactor == pageScaleFactor())
        return;

    page()->frameHost().visualViewport().setScale(scaleFactor);
}

void WebViewImpl::setDeviceScaleFactor(float scaleFactor)
{
    if (!page())
        return;

    page()->setDeviceScaleFactor(scaleFactor);

    if (m_layerTreeView)
        updateLayerTreeDeviceScaleFactor();
}

void WebViewImpl::setZoomFactorForDeviceScaleFactor(float zoomFactorForDeviceScaleFactor)
{
    m_zoomFactorForDeviceScaleFactor = zoomFactorForDeviceScaleFactor;
    if (!m_layerTreeView)
        return;
    setZoomLevel(m_zoomLevel);
}

void WebViewImpl::setDeviceColorProfile(const WebVector<char>& colorProfile)
{
    if (!page())
        return;

    Vector<char> deviceProfile;
    deviceProfile.append(colorProfile.data(), colorProfile.size());
    ImageDecoder::setTargetColorProfile(deviceProfile);

    page()->setDeviceColorProfile(deviceProfile);
}

void WebViewImpl::resetDeviceColorProfileForTesting()
{
    if (!page())
        return;

    page()->resetDeviceColorProfileForTesting();
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

void WebViewImpl::setDefaultPageScaleLimits(float minScale, float maxScale)
{
    return page()->frameHost().setDefaultPageScaleLimits(minScale, maxScale);
}

void WebViewImpl::setInitialPageScaleOverride(float initialPageScaleFactorOverride)
{
    PageScaleConstraints constraints = pageScaleConstraintsSet().userAgentConstraints();
    constraints.initialScale = initialPageScaleFactorOverride;

    if (constraints == pageScaleConstraintsSet().userAgentConstraints())
        return;

    pageScaleConstraintsSet().setNeedsReset(true);
    page()->frameHost().setUserAgentPageScaleConstraints(constraints);
}

void WebViewImpl::setMaximumLegibleScale(float maximumLegibleScale)
{
    m_maximumLegibleScale = maximumLegibleScale;
}

void WebViewImpl::setIgnoreViewportTagScaleLimits(bool ignore)
{
    PageScaleConstraints constraints = pageScaleConstraintsSet().userAgentConstraints();
    if (ignore) {
        constraints.minimumScale = pageScaleConstraintsSet().defaultConstraints().minimumScale;
        constraints.maximumScale = pageScaleConstraintsSet().defaultConstraints().maximumScale;
    } else {
        constraints.minimumScale = -1;
        constraints.maximumScale = -1;
    }
    page()->frameHost().setUserAgentPageScaleConstraints(constraints);
}

IntSize WebViewImpl::mainFrameSize()
{
    // The frame size should match the viewport size at minimum scale, since the
    // viewport must always be contained by the frame.
    FloatSize frameSize(m_size);
    frameSize.scale(1 / minimumPageScaleFactor());
    return expandedIntSize(frameSize);
}

PageScaleConstraintsSet& WebViewImpl::pageScaleConstraintsSet() const
{
    return page()->frameHost().pageScaleConstraintsSet();
}

void WebViewImpl::refreshPageScaleFactorAfterLayout()
{
    if (!mainFrame() || !page() || !page()->mainFrame() || !page()->mainFrame()->isLocalFrame() || !page()->deprecatedLocalMainFrame()->view())
        return;
    FrameView* view = page()->deprecatedLocalMainFrame()->view();

    updatePageDefinedViewportConstraints(mainFrameImpl()->frame()->document()->viewportDescription());
    pageScaleConstraintsSet().computeFinalConstraints();

    int verticalScrollbarWidth = 0;
    if (view->verticalScrollbar() && !view->verticalScrollbar()->isOverlayScrollbar())
        verticalScrollbarWidth = view->verticalScrollbar()->width();
    pageScaleConstraintsSet().adjustFinalConstraintsToContentsSize(contentsSize(), verticalScrollbarWidth, settings()->shrinksViewportContentToFit());

    float newPageScaleFactor = pageScaleFactor();
    if (pageScaleConstraintsSet().needsReset() && pageScaleConstraintsSet().finalConstraints().initialScale != -1) {
        newPageScaleFactor = pageScaleConstraintsSet().finalConstraints().initialScale;
        pageScaleConstraintsSet().setNeedsReset(false);
    }
    setPageScaleFactor(newPageScaleFactor);

    updateLayerTreeViewport();

    // Changes to page-scale during layout may require an additional frame.
    // We can't update the lifecycle here because we may be in the middle of layout in the
    // caller of this method.
    // TODO(chrishtr): clean all this up. All layout should happen in one lifecycle run (crbug.com/578239).
    if (mainFrameImpl()->frameView()->needsLayout())
        scheduleAnimation();
}

void WebViewImpl::updatePageDefinedViewportConstraints(const ViewportDescription& description)
{
    // If we're not reading the viewport meta tag, allow GPU rasterization.
    if (!settingsImpl()->viewportMetaEnabled()) {
        m_matchesHeuristicsForGpuRasterization = true;
        if (m_layerTreeView)
            m_layerTreeView->heuristicsForGpuRasterizationUpdated(m_matchesHeuristicsForGpuRasterization);
    }

    if (!page() || (!m_size.width && !m_size.height) || !page()->mainFrame()->isLocalFrame())
        return;

    if (!settings()->viewportEnabled()) {
        pageScaleConstraintsSet().clearPageDefinedConstraints();
        updateMainFrameLayoutSize();
        return;
    }

    Document* document = page()->deprecatedLocalMainFrame()->document();

    m_matchesHeuristicsForGpuRasterization = description.matchesHeuristicsForGpuRasterization();
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

    float oldInitialScale = pageScaleConstraintsSet().pageDefinedConstraints().initialScale;
    pageScaleConstraintsSet().updatePageDefinedConstraints(adjustedDescription, defaultMinWidth);

    if (settingsImpl()->clobberUserAgentInitialScaleQuirk()
        && pageScaleConstraintsSet().userAgentConstraints().initialScale != -1
        && pageScaleConstraintsSet().userAgentConstraints().initialScale * deviceScaleFactor() <= 1) {
        if (description.maxWidth == Length(DeviceWidth)
            || (description.maxWidth.type() == Auto && pageScaleConstraintsSet().pageDefinedConstraints().initialScale == 1.0f))
            setInitialPageScaleOverride(-1);
    }

    Settings& pageSettings = page()->settings();
    pageScaleConstraintsSet().adjustForAndroidWebViewQuirks(
        adjustedDescription,
        defaultMinWidth.intValue(),
        deviceScaleFactor(),
        settingsImpl()->supportDeprecatedTargetDensityDPI(),
        pageSettings.wideViewportQuirkEnabled(),
        pageSettings.useWideViewport(),
        pageSettings.loadWithOverviewMode(),
        settingsImpl()->viewportMetaNonUserScalableQuirk());
    float newInitialScale = pageScaleConstraintsSet().pageDefinedConstraints().initialScale;
    if (oldInitialScale != newInitialScale && newInitialScale != -1) {
        pageScaleConstraintsSet().setNeedsReset(true);
        if (mainFrameImpl() && mainFrameImpl()->frameView())
            mainFrameImpl()->frameView()->setNeedsLayout();
    }

    if (LocalFrame* frame = page()->deprecatedLocalMainFrame()) {
        if (TextAutosizer* textAutosizer = frame->document()->textAutosizer())
            textAutosizer->updatePageInfoInAllFrames();
    }

    updateMainFrameLayoutSize();
}

void WebViewImpl::updateMainFrameLayoutSize()
{
    if (m_shouldAutoResize || !mainFrameImpl())
        return;

    FrameView* view = mainFrameImpl()->frameView();
    if (!view)
        return;

    WebSize layoutSize = m_size;

    if (settings()->viewportEnabled())
        layoutSize = pageScaleConstraintsSet().layoutSize();

    if (page()->settings().forceZeroLayoutHeight())
        layoutSize.height = 0;

    view->setLayoutSize(layoutSize);
}

IntSize WebViewImpl::contentsSize() const
{
    if (!page()->mainFrame()->isLocalFrame())
        return IntSize();
    LayoutViewItem root = page()->deprecatedLocalMainFrame()->contentLayoutItem();
    if (root.isNull())
        return IntSize();
    return root.documentRect().size();
}

WebSize WebViewImpl::contentsPreferredMinimumSize()
{
    if (mainFrameImpl())
        mainFrameImpl()->frame()->view()->updateLifecycleToCompositingCleanPlusScrolling();

    Document* document = m_page->mainFrame()->isLocalFrame() ? m_page->deprecatedLocalMainFrame()->document() : nullptr;
    if (!document || document->layoutViewItem().isNull() || !document->documentElement() || !document->documentElement()->layoutBox())
        return WebSize();

    int widthScaled = document->layoutViewItem().minPreferredLogicalWidth().round(); // Already accounts for zoom.
    int heightScaled = document->documentElement()->layoutBox()->scrollHeight().round();
    return IntSize(widthScaled, heightScaled);
}

float WebViewImpl::defaultMinimumPageScaleFactor() const
{
    return pageScaleConstraintsSet().defaultConstraints().minimumScale;
}

float WebViewImpl::defaultMaximumPageScaleFactor() const
{
    return pageScaleConstraintsSet().defaultConstraints().maximumScale;
}

float WebViewImpl::minimumPageScaleFactor() const
{
    return pageScaleConstraintsSet().finalConstraints().minimumScale;
}

float WebViewImpl::maximumPageScaleFactor() const
{
    return pageScaleConstraintsSet().finalConstraints().maximumScale;
}

void WebViewImpl::resetScaleStateImmediately()
{
    pageScaleConstraintsSet().setNeedsReset(true);
}

void WebViewImpl::resetScrollAndScaleState()
{
    page()->frameHost().visualViewport().reset();

    if (!page()->mainFrame()->isLocalFrame())
        return;

    if (FrameView* frameView = toLocalFrame(page()->mainFrame())->view()) {
        ScrollableArea* scrollableArea = frameView->layoutViewportScrollableArea();

        if (scrollableArea->scrollPositionDouble() != DoublePoint::zero())
            scrollableArea->setScrollPosition(DoublePoint::zero(), ProgrammaticScroll);
    }

    pageScaleConstraintsSet().setNeedsReset(true);
}

void WebViewImpl::performMediaPlayerAction(const WebMediaPlayerAction& action,
                                           const WebPoint& location)
{
    HitTestResult result = hitTestResultForViewportPos(location);
    Node* node = result.innerNode();
    if (!isHTMLVideoElement(*node) && !isHTMLAudioElement(*node))
        return;

    HTMLMediaElement* mediaElement = toHTMLMediaElement(node);
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
        mediaElement->setBooleanAttribute(HTMLNames::controlsAttr, action.enable);
        break;
    default:
        NOTREACHED();
    }
}

void WebViewImpl::performPluginAction(const WebPluginAction& action,
                                      const WebPoint& location)
{
    // FIXME: Location is probably in viewport coordinates
    HitTestResult result = hitTestResultForRootFramePos(location);
    Node* node = result.innerNode();
    if (!isHTMLObjectElement(*node) && !isHTMLEmbedElement(*node))
        return;

    LayoutObject* object = node->layoutObject();
    if (object && object->isLayoutPart()) {
        Widget* widget = toLayoutPart(object)->widget();
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
                NOTREACHED();
            }
        }
    }
}

WebHitTestResult WebViewImpl::hitTestResultAt(const WebPoint& point)
{
    return coreHitTestResultAt(point);
}

HitTestResult WebViewImpl::coreHitTestResultAt(const WebPoint& pointInViewport)
{
    DocumentLifecycle::AllowThrottlingScope throttlingScope(mainFrameImpl()->frame()->document()->lifecycle());
    FrameView* view = mainFrameImpl()->frameView();
    IntPoint pointInRootFrame = view->contentsToFrame(view->viewportToContents(pointInViewport));
    return hitTestResultForRootFramePos(pointInRootFrame);
}

void WebViewImpl::dragSourceEndedAt(
    const WebPoint& clientPoint,
    const WebPoint& screenPoint,
    WebDragOperation operation)
{
    PlatformMouseEvent pme(clientPoint, screenPoint, LeftButton, PlatformEvent::MouseMoved,
        0, PlatformEvent::NoModifiers, PlatformMouseEvent::RealOrIndistinguishable, WTF::monotonicallyIncreasingTime());
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
    int modifiers)
{
    DCHECK(!m_currentDragData);

    m_currentDragData = DataObject::create(webDragData);
    m_operationsAllowed = operationsAllowed;

    return dragTargetDragEnterOrOver(clientPoint, screenPoint, DragEnter, modifiers);
}

WebDragOperation WebViewImpl::dragTargetDragOver(
    const WebPoint& clientPoint,
    const WebPoint& screenPoint,
    WebDragOperationsMask operationsAllowed,
    int modifiers)
{
    m_operationsAllowed = operationsAllowed;

    return dragTargetDragEnterOrOver(clientPoint, screenPoint, DragOver, modifiers);
}

void WebViewImpl::dragTargetDragLeave()
{
    DCHECK(m_currentDragData);

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

void WebViewImpl::dragTargetDrop(const WebDragData& webDragData, const WebPoint& clientPoint,
    const WebPoint& screenPoint,
    int modifiers)
{
    DCHECK(m_currentDragData);
    m_currentDragData = DataObject::create(webDragData);

    WebAutofillClient* autofillClient = mainFrameImpl() ? mainFrameImpl()->autofillClient() : 0;
    UserGestureNotifier notifier(autofillClient, &m_userGestureObserved);

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

    m_currentDragData->setModifiers(modifiers);
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
        const DocumentMarkerVector& documentMarkers = toLocalFrame(frame)->document()->markers().markers();
        for (size_t i = 0; i < documentMarkers.size(); ++i)
            result.append(documentMarkers[i]->hash());
    }
    markers->assign(result);
}

void WebViewImpl::removeSpellingMarkersUnderWords(const WebVector<WebString>& words)
{
    Vector<String> convertedWords;
    convertedWords.append(words.data(), words.size());

    for (Frame* frame = m_page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (frame->isLocalFrame())
            toLocalFrame(frame)->removeSpellingMarkersUnderWords(convertedWords);
    }
}

WebDragOperation WebViewImpl::dragTargetDragEnterOrOver(const WebPoint& clientPoint, const WebPoint& screenPoint, DragAction dragAction, int modifiers)
{
    DCHECK(m_currentDragData);

    m_currentDragData->setModifiers(modifiers);
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
        if (m_layerTreeView) {
            updateLayerTreeViewport();
        } else {
            WebRect damagedRect(0, 0, m_size.width, m_size.height);
            m_client->didInvalidateRect(damagedRect);
        }
    }
    updatePageOverlays();
}

void WebViewImpl::configureAutoResizeMode()
{
    if (!mainFrameImpl() || !mainFrameImpl()->frame() || !mainFrameImpl()->frame()->view())
        return;

    if (m_shouldAutoResize)
        mainFrameImpl()->frame()->view()->enableAutoSizeMode(m_minAutoSize, m_maxAutoSize);
    else
        mainFrameImpl()->frame()->view()->disableAutoSizeMode();
}

unsigned long WebViewImpl::createUniqueIdentifierForRequest()
{
    return createUniqueIdentifier();
}

void WebViewImpl::setCompositorDeviceScaleFactorOverride(float deviceScaleFactor)
{
    if (m_compositorDeviceScaleFactorOverride == deviceScaleFactor)
        return;
    m_compositorDeviceScaleFactorOverride = deviceScaleFactor;
    if (m_zoomFactorForDeviceScaleFactor) {
        setZoomLevel(zoomLevel());
        return;
    }
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

void WebViewImpl::enableDeviceEmulation(const WebDeviceEmulationParams& params)
{
    m_devToolsEmulator->enableDeviceEmulation(params);
}

void WebViewImpl::disableDeviceEmulation()
{
    m_devToolsEmulator->disableDeviceEmulation();
}

WebAXObject WebViewImpl::accessibilityObject()
{
    if (!mainFrameImpl())
        return WebAXObject();

    Document* document = mainFrameImpl()->frame()->document();
    return WebAXObject(toAXObjectCacheImpl(document->axObjectCache())->root());
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
    {
        ContextMenuAllowedScope scope;
        if (LocalFrame* focusedFrame = toLocalFrame(page()->focusController().focusedOrMainFrame()))
            focusedFrame->eventHandler().sendContextMenuEventForKey(nullptr);
    }
}

void WebViewImpl::didCloseContextMenu()
{
    LocalFrame* frame = m_page->focusController().focusedFrame();
    if (frame)
        frame->selection().setCaretBlinkingSuspended(false);
}

void WebViewImpl::extractSmartClipData(WebRect rectInViewport, WebString& clipText, WebString& clipHtml, WebRect& clipRectInViewport)
{
    LocalFrame* localFrame = toLocalFrame(focusedCoreFrame());
    if (!localFrame)
        return;
    SmartClipData clipData = SmartClip(localFrame).dataForRect(rectInViewport);
    clipText = clipData.clipData();
    clipRectInViewport = clipData.rectInViewport();

    WebLocalFrameImpl* frame = mainFrameImpl();
    if (!frame)
        return;
    WebPoint startPoint(rectInViewport.x, rectInViewport.y);
    WebPoint endPoint(rectInViewport.x + rectInViewport.width, rectInViewport.y + rectInViewport.height);
    VisiblePosition startVisiblePosition = frame->visiblePositionForViewportPoint(startPoint);
    VisiblePosition endVisiblePosition = frame->visiblePositionForViewportPoint(endPoint);

    Position startPosition = startVisiblePosition.deepEquivalent();
    Position endPosition = endVisiblePosition.deepEquivalent();

    // document() will return null if -webkit-user-select is set to none.
    if (!startPosition.document() || !endPosition.document())
        return;

    if (startPosition.compareTo(endPosition) <= 0) {
        clipHtml = createMarkup(startPosition, endPosition, AnnotateForInterchange, ConvertBlocksToInlines::NotConvert, ResolveNonLocalURLs);
    } else {
        clipHtml = createMarkup(endPosition, startPosition, AnnotateForInterchange, ConvertBlocksToInlines::NotConvert, ResolveNonLocalURLs);
    }
}

void WebViewImpl::hidePopups()
{
    cancelPagePopup();
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

    if (m_layerTreeView)
        m_layerTreeView->setHasTransparentBackground(this->isTransparent());
}

bool WebViewImpl::isTransparent() const
{
    return m_isTransparent;
}

void WebViewImpl::setBaseBackgroundColor(WebColor color)
{
    if (m_baseBackgroundColor == color)
        return;

    m_baseBackgroundColor = color;

    if (m_page->mainFrame() && m_page->mainFrame()->isLocalFrame())
        m_page->deprecatedLocalMainFrame()->view()->setBaseBackgroundColor(color);

    updateAllLifecyclePhases();
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
    m_page->chromeClient().setWindowFeatures(features);
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
    LayoutThemeDefault::setSelectionColors(activeBackgroundColor, activeForegroundColor, inactiveBackgroundColor, inactiveForegroundColor);
    LayoutTheme::theme().platformColorsDidChange();
#endif
}

void WebViewImpl::didCommitLoad(bool isNewNavigation, bool isNavigationWithinPage)
{
    if (!isNavigationWithinPage) {
        m_shouldDispatchFirstVisuallyNonEmptyLayout = true;
        m_shouldDispatchFirstLayoutAfterFinishedParsing = true;
        m_shouldDispatchFirstLayoutAfterFinishedLoading = true;

        if (isNewNavigation) {
            pageScaleConstraintsSet().setNeedsReset(true);
            m_pageImportanceSignals.onCommitLoad();
        }
    }

    // Give the visual viewport's scroll layer its initial size.
    page()->frameHost().visualViewport().mainFrameDidChangeSize();

    // Make sure link highlight from previous page is cleared.
    m_linkHighlights.clear();
    endActiveFlingAnimation();
    m_userGestureObserved = false;
}

void WebViewImpl::postLayoutResize(WebLocalFrameImpl* webframe)
{
    FrameView* view = webframe->frame()->view();
    if (webframe == mainFrame())
        view->resize(mainFrameSize());
    else
        view->resize(webframe->frameView()->size());
}

void WebViewImpl::layoutUpdated(WebLocalFrameImpl* webframe)
{
    LocalFrame* frame = webframe->frame();
    if (!m_client || !frame->isLocalRoot())
        return;

    if (m_shouldAutoResize) {
        WebSize frameSize = frame->view()->frameRect().size();
        if (frameSize != m_size) {
            m_size = frameSize;

            page()->frameHost().visualViewport().setSize(m_size);
            pageScaleConstraintsSet().didChangeInitialContainingBlockSize(m_size);

            m_client->didAutoResize(m_size);
            sendResizeEventAndRepaint();
        }
    }

    if (pageScaleConstraintsSet().constraintsDirty())
        refreshPageScaleFactorAfterLayout();

    FrameView* view = webframe->frame()->view();

    postLayoutResize(webframe);

    // Relayout immediately to avoid violating the rule that needsLayout()
    // isn't set at the end of a layout.
    if (view->needsLayout())
        view->layout();

    m_client->didUpdateLayout();
}

void WebViewImpl::didChangeContentsSize()
{
    pageScaleConstraintsSet().didChangeContentsSize(contentsSize(), pageScaleFactor());
}

void WebViewImpl::pageScaleFactorChanged()
{
    pageScaleConstraintsSet().setNeedsReset(false);
    updateLayerTreeViewport();
    m_client->pageScaleFactorChanged();
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
    DCHECK(!m_doingDragAndDrop);
    m_doingDragAndDrop = true;
    m_client->startDragging(WebLocalFrameImpl::fromFrame(frame), dragData, mask, dragImage, dragImageOffset);
}

void WebViewImpl::setIgnoreInputEvents(bool newValue)
{
    DCHECK_NE(m_ignoreInputEvents, newValue);
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

void WebViewImpl::setPageOverlayColor(WebColor color)
{
    if (m_pageColorOverlay)
        m_pageColorOverlay.reset();

    if (color == Color::transparent)
        return;

    m_pageColorOverlay = PageOverlay::create(this, new ColorOverlay(color));
    m_pageColorOverlay->update();
}

WebPageImportanceSignals* WebViewImpl::pageImportanceSignals()
{
    return &m_pageImportanceSignals;
}

Element* WebViewImpl::focusedElement() const
{
    LocalFrame* frame = m_page->focusController().focusedFrame();
    if (!frame)
        return nullptr;

    Document* document = frame->document();
    if (!document)
        return nullptr;

    return document->focusedElement();
}

HitTestResult WebViewImpl::hitTestResultForViewportPos(const IntPoint& posInViewport)
{
    IntPoint rootFramePoint(m_page->frameHost().visualViewport().viewportToRootFrame(posInViewport));
    return hitTestResultForRootFramePos(rootFramePoint);
}

HitTestResult WebViewImpl::hitTestResultForRootFramePos(const IntPoint& posInRootFrame)
{
    if (!m_page->mainFrame()->isLocalFrame())
        return HitTestResult();
    IntPoint docPoint(m_page->deprecatedLocalMainFrame()->view()->rootFrameToContents(posInRootFrame));
    HitTestResult result = m_page->deprecatedLocalMainFrame()->eventHandler().hitTestResultAtPoint(docPoint, HitTestRequest::ReadOnly | HitTestRequest::Active);
    result.setToShadowHostIfInUserAgentShadowRoot();
    return result;
}

WebHitTestResult WebViewImpl::hitTestResultForTap(const WebPoint& tapPointWindowPos, const WebSize& tapArea)
{
    if (!m_page->mainFrame()->isLocalFrame())
        return HitTestResult();

    WebGestureEvent tapEvent;
    tapEvent.x = tapPointWindowPos.x;
    tapEvent.y = tapPointWindowPos.y;
    tapEvent.type = WebInputEvent::GestureTap;
    // GestureTap is only ever from a touchscreen.
    tapEvent.sourceDevice = WebGestureDeviceTouchscreen;
    tapEvent.data.tap.tapCount = 1;
    tapEvent.data.tap.width = tapArea.width;
    tapEvent.data.tap.height = tapArea.height;

    PlatformGestureEventBuilder platformEvent(mainFrameImpl()->frameView(), tapEvent);

    HitTestResult result = m_page->deprecatedLocalMainFrame()->eventHandler().hitTestResultForGestureEvent(platformEvent, HitTestRequest::ReadOnly | HitTestRequest::Active).hitTestResult();

    result.setToShadowHostIfInUserAgentShadowRoot();
    return result;
}

void WebViewImpl::setTabsToLinks(bool enable)
{
    m_tabsToLinks = enable;
}

bool WebViewImpl::tabsToLinks() const
{
    return m_tabsToLinks;
}

void WebViewImpl::setRootGraphicsLayer(GraphicsLayer* layer)
{
    if (!m_layerTreeView)
        return;

    // In SPv2, we attach layers via PaintArtifactCompositor, rather than
    // supplying a root GraphicsLayer from PaintLayerCompositor.
    DCHECK(!RuntimeEnabledFeatures::slimmingPaintV2Enabled());

    VisualViewport& visualViewport = page()->frameHost().visualViewport();
    visualViewport.attachToLayerTree(layer);
    if (layer) {
        m_rootGraphicsLayer = visualViewport.rootGraphicsLayer();
        m_visualViewportContainerLayer = visualViewport.containerLayer();
        m_rootLayer = m_rootGraphicsLayer->platformLayer();
        updateRootLayerTransform();
        m_layerTreeView->setRootLayer(*m_rootLayer);
        // We register viewport layers here since there may not be a layer
        // tree view prior to this point.
        visualViewport.registerLayersWithTreeView(m_layerTreeView);
        updatePageOverlays();
        // TODO(enne): Work around page visibility changes not being
        // propagated to the WebView in some circumstances.  This needs to
        // be refreshed here when setting a new root layer to avoid being
        // stuck in a presumed incorrectly invisible state.
        m_layerTreeView->setVisible(page()->isPageVisible());
    } else {
        m_rootGraphicsLayer = nullptr;
        m_visualViewportContainerLayer = nullptr;
        m_rootLayer = nullptr;
        // This means that we're transitioning to a new page. Suppress
        // commits until Blink generates invalidations so we don't
        // attempt to paint too early in the next page load.
        m_layerTreeView->setDeferCommits(true);
        m_layerTreeView->clearRootLayer();
        visualViewport.clearLayersForTreeView(m_layerTreeView);
    }
}

void WebViewImpl::invalidateRect(const IntRect& rect)
{
    if (m_layerTreeView)
        updateLayerTreeViewport();
    else if (m_client)
        m_client->didInvalidateRect(rect);
}

PaintLayerCompositor* WebViewImpl::compositor() const
{
    WebLocalFrameImpl* frame = mainFrameImpl();
    if (!frame)
        return nullptr;

    Document* document = frame->frame()->document();
    if (!document || document->layoutViewItem().isNull())
        return nullptr;

    return document->layoutViewItem().compositor();
}

GraphicsLayer* WebViewImpl::rootGraphicsLayer()
{
    return m_rootGraphicsLayer;
}

void WebViewImpl::scheduleAnimation()
{
    if (m_layerTreeView) {
        m_layerTreeView->setNeedsBeginFrame();
        return;
    }
    if (m_client)
        m_client->scheduleAnimation();
}

void WebViewImpl::attachCompositorAnimationTimeline(CompositorAnimationTimeline* timeline)
{
    if (m_layerTreeView)
        m_layerTreeView->attachCompositorAnimationTimeline(timeline->animationTimeline());
}

void WebViewImpl::detachCompositorAnimationTimeline(CompositorAnimationTimeline* timeline)
{
    if (m_layerTreeView)
        m_layerTreeView->detachCompositorAnimationTimeline(timeline->animationTimeline());
}

void WebViewImpl::initializeLayerTreeView()
{
    if (m_client) {
        m_client->initializeLayerTreeView();
        m_layerTreeView = m_client->layerTreeView();
    }

    if (WebDevToolsAgentImpl* devTools = mainFrameDevToolsAgentImpl())
        devTools->layerTreeViewChanged(m_layerTreeView);

    m_page->settings().setAcceleratedCompositingEnabled(m_layerTreeView);
    if (m_layerTreeView)
        m_page->layerTreeViewInitialized(*m_layerTreeView);

    // FIXME: only unittests, click to play, Android printing, and printing (for headers and footers)
    // make this assert necessary. We should make them not hit this code and then delete allowsBrokenNullLayerTreeView.
    DCHECK(m_layerTreeView || !m_client || m_client->allowsBrokenNullLayerTreeView());

    if (Platform::current()->isThreadedAnimationEnabled() && m_layerTreeView) {
        m_linkHighlightsTimeline = CompositorAnimationTimeline::create();
        attachCompositorAnimationTimeline(m_linkHighlightsTimeline.get());
    }

    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled())
        attachPaintArtifactCompositor();
}

void WebViewImpl::applyViewportDeltas(
    const WebFloatSize& visualViewportDelta,
    const WebFloatSize& layoutViewportDelta,
    const WebFloatSize& elasticOverscrollDelta,
    float pageScaleDelta,
    float topControlsShownRatioDelta)
{
    if (!mainFrameImpl())
        return;
    FrameView* frameView = mainFrameImpl()->frameView();
    if (!frameView)
        return;

    ScrollableArea* layoutViewport = frameView->layoutViewportScrollableArea();
    VisualViewport& visualViewport = page()->frameHost().visualViewport();

    // Store the desired offsets for visual and layout viewports before setting
    // the top controls ratio since doing so will change the bounds and move the
    // viewports to keep the offsets valid. The compositor may have already done
    // that so we don't want to double apply the deltas here.
    FloatPoint visualViewportOffset = visualViewport.visibleRect().location();
    visualViewportOffset.move(
        visualViewportDelta.width,
        visualViewportDelta.height);
    DoublePoint layoutViewportPosition = layoutViewport->scrollPositionDouble()
        + DoubleSize(layoutViewportDelta.width, layoutViewportDelta.height);

    topControls().setShownRatio(topControls().shownRatio() + topControlsShownRatioDelta);

    setPageScaleFactorAndLocation(pageScaleFactor() * pageScaleDelta, visualViewportOffset);

    if (pageScaleDelta != 1) {
        m_doubleTapZoomPending = false;
        visualViewport.userDidChangeScale();
    }

    m_elasticOverscroll += elasticOverscrollDelta;
    frameView->didUpdateElasticOverscroll();

    if (layoutViewport->scrollPositionDouble() != layoutViewportPosition) {
        layoutViewport->setScrollPosition(layoutViewportPosition, CompositorScroll);
        if (DocumentLoader* documentLoader = mainFrameImpl()->frame()->loader().documentLoader())
            documentLoader->initialScrollState().wasScrolledByUser = true;
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
    DCHECK(page());
    DCHECK(m_layerTreeView);

    float deviceScaleFactor = m_compositorDeviceScaleFactorOverride ? m_compositorDeviceScaleFactorOverride : page()->deviceScaleFactor();
    m_layerTreeView->setDeviceScaleFactor(deviceScaleFactor);
}

void WebViewImpl::updateRootLayerTransform()
{
    if (m_visualViewportContainerLayer) {
        TransformationMatrix transform;
        transform.translate(m_rootLayerOffset.width, m_rootLayerOffset.height);
        transform = transform.scale(m_rootLayerScale);
        m_visualViewportContainerLayer->setTransform(transform);
    }
}

bool WebViewImpl::detectContentOnTouch(const GestureEventWithHitTestResults& targetedEvent)
{
    if (!m_page->mainFrame()->isLocalFrame())
        return false;

    // Need a local copy of the hit test as setToShadowHostIfInUserAgentShadowRoot() will modify it.
    HitTestResult touchHit = targetedEvent.hitTestResult();
    touchHit.setToShadowHostIfInUserAgentShadowRoot();

    if (touchHit.isContentEditable())
        return false;

    Node* node = touchHit.innerNode();
    if (!node || !node->isTextNode())
        return false;

    // Ignore when tapping on links or nodes listening to click events, unless the click event is on the
    // body element, in which case it's unlikely that the original node itself was intended to be clickable.
    for (; node && !isHTMLBodyElement(*node); node = LayoutTreeBuilderTraversal::parent(*node)) {
        if (node->isLink() || node->willRespondToTouchEvents() || node->willRespondToMouseClickEvents())
            return false;
    }

    WebContentDetectionResult content = m_client->detectContentAround(touchHit);
    if (!content.isValid())
        return false;

    // This code is called directly after hit test code, with no user code running in between,
    // thus it is assumed that the frame pointer is non-null.
    bool isMainFrame = node ? node->document().frame()->isMainFrame() : true;
    m_client->scheduleContentIntent(content.intent(), isMainFrame);
    return true;
}

WebViewScheduler* WebViewImpl::scheduler() const
{
    return m_scheduler.get();
}

void WebViewImpl::setVisibilityState(WebPageVisibilityState visibilityState,
                                     bool isInitialState) {
    DCHECK(visibilityState == WebPageVisibilityStateVisible || visibilityState == WebPageVisibilityStateHidden || visibilityState == WebPageVisibilityStatePrerender);

    if (page())
        m_page->setVisibilityState(static_cast<PageVisibilityState>(static_cast<int>(visibilityState)), isInitialState);

    bool visible = visibilityState == WebPageVisibilityStateVisible;
    if (m_layerTreeView && !m_overrideCompositorVisibility)
        m_layerTreeView->setVisible(visible);
    m_scheduler->setPageVisible(visible);
}

void WebViewImpl::setCompositorVisibility(bool isVisible)
{
    if (!isVisible)
        m_overrideCompositorVisibility = true;
    else
        m_overrideCompositorVisibility = false;
    if (m_layerTreeView)
        m_layerTreeView->setVisible(isVisible);
}

void WebViewImpl::pointerLockMouseEvent(const WebInputEvent& event)
{
    std::unique_ptr<UserGestureIndicator> gestureIndicator;
    AtomicString eventType;
    switch (event.type) {
    case WebInputEvent::MouseDown:
        eventType = EventTypeNames::mousedown;
        gestureIndicator = wrapUnique(new UserGestureIndicator(DefinitelyProcessingNewUserGesture));
        m_pointerLockGestureToken = gestureIndicator->currentToken();
        break;
    case WebInputEvent::MouseUp:
        eventType = EventTypeNames::mouseup;
        gestureIndicator = wrapUnique(new UserGestureIndicator(m_pointerLockGestureToken.release()));
        break;
    case WebInputEvent::MouseMove:
        eventType = EventTypeNames::mousemove;
        break;
    default:
        NOTREACHED();
    }

    const WebMouseEvent& mouseEvent = static_cast<const WebMouseEvent&>(event);

    if (page())
        page()->pointerLockController().dispatchLockedMouseEvent(
            PlatformMouseEventBuilder(mainFrameImpl()->frameView(), mouseEvent),
            eventType);
}

void WebViewImpl::forceNextWebGLContextCreationToFail()
{
    WebGLRenderingContext::forceNextWebGLContextCreationToFail();
}

void WebViewImpl::forceNextDrawingBufferCreationToFail()
{
    DrawingBuffer::forceNextDrawingBufferCreationToFail();
}

CompositorProxyClient* WebViewImpl::createCompositorProxyClient()
{
    if (!m_mutator) {
        std::unique_ptr<CompositorMutatorClient> mutatorClient = CompositorMutatorImpl::createClient();
        m_mutator = static_cast<CompositorMutatorImpl*>(mutatorClient->mutator());
        m_layerTreeView->setMutatorClient(std::move(mutatorClient));
    }
    return new CompositorProxyClientImpl(m_mutator);
}

void WebViewImpl::updatePageOverlays()
{
    if (m_pageColorOverlay)
        m_pageColorOverlay->update();
    if (InspectorOverlay* overlay = inspectorOverlay()) {
        PageOverlay* inspectorPageOverlay = overlay->pageOverlay();
        if (inspectorPageOverlay)
            inspectorPageOverlay->update();
    }
}

void WebViewImpl::attachPaintArtifactCompositor()
{
    if (!m_layerTreeView)
        return;

    // Otherwise, PaintLayerCompositor is expected to supply a root
    // GraphicsLayer via setRootGraphicsLayer.
    DCHECK(RuntimeEnabledFeatures::slimmingPaintV2Enabled());

    // TODO(jbroman): This should probably have hookups for overlays, visual
    // viewport, etc.

    WebLayer* rootLayer = m_paintArtifactCompositor.getWebLayer();
    DCHECK(rootLayer);
    m_layerTreeView->setRootLayer(*rootLayer);

    // TODO(jbroman): This is cargo-culted from setRootGraphicsLayer. Is it
    // necessary?
    m_layerTreeView->setVisible(page()->isPageVisible());
}

void WebViewImpl::detachPaintArtifactCompositor()
{
    if (!m_layerTreeView)
        return;

    m_layerTreeView->setDeferCommits(true);
    m_layerTreeView->clearRootLayer();
}

float WebViewImpl::deviceScaleFactor() const
{
    // TODO(oshima): Investigate if this should return the ScreenInfo's scale factor rather than
    // page's scale factor, which can be 1 in use-zoom-for-dsf mode.
    if (!page())
        return 1;

    return page()->deviceScaleFactor();
}

} // namespace blink
