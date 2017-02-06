/*
 * Copyright (C) 2008, 2011 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ScrollableArea_h
#define ScrollableArea_h

#include "platform/PlatformExport.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/geometry/DoublePoint.h"
#include "platform/graphics/Color.h"
#include "platform/heap/Handle.h"
#include "platform/scroll/ScrollAnimatorBase.h"
#include "platform/scroll/ScrollTypes.h"
#include "platform/scroll/Scrollbar.h"
#include "wtf/MathExtras.h"
#include "wtf/Noncopyable.h"
#include "wtf/Vector.h"

namespace blink {

class DoubleRect;
class FloatPoint;
class GraphicsLayer;
class HostWindow;
class PlatformWheelEvent;
class ProgrammaticScrollAnimator;
struct ScrollAlignment;
class ScrollAnimatorBase;
class CompositorAnimationTimeline;
class Widget;

enum IncludeScrollbarsInRect {
    ExcludeScrollbars,
    IncludeScrollbars,
};

class PLATFORM_EXPORT ScrollableArea : public GarbageCollectedMixin {
    WTF_MAKE_NONCOPYABLE(ScrollableArea);
public:
    static int pixelsPerLineStep(HostWindow*);
    static float minFractionToStepWhenPaging();
    static int maxOverlapBetweenPages();

    // Convert a non-finite scroll value (Infinity, -Infinity, NaN) to 0 as
    // per http://dev.w3.org/csswg/cssom-view/#normalize-non_finite-values.
    static double normalizeNonFiniteScroll(double value) { return std::isfinite(value) ? value : 0.0; }

    // The window that hosts the ScrollableArea. The ScrollableArea will communicate scrolls and repaints to the
    // host window in the window's coordinate space.
    virtual HostWindow* getHostWindow() const { return 0; }

    virtual ScrollResult userScroll(ScrollGranularity, const FloatSize&);

    virtual void setScrollPosition(const DoublePoint&, ScrollType, ScrollBehavior = ScrollBehaviorInstant);
    virtual void scrollBy(const DoubleSize&, ScrollType, ScrollBehavior = ScrollBehaviorInstant);
    void setScrollPositionSingleAxis(ScrollbarOrientation, double, ScrollType, ScrollBehavior = ScrollBehaviorInstant);

    // Scrolls the area so that the given rect, given in the document's content coordinates, such that it's
    // visible in the area. Returns the new location of the input rect relative once again to the document.
    // Note, in the case of a Document container, such as FrameView, the output will always be the input rect
    // since scrolling it can't change the location of content relative to the document, unlike an overflowing
    // element.
    virtual LayoutRect scrollIntoView(const LayoutRect& rectInContent, const ScrollAlignment& alignX, const ScrollAlignment& alignY, ScrollType = ProgrammaticScroll);

    // Returns a rect, in the space of the area's backing graphics layer, that contains the visual
    // region of all scrollbar parts.
    virtual LayoutRect visualRectForScrollbarParts() const = 0;

    static bool scrollBehaviorFromString(const String&, ScrollBehavior&);

    void contentAreaWillPaint() const;
    void mouseEnteredContentArea() const;
    void mouseExitedContentArea() const;
    void mouseMovedInContentArea() const;
    void mouseEnteredScrollbar(Scrollbar&) const;
    void mouseExitedScrollbar(Scrollbar&) const;
    void contentAreaDidShow() const;
    void contentAreaDidHide() const;

    void finishCurrentScrollAnimations() const;

    virtual void didAddScrollbar(Scrollbar&, ScrollbarOrientation);
    virtual void willRemoveScrollbar(Scrollbar&, ScrollbarOrientation);

    virtual void contentsResized();

    bool hasOverlayScrollbars() const;
    void setScrollbarOverlayStyle(ScrollbarOverlayStyle);
    void recalculateScrollbarOverlayStyle(Color);
    ScrollbarOverlayStyle getScrollbarOverlayStyle() const { return static_cast<ScrollbarOverlayStyle>(m_scrollbarOverlayStyle); }

    // This getter will create a ScrollAnimatorBase if it doesn't already exist.
    ScrollAnimatorBase& scrollAnimator() const;

    // This getter will return null if the ScrollAnimatorBase hasn't been created yet.
    ScrollAnimatorBase* existingScrollAnimator() const { return m_scrollAnimator; }

    ProgrammaticScrollAnimator& programmaticScrollAnimator() const;
    ProgrammaticScrollAnimator* existingProgrammaticScrollAnimator() const { return m_programmaticScrollAnimator; }

    virtual CompositorAnimationTimeline* compositorAnimationTimeline() const { return nullptr; }

    // See Source/core/layout/README.md for an explanation of scroll origin.
    const IntPoint& scrollOrigin() const { return m_scrollOrigin; }
    bool scrollOriginChanged() const { return m_scrollOriginChanged; }


    // This is used to determine whether the incoming fractional scroll offset should
    // be truncated to integer. Current rule is that if preferCompositingToLCDTextEnabled()
    // is disabled (which is true on low-dpi device by default) we should do the truncation.
    // The justification is that non-composited elements using fractional scroll offsets
    // is causing too much nasty bugs but does not add too benefit on low-dpi devices.
    virtual bool shouldUseIntegerScrollOffset() const { return !RuntimeEnabledFeatures::fractionalScrollOffsetsEnabled(); }

    virtual bool isActive() const = 0;
    virtual int scrollSize(ScrollbarOrientation) const = 0;
    void setScrollbarNeedsPaintInvalidation(ScrollbarOrientation);
    virtual bool isScrollCornerVisible() const = 0;
    virtual IntRect scrollCornerRect() const = 0;
    void setScrollCornerNeedsPaintInvalidation();
    virtual void getTickmarks(Vector<IntRect>&) const { }

    // Convert points and rects between the scrollbar and its containing Widget.
    // The client needs to implement these in order to be aware of layout effects
    // like CSS transforms.
    virtual IntRect convertFromScrollbarToContainingWidget(const Scrollbar& scrollbar, const IntRect& scrollbarRect) const
    {
        return scrollbar.Widget::convertToContainingWidget(scrollbarRect);
    }
    virtual IntRect convertFromContainingWidgetToScrollbar(const Scrollbar& scrollbar, const IntRect& parentRect) const
    {
        return scrollbar.Widget::convertFromContainingWidget(parentRect);
    }
    virtual IntPoint convertFromScrollbarToContainingWidget(const Scrollbar& scrollbar, const IntPoint& scrollbarPoint) const
    {
        return scrollbar.Widget::convertToContainingWidget(scrollbarPoint);
    }
    virtual IntPoint convertFromContainingWidgetToScrollbar(const Scrollbar& scrollbar, const IntPoint& parentPoint) const
    {
        return scrollbar.Widget::convertFromContainingWidget(parentPoint);
    }

    virtual Scrollbar* horizontalScrollbar() const { return nullptr; }
    virtual Scrollbar* verticalScrollbar() const { return nullptr; }

    // scrollPosition is relative to the scrollOrigin. i.e. If the page is RTL
    // then scrollPosition will be negative. By default, scrollPositionDouble()
    // just call into scrollPosition(). Subclass can override scrollPositionDouble()
    // to return floating point precision scrolloffset.
    // FIXME: Remove scrollPosition(). crbug.com/414283.
    virtual IntPoint scrollPosition() const = 0;
    virtual DoublePoint scrollPositionDouble() const { return DoublePoint(scrollPosition()); }
    virtual IntPoint minimumScrollPosition() const = 0;
    virtual DoublePoint minimumScrollPositionDouble() const { return DoublePoint(minimumScrollPosition()); }
    virtual IntPoint maximumScrollPosition() const = 0;
    virtual DoublePoint maximumScrollPositionDouble() const { return DoublePoint(maximumScrollPosition()); }

    virtual DoubleRect visibleContentRectDouble(IncludeScrollbarsInRect = ExcludeScrollbars) const;
    virtual IntRect visibleContentRect(IncludeScrollbarsInRect = ExcludeScrollbars) const;
    virtual int visibleHeight() const { return visibleContentRect().height(); }
    virtual int visibleWidth() const { return visibleContentRect().width(); }
    virtual IntSize contentsSize() const = 0;
    virtual IntPoint lastKnownMousePosition() const { return IntPoint(); }

    virtual bool shouldSuspendScrollAnimations() const { return true; }
    virtual void scrollbarStyleChanged() { }
    virtual void scrollbarVisibilityChanged() { }
    virtual bool scrollbarsCanBeActive() const = 0;

    // Returns the bounding box of this scrollable area, in the coordinate system of the enclosing scroll view.
    virtual IntRect scrollableAreaBoundingBox() const = 0;

    virtual bool scrollAnimatorEnabled() const { return false; }

    // NOTE: Only called from Internals for testing.
    void setScrollOffsetFromInternals(const IntPoint&);

    IntPoint clampScrollPosition(const IntPoint&) const;
    DoublePoint clampScrollPosition(const DoublePoint&) const;

    // Let subclasses provide a way of asking for and servicing scroll
    // animations.
    virtual bool scheduleAnimation();
    virtual void serviceScrollAnimations(double monotonicTime);
    virtual void updateCompositorScrollAnimations();
    virtual void registerForAnimation() { }
    virtual void deregisterForAnimation() { }

    virtual bool usesCompositedScrolling() const { return false; }
    virtual bool shouldScrollOnMainThread() const;

    // Returns true if the GraphicsLayer tree needs to be rebuilt.
    virtual bool updateAfterCompositingChange() { return false; }

    virtual bool userInputScrollable(ScrollbarOrientation) const = 0;
    virtual bool shouldPlaceVerticalScrollbarOnLeft() const = 0;

    // Convenience functions
    int scrollPosition(ScrollbarOrientation orientation) { return orientation == HorizontalScrollbar ? scrollPosition().x() : scrollPosition().y(); }
    int minimumScrollPosition(ScrollbarOrientation orientation) { return orientation == HorizontalScrollbar ? minimumScrollPosition().x() : minimumScrollPosition().y(); }
    int maximumScrollPosition(ScrollbarOrientation orientation) { return orientation == HorizontalScrollbar ? maximumScrollPosition().x() : maximumScrollPosition().y(); }
    int clampScrollPosition(ScrollbarOrientation orientation, int pos)  { return clampTo(pos, minimumScrollPosition(orientation), maximumScrollPosition(orientation)); }

    virtual GraphicsLayer* layerForContainer() const;
    virtual GraphicsLayer* layerForScrolling() const { return 0; }
    virtual GraphicsLayer* layerForHorizontalScrollbar() const { return 0; }
    virtual GraphicsLayer* layerForVerticalScrollbar() const { return 0; }
    virtual GraphicsLayer* layerForScrollCorner() const { return 0; }
    bool hasLayerForHorizontalScrollbar() const;
    bool hasLayerForVerticalScrollbar() const;
    bool hasLayerForScrollCorner() const;

    void layerForScrollingDidChange(CompositorAnimationTimeline*);

    void cancelScrollAnimation();
    virtual void cancelProgrammaticScrollAnimation();

    virtual ~ScrollableArea();

    // Called when any of horizontal scrollbar, vertical scrollbar and scroll corner is setNeedsPaintInvalidation.
    virtual void scrollControlWasSetNeedsPaintInvalidation() = 0;

    // Returns the default scroll style this area should scroll with when not
    // explicitly specified. E.g. The scrolling behavior of an element can be
    // specified in CSS.
    virtual ScrollBehavior scrollBehaviorStyle() const { return ScrollBehaviorInstant; }

    // TODO(bokan): This is only used in FrameView to check scrollability but is
    // needed here to allow RootFrameViewport to preserve wheelHandler
    // semantics. Not sure why it's FrameView specific, it could probably be
    // generalized to other types of ScrollableAreas.
    virtual bool isScrollable() { return true; }

    // TODO(bokan): FrameView::setScrollPosition uses updateScrollbars to scroll
    // which bails out early if its already in updateScrollbars, the effect being
    // that programmatic scrolls (i.e. setScrollPosition) are disabled when in
    // updateScrollbars. Expose this here to allow RootFrameViewport to match the
    // semantics for now but it should be cleaned up at the source.
    virtual bool isProgrammaticallyScrollable() { return true; }

    // Subtracts space occupied by this ScrollableArea's scrollbars.
    // Does nothing if overlay scrollbars are enabled.
    IntSize excludeScrollbars(const IntSize&) const;

    // Returns 0 if overlay scrollbars are enabled.
    int verticalScrollbarWidth() const;
    int horizontalScrollbarHeight() const;

    // Returns the widget associated with this ScrollableArea.
    virtual Widget* getWidget() { return nullptr; }

    virtual bool isFrameView() const { return false; }
    virtual bool isPaintLayerScrollableArea() const { return false; }

    // Need to promptly let go of owned animator objects.
    EAGERLY_FINALIZE();
    DECLARE_VIRTUAL_TRACE();

    virtual void clearScrollAnimators();

protected:
    ScrollableArea();

    ScrollbarOrientation scrollbarOrientationFromDirection(ScrollDirectionPhysical) const;
    float scrollStep(ScrollGranularity, ScrollbarOrientation) const;

    void setScrollOrigin(const IntPoint&);
    void resetScrollOriginChanged() { m_scrollOriginChanged = false; }

    // Needed to let the animators call scrollPositionChanged.
    friend class ScrollAnimatorBase;
    friend class ProgrammaticScrollAnimator;
    void scrollPositionChanged(const DoublePoint&, ScrollType);

    bool horizontalScrollbarNeedsPaintInvalidation() const { return m_horizontalScrollbarNeedsPaintInvalidation; }
    bool verticalScrollbarNeedsPaintInvalidation() const { return m_verticalScrollbarNeedsPaintInvalidation; }
    bool scrollCornerNeedsPaintInvalidation() const { return m_scrollCornerNeedsPaintInvalidation; }
    void clearNeedsPaintInvalidationForScrollControls()
    {
        m_horizontalScrollbarNeedsPaintInvalidation = false;
        m_verticalScrollbarNeedsPaintInvalidation = false;
        m_scrollCornerNeedsPaintInvalidation = false;
    }

private:
    void programmaticScrollHelper(const DoublePoint&, ScrollBehavior);
    void userScrollHelper(const DoublePoint&, ScrollBehavior);

    // This function should be overriden by subclasses to perform the actual scroll of the content.
    virtual void setScrollOffset(const DoublePoint& offset, ScrollType) = 0;

    virtual int lineStep(ScrollbarOrientation) const;
    virtual int pageStep(ScrollbarOrientation) const;
    virtual int documentStep(ScrollbarOrientation) const;
    virtual float pixelStep(ScrollbarOrientation) const;

    mutable Member<ScrollAnimatorBase> m_scrollAnimator;
    mutable Member<ProgrammaticScrollAnimator> m_programmaticScrollAnimator;

    unsigned m_scrollbarOverlayStyle : 2; // ScrollbarOverlayStyle

    unsigned m_scrollOriginChanged : 1;

    unsigned m_horizontalScrollbarNeedsPaintInvalidation : 1;
    unsigned m_verticalScrollbarNeedsPaintInvalidation : 1;
    unsigned m_scrollCornerNeedsPaintInvalidation : 1;

    // There are 6 possible combinations of writing mode and direction. Scroll origin will be non-zero in the x or y axis
    // if there is any reversed direction or writing-mode. The combinations are:
    // writing-mode / direction     scrollOrigin.x() set    scrollOrigin.y() set
    // horizontal-tb / ltr          NO                      NO
    // horizontal-tb / rtl          YES                     NO
    // vertical-lr / ltr            NO                      NO
    // vertical-lr / rtl            NO                      YES
    // vertical-rl / ltr            YES                     NO
    // vertical-rl / rtl            YES                     YES
    IntPoint m_scrollOrigin;
};

} // namespace blink

#endif // ScrollableArea_h
