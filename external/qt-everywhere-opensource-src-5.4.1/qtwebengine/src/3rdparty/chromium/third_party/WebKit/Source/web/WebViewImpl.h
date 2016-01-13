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

#ifndef WebViewImpl_h
#define WebViewImpl_h

#include "core/page/PagePopupDriver.h"
#include "platform/geometry/IntPoint.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/GraphicsLayer.h"
#include "public/platform/WebGestureCurveTarget.h"
#include "public/platform/WebLayer.h"
#include "public/platform/WebPoint.h"
#include "public/platform/WebRect.h"
#include "public/platform/WebSize.h"
#include "public/platform/WebString.h"
#include "public/web/WebInputEvent.h"
#include "public/web/WebNavigationPolicy.h"
#include "public/web/WebView.h"
#include "web/BackForwardClientImpl.h"
#include "web/ChromeClientImpl.h"
#include "web/ContextMenuClientImpl.h"
#include "web/DragClientImpl.h"
#include "web/EditorClientImpl.h"
#include "web/InspectorClientImpl.h"
#include "web/MediaKeysClientImpl.h"
#include "web/PageOverlayList.h"
#include "web/PageScaleConstraintsSet.h"
#include "web/PageWidgetDelegate.h"
#include "web/SpellCheckerClientImpl.h"
#include "web/StorageClientImpl.h"
#include "wtf/OwnPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"

namespace WebCore {
class DataObject;
class Frame;
class RenderLayerCompositor;
class UserGestureToken;
}

namespace blink {
class LinkHighlight;
class PopupContainer;
class WebActiveGestureAnimation;
class WebDevToolsAgentPrivate;
class WebLocalFrameImpl;
class WebImage;
class WebPagePopupImpl;
class WebPlugin;
class WebSettingsImpl;
class FullscreenController;

class WebViewImpl FINAL : public WebView
    , public RefCounted<WebViewImpl>
    , public WebGestureCurveTarget
    , public WebCore::PagePopupDriver
    , public PageWidgetEventHandler {
public:
    static WebViewImpl* create(WebViewClient*);

    // WebWidget methods:
    virtual void close() OVERRIDE;
    virtual WebSize size() OVERRIDE;
    virtual void willStartLiveResize() OVERRIDE;
    virtual void resize(const WebSize&) OVERRIDE;
    virtual void resizePinchViewport(const WebSize&) OVERRIDE;
    virtual void willEndLiveResize() OVERRIDE;
    virtual void willEnterFullScreen() OVERRIDE;
    virtual void didEnterFullScreen() OVERRIDE;
    virtual void willExitFullScreen() OVERRIDE;
    virtual void didExitFullScreen() OVERRIDE;
    virtual void animate(double) OVERRIDE;
    virtual void layout() OVERRIDE;
    virtual void paint(WebCanvas*, const WebRect&) OVERRIDE;
#if OS(ANDROID)
    virtual void paintCompositedDeprecated(WebCanvas*, const WebRect&) OVERRIDE;
#endif
    virtual void compositeAndReadbackAsync(WebCompositeAndReadbackAsyncCallback*) OVERRIDE;
    virtual bool isTrackingRepaints() const OVERRIDE;
    virtual void themeChanged() OVERRIDE;
    virtual bool handleInputEvent(const WebInputEvent&) OVERRIDE;
    virtual void setCursorVisibilityState(bool isVisible) OVERRIDE;
    virtual bool hasTouchEventHandlersAt(const WebPoint&) OVERRIDE;
    virtual void applyScrollAndScale(const WebSize&, float) OVERRIDE;
    virtual void mouseCaptureLost() OVERRIDE;
    virtual void setFocus(bool enable) OVERRIDE;
    virtual bool setComposition(
        const WebString& text,
        const WebVector<WebCompositionUnderline>& underlines,
        int selectionStart,
        int selectionEnd) OVERRIDE;
    virtual bool confirmComposition() OVERRIDE;
    virtual bool confirmComposition(ConfirmCompositionBehavior selectionBehavior) OVERRIDE;
    virtual bool confirmComposition(const WebString& text) OVERRIDE;
    virtual bool compositionRange(size_t* location, size_t* length) OVERRIDE;
    virtual WebTextInputInfo textInputInfo() OVERRIDE;
    virtual WebColor backgroundColor() const OVERRIDE;
    virtual bool selectionBounds(WebRect& anchor, WebRect& focus) const OVERRIDE;
    virtual void didShowCandidateWindow() OVERRIDE;
    virtual void didUpdateCandidateWindow() OVERRIDE;
    virtual void didHideCandidateWindow() OVERRIDE;
    virtual bool selectionTextDirection(WebTextDirection& start, WebTextDirection& end) const OVERRIDE;
    virtual bool isSelectionAnchorFirst() const OVERRIDE;
    virtual bool caretOrSelectionRange(size_t* location, size_t* length) OVERRIDE;
    virtual void setTextDirection(WebTextDirection) OVERRIDE;
    virtual bool isAcceleratedCompositingActive() const OVERRIDE;
    virtual void willCloseLayerTreeView() OVERRIDE;
    virtual void didAcquirePointerLock() OVERRIDE;
    virtual void didNotAcquirePointerLock() OVERRIDE;
    virtual void didLosePointerLock() OVERRIDE;
    virtual void didChangeWindowResizerRect() OVERRIDE;

    // WebView methods:
    virtual void setMainFrame(WebFrame*) OVERRIDE;
    virtual void setAutofillClient(WebAutofillClient*) OVERRIDE;
    virtual void setDevToolsAgentClient(WebDevToolsAgentClient*) OVERRIDE;
    virtual void setPrerendererClient(WebPrerendererClient*) OVERRIDE;
    virtual void setSpellCheckClient(WebSpellCheckClient*) OVERRIDE;
    virtual WebSettings* settings() OVERRIDE;
    virtual WebString pageEncoding() const OVERRIDE;
    virtual void setPageEncoding(const WebString&) OVERRIDE;
    virtual bool isTransparent() const OVERRIDE;
    virtual void setIsTransparent(bool value) OVERRIDE;
    virtual void setBaseBackgroundColor(WebColor) OVERRIDE;
    virtual bool tabsToLinks() const OVERRIDE;
    virtual void setTabsToLinks(bool value) OVERRIDE;
    virtual bool tabKeyCyclesThroughElements() const OVERRIDE;
    virtual void setTabKeyCyclesThroughElements(bool value) OVERRIDE;
    virtual bool isActive() const OVERRIDE;
    virtual void setIsActive(bool value) OVERRIDE;
    virtual void setDomainRelaxationForbidden(bool, const WebString& scheme) OVERRIDE;
    virtual void setWindowFeatures(const WebWindowFeatures&) OVERRIDE;
    virtual void setOpenedByDOM() OVERRIDE;
    virtual WebFrame* mainFrame() OVERRIDE;
    virtual WebFrame* findFrameByName(
        const WebString& name, WebFrame* relativeToFrame) OVERRIDE;
    virtual WebFrame* focusedFrame() OVERRIDE;
    virtual void setFocusedFrame(WebFrame*) OVERRIDE;
    virtual void setInitialFocus(bool reverse) OVERRIDE;
    virtual void clearFocusedElement() OVERRIDE;
    virtual void scrollFocusedNodeIntoRect(const WebRect&) OVERRIDE;
    virtual void zoomToFindInPageRect(const WebRect&) OVERRIDE;
    virtual void advanceFocus(bool reverse) OVERRIDE;
    virtual double zoomLevel() OVERRIDE;
    virtual double setZoomLevel(double) OVERRIDE;
    virtual void zoomLimitsChanged(double minimumZoomLevel, double maximumZoomLevel) OVERRIDE;
    virtual float textZoomFactor() OVERRIDE;
    virtual float setTextZoomFactor(float) OVERRIDE;
    virtual void setInitialPageScaleOverride(float) OVERRIDE;
    virtual bool zoomToMultipleTargetsRect(const WebRect&) OVERRIDE;
    virtual float pageScaleFactor() const OVERRIDE;
    virtual void setPageScaleFactorLimits(float minPageScale, float maxPageScale) OVERRIDE;
    virtual void setMainFrameScrollOffset(const WebPoint&) OVERRIDE;
    virtual void setPageScaleFactor(float) OVERRIDE;
    virtual void setPinchViewportOffset(const WebFloatPoint&) OVERRIDE;
    virtual WebFloatPoint pinchViewportOffset() const OVERRIDE;
    virtual float minimumPageScaleFactor() const OVERRIDE;
    virtual float maximumPageScaleFactor() const OVERRIDE;
    virtual void resetScrollAndScaleState() OVERRIDE;
    virtual void setIgnoreViewportTagScaleLimits(bool) OVERRIDE;
    virtual WebSize contentsPreferredMinimumSize() OVERRIDE;

    virtual float deviceScaleFactor() const OVERRIDE;
    virtual void setDeviceScaleFactor(float) OVERRIDE;

    virtual void setFixedLayoutSize(const WebSize&) OVERRIDE;

    virtual void enableAutoResizeMode(
        const WebSize& minSize,
        const WebSize& maxSize) OVERRIDE;
    virtual void disableAutoResizeMode() OVERRIDE;
    virtual void performMediaPlayerAction(
        const WebMediaPlayerAction& action,
        const WebPoint& location) OVERRIDE;
    virtual void performPluginAction(
        const WebPluginAction&,
        const WebPoint&) OVERRIDE;
    virtual WebHitTestResult hitTestResultAt(const WebPoint&) OVERRIDE;
    virtual void copyImageAt(const WebPoint&) OVERRIDE;
    virtual void saveImageAt(const WebPoint&) OVERRIDE;
    virtual void dragSourceEndedAt(
        const WebPoint& clientPoint,
        const WebPoint& screenPoint,
        WebDragOperation) OVERRIDE;
    virtual void dragSourceSystemDragEnded() OVERRIDE;
    virtual WebDragOperation dragTargetDragEnter(
        const WebDragData&,
        const WebPoint& clientPoint,
        const WebPoint& screenPoint,
        WebDragOperationsMask operationsAllowed,
        int keyModifiers) OVERRIDE;
    virtual WebDragOperation dragTargetDragOver(
        const WebPoint& clientPoint,
        const WebPoint& screenPoint,
        WebDragOperationsMask operationsAllowed,
        int keyModifiers) OVERRIDE;
    virtual void dragTargetDragLeave() OVERRIDE;
    virtual void dragTargetDrop(
        const WebPoint& clientPoint,
        const WebPoint& screenPoint,
        int keyModifiers) OVERRIDE;
    virtual void spellingMarkers(WebVector<uint32_t>* markers) OVERRIDE;
    virtual unsigned long createUniqueIdentifierForRequest() OVERRIDE;
    virtual void inspectElementAt(const WebPoint&) OVERRIDE;
    virtual WebString inspectorSettings() const OVERRIDE;
    virtual void setInspectorSettings(const WebString&) OVERRIDE;
    virtual bool inspectorSetting(const WebString& key, WebString* value) const OVERRIDE;
    virtual void setInspectorSetting(const WebString& key, const WebString& value) OVERRIDE;
    virtual void setCompositorDeviceScaleFactorOverride(float) OVERRIDE;
    virtual void setRootLayerTransform(const WebSize& offset, float scale) OVERRIDE;
    virtual WebDevToolsAgent* devToolsAgent() OVERRIDE;
    virtual WebAXObject accessibilityObject() OVERRIDE;
    virtual void setSelectionColors(unsigned activeBackgroundColor,
                                    unsigned activeForegroundColor,
                                    unsigned inactiveBackgroundColor,
                                    unsigned inactiveForegroundColor) OVERRIDE;
    virtual void performCustomContextMenuAction(unsigned action) OVERRIDE;
    virtual void showContextMenu() OVERRIDE;
    // FIXME: This should be removed when the chromium side patch lands
    // http://codereview.chromium.org/260623004
    virtual WebString getSmartClipData(WebRect) OVERRIDE;
    virtual void getSmartClipData(WebRect, WebString&, WebRect&) OVERRIDE;
    virtual void extractSmartClipData(WebRect, WebString&, WebString&, WebRect&) OVERRIDE;
    virtual void hidePopups() OVERRIDE;
    virtual void addPageOverlay(WebPageOverlay*, int /* zOrder */) OVERRIDE;
    virtual void removePageOverlay(WebPageOverlay*) OVERRIDE;
    virtual void transferActiveWheelFlingAnimation(const WebActiveWheelFlingParameters&) OVERRIDE;
    virtual bool endActiveFlingAnimation() OVERRIDE;
    virtual void setShowPaintRects(bool) OVERRIDE;
    void setShowDebugBorders(bool);
    virtual void setShowFPSCounter(bool) OVERRIDE;
    virtual void setContinuousPaintingEnabled(bool) OVERRIDE;
    virtual void setShowScrollBottleneckRects(bool) OVERRIDE;
    virtual void getSelectionRootBounds(WebRect& bounds) const OVERRIDE;
    virtual void acceptLanguagesChanged() OVERRIDE;

    // WebViewImpl

    void suppressInvalidations(bool enable);
    void invalidateRect(const WebCore::IntRect&);

    void setIgnoreInputEvents(bool newValue);
    void setBackgroundColorOverride(WebColor);
    void setZoomFactorOverride(float);
    WebDevToolsAgentPrivate* devToolsAgentPrivate() { return m_devToolsAgent.get(); }

    WebCore::Color baseBackgroundColor() const { return m_baseBackgroundColor; }

    PageOverlayList* pageOverlays() const { return m_pageOverlays.get(); }

    void setOverlayLayer(WebCore::GraphicsLayer*);

    const WebPoint& lastMouseDownPoint() const
    {
        return m_lastMouseDownPoint;
    }

    WebCore::Frame* focusedWebCoreFrame() const;

    // Returns the currently focused Element or null if no element has focus.
    WebCore::Element* focusedElement() const;

    static WebViewImpl* fromPage(WebCore::Page*);

    WebViewClient* client()
    {
        return m_client;
    }

    WebAutofillClient* autofillClient()
    {
        return m_autofillClient;
    }

    WebSpellCheckClient* spellCheckClient()
    {
        return m_spellCheckClient;
    }

    // Returns the page object associated with this view. This may be null when
    // the page is shutting down, but will be valid at all other times.
    WebCore::Page* page() const
    {
        return m_page.get();
    }

    // Returns the main frame associated with this view. This may be null when
    // the page is shutting down, but will be valid at all other times.
    WebLocalFrameImpl* mainFrameImpl();

    // Event related methods:
    void mouseContextMenu(const WebMouseEvent&);
    void mouseDoubleClick(const WebMouseEvent&);

    bool detectContentOnTouch(const WebPoint&);
    bool startPageScaleAnimation(const WebCore::IntPoint& targetPosition, bool useAnchor, float newScale, double durationInSeconds);

    void hasTouchEventHandlers(bool);

    // WebGestureCurveTarget implementation for fling.
    virtual bool scrollBy(const WebFloatSize& delta, const WebFloatSize& velocity) OVERRIDE;

    // Handles context menu events orignated via the the keyboard. These
    // include the VK_APPS virtual key and the Shift+F10 combine. Code is
    // based on the Webkit function bool WebView::handleContextMenuEvent(WPARAM
    // wParam, LPARAM lParam) in webkit\webkit\win\WebView.cpp. The only
    // significant change in this function is the code to convert from a
    // Keyboard event to the Right Mouse button down event.
    bool sendContextMenuEvent(const WebKeyboardEvent&);

    // Notifies the WebView that a load has been committed. isNewNavigation
    // will be true if a new session history item should be created for that
    // load. isNavigationWithinPage will be true if the navigation does
    // not take the user away from the current page.
    void didCommitLoad(bool isNewNavigation, bool isNavigationWithinPage);

    // Indicates two things:
    //   1) This view may have a new layout now.
    //   2) Calling layout() is a no-op.
    // After calling WebWidget::layout(), expect to get this notification
    // unless the view did not need a layout.
    void layoutUpdated(WebLocalFrameImpl*);

    void willInsertBody(WebLocalFrameImpl*);
    void didChangeContentsSize();
    void deviceOrPageScaleFactorChanged();

    // Returns true if popup menus should be rendered by the browser, false if
    // they should be rendered by WebKit (which is the default).
    static bool useExternalPopupMenus();

    bool contextMenuAllowed() const
    {
        return m_contextMenuAllowed;
    }

    bool shouldAutoResize() const
    {
        return m_shouldAutoResize;
    }

    WebCore::IntSize minAutoSize() const
    {
        return m_minAutoSize;
    }

    WebCore::IntSize maxAutoSize() const
    {
        return m_maxAutoSize;
    }

    void updateMainFrameLayoutSize();
    void updatePageDefinedViewportConstraints(const WebCore::ViewportDescription&);

    // Start a system drag and drop operation.
    void startDragging(
        WebCore::LocalFrame*,
        const WebDragData& dragData,
        WebDragOperationsMask mask,
        const WebImage& dragImage,
        const WebPoint& dragImageOffset);

    // Tries to scroll the currently focused element and bubbles up through the
    // DOM and frame hierarchies. Returns true if something was scrolled.
    bool bubblingScroll(WebCore::ScrollDirection, WebCore::ScrollGranularity);

    // Notification that a popup was opened/closed.
    void popupOpened(PopupContainer*);
    void popupClosed(PopupContainer*);
    // PagePopupDriver functions.
    virtual WebCore::PagePopup* openPagePopup(WebCore::PagePopupClient*, const WebCore::IntRect& originBoundsInRootView) OVERRIDE;
    virtual void closePagePopup(WebCore::PagePopup*) OVERRIDE;

    // Returns the input event we're currently processing. This is used in some
    // cases where the WebCore DOM event doesn't have the information we need.
    static const WebInputEvent* currentInputEvent()
    {
        return m_currentInputEvent;
    }

    WebCore::GraphicsLayer* rootGraphicsLayer();
    void setRootGraphicsLayer(WebCore::GraphicsLayer*);
    void scheduleCompositingLayerSync();
    void scrollRootLayer();
    WebCore::GraphicsLayerFactory* graphicsLayerFactory() const;
    WebCore::RenderLayerCompositor* compositor() const;
    void registerForAnimations(WebLayer*);
    void scheduleAnimation();

    virtual void setVisibilityState(WebPageVisibilityState, bool) OVERRIDE;

    PopupContainer* selectPopup() const { return m_selectPopup.get(); }
    bool hasOpenedPopup() const { return m_selectPopup || m_pagePopup; }

    // Returns true if the event leads to scrolling.
    static bool mapKeyCodeForScroll(int keyCode,
                                   WebCore::ScrollDirection* scrollDirection,
                                   WebCore::ScrollGranularity* scrollGranularity);

    // Called by a full frame plugin inside this view to inform it that its
    // zoom level has been updated.  The plugin should only call this function
    // if the zoom change was triggered by the browser, it's only needed in case
    // a plugin can update its own zoom, say because of its own UI.
    void fullFramePluginZoomLevelChanged(double zoomLevel);

    void computeScaleAndScrollForBlockRect(const WebPoint& hitPoint, const WebRect& blockRect, float padding, float defaultScaleWhenAlreadyLegible, float& scale, WebPoint& scroll);
    WebCore::Node* bestTapNode(const WebCore::PlatformGestureEvent& tapEvent);
    void enableTapHighlightAtPoint(const WebCore::PlatformGestureEvent& tapEvent);
    void enableTapHighlights(WillBeHeapVector<RawPtrWillBeMember<WebCore::Node> >&);
    void computeScaleAndScrollForFocusedNode(WebCore::Node* focusedNode, float& scale, WebCore::IntPoint& scroll, bool& needAnimation);

    void animateDoubleTapZoom(const WebCore::IntPoint&);

    void enableFakePageScaleAnimationForTesting(bool);
    bool fakeDoubleTapAnimationPendingForTesting() const { return m_doubleTapZoomPending; }
    WebCore::IntPoint fakePageScaleAnimationTargetPositionForTesting() const { return m_fakePageScaleAnimationTargetPosition; }
    float fakePageScaleAnimationPageScaleForTesting() const { return m_fakePageScaleAnimationPageScaleFactor; }
    bool fakePageScaleAnimationUseAnchorForTesting() const { return m_fakePageScaleAnimationUseAnchor; }

    void enterFullScreenForElement(WebCore::Element*);
    void exitFullScreenForElement(WebCore::Element*);

    // Exposed for the purpose of overriding device metrics.
    void sendResizeEventAndRepaint();

    // Exposed for testing purposes.
    bool hasHorizontalScrollbar();
    bool hasVerticalScrollbar();

    // Pointer Lock calls allow a page to capture all mouse events and
    // disable the system cursor.
    bool requestPointerLock();
    void requestPointerUnlock();
    bool isPointerLocked();

    // Heuristic-based function for determining if we should disable workarounds
    // for viewing websites that are not optimized for mobile devices.
    bool shouldDisableDesktopWorkarounds();

    // Exposed for tests.
    unsigned numLinkHighlights() { return m_linkHighlights.size(); }
    LinkHighlight* linkHighlight(int i) { return m_linkHighlights[i].get(); }

    WebSettingsImpl* settingsImpl();

    // Returns the bounding box of the block type node touched by the WebRect.
    WebRect computeBlockBounds(const WebRect&, bool ignoreClipping);

    WebCore::IntPoint clampOffsetAtScale(const WebCore::IntPoint& offset, float scale);

    // Exposed for tests.
    WebVector<WebCompositionUnderline> compositionUnderlines() const;

    WebLayerTreeView* layerTreeView() const { return m_layerTreeView; }

    bool pinchVirtualViewportEnabled() const;

    bool matchesHeuristicsForGpuRasterizationForTesting() const { return m_matchesHeuristicsForGpuRasterization; }

private:
    // TODO(bokan): Remains for legacy pinch. Remove once it's gone. Made private to
    // prevent external usage
    virtual void setPageScaleFactor(float scaleFactor, const WebPoint& origin) OVERRIDE;

    float legibleScale() const;
    void refreshPageScaleFactorAfterLayout();
    void resumeTreeViewCommits();
    void setUserAgentPageScaleConstraints(WebCore::PageScaleConstraints newConstraints);
    float clampPageScaleFactorToLimits(float) const;
    WebCore::IntSize contentsSize() const;

    void resetSavedScrollAndScaleState();

    void updateMainFrameScrollPosition(const WebCore::IntPoint& scrollPosition, bool programmaticScroll);

    friend class WebView;  // So WebView::Create can call our constructor
    friend class WTF::RefCounted<WebViewImpl>;
    friend void setCurrentInputEventForTest(const WebInputEvent*);

    enum DragAction {
      DragEnter,
      DragOver
    };

    explicit WebViewImpl(WebViewClient*);
    virtual ~WebViewImpl();

    WebTextInputType textInputType();

    WebString inputModeOfFocusedElement();

    // Returns true if the event was actually processed.
    bool keyEventDefault(const WebKeyboardEvent&);

    bool confirmComposition(const WebString& text, ConfirmCompositionBehavior);

    // Returns true if the view was scrolled.
    bool scrollViewWithKeyboard(int keyCode, int modifiers);

    void hideSelectPopup();

    // Converts |pos| from window coordinates to contents coordinates and gets
    // the HitTestResult for it.
    WebCore::HitTestResult hitTestResultForWindowPos(const WebCore::IntPoint&);

    // Consolidate some common code between starting a drag over a target and
    // updating a drag over a target. If we're starting a drag, |isEntering|
    // should be true.
    WebDragOperation dragTargetDragEnterOrOver(const WebPoint& clientPoint,
                                               const WebPoint& screenPoint,
                                               DragAction,
                                               int keyModifiers);

    void configureAutoResizeMode();

    void setIsAcceleratedCompositingActive(bool);
    void doComposite();
    void reallocateRenderer();
    void updateLayerTreeViewport();
    void updateLayerTreeBackgroundColor();
    void updateRootLayerTransform();
    void updateLayerTreeDeviceScaleFactor();

    // Helper function: Widens the width of |source| by the specified margins
    // while keeping it smaller than page width.
    WebRect widenRectWithinPageBounds(const WebRect& source, int targetMargin, int minimumMargin);

    void pointerLockMouseEvent(const WebInputEvent&);

    // PageWidgetEventHandler functions
    virtual void handleMouseLeave(WebCore::LocalFrame&, const WebMouseEvent&) OVERRIDE;
    virtual void handleMouseDown(WebCore::LocalFrame&, const WebMouseEvent&) OVERRIDE;
    virtual void handleMouseUp(WebCore::LocalFrame&, const WebMouseEvent&) OVERRIDE;
    virtual bool handleMouseWheel(WebCore::LocalFrame&, const WebMouseWheelEvent&) OVERRIDE;
    virtual bool handleGestureEvent(const WebGestureEvent&) OVERRIDE;
    virtual bool handleKeyEvent(const WebKeyboardEvent&) OVERRIDE;
    virtual bool handleCharEvent(const WebKeyboardEvent&) OVERRIDE;

    WebCore::InputMethodContext* inputMethodContext();
    WebPlugin* focusedPluginIfInputMethodSupported(WebCore::LocalFrame*);

    WebViewClient* m_client; // Can be 0 (e.g. unittests, shared workers, etc.)
    WebAutofillClient* m_autofillClient;
    WebSpellCheckClient* m_spellCheckClient;

    ChromeClientImpl m_chromeClientImpl;
    ContextMenuClientImpl m_contextMenuClientImpl;
    DragClientImpl m_dragClientImpl;
    EditorClientImpl m_editorClientImpl;
    InspectorClientImpl m_inspectorClientImpl;
    BackForwardClientImpl m_backForwardClientImpl;
    SpellCheckerClientImpl m_spellCheckerClientImpl;
    StorageClientImpl m_storageClientImpl;

    WebSize m_size;
    bool m_fixedLayoutSizeLock;
    // If true, automatically resize the render view around its content.
    bool m_shouldAutoResize;
    // The lower bound on the size when auto-resizing.
    WebCore::IntSize m_minAutoSize;
    // The upper bound on the size when auto-resizing.
    WebCore::IntSize m_maxAutoSize;

    OwnPtrWillBePersistent<WebCore::Page> m_page;

    // An object that can be used to manipulate m_page->settings() without linking
    // against WebCore. This is lazily allocated the first time GetWebSettings()
    // is called.
    OwnPtr<WebSettingsImpl> m_webSettings;

    // A copy of the web drop data object we received from the browser.
    RefPtrWillBePersistent<WebCore::DataObject> m_currentDragData;

    // The point relative to the client area where the mouse was last pressed
    // down. This is used by the drag client to determine what was under the
    // mouse when the drag was initiated. We need to track this here in
    // WebViewImpl since DragClient::startDrag does not pass the position the
    // mouse was at when the drag was initiated, only the current point, which
    // can be misleading as it is usually not over the element the user actually
    // dragged by the time a drag is initiated.
    WebPoint m_lastMouseDownPoint;

    // Keeps track of the current zoom level. 0 means no zoom, positive numbers
    // mean zoom in, negative numbers mean zoom out.
    double m_zoomLevel;

    double m_minimumZoomLevel;

    double m_maximumZoomLevel;

    PageScaleConstraintsSet m_pageScaleConstraintsSet;

    // The scale moved to by the latest double tap zoom, if any.
    float m_doubleTapZoomPageScaleFactor;
    // Have we sent a double-tap zoom and not yet heard back the scale?
    bool m_doubleTapZoomPending;

    // Used for testing purposes.
    bool m_enableFakePageScaleAnimationForTesting;
    WebCore::IntPoint m_fakePageScaleAnimationTargetPosition;
    float m_fakePageScaleAnimationPageScaleFactor;
    bool m_fakePageScaleAnimationUseAnchor;

    bool m_contextMenuAllowed;

    bool m_doingDragAndDrop;

    bool m_ignoreInputEvents;

    float m_compositorDeviceScaleFactorOverride;
    WebSize m_rootLayerOffset;
    float m_rootLayerScale;

    // Webkit expects keyPress events to be suppressed if the associated keyDown
    // event was handled. Safari implements this behavior by peeking out the
    // associated WM_CHAR event if the keydown was handled. We emulate
    // this behavior by setting this flag if the keyDown was handled.
    bool m_suppressNextKeypressEvent;

    // Represents whether or not this object should process incoming IME events.
    bool m_imeAcceptEvents;

    // The available drag operations (copy, move link...) allowed by the source.
    WebDragOperation m_operationsAllowed;

    // The current drag operation as negotiated by the source and destination.
    // When not equal to DragOperationNone, the drag data can be dropped onto the
    // current drop target in this WebView (the drop target can accept the drop).
    WebDragOperation m_dragOperation;

    // The popup associated with a select element.
    RefPtr<PopupContainer> m_selectPopup;

    // The popup associated with an input element.
    RefPtr<WebPagePopupImpl> m_pagePopup;

    OwnPtr<WebDevToolsAgentPrivate> m_devToolsAgent;
    OwnPtr<PageOverlayList> m_pageOverlays;

    // Whether the webview is rendering transparently.
    bool m_isTransparent;

    // Whether the user can press tab to focus links.
    bool m_tabsToLinks;

    // Inspector settings.
    WebString m_inspectorSettings;

    typedef HashMap<WTF::String, WTF::String> SettingsMap;
    OwnPtr<SettingsMap> m_inspectorSettingsMap;

    // If set, the (plugin) node which has mouse capture.
    RefPtrWillBePersistent<WebCore::Node> m_mouseCaptureNode;
    RefPtr<WebCore::UserGestureToken> m_mouseCaptureGestureToken;

    WebCore::IntRect m_rootLayerScrollDamage;
    WebLayerTreeView* m_layerTreeView;
    WebLayer* m_rootLayer;
    WebCore::GraphicsLayer* m_rootGraphicsLayer;
    WebCore::GraphicsLayer* m_rootTransformLayer;
    OwnPtr<WebCore::GraphicsLayerFactory> m_graphicsLayerFactory;
    bool m_isAcceleratedCompositingActive;
    bool m_layerTreeViewCommitsDeferred;
    bool m_matchesHeuristicsForGpuRasterization;
    // If true, the graphics context is being restored.
    bool m_recreatingGraphicsContext;
    static const WebInputEvent* m_currentInputEvent;

    MediaKeysClientImpl m_mediaKeysClientImpl;
    OwnPtr<WebActiveGestureAnimation> m_gestureAnimation;
    WebPoint m_positionOnFlingStart;
    WebPoint m_globalPositionOnFlingStart;
    int m_flingModifier;
    bool m_flingSourceDevice;
    Vector<OwnPtr<LinkHighlight> > m_linkHighlights;
    OwnPtr<FullscreenController> m_fullscreenController;

    bool m_showFPSCounter;
    bool m_showPaintRects;
    bool m_showDebugBorders;
    bool m_continuousPaintingEnabled;
    bool m_showScrollBottleneckRects;
    WebColor m_baseBackgroundColor;
    WebColor m_backgroundColorOverride;
    float m_zoomFactorOverride;

    bool m_userGestureObserved;
};

// We have no ways to check if the specified WebView is an instance of
// WebViewImpl because WebViewImpl is the only implementation of WebView.
DEFINE_TYPE_CASTS(WebViewImpl, WebView, webView, true, true);

} // namespace blink

#endif
