/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "core/frame/VisualViewport.h"

#include "core/dom/DOMNodeIds.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/PageScaleConstraints.h"
#include "core/frame/PageScaleConstraintsSet.h"
#include "core/frame/RootFrameViewport.h"
#include "core/frame/Settings.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/layout/TextAutosizer.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "platform/Histogram.h"
#include "platform/geometry/DoubleRect.h"
#include "platform/geometry/FloatSize.h"
#include "platform/graphics/CompositorMutableProperties.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/scroll/Scrollbar.h"
#include "platform/scroll/ScrollbarThemeOverlay.h"
#include "platform/tracing/TraceEvent.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebScrollbar.h"
#include "public/platform/WebScrollbarLayer.h"
#include <memory>

namespace blink {

VisualViewport::VisualViewport(FrameHost& owner)
    : m_frameHost(&owner),
      m_scale(1),
      m_browserControlsAdjustment(0),
      m_maxPageScale(-1),
      m_trackPinchZoomStatsForPage(false) {
  reset();
}

VisualViewport::~VisualViewport() {
  sendUMAMetrics();
}

DEFINE_TRACE(VisualViewport) {
  visitor->trace(m_frameHost);
  ScrollableArea::trace(visitor);
}

void VisualViewport::updateStyleAndLayoutIgnorePendingStylesheets() {
  if (!mainFrame())
    return;

  if (Document* document = mainFrame()->document())
    document->updateStyleAndLayoutIgnorePendingStylesheets();
}

void VisualViewport::enqueueScrollEvent() {
  if (!RuntimeEnabledFeatures::visualViewportAPIEnabled())
    return;

  if (Document* document = mainFrame()->document())
    document->enqueueVisualViewportScrollEvent();
}

void VisualViewport::enqueueResizeEvent() {
  if (!RuntimeEnabledFeatures::visualViewportAPIEnabled())
    return;

  if (Document* document = mainFrame()->document())
    document->enqueueVisualViewportResizeEvent();
}

void VisualViewport::setSize(const IntSize& size) {
  if (m_size == size)
    return;

  TRACE_EVENT2("blink", "VisualViewport::setSize", "width", size.width(),
               "height", size.height());
  bool widthDidChange = size.width() != m_size.width();
  m_size = size;

  if (m_innerViewportContainerLayer) {
    m_innerViewportContainerLayer->setSize(FloatSize(m_size));

    // Need to re-compute sizes for the overlay scrollbars.
    initializeScrollbars();
  }

  if (!mainFrame())
    return;

  enqueueResizeEvent();

  bool autosizerNeedsUpdating =
      widthDidChange && mainFrame()->settings() &&
      mainFrame()->settings()->textAutosizingEnabled();

  if (autosizerNeedsUpdating) {
    // This needs to happen after setting the m_size member since it'll be read
    // in the update call.
    if (TextAutosizer* textAutosizer = mainFrame()->document()->textAutosizer())
      textAutosizer->updatePageInfoInAllFrames();
  }
}

void VisualViewport::reset() {
  setScaleAndLocation(1, FloatPoint());
}

void VisualViewport::mainFrameDidChangeSize() {
  TRACE_EVENT0("blink", "VisualViewport::mainFrameDidChangeSize");

  // In unit tests we may not have initialized the layer tree.
  if (m_innerViewportScrollLayer)
    m_innerViewportScrollLayer->setSize(FloatSize(contentsSize()));

  clampToBoundaries();
}

FloatSize VisualViewport::visibleSize() const {
  FloatSize scaledSize(m_size);
  scaledSize.expand(0, m_browserControlsAdjustment);
  scaledSize.scale(1 / m_scale);
  return scaledSize;
}

FloatRect VisualViewport::visibleRect() const {
  return FloatRect(FloatPoint(scrollOffset()), visibleSize());
}

FloatRect VisualViewport::visibleRectInDocument() const {
  if (!mainFrame() || !mainFrame()->view())
    return FloatRect();

  FloatPoint viewLocation =
      FloatPoint(mainFrame()->view()->getScrollableArea()->scrollOffset());
  return FloatRect(viewLocation, visibleSize());
}

FloatRect VisualViewport::mainViewToViewportCSSPixels(
    const FloatRect& rect) const {
  // Note, this is in CSS Pixels so we don't apply scale.
  FloatRect rectInViewport = rect;
  rectInViewport.move(-scrollOffset());
  return rectInViewport;
}

FloatPoint VisualViewport::viewportCSSPixelsToRootFrame(
    const FloatPoint& point) const {
  // Note, this is in CSS Pixels so we don't apply scale.
  FloatPoint pointInRootFrame = point;
  pointInRootFrame.move(scrollOffset());
  return pointInRootFrame;
}

void VisualViewport::setLocation(const FloatPoint& newLocation) {
  setScaleAndLocation(m_scale, newLocation);
}

void VisualViewport::move(const ScrollOffset& delta) {
  setLocation(FloatPoint(m_offset + delta));
}

void VisualViewport::setScale(float scale) {
  setScaleAndLocation(scale, FloatPoint(m_offset));
}

double VisualViewport::scrollLeft() {
  if (!mainFrame())
    return 0;

  updateStyleAndLayoutIgnorePendingStylesheets();

  return adjustScrollForAbsoluteZoom(visibleRect().x(),
                                     mainFrame()->pageZoomFactor());
}

double VisualViewport::scrollTop() {
  if (!mainFrame())
    return 0;

  updateStyleAndLayoutIgnorePendingStylesheets();

  return adjustScrollForAbsoluteZoom(visibleRect().y(),
                                     mainFrame()->pageZoomFactor());
}

double VisualViewport::clientWidth() {
  if (!mainFrame())
    return 0;

  updateStyleAndLayoutIgnorePendingStylesheets();

  float width = adjustScrollForAbsoluteZoom(visibleSize().width(),
                                            mainFrame()->pageZoomFactor());
  return width - mainFrame()->view()->verticalScrollbarWidth() / m_scale;
}

double VisualViewport::clientHeight() {
  if (!mainFrame())
    return 0;

  updateStyleAndLayoutIgnorePendingStylesheets();

  float height = adjustScrollForAbsoluteZoom(visibleSize().height(),
                                             mainFrame()->pageZoomFactor());
  return height - mainFrame()->view()->horizontalScrollbarHeight() / m_scale;
}

double VisualViewport::pageScale() {
  updateStyleAndLayoutIgnorePendingStylesheets();

  return m_scale;
}

void VisualViewport::setScaleAndLocation(float scale,
                                         const FloatPoint& location) {
  if (didSetScaleOrLocation(scale, location))
    notifyRootFrameViewport();
}

bool VisualViewport::didSetScaleOrLocation(float scale,
                                           const FloatPoint& location) {
  if (!mainFrame())
    return false;

  bool valuesChanged = false;

  if (scale != m_scale) {
    m_scale = scale;
    valuesChanged = true;
    frameHost().chromeClient().pageScaleFactorChanged();
    enqueueResizeEvent();
  }

  ScrollOffset clampedOffset = clampScrollOffset(toScrollOffset(location));

  if (clampedOffset != m_offset) {
    m_offset = clampedOffset;
    scrollAnimator().setCurrentOffset(m_offset);

    // SVG runs with accelerated compositing disabled so no
    // ScrollingCoordinator.
    if (ScrollingCoordinator* coordinator =
            frameHost().page().scrollingCoordinator())
      coordinator->scrollableAreaScrollLayerDidChange(this);

    if (!frameHost().settings().inertVisualViewport()) {
      if (Document* document = mainFrame()->document())
        document->enqueueScrollEventForNode(document);
    }

    enqueueScrollEvent();

    mainFrame()->view()->didChangeScrollOffset();
    valuesChanged = true;
  }

  if (!valuesChanged)
    return false;

  InspectorInstrumentation::didUpdateLayout(mainFrame());
  mainFrame()->loader().saveScrollState();

  clampToBoundaries();

  return true;
}

bool VisualViewport::magnifyScaleAroundAnchor(float magnifyDelta,
                                              const FloatPoint& anchor) {
  const float oldPageScale = scale();
  const float newPageScale =
      frameHost().chromeClient().clampPageScaleFactorToLimits(magnifyDelta *
                                                              oldPageScale);
  if (newPageScale == oldPageScale)
    return false;
  if (!mainFrame() || !mainFrame()->view())
    return false;

  // Keep the center-of-pinch anchor in a stable position over the course
  // of the magnify.
  FloatPoint anchorAtOldScale = anchor.scaledBy(1.f / oldPageScale);
  FloatPoint anchorAtNewScale = anchor.scaledBy(1.f / newPageScale);
  FloatSize anchorDelta = anchorAtOldScale - anchorAtNewScale;

  // First try to use the anchor's delta to scroll the FrameView.
  FloatSize anchorDeltaUnusedByScroll = anchorDelta;

  // Manually bubble any remaining anchor delta up to the visual viewport.
  FloatPoint newLocation(FloatPoint(scrollOffset()) +
                         anchorDeltaUnusedByScroll);
  setScaleAndLocation(newPageScale, newLocation);
  return true;
}

// Modifies the top of the graphics layer tree to add layers needed to support
// the inner/outer viewport fixed-position model for pinch zoom. When finished,
// the tree will look like this (with * denoting added layers):
//
// *rootTransformLayer
//  +- *innerViewportContainerLayer (fixed pos container)
//     +- *overscrollElasticityLayer
//     |   +- *pageScaleLayer
//     |       +- *innerViewportScrollLayer
//     |           +-- overflowControlsHostLayer (root layer)
//     |               | [ owned by PaintLayerCompositor ]
//     |               +-- outerViewportContainerLayer (fixed pos container)
//     |               |     [frame container layer in PaintLayerCompositor]
//     |               |   +-- outerViewportScrollLayer
//     |               |       | [frame scroll layer in PaintLayerCompositor]
//     |               |       +-- content layers ...
//     +- *PageOverlay for InspectorOverlay
//     +- *PageOverlay for ColorOverlay
//     +- horizontalScrollbarLayer [ owned by PaintLayerCompositor ]
//     +- verticalScrollbarLayer [ owned by PaintLayerCompositor ]
//     +- scroll corner (non-overlay only) [ owned by PaintLayerCompositor ]
//
void VisualViewport::attachToLayerTree(GraphicsLayer* currentLayerTreeRoot) {
  TRACE_EVENT1("blink", "VisualViewport::attachToLayerTree",
               "currentLayerTreeRoot", (bool)currentLayerTreeRoot);
  if (!currentLayerTreeRoot) {
    if (m_innerViewportScrollLayer)
      m_innerViewportScrollLayer->removeAllChildren();
    return;
  }

  if (currentLayerTreeRoot->parent() &&
      currentLayerTreeRoot->parent() == m_innerViewportScrollLayer.get())
    return;

  if (!m_innerViewportScrollLayer) {
    ASSERT(!m_overlayScrollbarHorizontal && !m_overlayScrollbarVertical &&
           !m_overscrollElasticityLayer && !m_pageScaleLayer &&
           !m_innerViewportContainerLayer);

    // FIXME: The root transform layer should only be created on demand.
    m_rootTransformLayer = GraphicsLayer::create(this);
    m_innerViewportContainerLayer = GraphicsLayer::create(this);
    m_overscrollElasticityLayer = GraphicsLayer::create(this);
    m_pageScaleLayer = GraphicsLayer::create(this);
    m_innerViewportScrollLayer = GraphicsLayer::create(this);
    m_overlayScrollbarHorizontal = GraphicsLayer::create(this);
    m_overlayScrollbarVertical = GraphicsLayer::create(this);

    ScrollingCoordinator* coordinator =
        frameHost().page().scrollingCoordinator();
    ASSERT(coordinator);
    coordinator->setLayerIsContainerForFixedPositionLayers(
        m_innerViewportScrollLayer.get(), true);

    // Set masks to bounds so the compositor doesn't clobber a manually
    // set inner viewport container layer size.
    m_innerViewportContainerLayer->setMasksToBounds(
        frameHost().settings().mainFrameClipsContent());
    m_innerViewportContainerLayer->setSize(FloatSize(m_size));

    m_innerViewportScrollLayer->platformLayer()->setScrollClipLayer(
        m_innerViewportContainerLayer->platformLayer());
    m_innerViewportScrollLayer->platformLayer()->setUserScrollable(true, true);
    if (mainFrame()) {
      if (Document* document = mainFrame()->document()) {
        m_innerViewportScrollLayer->setElementId(createCompositorElementId(
            DOMNodeIds::idForNode(document), CompositorSubElementId::Viewport));
      }
    }

    m_rootTransformLayer->addChild(m_innerViewportContainerLayer.get());
    m_innerViewportContainerLayer->addChild(m_overscrollElasticityLayer.get());
    m_overscrollElasticityLayer->addChild(m_pageScaleLayer.get());
    m_pageScaleLayer->addChild(m_innerViewportScrollLayer.get());

    // Ensure this class is set as the scroll layer's ScrollableArea.
    coordinator->scrollableAreaScrollLayerDidChange(this);

    initializeScrollbars();
  }

  m_innerViewportScrollLayer->removeAllChildren();
  m_innerViewportScrollLayer->addChild(currentLayerTreeRoot);
}

void VisualViewport::initializeScrollbars() {
  // Do nothing if not attached to layer tree yet - will initialize upon attach.
  if (!m_innerViewportContainerLayer)
    return;

  if (visualViewportSuppliesScrollbars() &&
      !frameHost().settings().hideScrollbars()) {
    if (!m_overlayScrollbarHorizontal->parent())
      m_innerViewportContainerLayer->addChild(
          m_overlayScrollbarHorizontal.get());
    if (!m_overlayScrollbarVertical->parent())
      m_innerViewportContainerLayer->addChild(m_overlayScrollbarVertical.get());
  } else {
    m_overlayScrollbarHorizontal->removeFromParent();
    m_overlayScrollbarVertical->removeFromParent();
  }

  setupScrollbar(WebScrollbar::Horizontal);
  setupScrollbar(WebScrollbar::Vertical);
}

void VisualViewport::setupScrollbar(WebScrollbar::Orientation orientation) {
  bool isHorizontal = orientation == WebScrollbar::Horizontal;
  GraphicsLayer* scrollbarGraphicsLayer =
      isHorizontal ? m_overlayScrollbarHorizontal.get()
                   : m_overlayScrollbarVertical.get();
  std::unique_ptr<WebScrollbarLayer>& webScrollbarLayer =
      isHorizontal ? m_webOverlayScrollbarHorizontal
                   : m_webOverlayScrollbarVertical;

  ScrollbarThemeOverlay& theme = ScrollbarThemeOverlay::mobileTheme();
  int thumbThickness = theme.thumbThickness();
  int scrollbarThickness = theme.scrollbarThickness(RegularScrollbar);
  int scrollbarMargin = theme.scrollbarMargin();

  if (!webScrollbarLayer) {
    ScrollingCoordinator* coordinator =
        frameHost().page().scrollingCoordinator();
    ASSERT(coordinator);
    ScrollbarOrientation webcoreOrientation =
        isHorizontal ? HorizontalScrollbar : VerticalScrollbar;
    webScrollbarLayer = coordinator->createSolidColorScrollbarLayer(
        webcoreOrientation, thumbThickness, scrollbarMargin, false);

    // The compositor will control the scrollbar's visibility. Set to invisible
    // by default so scrollbars don't show up in layout tests.
    webScrollbarLayer->layer()->setOpacity(0);
    scrollbarGraphicsLayer->setContentsToPlatformLayer(
        webScrollbarLayer->layer());
    scrollbarGraphicsLayer->setDrawsContent(false);
  }

  int xPosition = isHorizontal ? 0
                               : m_innerViewportContainerLayer->size().width() -
                                     scrollbarThickness;
  int yPosition =
      isHorizontal
          ? m_innerViewportContainerLayer->size().height() - scrollbarThickness
          : 0;
  int width =
      isHorizontal
          ? m_innerViewportContainerLayer->size().width() - scrollbarThickness
          : scrollbarThickness;
  int height = isHorizontal ? scrollbarThickness
                            : m_innerViewportContainerLayer->size().height() -
                                  scrollbarThickness;

  // Use the GraphicsLayer to position the scrollbars.
  scrollbarGraphicsLayer->setPosition(IntPoint(xPosition, yPosition));
  scrollbarGraphicsLayer->setSize(FloatSize(width, height));
  scrollbarGraphicsLayer->setContentsRect(IntRect(0, 0, width, height));
}

void VisualViewport::setScrollLayerOnScrollbars(WebLayer* scrollLayer) const {
  // TODO(bokan): This is currently done while registering viewport layers
  // with the compositor but could it actually be done earlier, like in
  // setupScrollbars? Then we wouldn't need this method.
  m_webOverlayScrollbarHorizontal->setScrollLayer(scrollLayer);
  m_webOverlayScrollbarVertical->setScrollLayer(scrollLayer);
}

bool VisualViewport::visualViewportSuppliesScrollbars() const {
  return frameHost().settings().viewportEnabled();
}

bool VisualViewport::scrollAnimatorEnabled() const {
  return frameHost().settings().scrollAnimatorEnabled();
}

HostWindow* VisualViewport::getHostWindow() const {
  return &frameHost().chromeClient();
}

bool VisualViewport::shouldUseIntegerScrollOffset() const {
  LocalFrame* frame = mainFrame();
  if (frame && frame->settings() &&
      !frame->settings()->preferCompositingToLCDTextEnabled())
    return true;

  return ScrollableArea::shouldUseIntegerScrollOffset();
}

void VisualViewport::setScrollOffset(const ScrollOffset& offset,
                                     ScrollType scrollType,
                                     ScrollBehavior scrollBehavior) {
  // We clamp the offset here, because the ScrollAnimator may otherwise be
  // set to a non-clamped offset by ScrollableArea::setScrollOffset,
  // which may lead to incorrect scrolling behavior in RootFrameViewport down
  // the line.
  // TODO(eseckler): Solve this instead by ensuring that ScrollableArea and
  // ScrollAnimator are kept in sync. This requires that ScrollableArea always
  // stores fractional offsets and that truncation happens elsewhere, see
  // crbug.com/626315.
  ScrollOffset newScrollOffset = clampScrollOffset(offset);
  ScrollableArea::setScrollOffset(newScrollOffset, scrollType, scrollBehavior);
}

int VisualViewport::scrollSize(ScrollbarOrientation orientation) const {
  IntSize scrollDimensions =
      maximumScrollOffsetInt() - minimumScrollOffsetInt();
  return (orientation == HorizontalScrollbar) ? scrollDimensions.width()
                                              : scrollDimensions.height();
}

IntSize VisualViewport::minimumScrollOffsetInt() const {
  return IntSize();
}

IntSize VisualViewport::maximumScrollOffsetInt() const {
  return flooredIntSize(maximumScrollOffset());
}

ScrollOffset VisualViewport::maximumScrollOffset() const {
  if (!mainFrame())
    return ScrollOffset();

  // TODO(bokan): We probably shouldn't be storing the bounds in a float.
  // crbug.com/470718.
  FloatSize frameViewSize(contentsSize());

  if (m_browserControlsAdjustment) {
    float minScale =
        frameHost().pageScaleConstraintsSet().finalConstraints().minimumScale;
    frameViewSize.expand(0, m_browserControlsAdjustment / minScale);
  }

  frameViewSize.scale(m_scale);
  frameViewSize = FloatSize(flooredIntSize(frameViewSize));

  FloatSize viewportSize(m_size);
  viewportSize.expand(0, ceilf(m_browserControlsAdjustment));

  FloatSize maxPosition = frameViewSize - viewportSize;
  maxPosition.scale(1 / m_scale);
  return ScrollOffset(maxPosition);
}

IntPoint VisualViewport::clampDocumentOffsetAtScale(const IntPoint& offset,
                                                    float scale) {
  if (!mainFrame() || !mainFrame()->view())
    return IntPoint();

  FrameView* view = mainFrame()->view();

  FloatSize scaledSize(m_size);
  scaledSize.scale(1 / scale);

  IntSize visualViewportMax =
      flooredIntSize(FloatSize(contentsSize()) - scaledSize);
  IntSize max = view->maximumScrollOffsetInt() + visualViewportMax;
  IntSize min =
      view->minimumScrollOffsetInt();  // VisualViewportMin should be (0, 0)

  IntSize clamped = toIntSize(offset);
  clamped = clamped.shrunkTo(max);
  clamped = clamped.expandedTo(min);
  return IntPoint(clamped);
}

void VisualViewport::setBrowserControlsAdjustment(float adjustment) {
  m_browserControlsAdjustment = adjustment;
}

IntRect VisualViewport::scrollableAreaBoundingBox() const {
  // This method should return the bounding box in the parent view's coordinate
  // space; however, VisualViewport technically isn't a child of any Frames.
  // Nonetheless, the VisualViewport always occupies the entire main frame so
  // just return that.
  LocalFrame* frame = mainFrame();

  if (!frame || !frame->view())
    return IntRect();

  return frame->view()->frameRect();
}

IntSize VisualViewport::contentsSize() const {
  LocalFrame* frame = mainFrame();

  if (!frame || !frame->view())
    return IntSize();

  return frame->view()->visibleContentRect(IncludeScrollbars).size();
}

IntRect VisualViewport::visibleContentRect(
    IncludeScrollbarsInRect scrollbarInclusion) const {
  // TODO(ymalik): We're losing precision here and below. visibleRect should
  // be replaced with visibleContentRect.
  IntRect rect = IntRect(visibleRect());
  if (scrollbarInclusion == ExcludeScrollbars) {
    RootFrameViewport* rootFrameViewport =
        mainFrame()->view()->getRootFrameViewport();
    DCHECK(rootFrameViewport);
    rect.contract(rootFrameViewport->verticalScrollbarWidth() / m_scale,
                  rootFrameViewport->horizontalScrollbarHeight() / m_scale);
  }
  return rect;
}

void VisualViewport::updateScrollOffset(const ScrollOffset& position,
                                        ScrollType scrollType) {
  if (didSetScaleOrLocation(m_scale, FloatPoint(position)) &&
      scrollType != AnchoringScroll)
    notifyRootFrameViewport();
}

GraphicsLayer* VisualViewport::layerForContainer() const {
  return m_innerViewportContainerLayer.get();
}

GraphicsLayer* VisualViewport::layerForScrolling() const {
  return m_innerViewportScrollLayer.get();
}

GraphicsLayer* VisualViewport::layerForHorizontalScrollbar() const {
  return m_overlayScrollbarHorizontal.get();
}

GraphicsLayer* VisualViewport::layerForVerticalScrollbar() const {
  return m_overlayScrollbarVertical.get();
}

IntRect VisualViewport::computeInterestRect(const GraphicsLayer*,
                                            const IntRect&) const {
  return IntRect();
}

void VisualViewport::paintContents(const GraphicsLayer*,
                                   GraphicsContext&,
                                   GraphicsLayerPaintingPhase,
                                   const IntRect&) const {}

LocalFrame* VisualViewport::mainFrame() const {
  return frameHost().page().mainFrame() &&
                 frameHost().page().mainFrame()->isLocalFrame()
             ? frameHost().page().deprecatedLocalMainFrame()
             : 0;
}

Widget* VisualViewport::getWidget() {
  return mainFrame()->view();
}

void VisualViewport::clampToBoundaries() {
  setLocation(FloatPoint(m_offset));
}

FloatRect VisualViewport::viewportToRootFrame(
    const FloatRect& rectInViewport) const {
  FloatRect rectInRootFrame = rectInViewport;
  rectInRootFrame.scale(1 / scale());
  rectInRootFrame.move(scrollOffset());
  return rectInRootFrame;
}

IntRect VisualViewport::viewportToRootFrame(
    const IntRect& rectInViewport) const {
  // FIXME: How to snap to pixels?
  return enclosingIntRect(viewportToRootFrame(FloatRect(rectInViewport)));
}

FloatRect VisualViewport::rootFrameToViewport(
    const FloatRect& rectInRootFrame) const {
  FloatRect rectInViewport = rectInRootFrame;
  rectInViewport.move(-scrollOffset());
  rectInViewport.scale(scale());
  return rectInViewport;
}

IntRect VisualViewport::rootFrameToViewport(
    const IntRect& rectInRootFrame) const {
  // FIXME: How to snap to pixels?
  return enclosingIntRect(rootFrameToViewport(FloatRect(rectInRootFrame)));
}

FloatPoint VisualViewport::viewportToRootFrame(
    const FloatPoint& pointInViewport) const {
  FloatPoint pointInRootFrame = pointInViewport;
  pointInRootFrame.scale(1 / scale(), 1 / scale());
  pointInRootFrame.move(scrollOffset());
  return pointInRootFrame;
}

FloatPoint VisualViewport::rootFrameToViewport(
    const FloatPoint& pointInRootFrame) const {
  FloatPoint pointInViewport = pointInRootFrame;
  pointInViewport.move(-scrollOffset());
  pointInViewport.scale(scale(), scale());
  return pointInViewport;
}

IntPoint VisualViewport::viewportToRootFrame(
    const IntPoint& pointInViewport) const {
  // FIXME: How to snap to pixels?
  return flooredIntPoint(
      FloatPoint(viewportToRootFrame(FloatPoint(pointInViewport))));
}

IntPoint VisualViewport::rootFrameToViewport(
    const IntPoint& pointInRootFrame) const {
  // FIXME: How to snap to pixels?
  return flooredIntPoint(
      FloatPoint(rootFrameToViewport(FloatPoint(pointInRootFrame))));
}

void VisualViewport::startTrackingPinchStats() {
  if (!mainFrame())
    return;

  Document* document = mainFrame()->document();
  if (!document)
    return;

  if (!document->url().protocolIsInHTTPFamily())
    return;

  m_trackPinchZoomStatsForPage = !shouldDisableDesktopWorkarounds();
}

void VisualViewport::userDidChangeScale() {
  if (!m_trackPinchZoomStatsForPage)
    return;

  m_maxPageScale = std::max(m_maxPageScale, m_scale);
}

void VisualViewport::sendUMAMetrics() {
  if (m_trackPinchZoomStatsForPage) {
    bool didScale = m_maxPageScale > 0;

    DEFINE_STATIC_LOCAL(EnumerationHistogram, didScaleHistogram,
                        ("Viewport.DidScalePage", 2));
    didScaleHistogram.count(didScale ? 1 : 0);

    if (didScale) {
      int zoomPercentage = floor(m_maxPageScale * 100);

      // See the PageScaleFactor enumeration in histograms.xml for the bucket
      // ranges.
      int bucket = floor(zoomPercentage / 25.f);

      DEFINE_STATIC_LOCAL(EnumerationHistogram, maxScaleHistogram,
                          ("Viewport.MaxPageScale", 21));
      maxScaleHistogram.count(bucket);
    }
  }

  m_maxPageScale = -1;
  m_trackPinchZoomStatsForPage = false;
}

bool VisualViewport::shouldDisableDesktopWorkarounds() const {
  if (!mainFrame() || !mainFrame()->view())
    return false;

  if (!mainFrame()->settings()->viewportEnabled())
    return false;

  // A document is considered adapted to small screen UAs if one of these holds:
  // 1. The author specified viewport has a constrained width that is equal to
  //    the initial viewport width.
  // 2. The author has disabled viewport zoom.
  const PageScaleConstraints& constraints =
      frameHost().pageScaleConstraintsSet().pageDefinedConstraints();

  return mainFrame()->view()->layoutSize().width() == m_size.width() ||
         (constraints.minimumScale == constraints.maximumScale &&
          constraints.minimumScale != -1);
}

CompositorAnimationTimeline* VisualViewport::compositorAnimationTimeline()
    const {
  ScrollingCoordinator* c = frameHost().page().scrollingCoordinator();
  return c ? c->compositorAnimationTimeline() : nullptr;
}

void VisualViewport::notifyRootFrameViewport() const {
  if (!mainFrame() || !mainFrame()->view())
    return;

  RootFrameViewport* rootFrameViewport =
      mainFrame()->view()->getRootFrameViewport();

  if (!rootFrameViewport)
    return;

  rootFrameViewport->didUpdateVisualViewport();
}

String VisualViewport::debugName(const GraphicsLayer* graphicsLayer) const {
  String name;
  if (graphicsLayer == m_innerViewportContainerLayer.get()) {
    name = "Inner Viewport Container Layer";
  } else if (graphicsLayer == m_overscrollElasticityLayer.get()) {
    name = "Overscroll Elasticity Layer";
  } else if (graphicsLayer == m_pageScaleLayer.get()) {
    name = "Page Scale Layer";
  } else if (graphicsLayer == m_innerViewportScrollLayer.get()) {
    name = "Inner Viewport Scroll Layer";
  } else if (graphicsLayer == m_overlayScrollbarHorizontal.get()) {
    name = "Overlay Scrollbar Horizontal Layer";
  } else if (graphicsLayer == m_overlayScrollbarVertical.get()) {
    name = "Overlay Scrollbar Vertical Layer";
  } else if (graphicsLayer == m_rootTransformLayer.get()) {
    name = "Root Transform Layer";
  } else {
    ASSERT_NOT_REACHED();
  }

  return name;
}

}  // namespace blink
