// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/RootFrameViewport.h"

#include "core/layout/ScrollAlignment.h"
#include "platform/geometry/DoubleRect.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/scroll/ScrollableArea.h"
#include "testing/gtest/include/gtest/gtest.h"

#define EXPECT_POINT_EQ(expected, actual) \
    do { \
        EXPECT_EQ((expected).x(), (actual).x()); \
        EXPECT_EQ((expected).y(), (actual).y()); \
    } while (false)
#define EXPECT_SIZE_EQ(expected, actual) \
    do { \
        EXPECT_EQ((expected).width(), (actual).width()); \
        EXPECT_EQ((expected).height(), (actual).height()); \
    } while (false)

namespace blink {

class ScrollableAreaStub : public GarbageCollectedFinalized<ScrollableAreaStub>, public ScrollableArea {
    USING_GARBAGE_COLLECTED_MIXIN(ScrollableAreaStub);
public:
    static ScrollableAreaStub* create(const IntSize& viewportSize, const IntSize& contentsSize)
    {
        return new ScrollableAreaStub(viewportSize, contentsSize);
    }

    void setViewportSize(const IntSize& viewportSize)
    {
        m_viewportSize = viewportSize;
    }

    IntSize viewportSize() const { return m_viewportSize; }

    // ScrollableArea Impl
    int scrollSize(ScrollbarOrientation orientation) const override
    {
        IntSize scrollDimensions = maximumScrollPosition() - minimumScrollPosition();
        return (orientation == HorizontalScrollbar) ? scrollDimensions.width() : scrollDimensions.height();
    }

    void setUserInputScrollable(bool x, bool y)
    {
        m_userInputScrollableX = x;
        m_userInputScrollableY = y;
    }

    IntPoint scrollPosition() const override { return flooredIntPoint(m_scrollPosition); }
    DoublePoint scrollPositionDouble() const override { return m_scrollPosition; }
    IntPoint minimumScrollPosition() const override { return IntPoint(); }
    DoublePoint minimumScrollPositionDouble() const override { return DoublePoint(); }
    IntPoint maximumScrollPosition() const override { return flooredIntPoint(maximumScrollPositionDouble()); }

    IntSize contentsSize() const override { return m_contentsSize; }
    void setContentSize(const IntSize& contentsSize)
    {
        m_contentsSize = contentsSize;
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        ScrollableArea::trace(visitor);
    }

protected:
    ScrollableAreaStub(const IntSize& viewportSize, const IntSize& contentsSize)
        : m_userInputScrollableX(true)
        , m_userInputScrollableY(true)
        , m_viewportSize(viewportSize)
        , m_contentsSize(contentsSize)
    {
    }

    void setScrollOffset(const DoublePoint& offset, ScrollType) override { m_scrollPosition = offset; }
    bool shouldUseIntegerScrollOffset() const override { return true; }
    LayoutRect visualRectForScrollbarParts() const override { ASSERT_NOT_REACHED(); return LayoutRect(); }
    bool isActive() const override { return true; }
    bool isScrollCornerVisible() const override { return true; }
    IntRect scrollCornerRect() const override { return IntRect(); }
    bool scrollbarsCanBeActive() const override { return true; }
    IntRect scrollableAreaBoundingBox() const override { return IntRect(); }
    bool shouldPlaceVerticalScrollbarOnLeft() const override { return true; }
    void scrollControlWasSetNeedsPaintInvalidation() override { }
    GraphicsLayer* layerForContainer() const override { return nullptr; }
    GraphicsLayer* layerForScrolling() const override { return nullptr; }
    GraphicsLayer* layerForHorizontalScrollbar() const override { return nullptr; }
    GraphicsLayer* layerForVerticalScrollbar() const override { return nullptr; }
    bool userInputScrollable(ScrollbarOrientation orientation) const override
    {
        return orientation == HorizontalScrollbar ? m_userInputScrollableX : m_userInputScrollableY;
    }

    DoublePoint clampedScrollOffset(const DoublePoint& offset)
    {
        DoublePoint clampedOffset(offset);
        clampedOffset = clampedOffset.shrunkTo(FloatPoint(maximumScrollPositionDouble()));
        clampedOffset = clampedOffset.expandedTo(FloatPoint(minimumScrollPositionDouble()));
        return clampedOffset;
    }

    bool m_userInputScrollableX;
    bool m_userInputScrollableY;
    DoublePoint m_scrollPosition;
    IntSize m_viewportSize;
    IntSize m_contentsSize;
};

class RootFrameViewStub : public ScrollableAreaStub {
public:
    static RootFrameViewStub* create(const IntSize& viewportSize, const IntSize& contentsSize)
    {
        return new RootFrameViewStub(viewportSize, contentsSize);
    }

    DoublePoint maximumScrollPositionDouble() const override
    {
        return IntPoint(contentsSize() - viewportSize());
    }

private:
    RootFrameViewStub(const IntSize& viewportSize, const IntSize& contentsSize)
        : ScrollableAreaStub(viewportSize, contentsSize)
    {
    }

    int visibleWidth() const override { return m_viewportSize.width(); }
    int visibleHeight() const override { return m_viewportSize.height(); }
};

class VisualViewportStub : public ScrollableAreaStub {
public:
    static VisualViewportStub* create(const IntSize& viewportSize, const IntSize& contentsSize)
    {
        return new VisualViewportStub(viewportSize, contentsSize);
    }

    DoublePoint maximumScrollPositionDouble() const override
    {
        DoubleSize visibleViewport = viewportSize();
        visibleViewport.scale(1 / m_scale);

        DoubleSize maxPosition = DoubleSize(contentsSize()) - visibleViewport;
        return DoublePoint(maxPosition);
    }

    void setScale(float scale) { m_scale = scale; }

private:
    VisualViewportStub(const IntSize& viewportSize, const IntSize& contentsSize)
        : ScrollableAreaStub(viewportSize, contentsSize)
        , m_scale(1)
    {
    }

    int visibleWidth() const override { return m_viewportSize.width() / m_scale; }
    int visibleHeight() const override { return m_viewportSize.height() / m_scale; }
    DoubleRect visibleContentRectDouble(IncludeScrollbarsInRect) const override
    {
        DoubleSize size = m_viewportSize;
        size.scale(1 / m_scale);
        DoubleRect rect(scrollPositionDouble(), size);
        return rect;
    }

    float m_scale;
};

class RootFrameViewportTest : public ::testing::Test {
public:
    RootFrameViewportTest()
    {
    }

protected:
    virtual void SetUp()
    {
    }
};

// Tests that scrolling the viewport when the layout viewport is
// !userInputScrollable (as happens when overflow:hidden is set) works
// correctly, that is, the visual viewport can scroll, but not the layout.
TEST_F(RootFrameViewportTest, UserInputScrollable)
{
    IntSize viewportSize(100, 150);
    RootFrameViewStub* layoutViewport = RootFrameViewStub::create(viewportSize, IntSize(200, 300));
    VisualViewportStub* visualViewport = VisualViewportStub::create(viewportSize, viewportSize);

    ScrollableArea* rootFrameViewport = RootFrameViewport::create(*visualViewport, *layoutViewport);

    visualViewport->setScale(2);

    // Disable just the layout viewport's horizontal scrolling, the
    // RootFrameViewport should remain scrollable overall.
    layoutViewport->setUserInputScrollable(false, true);
    visualViewport->setUserInputScrollable(true, true);

    EXPECT_TRUE(rootFrameViewport->userInputScrollable(HorizontalScrollbar));
    EXPECT_TRUE(rootFrameViewport->userInputScrollable(VerticalScrollbar));

    // Layout viewport shouldn't scroll since it's not horizontally scrollable,
    // but visual viewport should.
    rootFrameViewport->userScroll(ScrollByPixel, FloatSize(300, 0));
    EXPECT_POINT_EQ(DoublePoint(0, 0), layoutViewport->scrollPositionDouble());
    EXPECT_POINT_EQ(DoublePoint(50, 0), visualViewport->scrollPositionDouble());
    EXPECT_POINT_EQ(DoublePoint(50, 0), rootFrameViewport->scrollPositionDouble());

    // Vertical scrolling should be unaffected.
    rootFrameViewport->userScroll(ScrollByPixel, FloatSize(0, 300));
    EXPECT_POINT_EQ(DoublePoint(0, 150), layoutViewport->scrollPositionDouble());
    EXPECT_POINT_EQ(DoublePoint(50, 75), visualViewport->scrollPositionDouble());
    EXPECT_POINT_EQ(DoublePoint(50, 225), rootFrameViewport->scrollPositionDouble());

    // Try the same checks as above but for the vertical direction.
    // ===============================================

    rootFrameViewport->setScrollPosition(DoublePoint(), ProgrammaticScroll);

    // Disable just the layout viewport's vertical scrolling, the
    // RootFrameViewport should remain scrollable overall.
    layoutViewport->setUserInputScrollable(true, false);
    visualViewport->setUserInputScrollable(true, true);

    EXPECT_TRUE(rootFrameViewport->userInputScrollable(HorizontalScrollbar));
    EXPECT_TRUE(rootFrameViewport->userInputScrollable(VerticalScrollbar));

    // Layout viewport shouldn't scroll since it's not vertically scrollable,
    // but visual viewport should.
    rootFrameViewport->userScroll(ScrollByPixel, FloatSize(0, 300));
    EXPECT_POINT_EQ(DoublePoint(0, 0), layoutViewport->scrollPositionDouble());
    EXPECT_POINT_EQ(DoublePoint(0, 75), visualViewport->scrollPositionDouble());
    EXPECT_POINT_EQ(DoublePoint(0, 75), rootFrameViewport->scrollPositionDouble());

    // Horizontal scrolling should be unaffected.
    rootFrameViewport->userScroll(ScrollByPixel, FloatSize(300, 0));
    EXPECT_POINT_EQ(DoublePoint(100, 0), layoutViewport->scrollPositionDouble());
    EXPECT_POINT_EQ(DoublePoint(50, 75), visualViewport->scrollPositionDouble());
    EXPECT_POINT_EQ(DoublePoint(150, 75), rootFrameViewport->scrollPositionDouble());
}

// Make sure scrolls using the scroll animator (scroll(), setScrollPosition())
// work correctly when one of the subviewports is explicitly scrolled without using the
// RootFrameViewport interface.
TEST_F(RootFrameViewportTest, TestScrollAnimatorUpdatedBeforeScroll)
{
    IntSize viewportSize(100, 150);
    RootFrameViewStub* layoutViewport = RootFrameViewStub::create(viewportSize, IntSize(200, 300));
    VisualViewportStub* visualViewport = VisualViewportStub::create(viewportSize, viewportSize);

    ScrollableArea* rootFrameViewport = RootFrameViewport::create(*visualViewport, *layoutViewport);

    visualViewport->setScale(2);

    visualViewport->setScrollPosition(DoublePoint(50, 75), ProgrammaticScroll);
    EXPECT_POINT_EQ(DoublePoint(50, 75), rootFrameViewport->scrollPositionDouble());

    // If the scroll animator doesn't update, it will still think it's at (0, 0) and so it
    // may early exit.
    rootFrameViewport->setScrollPosition(DoublePoint(0, 0), ProgrammaticScroll);
    EXPECT_POINT_EQ(DoublePoint(0, 0), rootFrameViewport->scrollPositionDouble());
    EXPECT_POINT_EQ(DoublePoint(0, 0), visualViewport->scrollPositionDouble());

    // Try again for userScroll()
    visualViewport->setScrollPosition(DoublePoint(50, 75), ProgrammaticScroll);
    EXPECT_POINT_EQ(DoublePoint(50, 75), rootFrameViewport->scrollPositionDouble());

    rootFrameViewport->userScroll(ScrollByPixel, FloatSize(-50, 0));
    EXPECT_POINT_EQ(DoublePoint(0, 75), rootFrameViewport->scrollPositionDouble());
    EXPECT_POINT_EQ(DoublePoint(0, 75), visualViewport->scrollPositionDouble());

    // Make sure the layout viewport is also accounted for.
    rootFrameViewport->setScrollPosition(DoublePoint(0, 0), ProgrammaticScroll);
    layoutViewport->setScrollPosition(DoublePoint(100, 150), ProgrammaticScroll);
    EXPECT_POINT_EQ(DoublePoint(100, 150), rootFrameViewport->scrollPositionDouble());

    rootFrameViewport->userScroll(ScrollByPixel, FloatSize(-100, 0));
    EXPECT_POINT_EQ(DoublePoint(0, 150), rootFrameViewport->scrollPositionDouble());
    EXPECT_POINT_EQ(DoublePoint(0, 150), layoutViewport->scrollPositionDouble());
}

// Test that the scrollIntoView correctly scrolls the main frame
// and visual viewport such that the given rect is centered in the viewport.
TEST_F(RootFrameViewportTest, ScrollIntoView)
{
    IntSize viewportSize(100, 150);
    RootFrameViewStub* layoutViewport = RootFrameViewStub::create(viewportSize, IntSize(200, 300));
    VisualViewportStub* visualViewport = VisualViewportStub::create(viewportSize, viewportSize);

    ScrollableArea* rootFrameViewport = RootFrameViewport::create(*visualViewport, *layoutViewport);

    // Test that the visual viewport is scrolled if the viewport has been
    // resized (as is the case when the ChromeOS keyboard comes up) but not
    // scaled.
    visualViewport->setViewportSize(IntSize(100, 100));
    rootFrameViewport->scrollIntoView(
        LayoutRect(100, 250, 50, 50),
        ScrollAlignment::alignToEdgeIfNeeded,
        ScrollAlignment::alignToEdgeIfNeeded);
    EXPECT_POINT_EQ(DoublePoint(50, 150), layoutViewport->scrollPositionDouble());
    EXPECT_POINT_EQ(DoublePoint(0, 50), visualViewport->scrollPositionDouble());

    rootFrameViewport->scrollIntoView(
        LayoutRect(25, 75, 50, 50),
        ScrollAlignment::alignToEdgeIfNeeded,
        ScrollAlignment::alignToEdgeIfNeeded);
    EXPECT_POINT_EQ(DoublePoint(25, 75), layoutViewport->scrollPositionDouble());
    EXPECT_POINT_EQ(DoublePoint(0, 0), visualViewport->scrollPositionDouble());

    // Reset the visual viewport's size, scale the page, and repeat the test
    visualViewport->setViewportSize(IntSize(100, 150));
    visualViewport->setScale(2);
    rootFrameViewport->setScrollPosition(DoublePoint(), ProgrammaticScroll);

    rootFrameViewport->scrollIntoView(
        LayoutRect(50, 75, 50, 75),
        ScrollAlignment::alignToEdgeIfNeeded,
        ScrollAlignment::alignToEdgeIfNeeded);
    EXPECT_POINT_EQ(DoublePoint(0, 0), layoutViewport->scrollPositionDouble());
    EXPECT_POINT_EQ(DoublePoint(50, 75), visualViewport->scrollPositionDouble());

    rootFrameViewport->scrollIntoView(
        LayoutRect(190, 290, 10, 10),
        ScrollAlignment::alignToEdgeIfNeeded,
        ScrollAlignment::alignToEdgeIfNeeded);
    EXPECT_POINT_EQ(DoublePoint(100, 150), layoutViewport->scrollPositionDouble());
    EXPECT_POINT_EQ(DoublePoint(50, 75), visualViewport->scrollPositionDouble());

    // Scrolling into view the viewport rect itself should be a no-op.
    visualViewport->setViewportSize(IntSize(100, 100));
    visualViewport->setScale(1.5f);
    visualViewport->setScrollPosition(DoublePoint(0, 10), ProgrammaticScroll);
    layoutViewport->setScrollPosition(DoublePoint(50, 50), ProgrammaticScroll);
    rootFrameViewport->setScrollPosition(rootFrameViewport->scrollPositionDouble(), ProgrammaticScroll);

    rootFrameViewport->scrollIntoView(
        LayoutRect(rootFrameViewport->visibleContentRectDouble(ExcludeScrollbars)),
        ScrollAlignment::alignToEdgeIfNeeded,
        ScrollAlignment::alignToEdgeIfNeeded);
    EXPECT_POINT_EQ(DoublePoint(50, 50), layoutViewport->scrollPositionDouble());
    EXPECT_POINT_EQ(DoublePoint(0, 10), visualViewport->scrollPositionDouble());

    rootFrameViewport->scrollIntoView(
        LayoutRect(rootFrameViewport->visibleContentRectDouble(ExcludeScrollbars)),
        ScrollAlignment::alignCenterAlways,
        ScrollAlignment::alignCenterAlways);
    EXPECT_POINT_EQ(DoublePoint(50, 50), layoutViewport->scrollPositionDouble());
    EXPECT_POINT_EQ(DoublePoint(0, 10), visualViewport->scrollPositionDouble());

    rootFrameViewport->scrollIntoView(
        LayoutRect(rootFrameViewport->visibleContentRectDouble(ExcludeScrollbars)),
        ScrollAlignment::alignTopAlways,
        ScrollAlignment::alignTopAlways);
    EXPECT_POINT_EQ(DoublePoint(50, 50), layoutViewport->scrollPositionDouble());
    EXPECT_POINT_EQ(DoublePoint(0, 10), visualViewport->scrollPositionDouble());
}

// Tests that the setScrollPosition method works correctly with both viewports.
TEST_F(RootFrameViewportTest, SetScrollPosition)
{
    IntSize viewportSize(500, 500);
    RootFrameViewStub* layoutViewport = RootFrameViewStub::create(viewportSize, IntSize(1000, 2000));
    VisualViewportStub* visualViewport = VisualViewportStub::create(viewportSize, viewportSize);

    ScrollableArea* rootFrameViewport = RootFrameViewport::create(*visualViewport, *layoutViewport);

    visualViewport->setScale(2);

    // Ensure that the visual viewport scrolls first.
    rootFrameViewport->setScrollPosition(DoublePoint(100, 100), ProgrammaticScroll);
    EXPECT_POINT_EQ(DoublePoint(100, 100), visualViewport->scrollPositionDouble());
    EXPECT_POINT_EQ(DoublePoint(0, 0), layoutViewport->scrollPositionDouble());

    // Scroll to the visual viewport's extent, the layout viewport should scroll the
    // remainder.
    rootFrameViewport->setScrollPosition(DoublePoint(300, 400), ProgrammaticScroll);
    EXPECT_POINT_EQ(DoublePoint(250, 250), visualViewport->scrollPositionDouble());
    EXPECT_POINT_EQ(DoublePoint(50, 150), layoutViewport->scrollPositionDouble());

    // Only the layout viewport should scroll further. Make sure it doesn't scroll
    // out of bounds.
    rootFrameViewport->setScrollPosition(DoublePoint(780, 1780), ProgrammaticScroll);
    EXPECT_POINT_EQ(DoublePoint(250, 250), visualViewport->scrollPositionDouble());
    EXPECT_POINT_EQ(DoublePoint(500, 1500), layoutViewport->scrollPositionDouble());

    // Scroll all the way back.
    rootFrameViewport->setScrollPosition(DoublePoint(0, 0), ProgrammaticScroll);
    EXPECT_POINT_EQ(DoublePoint(0, 0), visualViewport->scrollPositionDouble());
    EXPECT_POINT_EQ(DoublePoint(0, 0), layoutViewport->scrollPositionDouble());
}

// Tests that the visible rect (i.e. visual viewport rect) is correctly
// calculated, taking into account both viewports and page scale.
TEST_F(RootFrameViewportTest, VisibleContentRect)
{
    IntSize viewportSize(500, 401);
    RootFrameViewStub* layoutViewport = RootFrameViewStub::create(viewportSize, IntSize(1000, 2000));
    VisualViewportStub* visualViewport = VisualViewportStub::create(viewportSize, viewportSize);

    ScrollableArea* rootFrameViewport = RootFrameViewport::create(*visualViewport, *layoutViewport);

    rootFrameViewport->setScrollPosition(DoublePoint(100, 75), ProgrammaticScroll);

    EXPECT_POINT_EQ(DoublePoint(100, 75), rootFrameViewport->visibleContentRect().location());
    EXPECT_POINT_EQ(DoublePoint(100, 75), rootFrameViewport->visibleContentRectDouble().location());
    EXPECT_SIZE_EQ(DoubleSize(500, 401), rootFrameViewport->visibleContentRect().size());
    EXPECT_SIZE_EQ(DoubleSize(500, 401), rootFrameViewport->visibleContentRectDouble().size());

    visualViewport->setScale(2);

    EXPECT_POINT_EQ(DoublePoint(100, 75), rootFrameViewport->visibleContentRect().location());
    EXPECT_POINT_EQ(DoublePoint(100, 75), rootFrameViewport->visibleContentRectDouble().location());
    EXPECT_SIZE_EQ(DoubleSize(250, 201), rootFrameViewport->visibleContentRect().size());
    EXPECT_SIZE_EQ(DoubleSize(250, 200.5), rootFrameViewport->visibleContentRectDouble().size());
}

// Tests that scrolls on the root frame scroll the visual viewport before
// trying to scroll the layout viewport.
TEST_F(RootFrameViewportTest, ViewportScrollOrder)
{
    IntSize viewportSize(100, 100);
    RootFrameViewStub* layoutViewport = RootFrameViewStub::create(viewportSize, IntSize(200, 300));
    VisualViewportStub* visualViewport = VisualViewportStub::create(viewportSize, viewportSize);

    ScrollableArea* rootFrameViewport =
        RootFrameViewport::create(*visualViewport, *layoutViewport);

    visualViewport->setScale(2);

    rootFrameViewport->setScrollPosition(DoublePoint(40, 40), UserScroll);
    EXPECT_POINT_EQ(DoublePoint(40, 40), visualViewport->scrollPositionDouble());
    EXPECT_POINT_EQ(DoublePoint(0, 0), layoutViewport->scrollPositionDouble());

    rootFrameViewport->setScrollPosition(DoublePoint(60, 60), ProgrammaticScroll);
    EXPECT_POINT_EQ(DoublePoint(50, 50), visualViewport->scrollPositionDouble());
    EXPECT_POINT_EQ(DoublePoint(10, 10), layoutViewport->scrollPositionDouble());
}

} // namespace blink
