/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
 * Copyright (C) 2008, 2011 Apple Inc. All Rights Reserved.
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

#include "platform/scroll/ScrollableArea.h"

#include "platform/HostWindow.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/scroll/MainThreadScrollingReason.h"
#include "platform/scroll/ProgrammaticScrollAnimator.h"
#include "platform/scroll/ScrollbarTheme.h"

#include "platform/tracing/TraceEvent.h"

static const int kPixelsPerLineStep = 40;
static const float kMinFractionToStepWhenPaging = 0.875f;

namespace blink {

int ScrollableArea::pixelsPerLineStep(HostWindow* host) {
  if (!host)
    return kPixelsPerLineStep;
  return host->windowToViewportScalar(kPixelsPerLineStep);
}

float ScrollableArea::minFractionToStepWhenPaging() {
  return kMinFractionToStepWhenPaging;
}

int ScrollableArea::maxOverlapBetweenPages() {
  static int maxOverlapBetweenPages =
      ScrollbarTheme::theme().maxOverlapBetweenPages();
  return maxOverlapBetweenPages;
}

ScrollableArea::ScrollableArea()
    : m_scrollbarOverlayColorTheme(ScrollbarOverlayColorThemeDark),
      m_scrollOriginChanged(false),
      m_horizontalScrollbarNeedsPaintInvalidation(false),
      m_verticalScrollbarNeedsPaintInvalidation(false),
      m_scrollCornerNeedsPaintInvalidation(false),
      m_scrollbarsHidden(false),
      m_scrollbarCaptured(false) {}

ScrollableArea::~ScrollableArea() {}

void ScrollableArea::clearScrollableArea() {
#if OS(MACOSX)
  if (m_scrollAnimator)
    m_scrollAnimator->dispose();
#endif
  m_scrollAnimator.clear();
  m_programmaticScrollAnimator.clear();
  if (m_fadeOverlayScrollbarsTimer)
    m_fadeOverlayScrollbarsTimer->stop();
}

ScrollAnimatorBase& ScrollableArea::scrollAnimator() const {
  if (!m_scrollAnimator)
    m_scrollAnimator =
        ScrollAnimatorBase::create(const_cast<ScrollableArea*>(this));

  return *m_scrollAnimator;
}

ProgrammaticScrollAnimator& ScrollableArea::programmaticScrollAnimator() const {
  if (!m_programmaticScrollAnimator)
    m_programmaticScrollAnimator =
        ProgrammaticScrollAnimator::create(const_cast<ScrollableArea*>(this));

  return *m_programmaticScrollAnimator;
}

void ScrollableArea::setScrollOrigin(const IntPoint& origin) {
  if (m_scrollOrigin != origin) {
    m_scrollOrigin = origin;
    m_scrollOriginChanged = true;
  }
}

GraphicsLayer* ScrollableArea::layerForContainer() const {
  return layerForScrolling() ? layerForScrolling()->parent() : 0;
}

ScrollbarOrientation ScrollableArea::scrollbarOrientationFromDirection(
    ScrollDirectionPhysical direction) const {
  return (direction == ScrollUp || direction == ScrollDown)
             ? VerticalScrollbar
             : HorizontalScrollbar;
}

float ScrollableArea::scrollStep(ScrollGranularity granularity,
                                 ScrollbarOrientation orientation) const {
  switch (granularity) {
    case ScrollByLine:
      return lineStep(orientation);
    case ScrollByPage:
      return pageStep(orientation);
    case ScrollByDocument:
      return documentStep(orientation);
    case ScrollByPixel:
    case ScrollByPrecisePixel:
      return pixelStep(orientation);
    default:
      ASSERT_NOT_REACHED();
      return 0.0f;
  }
}

ScrollResult ScrollableArea::userScroll(ScrollGranularity granularity,
                                        const ScrollOffset& delta) {
  float stepX = scrollStep(granularity, HorizontalScrollbar);
  float stepY = scrollStep(granularity, VerticalScrollbar);

  ScrollOffset pixelDelta(delta);
  pixelDelta.scale(stepX, stepY);

  ScrollOffset scrollableAxisDelta(
      userInputScrollable(HorizontalScrollbar) ? pixelDelta.width() : 0,
      userInputScrollable(VerticalScrollbar) ? pixelDelta.height() : 0);

  if (scrollableAxisDelta.isZero()) {
    return ScrollResult(false, false, pixelDelta.width(), pixelDelta.height());
  }

  cancelProgrammaticScrollAnimation();

  ScrollResult result = scrollAnimator().userScroll(granularity, pixelDelta);

  // Delta that wasn't scrolled because the axis is !userInputScrollable
  // should count as unusedScrollDelta.
  ScrollOffset unscrollableAxisDelta = pixelDelta - scrollableAxisDelta;
  result.unusedScrollDeltaX += unscrollableAxisDelta.width();
  result.unusedScrollDeltaY += unscrollableAxisDelta.height();

  return result;
}

void ScrollableArea::setScrollOffset(const ScrollOffset& offset,
                                     ScrollType scrollType,
                                     ScrollBehavior behavior) {
  ScrollOffset clampedOffset = clampScrollOffset(offset);
  if (clampedOffset == scrollOffset())
    return;

  if (behavior == ScrollBehaviorAuto)
    behavior = scrollBehaviorStyle();

  switch (scrollType) {
    case CompositorScroll:
    case ClampingScroll:
      scrollOffsetChanged(clampedOffset, scrollType);
      break;
    case AnchoringScroll:
      scrollAnimator().adjustAnimationAndSetScrollOffset(clampedOffset,
                                                         scrollType);
      break;
    case ProgrammaticScroll:
      programmaticScrollHelper(clampedOffset, behavior);
      break;
    case UserScroll:
      userScrollHelper(clampedOffset, behavior);
      break;
    default:
      ASSERT_NOT_REACHED();
  }
}

void ScrollableArea::scrollBy(const ScrollOffset& delta,
                              ScrollType type,
                              ScrollBehavior behavior) {
  setScrollOffset(scrollOffset() + delta, type, behavior);
}

void ScrollableArea::setScrollOffsetSingleAxis(ScrollbarOrientation orientation,
                                               float offset,
                                               ScrollType scrollType,
                                               ScrollBehavior behavior) {
  ScrollOffset newOffset;
  if (orientation == HorizontalScrollbar)
    newOffset = ScrollOffset(offset, scrollAnimator().currentOffset().height());
  else
    newOffset = ScrollOffset(scrollAnimator().currentOffset().width(), offset);

  // TODO(bokan): Note, this doesn't use the derived class versions since this
  // method is currently used exclusively by code that adjusts the position by
  // the scroll origin and the derived class versions differ on whether they
  // take that into account or not.
  ScrollableArea::setScrollOffset(newOffset, scrollType, behavior);
}

void ScrollableArea::programmaticScrollHelper(const ScrollOffset& offset,
                                              ScrollBehavior scrollBehavior) {
  cancelScrollAnimation();

  if (scrollBehavior == ScrollBehaviorSmooth)
    programmaticScrollAnimator().animateToOffset(offset);
  else
    programmaticScrollAnimator().scrollToOffsetWithoutAnimation(offset);
}

void ScrollableArea::userScrollHelper(const ScrollOffset& offset,
                                      ScrollBehavior scrollBehavior) {
  cancelProgrammaticScrollAnimation();

  float x = userInputScrollable(HorizontalScrollbar)
                ? offset.width()
                : scrollAnimator().currentOffset().width();
  float y = userInputScrollable(VerticalScrollbar)
                ? offset.height()
                : scrollAnimator().currentOffset().height();

  // Smooth user scrolls (keyboard, wheel clicks) are handled via the userScroll
  // method.
  // TODO(bokan): The userScroll method should probably be modified to call this
  //              method and ScrollAnimatorBase to have a simpler
  //              animateToOffset method like the ProgrammaticScrollAnimator.
  ASSERT(scrollBehavior == ScrollBehaviorInstant);
  scrollAnimator().scrollToOffsetWithoutAnimation(ScrollOffset(x, y));
}

LayoutRect ScrollableArea::scrollIntoView(const LayoutRect& rectInContent,
                                          const ScrollAlignment& alignX,
                                          const ScrollAlignment& alignY,
                                          ScrollType) {
  // TODO(bokan): This should really be implemented here but ScrollAlignment is
  // in Core which is a dependency violation.
  ASSERT_NOT_REACHED();
  return LayoutRect();
}

void ScrollableArea::scrollOffsetChanged(const ScrollOffset& offset,
                                         ScrollType scrollType) {
  TRACE_EVENT0("blink", "ScrollableArea::scrollOffsetChanged");

  ScrollOffset oldOffset = scrollOffset();
  ScrollOffset truncatedOffset = shouldUseIntegerScrollOffset()
                                     ? ScrollOffset(flooredIntSize(offset))
                                     : offset;

  // Tell the derived class to scroll its contents.
  updateScrollOffset(truncatedOffset, scrollType);

  // Tell the scrollbars to update their thumb postions.
  // If the scrollbar does not have its own layer, it must always be
  // invalidated to reflect the new thumb offset, even if the theme did not
  // invalidate any individual part.
  if (Scrollbar* horizontalScrollbar = this->horizontalScrollbar())
    horizontalScrollbar->offsetDidChange();
  if (Scrollbar* verticalScrollbar = this->verticalScrollbar())
    verticalScrollbar->offsetDidChange();

  if (scrollOffset() != oldOffset)
    scrollAnimator().notifyContentAreaScrolled(scrollOffset() - oldOffset);

  scrollAnimator().setCurrentOffset(offset);
}

bool ScrollableArea::scrollBehaviorFromString(const String& behaviorString,
                                              ScrollBehavior& behavior) {
  if (behaviorString == "auto")
    behavior = ScrollBehaviorAuto;
  else if (behaviorString == "instant")
    behavior = ScrollBehaviorInstant;
  else if (behaviorString == "smooth")
    behavior = ScrollBehaviorSmooth;
  else
    return false;

  return true;
}

// NOTE: Only called from Internals for testing.
void ScrollableArea::updateScrollOffsetFromInternals(const IntSize& offset) {
  scrollOffsetChanged(ScrollOffset(offset), ProgrammaticScroll);
}

void ScrollableArea::contentAreaWillPaint() const {
  if (ScrollAnimatorBase* scrollAnimator = existingScrollAnimator())
    scrollAnimator->contentAreaWillPaint();
}

void ScrollableArea::mouseEnteredContentArea() const {
  if (ScrollAnimatorBase* scrollAnimator = existingScrollAnimator())
    scrollAnimator->mouseEnteredContentArea();
}

void ScrollableArea::mouseExitedContentArea() const {
  if (ScrollAnimatorBase* scrollAnimator = existingScrollAnimator())
    scrollAnimator->mouseEnteredContentArea();
}

void ScrollableArea::mouseMovedInContentArea() const {
  if (ScrollAnimatorBase* scrollAnimator = existingScrollAnimator())
    scrollAnimator->mouseMovedInContentArea();
}

void ScrollableArea::mouseEnteredScrollbar(Scrollbar& scrollbar) {
  scrollAnimator().mouseEnteredScrollbar(scrollbar);
  // Restart the fade out timer.
  showOverlayScrollbars();
}

void ScrollableArea::mouseExitedScrollbar(Scrollbar& scrollbar) {
  scrollAnimator().mouseExitedScrollbar(scrollbar);
}

void ScrollableArea::mouseCapturedScrollbar() {
  m_scrollbarCaptured = true;
  showOverlayScrollbars();
  if (m_fadeOverlayScrollbarsTimer)
    m_fadeOverlayScrollbarsTimer->stop();
}

void ScrollableArea::mouseReleasedScrollbar() {
  m_scrollbarCaptured = false;
  // This will kick off the fade out timer.
  showOverlayScrollbars();
}

void ScrollableArea::contentAreaDidShow() const {
  if (ScrollAnimatorBase* scrollAnimator = existingScrollAnimator())
    scrollAnimator->contentAreaDidShow();
}

void ScrollableArea::contentAreaDidHide() const {
  if (ScrollAnimatorBase* scrollAnimator = existingScrollAnimator())
    scrollAnimator->contentAreaDidHide();
}

void ScrollableArea::finishCurrentScrollAnimations() const {
  if (ScrollAnimatorBase* scrollAnimator = existingScrollAnimator())
    scrollAnimator->finishCurrentScrollAnimations();
}

void ScrollableArea::didAddScrollbar(Scrollbar& scrollbar,
                                     ScrollbarOrientation orientation) {
  if (orientation == VerticalScrollbar)
    scrollAnimator().didAddVerticalScrollbar(scrollbar);
  else
    scrollAnimator().didAddHorizontalScrollbar(scrollbar);

  // <rdar://problem/9797253> AppKit resets the scrollbar's style when you
  // attach a scrollbar
  setScrollbarOverlayColorTheme(getScrollbarOverlayColorTheme());
}

void ScrollableArea::willRemoveScrollbar(Scrollbar& scrollbar,
                                         ScrollbarOrientation orientation) {
  if (ScrollAnimatorBase* scrollAnimator = existingScrollAnimator()) {
    if (orientation == VerticalScrollbar)
      scrollAnimator->willRemoveVerticalScrollbar(scrollbar);
    else
      scrollAnimator->willRemoveHorizontalScrollbar(scrollbar);
  }
}

void ScrollableArea::contentsResized() {
  showOverlayScrollbars();
  if (ScrollAnimatorBase* scrollAnimator = existingScrollAnimator())
    scrollAnimator->contentsResized();
}

bool ScrollableArea::hasOverlayScrollbars() const {
  Scrollbar* vScrollbar = verticalScrollbar();
  if (vScrollbar && vScrollbar->isOverlayScrollbar())
    return true;
  Scrollbar* hScrollbar = horizontalScrollbar();
  return hScrollbar && hScrollbar->isOverlayScrollbar();
}

void ScrollableArea::setScrollbarOverlayColorTheme(
    ScrollbarOverlayColorTheme overlayTheme) {
  m_scrollbarOverlayColorTheme = overlayTheme;

  if (Scrollbar* scrollbar = horizontalScrollbar()) {
    ScrollbarTheme::theme().updateScrollbarOverlayColorTheme(*scrollbar);
    scrollbar->setNeedsPaintInvalidation(AllParts);
  }

  if (Scrollbar* scrollbar = verticalScrollbar()) {
    ScrollbarTheme::theme().updateScrollbarOverlayColorTheme(*scrollbar);
    scrollbar->setNeedsPaintInvalidation(AllParts);
  }
}

void ScrollableArea::recalculateScrollbarOverlayColorTheme(
    Color backgroundColor) {
  ScrollbarOverlayColorTheme oldOverlayTheme = getScrollbarOverlayColorTheme();
  ScrollbarOverlayColorTheme overlayTheme = ScrollbarOverlayColorThemeDark;

  // Reduce the background color from RGB to a lightness value
  // and determine which scrollbar style to use based on a lightness
  // heuristic.
  double hue, saturation, lightness;
  backgroundColor.getHSL(hue, saturation, lightness);
  if (lightness <= .5)
    overlayTheme = ScrollbarOverlayColorThemeLight;

  if (oldOverlayTheme != overlayTheme)
    setScrollbarOverlayColorTheme(overlayTheme);
}

void ScrollableArea::setScrollbarNeedsPaintInvalidation(
    ScrollbarOrientation orientation) {
  if (orientation == HorizontalScrollbar) {
    if (GraphicsLayer* graphicsLayer = layerForHorizontalScrollbar()) {
      graphicsLayer->setNeedsDisplay();
      graphicsLayer->setContentsNeedsDisplay();
    }
    m_horizontalScrollbarNeedsPaintInvalidation = true;
  } else {
    if (GraphicsLayer* graphicsLayer = layerForVerticalScrollbar()) {
      graphicsLayer->setNeedsDisplay();
      graphicsLayer->setContentsNeedsDisplay();
    }
    m_verticalScrollbarNeedsPaintInvalidation = true;
  }

  scrollControlWasSetNeedsPaintInvalidation();
}

void ScrollableArea::setScrollCornerNeedsPaintInvalidation() {
  if (GraphicsLayer* graphicsLayer = layerForScrollCorner()) {
    graphicsLayer->setNeedsDisplay();
    return;
  }
  m_scrollCornerNeedsPaintInvalidation = true;
  scrollControlWasSetNeedsPaintInvalidation();
}

bool ScrollableArea::hasLayerForHorizontalScrollbar() const {
  return layerForHorizontalScrollbar();
}

bool ScrollableArea::hasLayerForVerticalScrollbar() const {
  return layerForVerticalScrollbar();
}

bool ScrollableArea::hasLayerForScrollCorner() const {
  return layerForScrollCorner();
}

void ScrollableArea::layerForScrollingDidChange(
    CompositorAnimationTimeline* timeline) {
  if (ProgrammaticScrollAnimator* programmaticScrollAnimator =
          existingProgrammaticScrollAnimator())
    programmaticScrollAnimator->layerForCompositedScrollingDidChange(timeline);
  if (ScrollAnimatorBase* scrollAnimator = existingScrollAnimator())
    scrollAnimator->layerForCompositedScrollingDidChange(timeline);
}

bool ScrollableArea::scheduleAnimation() {
  if (HostWindow* window = getHostWindow()) {
    window->scheduleAnimation(getWidget());
    return true;
  }
  return false;
}

void ScrollableArea::serviceScrollAnimations(double monotonicTime) {
  bool requiresAnimationService = false;
  if (ScrollAnimatorBase* scrollAnimator = existingScrollAnimator()) {
    scrollAnimator->tickAnimation(monotonicTime);
    if (scrollAnimator->hasAnimationThatRequiresService())
      requiresAnimationService = true;
  }
  if (ProgrammaticScrollAnimator* programmaticScrollAnimator =
          existingProgrammaticScrollAnimator()) {
    programmaticScrollAnimator->tickAnimation(monotonicTime);
    if (programmaticScrollAnimator->hasAnimationThatRequiresService())
      requiresAnimationService = true;
  }
  if (!requiresAnimationService)
    deregisterForAnimation();
}

void ScrollableArea::updateCompositorScrollAnimations() {
  if (ProgrammaticScrollAnimator* programmaticScrollAnimator =
          existingProgrammaticScrollAnimator())
    programmaticScrollAnimator->updateCompositorAnimations();

  if (ScrollAnimatorBase* scrollAnimator = existingScrollAnimator())
    scrollAnimator->updateCompositorAnimations();
}

void ScrollableArea::cancelScrollAnimation() {
  if (ScrollAnimatorBase* scrollAnimator = existingScrollAnimator())
    scrollAnimator->cancelAnimation();
}

void ScrollableArea::cancelProgrammaticScrollAnimation() {
  if (ProgrammaticScrollAnimator* programmaticScrollAnimator =
          existingProgrammaticScrollAnimator())
    programmaticScrollAnimator->cancelAnimation();
}

bool ScrollableArea::shouldScrollOnMainThread() const {
  if (GraphicsLayer* layer = layerForScrolling()) {
    uint32_t reasons = layer->platformLayer()->mainThreadScrollingReasons();
    // Should scroll on main thread unless the reason is the one that is set
    // by the ScrollAnimator, in which case, the animation can still be
    // scheduled on the compositor.
    // TODO(ymalik): We have a non-transient "main thread scrolling reason"
    // that doesn't actually cause shouldScrollOnMainThread() to be true.
    // This is confusing and should be cleaned up.
    return !!(reasons &
              ~MainThreadScrollingReason::kHandlingScrollFromMainThread);
  }
  return true;
}

bool ScrollableArea::scrollbarsHidden() const {
  return hasOverlayScrollbars() && m_scrollbarsHidden;
}

void ScrollableArea::setScrollbarsHidden(bool hidden) {
  if (m_scrollbarsHidden == static_cast<unsigned>(hidden))
    return;

  m_scrollbarsHidden = hidden;
  scrollbarVisibilityChanged();
}

void ScrollableArea::fadeOverlayScrollbarsTimerFired(TimerBase*) {
  setScrollbarsHidden(true);
}

void ScrollableArea::showOverlayScrollbars() {
  if (!ScrollbarTheme::theme().usesOverlayScrollbars())
    return;

  setScrollbarsHidden(false);

  const double timeUntilDisable =
      ScrollbarTheme::theme().overlayScrollbarFadeOutDelaySeconds() +
      ScrollbarTheme::theme().overlayScrollbarFadeOutDurationSeconds();

  // If the overlay scrollbars don't fade out, don't do anything. This is the
  // case for the mock overlays used in tests and on Mac, where the fade-out is
  // animated in ScrollAnimatorMac.
  if (!timeUntilDisable)
    return;

  if (!m_fadeOverlayScrollbarsTimer) {
    m_fadeOverlayScrollbarsTimer.reset(new Timer<ScrollableArea>(
        this, &ScrollableArea::fadeOverlayScrollbarsTimerFired));
  }

  if (!m_scrollbarCaptured) {
    m_fadeOverlayScrollbarsTimer->startOneShot(timeUntilDisable,
                                               BLINK_FROM_HERE);
  }
}

IntRect ScrollableArea::visibleContentRect(
    IncludeScrollbarsInRect scrollbarInclusion) const {
  int scrollbarWidth =
      scrollbarInclusion == IncludeScrollbars ? verticalScrollbarWidth() : 0;
  int scrollbarHeight =
      scrollbarInclusion == IncludeScrollbars ? horizontalScrollbarHeight() : 0;

  return enclosingIntRect(
      IntRect(scrollOffset().width(), scrollOffset().height(),
              std::max(0, visibleWidth() + scrollbarWidth),
              std::max(0, visibleHeight() + scrollbarHeight)));
}

IntSize ScrollableArea::clampScrollOffset(const IntSize& scrollOffset) const {
  return scrollOffset.shrunkTo(maximumScrollOffsetInt())
      .expandedTo(minimumScrollOffsetInt());
}

ScrollOffset ScrollableArea::clampScrollOffset(
    const ScrollOffset& scrollOffset) const {
  return scrollOffset.shrunkTo(maximumScrollOffset())
      .expandedTo(minimumScrollOffset());
}

int ScrollableArea::lineStep(ScrollbarOrientation) const {
  return pixelsPerLineStep(getHostWindow());
}

int ScrollableArea::pageStep(ScrollbarOrientation orientation) const {
  IntRect visibleRect = visibleContentRect(IncludeScrollbars);
  int length = (orientation == HorizontalScrollbar) ? visibleRect.width()
                                                    : visibleRect.height();
  int minPageStep = static_cast<float>(length) * minFractionToStepWhenPaging();
  int pageStep = std::max(minPageStep, length - maxOverlapBetweenPages());

  return std::max(pageStep, 1);
}

int ScrollableArea::documentStep(ScrollbarOrientation orientation) const {
  return scrollSize(orientation);
}

float ScrollableArea::pixelStep(ScrollbarOrientation) const {
  return 1;
}

int ScrollableArea::verticalScrollbarWidth(
    OverlayScrollbarClipBehavior behavior) const {
  DCHECK_EQ(behavior, IgnoreOverlayScrollbarSize);
  if (Scrollbar* verticalBar = verticalScrollbar())
    return !verticalBar->isOverlayScrollbar() ? verticalBar->width() : 0;
  return 0;
}

int ScrollableArea::horizontalScrollbarHeight(
    OverlayScrollbarClipBehavior behavior) const {
  DCHECK_EQ(behavior, IgnoreOverlayScrollbarSize);
  if (Scrollbar* horizontalBar = horizontalScrollbar())
    return !horizontalBar->isOverlayScrollbar() ? horizontalBar->height() : 0;
  return 0;
}

FloatQuad ScrollableArea::localToVisibleContentQuad(const FloatQuad& quad,
                                                    const LayoutObject*,
                                                    unsigned) const {
  FloatQuad result(quad);
  result.move(-scrollOffset());
  return result;
}

IntSize ScrollableArea::excludeScrollbars(const IntSize& size) const {
  return IntSize(std::max(0, size.width() - verticalScrollbarWidth()),
                 std::max(0, size.height() - horizontalScrollbarHeight()));
}

DEFINE_TRACE(ScrollableArea) {
  visitor->trace(m_scrollAnimator);
  visitor->trace(m_programmaticScrollAnimator);
}

}  // namespace blink
