// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef WebViewFrameWidget_h
#define WebViewFrameWidget_h

#include "platform/heap/Handle.h"
#include "web/WebFrameWidgetBase.h"
#include "web/WebInputMethodControllerImpl.h"
#include "web/WebLocalFrameImpl.h"
#include "wtf/Noncopyable.h"
#include "wtf/RefPtr.h"

namespace blink {

class WebViewImpl;
class WebWidgetClient;

// Shim class to help normalize the widget interfaces in the Blink public API.
// For OOPI, subframes have WebFrameWidgets for input and rendering.
// Unfortunately, the main frame still uses WebView's WebWidget for input and
// rendering. This results in complex code, since there are two different
// implementations of WebWidget and code needs to have branches to handle both
// cases.
// This class allows a Blink embedder to create a WebFrameWidget that can be
// used for the main frame. Internally, it currently wraps WebView's WebWidget
// and just forwards almost everything to it.
// After the embedder starts using a WebFrameWidget for the main frame,
// WebView will be updated to no longer inherit WebWidget. The eventual goal is
// to unfork the widget code duplicated in WebFrameWidgetImpl and WebViewImpl
// into one class.
// A more detailed writeup of this transition can be read at
// https://goo.gl/7yVrnb.
class WebViewFrameWidget : public WebFrameWidgetBase {
  WTF_MAKE_NONCOPYABLE(WebViewFrameWidget);

 public:
  explicit WebViewFrameWidget(WebWidgetClient*,
                              WebViewImpl&,
                              WebLocalFrameImpl&);
  virtual ~WebViewFrameWidget();

  // WebFrameWidget overrides:
  void close() override;
  WebSize size() override;
  void resize(const WebSize&) override;
  void resizeVisualViewport(const WebSize&) override;
  void didEnterFullscreen() override;
  void didExitFullscreen() override;
  void beginFrame(double lastFrameTimeMonotonic) override;
  void updateAllLifecyclePhases() override;
  void paint(WebCanvas*, const WebRect& viewPort) override;
  void layoutAndPaintAsync(WebLayoutAndPaintAsyncCallback*) override;
  void compositeAndReadbackAsync(
      WebCompositeAndReadbackAsyncCallback*) override;
  void themeChanged() override;
  WebInputEventResult handleInputEvent(const WebInputEvent&) override;
  void setCursorVisibilityState(bool isVisible) override;
  bool hasTouchEventHandlersAt(const WebPoint&) override;
  void applyViewportDeltas(const WebFloatSize& visualViewportDelta,
                           const WebFloatSize& layoutViewportDelta,
                           const WebFloatSize& elasticOverscrollDelta,
                           float scaleFactor,
                           float browserControlsShownRatioDelta) override;
  void mouseCaptureLost() override;
  void setFocus(bool) override;
  WebRange compositionRange() override;
  WebTextInputInfo textInputInfo() override;
  WebTextInputType textInputType() override;
  bool selectionBounds(WebRect& anchor, WebRect& focus) const override;
  bool selectionTextDirection(WebTextDirection& start,
                              WebTextDirection& end) const override;
  bool isSelectionAnchorFirst() const override;
  WebRange caretOrSelectionRange() override;
  void setTextDirection(WebTextDirection) override;
  bool isAcceleratedCompositingActive() const override;
  bool isWebView() const override { return false; }
  bool isPagePopup() const override { return false; }
  void willCloseLayerTreeView() override;
  void didAcquirePointerLock() override;
  void didNotAcquirePointerLock() override;
  void didLosePointerLock() override;
  WebColor backgroundColor() const override;
  WebPagePopup* pagePopup() const override;
  bool getCompositionCharacterBounds(WebVector<WebRect>& bounds) override;
  void applyReplacementRange(const WebRange&) override;
  void updateBrowserControlsState(WebBrowserControlsState constraints,
                                  WebBrowserControlsState current,
                                  bool animate) override;
  void setVisibilityState(WebPageVisibilityState) override;
  bool isTransparent() const override;
  void setIsTransparent(bool) override;
  void setBaseBackgroundColor(WebColor) override;
  WebLocalFrameImpl* localRoot() const override;
  WebInputMethodControllerImpl* getActiveWebInputMethodController()
      const override;

  // WebFrameWidgetBase overrides:
  bool forSubframe() const override { return false; }
  void scheduleAnimation() override;
  CompositorProxyClient* createCompositorProxyClient() override;
  void setRootGraphicsLayer(GraphicsLayer*) override;
  void setRootLayer(WebLayer*) override;
  void attachCompositorAnimationTimeline(CompositorAnimationTimeline*) override;
  void detachCompositorAnimationTimeline(CompositorAnimationTimeline*) override;
  WebWidgetClient* client() const override { return m_client; }
  HitTestResult coreHitTestResultAt(const WebPoint&) override;

 private:
  WebWidgetClient* m_client;
  RefPtr<WebViewImpl> m_webView;
  Persistent<WebLocalFrameImpl> m_mainFrame;
};

}  // namespace blink

#endif  // WebViewFrameWidget_h
