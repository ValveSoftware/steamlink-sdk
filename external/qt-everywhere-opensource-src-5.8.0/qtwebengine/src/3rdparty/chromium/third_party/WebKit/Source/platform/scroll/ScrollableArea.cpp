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
#include "platform/Logging.h"
#include "platform/geometry/DoubleRect.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/scroll/MainThreadScrollingReason.h"
#include "platform/scroll/ProgrammaticScrollAnimator.h"
#include "platform/scroll/ScrollbarTheme.h"

#include "platform/TraceEvent.h"

static const int kPixelsPerLineStep = 40;
static const float kMinFractionToStepWhenPaging = 0.875f;

namespace blink {

struct SameSizeAsScrollableArea {
    virtual ~SameSizeAsScrollableArea();
#if ENABLE(ASSERT)
    VerifyEagerFinalization verifyEager;
#endif
    Member<void*> pointer[2];
    unsigned bitfields : 16;
    IntPoint origin;
};

static_assert(sizeof(ScrollableArea) == sizeof(SameSizeAsScrollableArea), "ScrollableArea should stay small");

int ScrollableArea::pixelsPerLineStep(HostWindow* host)
{
    if (!host)
        return kPixelsPerLineStep;
    return host->windowToViewportScalar(kPixelsPerLineStep);
}

float ScrollableArea::minFractionToStepWhenPaging()
{
    return kMinFractionToStepWhenPaging;
}

int ScrollableArea::maxOverlapBetweenPages()
{
    static int maxOverlapBetweenPages = ScrollbarTheme::theme().maxOverlapBetweenPages();
    return maxOverlapBetweenPages;
}

ScrollableArea::ScrollableArea()
    : m_scrollbarOverlayStyle(ScrollbarOverlayStyleDefault)
    , m_scrollOriginChanged(false)
    , m_horizontalScrollbarNeedsPaintInvalidation(false)
    , m_verticalScrollbarNeedsPaintInvalidation(false)
    , m_scrollCornerNeedsPaintInvalidation(false)
{
}

ScrollableArea::~ScrollableArea()
{
}

void ScrollableArea::clearScrollAnimators()
{
#if OS(MACOSX)
    if (m_scrollAnimator)
        m_scrollAnimator->dispose();
#endif
    m_scrollAnimator.clear();
    m_programmaticScrollAnimator.clear();
}

ScrollAnimatorBase& ScrollableArea::scrollAnimator() const
{
    if (!m_scrollAnimator)
        m_scrollAnimator = ScrollAnimatorBase::create(const_cast<ScrollableArea*>(this));

    return *m_scrollAnimator;
}

ProgrammaticScrollAnimator& ScrollableArea::programmaticScrollAnimator() const
{
    if (!m_programmaticScrollAnimator)
        m_programmaticScrollAnimator = ProgrammaticScrollAnimator::create(const_cast<ScrollableArea*>(this));

    return *m_programmaticScrollAnimator;
}

void ScrollableArea::setScrollOrigin(const IntPoint& origin)
{
    if (m_scrollOrigin != origin) {
        m_scrollOrigin = origin;
        m_scrollOriginChanged = true;
    }
}

GraphicsLayer* ScrollableArea::layerForContainer() const
{
    return layerForScrolling() ? layerForScrolling()->parent() : 0;
}

ScrollbarOrientation ScrollableArea::scrollbarOrientationFromDirection(ScrollDirectionPhysical direction) const
{
    return (direction == ScrollUp  || direction == ScrollDown) ? VerticalScrollbar : HorizontalScrollbar;
}

float ScrollableArea::scrollStep(ScrollGranularity granularity, ScrollbarOrientation orientation) const
{
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

ScrollResult ScrollableArea::userScroll(ScrollGranularity granularity, const FloatSize& delta)
{
    float stepX = scrollStep(granularity, HorizontalScrollbar);
    float stepY = scrollStep(granularity, VerticalScrollbar);

    FloatSize pixelDelta(delta);
    pixelDelta.scale(stepX, stepY);

    FloatSize scrollableAxisDelta(
        userInputScrollable(HorizontalScrollbar) ? pixelDelta.width() : 0,
        userInputScrollable(VerticalScrollbar) ? pixelDelta.height() : 0);

    if (scrollableAxisDelta.isZero()) {
        return ScrollResult(
            false,
            false,
            pixelDelta.width(),
            pixelDelta.height());
    }

    cancelProgrammaticScrollAnimation();

    ScrollResult result = scrollAnimator().userScroll(granularity, pixelDelta);

    // Delta that wasn't scrolled because the axis is !userInputScrollable
    // should count as unusedScrollDelta.
    FloatSize unscrollableAxisDelta = pixelDelta - scrollableAxisDelta;
    result.unusedScrollDeltaX += unscrollableAxisDelta.width();
    result.unusedScrollDeltaY += unscrollableAxisDelta.height();

    return result;
}

void ScrollableArea::setScrollPosition(const DoublePoint& position, ScrollType scrollType, ScrollBehavior behavior)
{
    if (behavior == ScrollBehaviorAuto)
        behavior = scrollBehaviorStyle();

    switch (scrollType) {
    case CompositorScroll:
    case AnchoringScroll:
        scrollPositionChanged(clampScrollPosition(position), scrollType);
        break;
    case ProgrammaticScroll:
        programmaticScrollHelper(position, behavior);
        break;
    case UserScroll:
        userScrollHelper(position, behavior);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

void ScrollableArea::scrollBy(const DoubleSize& delta, ScrollType type, ScrollBehavior behavior)
{
    setScrollPosition(scrollPositionDouble() + delta, type, behavior);
}

void ScrollableArea::setScrollPositionSingleAxis(ScrollbarOrientation orientation, double position, ScrollType scrollType, ScrollBehavior behavior)
{
    DoublePoint newPosition;
    if (orientation == HorizontalScrollbar)
        newPosition = DoublePoint(position, scrollAnimator().currentPosition().y());
    else
        newPosition = DoublePoint(scrollAnimator().currentPosition().x(), position);

    // TODO(bokan): Note, this doesn't use the derived class versions since this method is currently used
    // exclusively by code that adjusts the position by the scroll origin and the derived class versions
    // differ on whether they take that into account or not.
    ScrollableArea::setScrollPosition(newPosition, scrollType, behavior);
}

void ScrollableArea::programmaticScrollHelper(const DoublePoint& position, ScrollBehavior scrollBehavior)
{
    cancelScrollAnimation();

    if (scrollBehavior == ScrollBehaviorSmooth)
        programmaticScrollAnimator().animateToOffset(toFloatPoint(position));
    else
        programmaticScrollAnimator().scrollToOffsetWithoutAnimation(toFloatPoint(position));
}

void ScrollableArea::userScrollHelper(const DoublePoint& position, ScrollBehavior scrollBehavior)
{
    cancelProgrammaticScrollAnimation();

    double x = userInputScrollable(HorizontalScrollbar) ? position.x() : scrollAnimator().currentPosition().x();
    double y = userInputScrollable(VerticalScrollbar) ? position.y() : scrollAnimator().currentPosition().y();

    // Smooth user scrolls (keyboard, wheel clicks) are handled via the userScroll method.
    // TODO(bokan): The userScroll method should probably be modified to call this method
    //              and ScrollAnimatorBase to have a simpler animateToOffset method like the
    //              ProgrammaticScrollAnimator.
    ASSERT(scrollBehavior == ScrollBehaviorInstant);
    scrollAnimator().scrollToOffsetWithoutAnimation(FloatPoint(x, y));
}

LayoutRect ScrollableArea::scrollIntoView(const LayoutRect& rectInContent, const ScrollAlignment& alignX, const ScrollAlignment& alignY, ScrollType)
{
    // TODO(bokan): This should really be implemented here but ScrollAlignment is in Core which is a dependency violation.
    ASSERT_NOT_REACHED();
    return LayoutRect();
}

void ScrollableArea::scrollPositionChanged(const DoublePoint& position, ScrollType scrollType)
{
    TRACE_EVENT0("blink", "ScrollableArea::scrollPositionChanged");

    DoublePoint oldPosition = scrollPositionDouble();
    DoublePoint truncatedPosition = shouldUseIntegerScrollOffset() ? flooredIntPoint(position) : position;

    // Tell the derived class to scroll its contents.
    setScrollOffset(truncatedPosition, scrollType);

    // Tell the scrollbars to update their thumb postions.
    // If the scrollbar does not have its own layer, it must always be
    // invalidated to reflect the new thumb position, even if the theme did not
    // invalidate any individual part.
    if (Scrollbar* horizontalScrollbar = this->horizontalScrollbar())
        horizontalScrollbar->offsetDidChange();
    if (Scrollbar* verticalScrollbar = this->verticalScrollbar())
        verticalScrollbar->offsetDidChange();

    if (scrollPositionDouble() != oldPosition) {
        // FIXME: Pass in DoubleSize. crbug.com/414283.
        scrollAnimator().notifyContentAreaScrolled(toFloatSize(scrollPositionDouble() - oldPosition));
    }

    scrollAnimator().setCurrentPosition(toFloatPoint(position));
}

bool ScrollableArea::scrollBehaviorFromString(const String& behaviorString, ScrollBehavior& behavior)
{
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
void ScrollableArea::setScrollOffsetFromInternals(const IntPoint& offset)
{
    scrollPositionChanged(DoublePoint(offset), ProgrammaticScroll);
}

void ScrollableArea::contentAreaWillPaint() const
{
    if (ScrollAnimatorBase* scrollAnimator = existingScrollAnimator())
        scrollAnimator->contentAreaWillPaint();
}

void ScrollableArea::mouseEnteredContentArea() const
{
    if (ScrollAnimatorBase* scrollAnimator = existingScrollAnimator())
        scrollAnimator->mouseEnteredContentArea();
}

void ScrollableArea::mouseExitedContentArea() const
{
    if (ScrollAnimatorBase* scrollAnimator = existingScrollAnimator())
        scrollAnimator->mouseEnteredContentArea();
}

void ScrollableArea::mouseMovedInContentArea() const
{
    if (ScrollAnimatorBase* scrollAnimator = existingScrollAnimator())
        scrollAnimator->mouseMovedInContentArea();
}

void ScrollableArea::mouseEnteredScrollbar(Scrollbar& scrollbar) const
{
    scrollAnimator().mouseEnteredScrollbar(scrollbar);
}

void ScrollableArea::mouseExitedScrollbar(Scrollbar& scrollbar) const
{
    scrollAnimator().mouseExitedScrollbar(scrollbar);
}

void ScrollableArea::contentAreaDidShow() const
{
    if (ScrollAnimatorBase* scrollAnimator = existingScrollAnimator())
        scrollAnimator->contentAreaDidShow();
}

void ScrollableArea::contentAreaDidHide() const
{
    if (ScrollAnimatorBase* scrollAnimator = existingScrollAnimator())
        scrollAnimator->contentAreaDidHide();
}

void ScrollableArea::finishCurrentScrollAnimations() const
{
    if (ScrollAnimatorBase* scrollAnimator = existingScrollAnimator())
        scrollAnimator->finishCurrentScrollAnimations();
}

void ScrollableArea::didAddScrollbar(Scrollbar& scrollbar, ScrollbarOrientation orientation)
{
    if (orientation == VerticalScrollbar)
        scrollAnimator().didAddVerticalScrollbar(scrollbar);
    else
        scrollAnimator().didAddHorizontalScrollbar(scrollbar);

    // <rdar://problem/9797253> AppKit resets the scrollbar's style when you attach a scrollbar
    setScrollbarOverlayStyle(getScrollbarOverlayStyle());
}

void ScrollableArea::willRemoveScrollbar(Scrollbar& scrollbar, ScrollbarOrientation orientation)
{
    if (ScrollAnimatorBase* scrollAnimator = existingScrollAnimator()) {
        if (orientation == VerticalScrollbar)
            scrollAnimator->willRemoveVerticalScrollbar(scrollbar);
        else
            scrollAnimator->willRemoveHorizontalScrollbar(scrollbar);
    }
}

void ScrollableArea::contentsResized()
{
    if (ScrollAnimatorBase* scrollAnimator = existingScrollAnimator())
        scrollAnimator->contentsResized();
}

bool ScrollableArea::hasOverlayScrollbars() const
{
    Scrollbar* vScrollbar = verticalScrollbar();
    if (vScrollbar && vScrollbar->isOverlayScrollbar())
        return true;
    Scrollbar* hScrollbar = horizontalScrollbar();
    return hScrollbar && hScrollbar->isOverlayScrollbar();
}

void ScrollableArea::setScrollbarOverlayStyle(ScrollbarOverlayStyle overlayStyle)
{
    m_scrollbarOverlayStyle = overlayStyle;

    if (Scrollbar* scrollbar = horizontalScrollbar()) {
        ScrollbarTheme::theme().updateScrollbarOverlayStyle(*scrollbar);
        scrollbar->setNeedsPaintInvalidation(AllParts);
    }

    if (Scrollbar* scrollbar = verticalScrollbar()) {
        ScrollbarTheme::theme().updateScrollbarOverlayStyle(*scrollbar);
        scrollbar->setNeedsPaintInvalidation(AllParts);
    }
}

void ScrollableArea::recalculateScrollbarOverlayStyle(Color backgroundColor)
{
    ScrollbarOverlayStyle oldOverlayStyle = getScrollbarOverlayStyle();
    ScrollbarOverlayStyle overlayStyle = ScrollbarOverlayStyleDefault;

    // Reduce the background color from RGB to a lightness value
    // and determine which scrollbar style to use based on a lightness
    // heuristic.
    double hue, saturation, lightness;
    backgroundColor.getHSL(hue, saturation, lightness);
    if (lightness <= .5)
        overlayStyle = ScrollbarOverlayStyleLight;

    if (oldOverlayStyle != overlayStyle)
        setScrollbarOverlayStyle(overlayStyle);
}

void ScrollableArea::setScrollbarNeedsPaintInvalidation(ScrollbarOrientation orientation)
{
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

void ScrollableArea::setScrollCornerNeedsPaintInvalidation()
{
    if (GraphicsLayer* graphicsLayer = layerForScrollCorner()) {
        graphicsLayer->setNeedsDisplay();
        return;
    }
    m_scrollCornerNeedsPaintInvalidation = true;
    scrollControlWasSetNeedsPaintInvalidation();
}

bool ScrollableArea::hasLayerForHorizontalScrollbar() const
{
    return layerForHorizontalScrollbar();
}

bool ScrollableArea::hasLayerForVerticalScrollbar() const
{
    return layerForVerticalScrollbar();
}

bool ScrollableArea::hasLayerForScrollCorner() const
{
    return layerForScrollCorner();
}

void ScrollableArea::layerForScrollingDidChange(CompositorAnimationTimeline* timeline)
{
    if (ProgrammaticScrollAnimator* programmaticScrollAnimator = existingProgrammaticScrollAnimator())
        programmaticScrollAnimator->layerForCompositedScrollingDidChange(timeline);
    if (ScrollAnimatorBase* scrollAnimator = existingScrollAnimator())
        scrollAnimator->layerForCompositedScrollingDidChange(timeline);
}

bool ScrollableArea::scheduleAnimation()
{
    if (HostWindow* window = getHostWindow()) {
        window->scheduleAnimation(getWidget());
        return true;
    }
    return false;
}

void ScrollableArea::serviceScrollAnimations(double monotonicTime)
{
    bool requiresAnimationService = false;
    if (ScrollAnimatorBase* scrollAnimator = existingScrollAnimator()) {
        scrollAnimator->tickAnimation(monotonicTime);
        if (scrollAnimator->hasAnimationThatRequiresService())
            requiresAnimationService = true;
    }
    if (ProgrammaticScrollAnimator* programmaticScrollAnimator = existingProgrammaticScrollAnimator()) {
        programmaticScrollAnimator->tickAnimation(monotonicTime);
        if (programmaticScrollAnimator->hasAnimationThatRequiresService())
            requiresAnimationService = true;
    }
    if (!requiresAnimationService)
        deregisterForAnimation();
}

void ScrollableArea::updateCompositorScrollAnimations()
{
    if (ProgrammaticScrollAnimator* programmaticScrollAnimator = existingProgrammaticScrollAnimator())
        programmaticScrollAnimator->updateCompositorAnimations();

    if (ScrollAnimatorBase* scrollAnimator = existingScrollAnimator())
        scrollAnimator->updateCompositorAnimations();
}

void ScrollableArea::cancelScrollAnimation()
{
    if (ScrollAnimatorBase* scrollAnimator = existingScrollAnimator())
        scrollAnimator->cancelAnimation();
}

void ScrollableArea::cancelProgrammaticScrollAnimation()
{
    if (ProgrammaticScrollAnimator* programmaticScrollAnimator = existingProgrammaticScrollAnimator())
        programmaticScrollAnimator->cancelAnimation();
}

bool ScrollableArea::shouldScrollOnMainThread() const
{
    if (GraphicsLayer* layer = layerForScrolling()) {
        uint32_t reasons = layer->platformLayer()->mainThreadScrollingReasons();
        // Should scroll on main thread unless the reason is the one that is set
        // by the ScrollAnimator, in which case, the animation can still be
        // scheduled on the compositor.
        // TODO(ymalik): We have a non-transient "main thread scrolling reason"
        // that doesn't actually cause shouldScrollOnMainThread() to be true.
        // This is confusing and should be cleaned up.
        return !!(reasons & ~MainThreadScrollingReason::kAnimatingScrollOnMainThread);
    }
    return true;
}

DoubleRect ScrollableArea::visibleContentRectDouble(IncludeScrollbarsInRect scrollbarInclusion) const
{
    return visibleContentRect(scrollbarInclusion);
}

IntRect ScrollableArea::visibleContentRect(IncludeScrollbarsInRect scrollbarInclusion) const
{
    int scrollbarWidth = scrollbarInclusion == IncludeScrollbars ? verticalScrollbarWidth() : 0;
    int scrollbarHeight = scrollbarInclusion == IncludeScrollbars ? horizontalScrollbarHeight() : 0;

    return IntRect(
        scrollPosition().x(),
        scrollPosition().y(),
        std::max(0, visibleWidth() + scrollbarWidth),
        std::max(0, visibleHeight() + scrollbarHeight));
}

IntPoint ScrollableArea::clampScrollPosition(const IntPoint& scrollPosition) const
{
    return scrollPosition.shrunkTo(maximumScrollPosition()).expandedTo(minimumScrollPosition());
}

DoublePoint ScrollableArea::clampScrollPosition(const DoublePoint& scrollPosition) const
{
    return scrollPosition.shrunkTo(maximumScrollPositionDouble()).expandedTo(minimumScrollPositionDouble());
}

int ScrollableArea::lineStep(ScrollbarOrientation) const
{
    return pixelsPerLineStep(getHostWindow());
}

int ScrollableArea::pageStep(ScrollbarOrientation orientation) const
{
    IntRect visibleRect = visibleContentRect(IncludeScrollbars);
    int length = (orientation == HorizontalScrollbar) ? visibleRect.width() : visibleRect.height();
    int minPageStep = static_cast<float>(length) * minFractionToStepWhenPaging();
    int pageStep = std::max(minPageStep, length - maxOverlapBetweenPages());

    return std::max(pageStep, 1);
}

int ScrollableArea::documentStep(ScrollbarOrientation orientation) const
{
    return scrollSize(orientation);
}

float ScrollableArea::pixelStep(ScrollbarOrientation) const
{
    return 1;
}

int ScrollableArea::verticalScrollbarWidth() const
{
    if (Scrollbar* verticalBar = verticalScrollbar())
        return !verticalBar->isOverlayScrollbar() ? verticalBar->width() : 0;
    return 0;
}

int ScrollableArea::horizontalScrollbarHeight() const
{
    if (Scrollbar* horizontalBar = horizontalScrollbar())
        return !horizontalBar->isOverlayScrollbar() ? horizontalBar->height() : 0;
    return 0;
}

IntSize ScrollableArea::excludeScrollbars(const IntSize& size) const
{
    return IntSize(std::max(0, size.width() - verticalScrollbarWidth()),
        std::max(0, size.height() - horizontalScrollbarHeight()));
}

DEFINE_TRACE(ScrollableArea)
{
    visitor->trace(m_scrollAnimator);
    visitor->trace(m_programmaticScrollAnimator);
}

} // namespace blink
