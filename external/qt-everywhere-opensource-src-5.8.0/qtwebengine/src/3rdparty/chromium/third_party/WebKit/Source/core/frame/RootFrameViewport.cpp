// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/RootFrameViewport.h"

#include "core/frame/FrameView.h"
#include "core/layout/ScrollAlignment.h"
#include "platform/geometry/DoubleRect.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/LayoutRect.h"

namespace blink {

RootFrameViewport::RootFrameViewport(ScrollableArea& visualViewport, ScrollableArea& layoutViewport)
    : m_visualViewport(visualViewport)
    , m_layoutViewport(layoutViewport)
{
}

void RootFrameViewport::updateScrollAnimator()
{
    scrollAnimator().setCurrentPosition(toFloatPoint(scrollOffsetFromScrollAnimators()));
}

DoublePoint RootFrameViewport::scrollOffsetFromScrollAnimators() const
{
    return visualViewport().scrollAnimator().currentPosition() + layoutViewport().scrollAnimator().currentPosition();
}

DoubleRect RootFrameViewport::visibleContentRectDouble(IncludeScrollbarsInRect scrollbarInclusion) const
{
    return DoubleRect(scrollPositionDouble(), visualViewport().visibleContentRectDouble(scrollbarInclusion).size());
}

IntRect RootFrameViewport::visibleContentRect(IncludeScrollbarsInRect scrollbarInclusion) const
{
    return enclosingIntRect(visibleContentRectDouble(scrollbarInclusion));
}

bool RootFrameViewport::shouldUseIntegerScrollOffset() const
{
    // Fractionals are floored in the ScrollAnimatorBase but it's important that the ScrollAnimators of the
    // visual and layout viewports get the precise fractional number so never use integer scrolling for
    // RootFrameViewport, we'll let the truncation happen in the subviewports.
    return false;
}

bool RootFrameViewport::isActive() const
{
    return layoutViewport().isActive();
}

int RootFrameViewport::scrollSize(ScrollbarOrientation orientation) const
{
    IntSize scrollDimensions = maximumScrollPosition() - minimumScrollPosition();
    return (orientation == HorizontalScrollbar) ? scrollDimensions.width() : scrollDimensions.height();
}

bool RootFrameViewport::isScrollCornerVisible() const
{
    return layoutViewport().isScrollCornerVisible();
}

IntRect RootFrameViewport::scrollCornerRect() const
{
    return layoutViewport().scrollCornerRect();
}

void RootFrameViewport::setScrollPosition(const DoublePoint& position, ScrollType scrollType, ScrollBehavior scrollBehavior)
{
    updateScrollAnimator();

    if (scrollBehavior == ScrollBehaviorAuto)
        scrollBehavior = scrollBehaviorStyle();

    if (scrollType == ProgrammaticScroll && !layoutViewport().isProgrammaticallyScrollable())
        return;

    if (scrollBehavior == ScrollBehaviorSmooth) {
        distributeScrollBetweenViewports(position, scrollType, scrollBehavior);
        return;
    }

    DoublePoint clampedPosition = clampScrollPosition(position);
    ScrollableArea::setScrollPosition(clampedPosition, scrollType, scrollBehavior);
}

ScrollBehavior RootFrameViewport::scrollBehaviorStyle() const
{
    return layoutViewport().scrollBehaviorStyle();
}

LayoutRect RootFrameViewport::scrollIntoView(const LayoutRect& rectInContent, const ScrollAlignment& alignX, const ScrollAlignment& alignY, ScrollType scrollType)
{
    // We want to move the rect into the viewport that excludes the scrollbars so we intersect
    // the visual viewport with the scrollbar-excluded frameView content rect. However, we don't
    // use visibleContentRect directly since it floors the scroll position. Instead, we use
    // ScrollAnimatorBase::currentPosition and construct a LayoutRect from that.

    LayoutRect frameRectInContent = LayoutRect(
        layoutViewport().scrollAnimator().currentPosition(),
        layoutViewport().visibleContentRect().size());
    LayoutRect visualRectInContent = LayoutRect(
        scrollOffsetFromScrollAnimators(),
        visualViewport().visibleContentRect().size());

    // Intersect layout and visual rects to exclude the scrollbar from the view rect.
    LayoutRect viewRectInContent = intersection(visualRectInContent, frameRectInContent);
    LayoutRect targetViewport =
        ScrollAlignment::getRectToExpose(viewRectInContent, rectInContent, alignX, alignY);
    if (targetViewport != viewRectInContent)
        setScrollPosition(DoublePoint(targetViewport.x(), targetViewport.y()), scrollType);

    // RootFrameViewport only changes the viewport relative to the document so we can't change the input
    // rect's location relative to the document origin.
    return rectInContent;
}

void RootFrameViewport::setScrollOffset(const DoublePoint& offset, ScrollType scrollType)
{
    distributeScrollBetweenViewports(DoublePoint(offset), scrollType, ScrollBehaviorInstant);
}

void RootFrameViewport::distributeScrollBetweenViewports(const DoublePoint& offset, ScrollType scrollType, ScrollBehavior behavior)
{
    // Make sure we use the scroll positions as reported by each viewport's ScrollAnimatorBase, since its
    // ScrollableArea's position may have the fractional part truncated off.
    DoublePoint oldPosition = scrollOffsetFromScrollAnimators();

    DoubleSize delta = offset - oldPosition;

    if (delta.isZero())
        return;

    DoublePoint targetPosition = visualViewport().clampScrollPosition(
        visualViewport().scrollAnimator().currentPosition() + delta);

    visualViewport().setScrollPosition(targetPosition, scrollType, behavior);

    // Scroll the secondary viewport if all of the scroll was not applied to the
    // primary viewport.
    DoublePoint updatedPosition = layoutViewport().scrollAnimator().currentPosition() + FloatPoint(targetPosition);
    DoubleSize applied = updatedPosition - oldPosition;
    delta -= applied;

    if (delta.isZero())
        return;

    targetPosition = layoutViewport().clampScrollPosition(layoutViewport().scrollAnimator().currentPosition() + delta);
    layoutViewport().setScrollPosition(targetPosition, scrollType, behavior);
}

IntPoint RootFrameViewport::scrollPosition() const
{
    return flooredIntPoint(scrollPositionDouble());
}

DoublePoint RootFrameViewport::scrollPositionDouble() const
{
    return layoutViewport().scrollPositionDouble() + toDoubleSize(visualViewport().scrollPositionDouble());
}

IntPoint RootFrameViewport::minimumScrollPosition() const
{
    return IntPoint(layoutViewport().minimumScrollPosition() + visualViewport().minimumScrollPosition());
}

IntPoint RootFrameViewport::maximumScrollPosition() const
{
    return layoutViewport().maximumScrollPosition() + visualViewport().maximumScrollPosition();
}

DoublePoint RootFrameViewport::maximumScrollPositionDouble() const
{
    return layoutViewport().maximumScrollPositionDouble() + toDoubleSize(visualViewport().maximumScrollPositionDouble());
}

IntSize RootFrameViewport::contentsSize() const
{
    return layoutViewport().contentsSize();
}

bool RootFrameViewport::scrollbarsCanBeActive() const
{
    return layoutViewport().scrollbarsCanBeActive();
}

IntRect RootFrameViewport::scrollableAreaBoundingBox() const
{
    return layoutViewport().scrollableAreaBoundingBox();
}

bool RootFrameViewport::userInputScrollable(ScrollbarOrientation orientation) const
{
    return visualViewport().userInputScrollable(orientation) || layoutViewport().userInputScrollable(orientation);
}

bool RootFrameViewport::shouldPlaceVerticalScrollbarOnLeft() const
{
    return layoutViewport().shouldPlaceVerticalScrollbarOnLeft();
}

void RootFrameViewport::scrollControlWasSetNeedsPaintInvalidation()
{
    layoutViewport().scrollControlWasSetNeedsPaintInvalidation();
}

GraphicsLayer* RootFrameViewport::layerForContainer() const
{
    return layoutViewport().layerForContainer();
}

GraphicsLayer* RootFrameViewport::layerForScrolling() const
{
    return layoutViewport().layerForScrolling();
}

GraphicsLayer* RootFrameViewport::layerForHorizontalScrollbar() const
{
    return layoutViewport().layerForHorizontalScrollbar();
}

GraphicsLayer* RootFrameViewport::layerForVerticalScrollbar() const
{
    return layoutViewport().layerForVerticalScrollbar();
}

ScrollResult RootFrameViewport::userScroll(ScrollGranularity granularity, const FloatSize& delta)
{
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
        return ScrollResult(
            false,
            false,
            pixelDelta.width(),
            pixelDelta.height());
    }

    cancelProgrammaticScrollAnimation();

    // TODO(bokan): Why do we call userScroll on the animators directly and
    // not through the ScrollableAreas?
    ScrollResult visualResult = visualViewport().scrollAnimator().userScroll(
        granularity,
        visualConsumedDelta);

    if (visualConsumedDelta == pixelDelta)
        return visualResult;

    ScrollResult layoutResult = layoutViewport().scrollAnimator().userScroll(
        granularity,
        scrollableAxisDelta);

    // Remember to add any delta not used because of !userInputScrollable to the
    // unusedScrollDelta in the result.
    FloatSize unscrollableAxisDelta = layoutDelta - scrollableAxisDelta;

    return ScrollResult(
        visualResult.didScrollX || layoutResult.didScrollX,
        visualResult.didScrollY || layoutResult.didScrollY,
        layoutResult.unusedScrollDeltaX + unscrollableAxisDelta.width(),
        layoutResult.unusedScrollDeltaY + unscrollableAxisDelta.height());
}

bool RootFrameViewport::scrollAnimatorEnabled() const
{
    return layoutViewport().scrollAnimatorEnabled();
}

HostWindow* RootFrameViewport::getHostWindow() const
{
    return layoutViewport().getHostWindow();
}

void RootFrameViewport::serviceScrollAnimations(double monotonicTime)
{
    ScrollableArea::serviceScrollAnimations(monotonicTime);
    layoutViewport().serviceScrollAnimations(monotonicTime);
    visualViewport().serviceScrollAnimations(monotonicTime);
}

void RootFrameViewport::updateCompositorScrollAnimations()
{
    ScrollableArea::updateCompositorScrollAnimations();
    layoutViewport().updateCompositorScrollAnimations();
    visualViewport().updateCompositorScrollAnimations();
}

void RootFrameViewport::cancelProgrammaticScrollAnimation()
{
    ScrollableArea::cancelProgrammaticScrollAnimation();
    layoutViewport().cancelProgrammaticScrollAnimation();
    visualViewport().cancelProgrammaticScrollAnimation();
}

Widget* RootFrameViewport::getWidget()
{
    return visualViewport().getWidget();
}

void RootFrameViewport::clearScrollAnimators()
{
    ScrollableArea::clearScrollAnimators();
    layoutViewport().clearScrollAnimators();
    visualViewport().clearScrollAnimators();
}

DEFINE_TRACE(RootFrameViewport)
{
    visitor->trace(m_visualViewport);
    visitor->trace(m_layoutViewport);
    ScrollableArea::trace(visitor);
}

} // namespace blink
