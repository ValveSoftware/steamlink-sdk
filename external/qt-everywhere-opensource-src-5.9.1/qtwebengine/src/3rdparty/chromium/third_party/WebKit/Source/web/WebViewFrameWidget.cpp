// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "web/WebViewFrameWidget.h"

#include "web/WebInputMethodControllerImpl.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebViewImpl.h"

namespace blink {

WebViewFrameWidget::WebViewFrameWidget(WebWidgetClient* client,
                                       WebViewImpl& webView,
                                       WebLocalFrameImpl& mainFrame)
    : m_client(client), m_webView(&webView), m_mainFrame(&mainFrame) {
  m_mainFrame->setFrameWidget(this);
  m_webView->setCompositorVisibility(true);
}

WebViewFrameWidget::~WebViewFrameWidget() {}

void WebViewFrameWidget::close() {
  // Note: it's important to use the captured main frame pointer here. During
  // a frame swap, the swapped frame is detached *after* the frame tree is
  // updated. If the main frame is being swapped, then
  // m_webView()->mainFrameImpl() will no longer point to the original frame.
  m_webView->setCompositorVisibility(false);
  m_mainFrame->setFrameWidget(nullptr);
  m_mainFrame = nullptr;
  m_webView = nullptr;
  m_client = nullptr;

  // Note: this intentionally does not forward to WebView::close(), to make it
  // easier to untangle the cleanup logic later.

  delete this;
}

WebSize WebViewFrameWidget::size() {
  return m_webView->size();
}

void WebViewFrameWidget::resize(const WebSize& size) {
  return m_webView->resize(size);
}

void WebViewFrameWidget::resizeVisualViewport(const WebSize& size) {
  return m_webView->resizeVisualViewport(size);
}

void WebViewFrameWidget::didEnterFullscreen() {
  return m_webView->didEnterFullscreen();
}

void WebViewFrameWidget::didExitFullscreen() {
  return m_webView->didExitFullscreen();
}

void WebViewFrameWidget::beginFrame(double lastFrameTimeMonotonic) {
  return m_webView->beginFrame(lastFrameTimeMonotonic);
}

void WebViewFrameWidget::updateAllLifecyclePhases() {
  return m_webView->updateAllLifecyclePhases();
}

void WebViewFrameWidget::paint(WebCanvas* canvas, const WebRect& viewPort) {
  return m_webView->paint(canvas, viewPort);
}

void WebViewFrameWidget::layoutAndPaintAsync(
    WebLayoutAndPaintAsyncCallback* callback) {
  return m_webView->layoutAndPaintAsync(callback);
}

void WebViewFrameWidget::compositeAndReadbackAsync(
    WebCompositeAndReadbackAsyncCallback* callback) {
  return m_webView->compositeAndReadbackAsync(callback);
}

void WebViewFrameWidget::themeChanged() {
  return m_webView->themeChanged();
}

WebInputEventResult WebViewFrameWidget::handleInputEvent(
    const WebInputEvent& event) {
  return m_webView->handleInputEvent(event);
}

void WebViewFrameWidget::setCursorVisibilityState(bool isVisible) {
  return m_webView->setCursorVisibilityState(isVisible);
}

bool WebViewFrameWidget::hasTouchEventHandlersAt(const WebPoint& point) {
  return m_webView->hasTouchEventHandlersAt(point);
}

void WebViewFrameWidget::applyViewportDeltas(
    const WebFloatSize& visualViewportDelta,
    const WebFloatSize& layoutViewportDelta,
    const WebFloatSize& elasticOverscrollDelta,
    float scaleFactor,
    float browserControlsShownRatioDelta) {
  return m_webView->applyViewportDeltas(
      visualViewportDelta, layoutViewportDelta, elasticOverscrollDelta,
      scaleFactor, browserControlsShownRatioDelta);
}

void WebViewFrameWidget::mouseCaptureLost() {
  return m_webView->mouseCaptureLost();
}

void WebViewFrameWidget::setFocus(bool enable) {
  return m_webView->setFocus(enable);
}

WebRange WebViewFrameWidget::compositionRange() {
  return m_webView->compositionRange();
}

WebTextInputInfo WebViewFrameWidget::textInputInfo() {
  return m_webView->textInputInfo();
}

WebTextInputType WebViewFrameWidget::textInputType() {
  return m_webView->textInputType();
}

bool WebViewFrameWidget::selectionBounds(WebRect& anchor,
                                         WebRect& focus) const {
  return m_webView->selectionBounds(anchor, focus);
}

bool WebViewFrameWidget::selectionTextDirection(WebTextDirection& start,
                                                WebTextDirection& end) const {
  return m_webView->selectionTextDirection(start, end);
}

bool WebViewFrameWidget::isSelectionAnchorFirst() const {
  return m_webView->isSelectionAnchorFirst();
}

WebRange WebViewFrameWidget::caretOrSelectionRange() {
  return m_webView->caretOrSelectionRange();
}

void WebViewFrameWidget::setTextDirection(WebTextDirection direction) {
  return m_webView->setTextDirection(direction);
}

bool WebViewFrameWidget::isAcceleratedCompositingActive() const {
  return m_webView->isAcceleratedCompositingActive();
}

void WebViewFrameWidget::willCloseLayerTreeView() {
  return m_webView->willCloseLayerTreeView();
}

void WebViewFrameWidget::didAcquirePointerLock() {
  return m_webView->didAcquirePointerLock();
}

void WebViewFrameWidget::didNotAcquirePointerLock() {
  return m_webView->didNotAcquirePointerLock();
}

void WebViewFrameWidget::didLosePointerLock() {
  return m_webView->didLosePointerLock();
}

WebColor WebViewFrameWidget::backgroundColor() const {
  return m_webView->backgroundColor();
}

WebPagePopup* WebViewFrameWidget::pagePopup() const {
  return m_webView->pagePopup();
}

bool WebViewFrameWidget::getCompositionCharacterBounds(
    WebVector<WebRect>& bounds) {
  return m_webView->getCompositionCharacterBounds(bounds);
}

void WebViewFrameWidget::applyReplacementRange(const WebRange& range) {
  m_webView->applyReplacementRange(range);
}

void WebViewFrameWidget::updateBrowserControlsState(
    WebBrowserControlsState constraints,
    WebBrowserControlsState current,
    bool animate) {
  return m_webView->updateBrowserControlsState(constraints, current, animate);
}

void WebViewFrameWidget::setVisibilityState(
    WebPageVisibilityState visibilityState) {
  return m_webView->setVisibilityState(visibilityState, false);
}

void WebViewFrameWidget::setIsTransparent(bool isTransparent) {
  m_webView->setIsTransparent(isTransparent);
}

bool WebViewFrameWidget::isTransparent() const {
  return m_webView->isTransparent();
}

void WebViewFrameWidget::setBaseBackgroundColor(WebColor color) {
  m_webView->setBaseBackgroundColor(color);
}

WebLocalFrameImpl* WebViewFrameWidget::localRoot() const {
  return m_webView->mainFrameImpl();
}

WebInputMethodControllerImpl*
WebViewFrameWidget::getActiveWebInputMethodController() const {
  return m_webView->getActiveWebInputMethodController();
}

void WebViewFrameWidget::scheduleAnimation() {
  m_webView->scheduleAnimationForWidget();
}

CompositorProxyClient* WebViewFrameWidget::createCompositorProxyClient() {
  return m_webView->createCompositorProxyClient();
}

void WebViewFrameWidget::setRootGraphicsLayer(GraphicsLayer* layer) {
  m_webView->setRootGraphicsLayer(layer);
}

void WebViewFrameWidget::setRootLayer(WebLayer* layer) {
  m_webView->setRootLayer(layer);
}

void WebViewFrameWidget::attachCompositorAnimationTimeline(
    CompositorAnimationTimeline* compositorTimeline) {
  m_webView->attachCompositorAnimationTimeline(compositorTimeline);
}

void WebViewFrameWidget::detachCompositorAnimationTimeline(
    CompositorAnimationTimeline* compositorTimeline) {
  m_webView->detachCompositorAnimationTimeline(compositorTimeline);
}

HitTestResult WebViewFrameWidget::coreHitTestResultAt(const WebPoint& point) {
  return m_webView->coreHitTestResultAt(point);
}

}  // namespace blink
