// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/RootFrameViewport.h"

#include "core/layout/ScrollAlignment.h"
#include "platform/geometry/DoubleRect.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/scroll/ScrollableArea.h"
#include "testing/gtest/include/gtest/gtest.h"

#define EXPECT_POINT_EQ(expected, actual)    \
  do {                                       \
    EXPECT_EQ((expected).x(), (actual).x()); \
    EXPECT_EQ((expected).y(), (actual).y()); \
  } while (false)
#define EXPECT_SIZE_EQ(expected, actual)               \
  do {                                                 \
    EXPECT_EQ((expected).width(), (actual).width());   \
    EXPECT_EQ((expected).height(), (actual).height()); \
  } while (false)

namespace blink {

class ScrollableAreaStub : public GarbageCollectedFinalized<ScrollableAreaStub>,
                           public ScrollableArea {
  USING_GARBAGE_COLLECTED_MIXIN(ScrollableAreaStub);

 public:
  static ScrollableAreaStub* create(const IntSize& viewportSize,
                                    const IntSize& contentsSize) {
    return new ScrollableAreaStub(viewportSize, contentsSize);
  }

  void setViewportSize(const IntSize& viewportSize) {
    m_viewportSize = viewportSize;
  }

  IntSize viewportSize() const { return m_viewportSize; }

  // ScrollableArea Impl
  int scrollSize(ScrollbarOrientation orientation) const override {
    IntSize scrollDimensions =
        maximumScrollOffsetInt() - minimumScrollOffsetInt();

    return (orientation == HorizontalScrollbar) ? scrollDimensions.width()
                                                : scrollDimensions.height();
  }

  void setUserInputScrollable(bool x, bool y) {
    m_userInputScrollableX = x;
    m_userInputScrollableY = y;
  }

  IntSize scrollOffsetInt() const override {
    return flooredIntSize(m_scrollOffset);
  }
  ScrollOffset scrollOffset() const override { return m_scrollOffset; }
  IntSize minimumScrollOffsetInt() const override { return IntSize(); }
  ScrollOffset minimumScrollOffset() const override { return ScrollOffset(); }
  IntSize maximumScrollOffsetInt() const override {
    return flooredIntSize(maximumScrollOffset());
  }

  IntSize contentsSize() const override { return m_contentsSize; }
  void setContentSize(const IntSize& contentsSize) {
    m_contentsSize = contentsSize;
  }

  DEFINE_INLINE_VIRTUAL_TRACE() { ScrollableArea::trace(visitor); }

 protected:
  ScrollableAreaStub(const IntSize& viewportSize, const IntSize& contentsSize)
      : m_userInputScrollableX(true),
        m_userInputScrollableY(true),
        m_viewportSize(viewportSize),
        m_contentsSize(contentsSize) {}

  void updateScrollOffset(const ScrollOffset& offset, ScrollType) override {
    m_scrollOffset = offset;
  }
  bool shouldUseIntegerScrollOffset() const override { return true; }
  LayoutRect visualRectForScrollbarParts() const override {
    ASSERT_NOT_REACHED();
    return LayoutRect();
  }
  bool isActive() const override { return true; }
  bool isScrollCornerVisible() const override { return true; }
  IntRect scrollCornerRect() const override { return IntRect(); }
  bool scrollbarsCanBeActive() const override { return true; }
  IntRect scrollableAreaBoundingBox() const override { return IntRect(); }
  bool shouldPlaceVerticalScrollbarOnLeft() const override { return true; }
  void scrollControlWasSetNeedsPaintInvalidation() override {}
  GraphicsLayer* layerForContainer() const override { return nullptr; }
  GraphicsLayer* layerForScrolling() const override { return nullptr; }
  GraphicsLayer* layerForHorizontalScrollbar() const override {
    return nullptr;
  }
  GraphicsLayer* layerForVerticalScrollbar() const override { return nullptr; }
  bool userInputScrollable(ScrollbarOrientation orientation) const override {
    return orientation == HorizontalScrollbar ? m_userInputScrollableX
                                              : m_userInputScrollableY;
  }

  ScrollOffset clampedScrollOffset(const ScrollOffset& offset) {
    ScrollOffset minOffset = minimumScrollOffset();
    ScrollOffset maxOffset = maximumScrollOffset();
    float width = std::min(std::max(offset.width(), minOffset.width()),
                           maxOffset.width());
    float height = std::min(std::max(offset.height(), minOffset.height()),
                            maxOffset.height());
    return ScrollOffset(width, height);
  }

  bool m_userInputScrollableX;
  bool m_userInputScrollableY;
  ScrollOffset m_scrollOffset;
  IntSize m_viewportSize;
  IntSize m_contentsSize;
};

class RootFrameViewStub : public ScrollableAreaStub {
 public:
  static RootFrameViewStub* create(const IntSize& viewportSize,
                                   const IntSize& contentsSize) {
    return new RootFrameViewStub(viewportSize, contentsSize);
  }

  ScrollOffset maximumScrollOffset() const override {
    return ScrollOffset(contentsSize() - viewportSize());
  }

 private:
  RootFrameViewStub(const IntSize& viewportSize, const IntSize& contentsSize)
      : ScrollableAreaStub(viewportSize, contentsSize) {}

  int visibleWidth() const override { return m_viewportSize.width(); }
  int visibleHeight() const override { return m_viewportSize.height(); }
};

class VisualViewportStub : public ScrollableAreaStub {
 public:
  static VisualViewportStub* create(const IntSize& viewportSize,
                                    const IntSize& contentsSize) {
    return new VisualViewportStub(viewportSize, contentsSize);
  }

  ScrollOffset maximumScrollOffset() const override {
    ScrollOffset visibleViewport(viewportSize());
    visibleViewport.scale(1 / m_scale);

    ScrollOffset maxOffset = ScrollOffset(contentsSize()) - visibleViewport;
    return ScrollOffset(maxOffset);
  }

  void setScale(float scale) { m_scale = scale; }

 private:
  VisualViewportStub(const IntSize& viewportSize, const IntSize& contentsSize)
      : ScrollableAreaStub(viewportSize, contentsSize), m_scale(1) {}

  int visibleWidth() const override { return m_viewportSize.width() / m_scale; }
  int visibleHeight() const override {
    return m_viewportSize.height() / m_scale;
  }
  IntRect visibleContentRect(IncludeScrollbarsInRect) const override {
    FloatSize size(m_viewportSize);
    size.scale(1 / m_scale);
    return IntRect(IntPoint(flooredIntSize(scrollOffset())),
                   expandedIntSize(size));
  }

  float m_scale;
};

class RootFrameViewportTest : public ::testing::Test {
 public:
  RootFrameViewportTest() {}

 protected:
  virtual void SetUp() {}
};

// Tests that scrolling the viewport when the layout viewport is
// !userInputScrollable (as happens when overflow:hidden is set) works
// correctly, that is, the visual viewport can scroll, but not the layout.
TEST_F(RootFrameViewportTest, UserInputScrollable) {
  IntSize viewportSize(100, 150);
  RootFrameViewStub* layoutViewport =
      RootFrameViewStub::create(viewportSize, IntSize(200, 300));
  VisualViewportStub* visualViewport =
      VisualViewportStub::create(viewportSize, viewportSize);

  ScrollableArea* rootFrameViewport =
      RootFrameViewport::create(*visualViewport, *layoutViewport);

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
  EXPECT_SIZE_EQ(ScrollOffset(0, 0), layoutViewport->scrollOffset());
  EXPECT_SIZE_EQ(ScrollOffset(50, 0), visualViewport->scrollOffset());
  EXPECT_SIZE_EQ(ScrollOffset(50, 0), rootFrameViewport->scrollOffset());

  // Vertical scrolling should be unaffected.
  rootFrameViewport->userScroll(ScrollByPixel, FloatSize(0, 300));
  EXPECT_SIZE_EQ(ScrollOffset(0, 150), layoutViewport->scrollOffset());
  EXPECT_SIZE_EQ(ScrollOffset(50, 75), visualViewport->scrollOffset());
  EXPECT_SIZE_EQ(ScrollOffset(50, 225), rootFrameViewport->scrollOffset());

  // Try the same checks as above but for the vertical direction.
  // ===============================================

  rootFrameViewport->setScrollOffset(ScrollOffset(), ProgrammaticScroll);

  // Disable just the layout viewport's vertical scrolling, the
  // RootFrameViewport should remain scrollable overall.
  layoutViewport->setUserInputScrollable(true, false);
  visualViewport->setUserInputScrollable(true, true);

  EXPECT_TRUE(rootFrameViewport->userInputScrollable(HorizontalScrollbar));
  EXPECT_TRUE(rootFrameViewport->userInputScrollable(VerticalScrollbar));

  // Layout viewport shouldn't scroll since it's not vertically scrollable,
  // but visual viewport should.
  rootFrameViewport->userScroll(ScrollByPixel, FloatSize(0, 300));
  EXPECT_SIZE_EQ(ScrollOffset(0, 0), layoutViewport->scrollOffset());
  EXPECT_SIZE_EQ(ScrollOffset(0, 75), visualViewport->scrollOffset());
  EXPECT_SIZE_EQ(ScrollOffset(0, 75), rootFrameViewport->scrollOffset());

  // Horizontal scrolling should be unaffected.
  rootFrameViewport->userScroll(ScrollByPixel, FloatSize(300, 0));
  EXPECT_SIZE_EQ(ScrollOffset(100, 0), layoutViewport->scrollOffset());
  EXPECT_SIZE_EQ(ScrollOffset(50, 75), visualViewport->scrollOffset());
  EXPECT_SIZE_EQ(ScrollOffset(150, 75), rootFrameViewport->scrollOffset());
}

// Make sure scrolls using the scroll animator (scroll(), setScrollOffset())
// work correctly when one of the subviewports is explicitly scrolled without
// using the // RootFrameViewport interface.
TEST_F(RootFrameViewportTest, TestScrollAnimatorUpdatedBeforeScroll) {
  IntSize viewportSize(100, 150);
  RootFrameViewStub* layoutViewport =
      RootFrameViewStub::create(viewportSize, IntSize(200, 300));
  VisualViewportStub* visualViewport =
      VisualViewportStub::create(viewportSize, viewportSize);

  ScrollableArea* rootFrameViewport =
      RootFrameViewport::create(*visualViewport, *layoutViewport);

  visualViewport->setScale(2);

  visualViewport->setScrollOffset(ScrollOffset(50, 75), ProgrammaticScroll);
  EXPECT_SIZE_EQ(ScrollOffset(50, 75), rootFrameViewport->scrollOffset());

  // If the scroll animator doesn't update, it will still think it's at (0, 0)
  // and so it may early exit.
  rootFrameViewport->setScrollOffset(ScrollOffset(0, 0), ProgrammaticScroll);
  EXPECT_SIZE_EQ(ScrollOffset(0, 0), rootFrameViewport->scrollOffset());
  EXPECT_SIZE_EQ(ScrollOffset(0, 0), visualViewport->scrollOffset());

  // Try again for userScroll()
  visualViewport->setScrollOffset(ScrollOffset(50, 75), ProgrammaticScroll);
  EXPECT_SIZE_EQ(ScrollOffset(50, 75), rootFrameViewport->scrollOffset());

  rootFrameViewport->userScroll(ScrollByPixel, FloatSize(-50, 0));
  EXPECT_SIZE_EQ(ScrollOffset(0, 75), rootFrameViewport->scrollOffset());
  EXPECT_SIZE_EQ(ScrollOffset(0, 75), visualViewport->scrollOffset());

  // Make sure the layout viewport is also accounted for.
  rootFrameViewport->setScrollOffset(ScrollOffset(0, 0), ProgrammaticScroll);
  layoutViewport->setScrollOffset(ScrollOffset(100, 150), ProgrammaticScroll);
  EXPECT_SIZE_EQ(ScrollOffset(100, 150), rootFrameViewport->scrollOffset());

  rootFrameViewport->userScroll(ScrollByPixel, FloatSize(-100, 0));
  EXPECT_SIZE_EQ(ScrollOffset(0, 150), rootFrameViewport->scrollOffset());
  EXPECT_SIZE_EQ(ScrollOffset(0, 150), layoutViewport->scrollOffset());
}

// Test that the scrollIntoView correctly scrolls the main frame
// and visual viewport such that the given rect is centered in the viewport.
TEST_F(RootFrameViewportTest, ScrollIntoView) {
  IntSize viewportSize(100, 150);
  RootFrameViewStub* layoutViewport =
      RootFrameViewStub::create(viewportSize, IntSize(200, 300));
  VisualViewportStub* visualViewport =
      VisualViewportStub::create(viewportSize, viewportSize);

  ScrollableArea* rootFrameViewport =
      RootFrameViewport::create(*visualViewport, *layoutViewport);

  // Test that the visual viewport is scrolled if the viewport has been
  // resized (as is the case when the ChromeOS keyboard comes up) but not
  // scaled.
  visualViewport->setViewportSize(IntSize(100, 100));
  rootFrameViewport->scrollIntoView(LayoutRect(100, 250, 50, 50),
                                    ScrollAlignment::alignToEdgeIfNeeded,
                                    ScrollAlignment::alignToEdgeIfNeeded);
  EXPECT_SIZE_EQ(ScrollOffset(50, 150), layoutViewport->scrollOffset());
  EXPECT_SIZE_EQ(ScrollOffset(0, 50), visualViewport->scrollOffset());

  rootFrameViewport->scrollIntoView(LayoutRect(25, 75, 50, 50),
                                    ScrollAlignment::alignToEdgeIfNeeded,
                                    ScrollAlignment::alignToEdgeIfNeeded);
  EXPECT_SIZE_EQ(ScrollOffset(25, 75), layoutViewport->scrollOffset());
  EXPECT_SIZE_EQ(ScrollOffset(0, 0), visualViewport->scrollOffset());

  // Reset the visual viewport's size, scale the page, and repeat the test
  visualViewport->setViewportSize(IntSize(100, 150));
  visualViewport->setScale(2);
  rootFrameViewport->setScrollOffset(ScrollOffset(), ProgrammaticScroll);

  rootFrameViewport->scrollIntoView(LayoutRect(50, 75, 50, 75),
                                    ScrollAlignment::alignToEdgeIfNeeded,
                                    ScrollAlignment::alignToEdgeIfNeeded);
  EXPECT_SIZE_EQ(ScrollOffset(0, 0), layoutViewport->scrollOffset());
  EXPECT_SIZE_EQ(ScrollOffset(50, 75), visualViewport->scrollOffset());

  rootFrameViewport->scrollIntoView(LayoutRect(190, 290, 10, 10),
                                    ScrollAlignment::alignToEdgeIfNeeded,
                                    ScrollAlignment::alignToEdgeIfNeeded);
  EXPECT_SIZE_EQ(ScrollOffset(100, 150), layoutViewport->scrollOffset());
  EXPECT_SIZE_EQ(ScrollOffset(50, 75), visualViewport->scrollOffset());

  // Scrolling into view the viewport rect itself should be a no-op.
  visualViewport->setViewportSize(IntSize(100, 100));
  visualViewport->setScale(1.5f);
  visualViewport->setScrollOffset(ScrollOffset(0, 10), ProgrammaticScroll);
  layoutViewport->setScrollOffset(ScrollOffset(50, 50), ProgrammaticScroll);
  rootFrameViewport->setScrollOffset(rootFrameViewport->scrollOffset(),
                                     ProgrammaticScroll);

  rootFrameViewport->scrollIntoView(
      LayoutRect(rootFrameViewport->visibleContentRect(ExcludeScrollbars)),
      ScrollAlignment::alignToEdgeIfNeeded,
      ScrollAlignment::alignToEdgeIfNeeded);
  EXPECT_SIZE_EQ(ScrollOffset(50, 50), layoutViewport->scrollOffset());
  EXPECT_SIZE_EQ(ScrollOffset(0, 10), visualViewport->scrollOffset());

  rootFrameViewport->scrollIntoView(
      LayoutRect(rootFrameViewport->visibleContentRect(ExcludeScrollbars)),
      ScrollAlignment::alignCenterAlways, ScrollAlignment::alignCenterAlways);
  EXPECT_SIZE_EQ(ScrollOffset(50, 50), layoutViewport->scrollOffset());
  EXPECT_SIZE_EQ(ScrollOffset(0, 10), visualViewport->scrollOffset());

  rootFrameViewport->scrollIntoView(
      LayoutRect(rootFrameViewport->visibleContentRect(ExcludeScrollbars)),
      ScrollAlignment::alignTopAlways, ScrollAlignment::alignTopAlways);
  EXPECT_SIZE_EQ(ScrollOffset(50, 50), layoutViewport->scrollOffset());
  EXPECT_SIZE_EQ(ScrollOffset(0, 10), visualViewport->scrollOffset());
}

// Tests that the setScrollOffset method works correctly with both viewports.
TEST_F(RootFrameViewportTest, SetScrollOffset) {
  IntSize viewportSize(500, 500);
  RootFrameViewStub* layoutViewport =
      RootFrameViewStub::create(viewportSize, IntSize(1000, 2000));
  VisualViewportStub* visualViewport =
      VisualViewportStub::create(viewportSize, viewportSize);

  ScrollableArea* rootFrameViewport =
      RootFrameViewport::create(*visualViewport, *layoutViewport);

  visualViewport->setScale(2);

  // Ensure that the visual viewport scrolls first.
  rootFrameViewport->setScrollOffset(ScrollOffset(100, 100),
                                     ProgrammaticScroll);
  EXPECT_SIZE_EQ(ScrollOffset(100, 100), visualViewport->scrollOffset());
  EXPECT_SIZE_EQ(ScrollOffset(0, 0), layoutViewport->scrollOffset());

  // Scroll to the visual viewport's extent, the layout viewport should scroll
  // the remainder.
  rootFrameViewport->setScrollOffset(ScrollOffset(300, 400),
                                     ProgrammaticScroll);
  EXPECT_SIZE_EQ(ScrollOffset(250, 250), visualViewport->scrollOffset());
  EXPECT_SIZE_EQ(ScrollOffset(50, 150), layoutViewport->scrollOffset());

  // Only the layout viewport should scroll further. Make sure it doesn't scroll
  // out of bounds.
  rootFrameViewport->setScrollOffset(ScrollOffset(780, 1780),
                                     ProgrammaticScroll);
  EXPECT_SIZE_EQ(ScrollOffset(250, 250), visualViewport->scrollOffset());
  EXPECT_SIZE_EQ(ScrollOffset(500, 1500), layoutViewport->scrollOffset());

  // Scroll all the way back.
  rootFrameViewport->setScrollOffset(ScrollOffset(0, 0), ProgrammaticScroll);
  EXPECT_SIZE_EQ(ScrollOffset(0, 0), visualViewport->scrollOffset());
  EXPECT_SIZE_EQ(ScrollOffset(0, 0), layoutViewport->scrollOffset());
}

// Tests that the visible rect (i.e. visual viewport rect) is correctly
// calculated, taking into account both viewports and page scale.
TEST_F(RootFrameViewportTest, VisibleContentRect) {
  IntSize viewportSize(500, 401);
  RootFrameViewStub* layoutViewport =
      RootFrameViewStub::create(viewportSize, IntSize(1000, 2000));
  VisualViewportStub* visualViewport =
      VisualViewportStub::create(viewportSize, viewportSize);

  ScrollableArea* rootFrameViewport =
      RootFrameViewport::create(*visualViewport, *layoutViewport);

  rootFrameViewport->setScrollOffset(ScrollOffset(100, 75), ProgrammaticScroll);

  EXPECT_POINT_EQ(IntPoint(100, 75),
                  rootFrameViewport->visibleContentRect().location());
  EXPECT_SIZE_EQ(ScrollOffset(500, 401),
                 rootFrameViewport->visibleContentRect().size());

  visualViewport->setScale(2);

  EXPECT_POINT_EQ(IntPoint(100, 75),
                  rootFrameViewport->visibleContentRect().location());
  EXPECT_SIZE_EQ(ScrollOffset(250, 201),
                 rootFrameViewport->visibleContentRect().size());
}

// Tests that scrolls on the root frame scroll the visual viewport before
// trying to scroll the layout viewport.
TEST_F(RootFrameViewportTest, ViewportScrollOrder) {
  IntSize viewportSize(100, 100);
  RootFrameViewStub* layoutViewport =
      RootFrameViewStub::create(viewportSize, IntSize(200, 300));
  VisualViewportStub* visualViewport =
      VisualViewportStub::create(viewportSize, viewportSize);

  ScrollableArea* rootFrameViewport =
      RootFrameViewport::create(*visualViewport, *layoutViewport);

  visualViewport->setScale(2);

  rootFrameViewport->setScrollOffset(ScrollOffset(40, 40), UserScroll);
  EXPECT_SIZE_EQ(ScrollOffset(40, 40), visualViewport->scrollOffset());
  EXPECT_SIZE_EQ(ScrollOffset(0, 0), layoutViewport->scrollOffset());

  rootFrameViewport->setScrollOffset(ScrollOffset(60, 60), ProgrammaticScroll);
  EXPECT_SIZE_EQ(ScrollOffset(50, 50), visualViewport->scrollOffset());
  EXPECT_SIZE_EQ(ScrollOffset(10, 10), layoutViewport->scrollOffset());
}

// Tests that setting an alternate layout viewport scrolls the alternate
// instead of the original.
TEST_F(RootFrameViewportTest, SetAlternateLayoutViewport) {
  IntSize viewportSize(100, 100);
  RootFrameViewStub* layoutViewport =
      RootFrameViewStub::create(viewportSize, IntSize(200, 300));
  VisualViewportStub* visualViewport =
      VisualViewportStub::create(viewportSize, viewportSize);

  RootFrameViewStub* alternateScroller =
      RootFrameViewStub::create(viewportSize, IntSize(600, 500));

  RootFrameViewport* rootFrameViewport =
      RootFrameViewport::create(*visualViewport, *layoutViewport);

  visualViewport->setScale(2);

  rootFrameViewport->setScrollOffset(ScrollOffset(100, 100), UserScroll);
  EXPECT_SIZE_EQ(ScrollOffset(50, 50), visualViewport->scrollOffset());
  EXPECT_SIZE_EQ(ScrollOffset(50, 50), layoutViewport->scrollOffset());
  EXPECT_SIZE_EQ(ScrollOffset(100, 100), rootFrameViewport->scrollOffset());

  rootFrameViewport->setLayoutViewport(*alternateScroller);
  EXPECT_SIZE_EQ(ScrollOffset(50, 50), visualViewport->scrollOffset());
  EXPECT_SIZE_EQ(ScrollOffset(0, 0), alternateScroller->scrollOffset());
  EXPECT_SIZE_EQ(ScrollOffset(50, 50), rootFrameViewport->scrollOffset());

  rootFrameViewport->setScrollOffset(ScrollOffset(200, 200), UserScroll);
  EXPECT_SIZE_EQ(ScrollOffset(50, 50), visualViewport->scrollOffset());
  EXPECT_SIZE_EQ(ScrollOffset(150, 150), alternateScroller->scrollOffset());
  EXPECT_SIZE_EQ(ScrollOffset(200, 200), rootFrameViewport->scrollOffset());
  EXPECT_SIZE_EQ(ScrollOffset(50, 50), layoutViewport->scrollOffset());

  EXPECT_SIZE_EQ(ScrollOffset(550, 450),
                 rootFrameViewport->maximumScrollOffset());
}

}  // namespace blink
