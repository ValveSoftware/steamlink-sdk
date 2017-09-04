// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/RootFrameViewport.h"

#include "core/frame/FrameView.h"
#include "core/layout/ScrollAlignment.h"
#include "core/layout/ScrollAnchor.h"
#include "platform/geometry/DoubleRect.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/LayoutRect.h"

namespace blink {

RootFrameViewport::RootFrameViewport(ScrollableArea& visualViewport,
                                     ScrollableArea& layoutViewport)
    : m_visualViewport(visualViewport) {
  setLayoutViewport(layoutViewport);
}

void RootFrameViewport::setLayoutViewport(ScrollableArea& newLayoutViewport) {
  if (m_layoutViewport.get() == &newLayoutViewport)
    return;

  if (m_layoutViewport && m_layoutViewport->scrollAnchor())
    m_layoutViewport->scrollAnchor()->setScroller(m_layoutViewport.get());

  m_layoutViewport = &newLayoutViewport;

  if (m_layoutViewport->scrollAnchor())
    m_layoutViewport->scrollAnchor()->setScroller(this);
}

ScrollableArea& RootFrameViewport::layoutViewport() const {
  DCHECK(m_layoutViewport);
  return *m_layoutViewport;
}

LayoutRect RootFrameViewport::rootContentsToLayoutViewportContents(
    FrameView& rootFrameView,
    const LayoutRect& rect) const {
  LayoutRect ret(rect);

  // If the root FrameView is the layout viewport then coordinates in the
  // root FrameView's content space are already in the layout viewport's
  // content space.
  if (rootFrameView.layoutViewportScrollableArea() == &layoutViewport())
    return ret;

  // Make the given rect relative to the top of the layout viewport's content
  // by adding the scroll position.
  // TODO(bokan): This will have to be revisited if we ever remove the
  // restriction that a root scroller must be exactly screen filling.
  ret.move(LayoutSize(layoutViewport().scrollOffset()));

  return ret;
}

void RootFrameViewport::restoreToAnchor(const ScrollOffset& targetOffset) {
  // Clamp the scroll offset of each viewport now so that we force any invalid
  // offsets to become valid so we can compute the correct deltas.
  visualViewport().setScrollOffset(visualViewport().scrollOffset(),
                                   ProgrammaticScroll);
  layoutViewport().setScrollOffset(layoutViewport().scrollOffset(),
                                   ProgrammaticScroll);

  ScrollOffset delta = targetOffset - scrollOffset();

  visualViewport().setScrollOffset(visualViewport().scrollOffset() + delta,
                                   ProgrammaticScroll);

  delta = targetOffset - scrollOffset();

  // Since the main thread FrameView has integer scroll offsets, scroll it to
  // the next pixel and then we'll scroll the visual viewport again to
  // compensate for the sub-pixel offset. We need this "overscroll" to ensure
  // the pixel of which we want to be partially in appears fully inside the
  // FrameView since the VisualViewport is bounded by the FrameView.
  IntSize layoutDelta = IntSize(
      delta.width() < 0 ? floor(delta.width()) : ceil(delta.width()),
      delta.height() < 0 ? floor(delta.height()) : ceil(delta.height()));

  layoutViewport().setScrollOffset(
      ScrollOffset(layoutViewport().scrollOffsetInt() + layoutDelta),
      ProgrammaticScroll);

  delta = targetOffset - scrollOffset();
  visualViewport().setScrollOffset(visualViewport().scrollOffset() + delta,
                                   ProgrammaticScroll);
}

void RootFrameViewport::didUpdateVisualViewport() {
  if (RuntimeEnabledFeatures::scrollAnchoringEnabled()) {
    if (ScrollAnchor* anchor = layoutViewport().scrollAnchor())
      anchor->clear();
  }
}

LayoutBox* RootFrameViewport::layoutBox() const {
  return layoutViewport().layoutBox();
}

FloatQuad RootFrameViewport::localToVisibleContentQuad(
    const FloatQuad& quad,
    const LayoutObject* localObject,
    unsigned flags) const {
  if (!m_layoutViewport)
    return quad;
  FloatQuad viewportQuad =
      m_layoutViewport->localToVisibleContentQuad(quad, localObject, flags);
  if (m_visualViewport) {
    viewportQuad = m_visualViewport->localToVisibleContentQuad(
        viewportQuad, localObject, flags);
  }
  return viewportQuad;
}

int RootFrameViewport::horizontalScrollbarHeight(
    OverlayScrollbarClipBehavior behavior) const {
  return layoutViewport().horizontalScrollbarHeight(behavior);
}

int RootFrameViewport::verticalScrollbarWidth(
    OverlayScrollbarClipBehavior behavior) const {
  return layoutViewport().verticalScrollbarWidth(behavior);
}

void RootFrameViewport::updateScrollAnimator() {
  scrollAnimator().setCurrentOffset(
      toFloatSize(scrollOffsetFromScrollAnimators()));
}

ScrollOffset RootFrameViewport::scrollOffsetFromScrollAnimators() const {
  return visualViewport().scrollAnimator().currentOffset() +
         layoutViewport().scrollAnimator().currentOffset();
}

IntRect RootFrameViewport::visibleContentRect(
    IncludeScrollbarsInRect scrollbarInclusion) const {
  return IntRect(
      IntPoint(scrollOffsetInt()),
      visualViewport().visibleContentRect(scrollbarInclusion).size());
}

bool RootFrameViewport::shouldUseIntegerScrollOffset() const {
  // Fractionals are floored in the ScrollAnimatorBase but it's important that
  // the ScrollAnimators of the visual and layout viewports get the precise
  // fractional number so never use integer scrolling for RootFrameViewport,
  // we'll let the truncation happen in the subviewports.
  return false;
}

bool RootFrameViewport::isActive() const {
  return layoutViewport().isActive();
}

int RootFrameViewport::scrollSize(ScrollbarOrientation orientation) const {
  IntSize scrollDimensions =
      maximumScrollOffsetInt() - minimumScrollOffsetInt();
  return (orientation == HorizontalScrollbar) ? scrollDimensions.width()
                                              : scrollDimensions.height();
}

bool RootFrameViewport::isScrollCornerVisible() const {
  return layoutViewport().isScrollCornerVisible();
}

IntRect RootFrameViewport::scrollCornerRect() const {
  return layoutViewport().scrollCornerRect();
}

void RootFrameViewport::setScrollOffset(const ScrollOffset& offset,
                                        ScrollType scrollType,
                                        ScrollBehavior scrollBehavior) {
  updateScrollAnimator();

  if (scrollBehavior == ScrollBehaviorAuto)
    scrollBehavior = scrollBehaviorStyle();

  if (scrollType == ProgrammaticScroll &&
      !layoutViewport().isProgrammaticallyScrollable())
    return;

  if (scrollType == AnchoringScroll) {
    distributeScrollBetweenViewports(offset, scrollType, scrollBehavior,
                                     LayoutViewport);
    return;
  }

  if (scrollBehavior == ScrollBehaviorSmooth) {
    distributeScrollBetweenViewports(offset, scrollType, scrollBehavior,
                                     VisualViewport);
    return;
  }

  ScrollOffset clampedOffset = clampScrollOffset(offset);
  ScrollableArea::setScrollOffset(clampedOffset, scrollType, scrollBehavior);
}

ScrollBehavior RootFrameViewport::scrollBehaviorStyle() const {
  return layoutViewport().scrollBehaviorStyle();
}

LayoutRect RootFrameViewport::scrollIntoView(const LayoutRect& rectInContent,
                                             const ScrollAlignment& alignX,
                                             const ScrollAlignment& alignY,
                                             ScrollType scrollType) {
  // We want to move the rect into the viewport that excludes the scrollbars so
  // we intersect the visual viewport with the scrollbar-excluded frameView
  // content rect. However, we don't use visibleContentRect directly since it
  // floors the scroll offset. Instead, we use ScrollAnimatorBase::currentOffset
  // and construct a LayoutRect from that.
  LayoutRect frameRectInContent =
      LayoutRect(FloatPoint(layoutViewport().scrollAnimator().currentOffset()),
                 FloatSize(layoutViewport().visibleContentRect().size()));
  LayoutRect visualRectInContent =
      LayoutRect(FloatPoint(scrollOffsetFromScrollAnimators()),
                 FloatSize(visualViewport().visibleContentRect().size()));

  // Intersect layout and visual rects to exclude the scrollbar from the view
  // rect.
  LayoutRect viewRectInContent =
      intersection(visualRectInContent, frameRectInContent);
  LayoutRect targetViewport = ScrollAlignment::getRectToExpose(
      viewRectInContent, rectInContent, alignX, alignY);
  if (targetViewport != viewRectInContent) {
    setScrollOffset(ScrollOffset(targetViewport.x(), targetViewport.y()),
                    scrollType);
  }

  // RootFrameViewport only changes the viewport relative to the document so we
  // can't change the input rect's location relative to the document origin.
  return rectInContent;
}

void RootFrameViewport::updateScrollOffset(const ScrollOffset& offset,
                                           ScrollType scrollType) {
  distributeScrollBetweenViewports(offset, scrollType, ScrollBehaviorInstant,
                                   VisualViewport);
}

void RootFrameViewport::distributeScrollBetweenViewports(
    const ScrollOffset& offset,
    ScrollType scrollType,
    ScrollBehavior behavior,
    ViewportToScrollFirst scrollFirst) {
  // Make sure we use the scroll offsets as reported by each viewport's
  // ScrollAnimatorBase, since its ScrollableArea's offset may have the
  // fractional part truncated off.
  // TODO(szager): Now that scroll offsets are stored as floats, can we take the
  // scroll offset directly from the ScrollableArea's rather than the animators?
  ScrollOffset oldOffset = scrollOffsetFromScrollAnimators();

  ScrollOffset delta = offset - oldOffset;

  if (delta.isZero())
    return;

  ScrollableArea& primary =
      scrollFirst == VisualViewport ? visualViewport() : layoutViewport();
  ScrollableArea& secondary =
      scrollFirst == VisualViewport ? layoutViewport() : visualViewport();

  ScrollOffset targetOffset = primary.clampScrollOffset(
      primary.scrollAnimator().currentOffset() + delta);

  primary.setScrollOffset(targetOffset, scrollType, behavior);

  // Scroll the secondary viewport if all of the scroll was not applied to the
  // primary viewport.
  ScrollOffset updatedOffset =
      secondary.scrollAnimator().currentOffset() + FloatSize(targetOffset);
  ScrollOffset applied = updatedOffset - oldOffset;
  delta -= applied;

  if (delta.isZero())
    return;

  targetOffset = secondary.clampScrollOffset(
      secondary.scrollAnimator().currentOffset() + delta);
  secondary.setScrollOffset(targetOffset, scrollType, behavior);
}

IntSize RootFrameViewport::scrollOffsetInt() const {
  return flooredIntSize(scrollOffset());
}

ScrollOffset RootFrameViewport::scrollOffset() const {
  return layoutViewport().scrollOffset() + visualViewport().scrollOffset();
}

IntSize RootFrameViewport::minimumScrollOffsetInt() const {
  return IntSize(layoutViewport().minimumScrollOffsetInt() +
                 visualViewport().minimumScrollOffsetInt());
}

IntSize RootFrameViewport::maximumScrollOffsetInt() const {
  return layoutViewport().maximumScrollOffsetInt() +
         visualViewport().maximumScrollOffsetInt();
}

ScrollOffset RootFrameViewport::maximumScrollOffset() const {
  return layoutViewport().maximumScrollOffset() +
         visualViewport().maximumScrollOffset();
}

IntSize RootFrameViewport::contentsSize() const {
  return layoutViewport().contentsSize();
}

bool RootFrameViewport::scrollbarsCanBeActive() const {
  return layoutViewport().scrollbarsCanBeActive();
}

IntRect RootFrameViewport::scrollableAreaBoundingBox() const {
  return layoutViewport().scrollableAreaBoundingBox();
}

bool RootFrameViewport::userInputScrollable(
    ScrollbarOrientation orientation) const {
  return visualViewport().userInputScrollable(orientation) ||
         layoutViewport().userInputScrollable(orientation);
}

bool RootFrameViewport::shouldPlaceVerticalScrollbarOnLeft() const {
  return layoutViewport().shouldPlaceVerticalScrollbarOnLeft();
}

void RootFrameViewport::scrollControlWasSetNeedsPaintInvalidation() {
  layoutViewport().scrollControlWasSetNeedsPaintInvalidation();
}

GraphicsLayer* RootFrameViewport::layerForContainer() const {
  return layoutViewport().layerForContainer();
}

GraphicsLayer* RootFrameViewport::layerForScrolling() const {
  return layoutViewport().layerForScrolling();
}

GraphicsLayer* RootFrameViewport::layerForHorizontalScrollbar() const {
  return layoutViewport().layerForHorizontalScrollbar();
}

GraphicsLayer* RootFrameViewport::layerForVerticalScrollbar() const {
  return layoutViewport().layerForVerticalScrollbar();
}

GraphicsLayer* RootFrameViewport::layerForScrollCorner() const {
  return layoutViewport().layerForScrollCorner();
}

ScrollResult RootFrameViewport::userScroll(ScrollGranularity granularity,
                                           const FloatSize& delta) {
  // TODO(bokan/ymalik): Once smooth scrolling is permanently enabled we
  // should be able to remove this method override and use the base class
  // version: ScrollableArea::userScroll.

  updateScrollAnimator();

  // Distribute the scroll between the visual and layout viewport.

  float stepX = scrollStep(granularity, HorizontalScrollbar);
  float stepY = scrollStep(granularity, VerticalScrollbar);

  FloatSize pixelDelta(delta);
  pixelDelta.scale(stepX, stepY);

  // Precompute the amount of possible scrolling since, when animated,
  // ScrollAnimator::userScroll will report having consumed the total given
  // scroll delta, regardless of how much will actually scroll, but we need to
  // know how much to leave for the layout viewport.
  FloatSize visualConsumedDelta =
      visualViewport().scrollAnimator().computeDeltaToConsume(pixelDelta);

  // Split the remaining delta between scrollable and unscrollable axes of the
  // layout viewport. We only pass a delta to the scrollable axes and remember
  // how much was held back so we can add it to the unused delta in the
  // result.
  FloatSize layoutDelta = pixelDelta - visualConsumedDelta;
  FloatSize scrollableAxisDelta(
      layoutViewport().userInputScrollable(HorizontalScrollbar)
          ? layoutDelta.width()
          : 0,
      layoutViewport().userInputScrollable(VerticalScrollbar)
          ? layoutDelta.height()
          : 0);

  // If there won't be any scrolling, bail early so we don't produce any side
  // effects like cancelling existing animations.
  if (visualConsumedDelta.isZero() && scrollableAxisDelta.isZero()) {
    return ScrollResult(false, false, pixelDelta.width(), pixelDelta.height());
  }

  cancelProgrammaticScrollAnimation();

  // TODO(bokan): Why do we call userScroll on the animators directly and
  // not through the ScrollableAreas?
  ScrollResult visualResult = visualViewport().scrollAnimator().userScroll(
      granularity, visualConsumedDelta);

  if (visualConsumedDelta == pixelDelta)
    return visualResult;

  ScrollResult layoutResult = layoutViewport().scrollAnimator().userScroll(
      granularity, scrollableAxisDelta);

  // Remember to add any delta not used because of !userInputScrollable to the
  // unusedScrollDelta in the result.
  FloatSize unscrollableAxisDelta = layoutDelta - scrollableAxisDelta;

  return ScrollResult(
      visualResult.didScrollX || layoutResult.didScrollX,
      visualResult.didScrollY || layoutResult.didScrollY,
      layoutResult.unusedScrollDeltaX + unscrollableAxisDelta.width(),
      layoutResult.unusedScrollDeltaY + unscrollableAxisDelta.height());
}

bool RootFrameViewport::scrollAnimatorEnabled() const {
  return layoutViewport().scrollAnimatorEnabled();
}

HostWindow* RootFrameViewport::getHostWindow() const {
  return layoutViewport().getHostWindow();
}

void RootFrameViewport::serviceScrollAnimations(double monotonicTime) {
  ScrollableArea::serviceScrollAnimations(monotonicTime);
  layoutViewport().serviceScrollAnimations(monotonicTime);
  visualViewport().serviceScrollAnimations(monotonicTime);
}

void RootFrameViewport::updateCompositorScrollAnimations() {
  ScrollableArea::updateCompositorScrollAnimations();
  layoutViewport().updateCompositorScrollAnimations();
  visualViewport().updateCompositorScrollAnimations();
}

void RootFrameViewport::cancelProgrammaticScrollAnimation() {
  ScrollableArea::cancelProgrammaticScrollAnimation();
  layoutViewport().cancelProgrammaticScrollAnimation();
  visualViewport().cancelProgrammaticScrollAnimation();
}

Widget* RootFrameViewport::getWidget() {
  return visualViewport().getWidget();
}

void RootFrameViewport::clearScrollableArea() {
  ScrollableArea::clearScrollableArea();
  layoutViewport().clearScrollableArea();
  visualViewport().clearScrollableArea();
}

DEFINE_TRACE(RootFrameViewport) {
  visitor->trace(m_visualViewport);
  visitor->trace(m_layoutViewport);
  ScrollableArea::trace(visitor);
}

}  // namespace blink
