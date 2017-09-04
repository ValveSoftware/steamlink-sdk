/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#ifndef WebFrameWidgetImpl_h
#define WebFrameWidgetImpl_h

#include "platform/graphics/GraphicsLayer.h"
#include "platform/heap/SelfKeepAlive.h"
#include "platform/scroll/ScrollTypes.h"
#include "public/platform/WebInputEvent.h"
#include "public/platform/WebPoint.h"
#include "public/platform/WebSize.h"
#include "public/web/WebInputMethodController.h"
#include "web/PageWidgetDelegate.h"
#include "web/WebFrameWidgetBase.h"
#include "web/WebInputMethodControllerImpl.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebViewImpl.h"
#include "wtf/Assertions.h"
#include "wtf/HashSet.h"

namespace blink {
class Frame;
class Element;
class InspectorOverlay;
class LocalFrame;
class Page;
class PaintLayerCompositor;
class UserGestureToken;
class CompositorAnimationTimeline;
class WebLayer;
class WebLayerTreeView;
class WebMouseEvent;
class WebMouseWheelEvent;
class WebFrameWidgetImpl;

using WebFrameWidgetsSet =
    PersistentHeapHashSet<WeakMember<WebFrameWidgetImpl>>;

class WebFrameWidgetImpl final
    : public GarbageCollectedFinalized<WebFrameWidgetImpl>,
      public WebFrameWidgetBase,
      public PageWidgetEventHandler {
 public:
  static WebFrameWidgetImpl* create(WebWidgetClient*, WebLocalFrame*);
  static WebFrameWidgetsSet& allInstances();

  ~WebFrameWidgetImpl();

  // WebWidget functions:
  void close() override;
  WebSize size() override;
  void resize(const WebSize&) override;
  void resizeVisualViewport(const WebSize&) override;
  void didEnterFullscreen() override;
  void didExitFullscreen() override;
  void beginFrame(double lastFrameTimeMonotonic) override;
  void updateAllLifecyclePhases() override;
  void paint(WebCanvas*, const WebRect&) override;
  void layoutAndPaintAsync(WebLayoutAndPaintAsyncCallback*) override;
  void compositeAndReadbackAsync(
      WebCompositeAndReadbackAsyncCallback*) override;
  void themeChanged() override;
  WebInputEventResult handleInputEvent(const WebInputEvent&) override;
  void setCursorVisibilityState(bool isVisible) override;
  bool hasTouchEventHandlersAt(const WebPoint&) override;

  void applyViewportDeltas(const WebFloatSize& visualViewportDelta,
                           const WebFloatSize& mainFrameDelta,
                           const WebFloatSize& elasticOverscrollDelta,
                           float pageScaleDelta,
                           float browserControlsDelta) override;
  void mouseCaptureLost() override;
  void setFocus(bool enable) override;
  WebRange compositionRange() override;
  WebTextInputInfo textInputInfo() override;
  WebTextInputType textInputType() override;
  WebColor backgroundColor() const override;
  bool selectionBounds(WebRect& anchor, WebRect& focus) const override;
  bool selectionTextDirection(WebTextDirection& start,
                              WebTextDirection& end) const override;
  bool isSelectionAnchorFirst() const override;
  WebRange caretOrSelectionRange() override;
  void setTextDirection(WebTextDirection) override;
  bool isAcceleratedCompositingActive() const override;
  void willCloseLayerTreeView() override;
  void didAcquirePointerLock() override;
  void didNotAcquirePointerLock() override;
  void didLosePointerLock() override;
  bool getCompositionCharacterBounds(WebVector<WebRect>& bounds) override;
  void applyReplacementRange(const WebRange&) override;

  // WebFrameWidget implementation.
  WebLocalFrameImpl* localRoot() const override { return m_localRoot; }
  void setVisibilityState(WebPageVisibilityState) override;
  bool isTransparent() const override;
  void setIsTransparent(bool) override;
  void setBaseBackgroundColor(WebColor) override;
  WebInputMethodControllerImpl* getActiveWebInputMethodController()
      const override;

  Frame* focusedCoreFrame() const;

  // Returns the currently focused Element or null if no element has focus.
  Element* focusedElement() const;

  PaintLayerCompositor* compositor() const;

  // WebFrameWidgetBase overrides:
  bool forSubframe() const override { return true; }
  void scheduleAnimation() override;
  CompositorProxyClient* createCompositorProxyClient() override;
  WebWidgetClient* client() const override { return m_client; }
  void setRootGraphicsLayer(GraphicsLayer*) override;
  void setRootLayer(WebLayer*) override;
  void attachCompositorAnimationTimeline(CompositorAnimationTimeline*) override;
  void detachCompositorAnimationTimeline(CompositorAnimationTimeline*) override;
  HitTestResult coreHitTestResultAt(const WebPoint&) override;

  // Exposed for the purpose of overriding device metrics.
  void sendResizeEventAndRepaint();

  void updateMainFrameLayoutSize();

  void setIgnoreInputEvents(bool newValue);

  // Event related methods:
  void mouseContextMenu(const WebMouseEvent&);

  WebLayerTreeView* layerTreeView() const { return m_layerTreeView; }
  GraphicsLayer* rootGraphicsLayer() const { return m_rootGraphicsLayer; };

  Color baseBackgroundColor() const { return m_baseBackgroundColor; }

  DECLARE_TRACE();

 private:
  friend class WebFrameWidget;  // For WebFrameWidget::create.

  explicit WebFrameWidgetImpl(WebWidgetClient*, WebLocalFrame*);

  // Perform a hit test for a point relative to the root frame of the page.
  HitTestResult hitTestResultForRootFramePos(const IntPoint& posInRootFrame);

  void initializeLayerTreeView();

  void setIsAcceleratedCompositingActive(bool);
  void updateLayerTreeViewport();
  void updateLayerTreeBackgroundColor();
  void updateLayerTreeDeviceScaleFactor();

  // PageWidgetEventHandler functions
  void handleMouseLeave(LocalFrame&, const WebMouseEvent&) override;
  void handleMouseDown(LocalFrame&, const WebMouseEvent&) override;
  void handleMouseUp(LocalFrame&, const WebMouseEvent&) override;
  WebInputEventResult handleMouseWheel(LocalFrame&,
                                       const WebMouseWheelEvent&) override;
  WebInputEventResult handleGestureEvent(const WebGestureEvent&) override;
  WebInputEventResult handleKeyEvent(const WebKeyboardEvent&) override;
  WebInputEventResult handleCharEvent(const WebKeyboardEvent&) override;

  InspectorOverlay* inspectorOverlay();

  // This method returns the focused frame belonging to this WebWidget, that
  // is, a focused frame with the same local root as the one corresponding
  // to this widget. It will return nullptr if no frame is focused or, the
  // focused frame has a different local root.
  LocalFrame* focusedLocalFrameInWidget() const;

  WebPlugin* focusedPluginIfInputMethodSupported(LocalFrame*) const;

  LocalFrame* focusedLocalFrameAvailableForIme() const;

  WebWidgetClient* m_client;

  // WebFrameWidget is associated with a subtree of the frame tree,
  // corresponding to a maximal connected tree of LocalFrames. This member
  // points to the root of that subtree.
  Member<WebLocalFrameImpl> m_localRoot;

  WebSize m_size;

  // If set, the (plugin) node which has mouse capture.
  Member<Node> m_mouseCaptureNode;
  RefPtr<UserGestureToken> m_mouseCaptureGestureToken;

  // This is owned by the LayerTreeHostImpl, and should only be used on the
  // compositor thread. The LayerTreeHostImpl is indirectly owned by this
  // class so this pointer should be valid until this class is destructed.
  CrossThreadPersistent<CompositorMutatorImpl> m_mutator;

  WebLayerTreeView* m_layerTreeView;
  WebLayer* m_rootLayer;
  GraphicsLayer* m_rootGraphicsLayer;
  bool m_isAcceleratedCompositingActive;
  bool m_layerTreeViewClosed;

  bool m_suppressNextKeypressEvent;

  bool m_ignoreInputEvents;

  // Whether the WebFrameWidget is rendering transparently.
  bool m_isTransparent;

  // Represents whether or not this object should process incoming IME events.
  bool m_imeAcceptEvents;

  static const WebInputEvent* m_currentInputEvent;

  WebColor m_baseBackgroundColor;

  SelfKeepAlive<WebFrameWidgetImpl> m_selfKeepAlive;
};

DEFINE_TYPE_CASTS(WebFrameWidgetImpl,
                  WebFrameWidgetBase,
                  widget,
                  widget->forSubframe(),
                  widget.forSubframe());

}  // namespace blink

#endif
