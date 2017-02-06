// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scroll/ScrollableArea.h"

#include "platform/graphics/Color.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/scroll/ScrollbarTestSuite.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "platform/scroll/ScrollbarThemeMock.h"
#include "platform/testing/FakeGraphicsLayer.h"
#include "platform/testing/FakeGraphicsLayerClient.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "public/platform/Platform.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

using testing::_;
using testing::Return;

class ScrollbarThemeWithMockInvalidation : public ScrollbarThemeMock {
public:
    MOCK_CONST_METHOD0(shouldRepaintAllPartsOnInvalidation, bool());
    MOCK_CONST_METHOD3(invalidateOnThumbPositionChange, ScrollbarPart(const ScrollbarThemeClient&, float, float));
};

} // namespace

class ScrollableAreaTest : public ScrollbarTestSuite {
};

TEST_F(ScrollableAreaTest, ScrollAnimatorCurrentPositionShouldBeSync)
{
    MockScrollableArea* scrollableArea = MockScrollableArea::create(IntPoint(0, 100));
    scrollableArea->setScrollPosition(IntPoint(0, 10000), CompositorScroll);
    EXPECT_EQ(100.0, scrollableArea->scrollAnimator().currentPosition().y());
}

TEST_F(ScrollableAreaTest, ScrollbarTrackAndThumbRepaint)
{
    ScrollbarThemeWithMockInvalidation theme;
    MockScrollableArea* scrollableArea = MockScrollableArea::create(IntPoint(0, 100));
    Scrollbar* scrollbar = Scrollbar::createForTesting(scrollableArea, HorizontalScrollbar, RegularScrollbar, &theme);

    EXPECT_CALL(theme, shouldRepaintAllPartsOnInvalidation()).WillRepeatedly(Return(true));
    EXPECT_TRUE(scrollbar->trackNeedsRepaint());
    EXPECT_TRUE(scrollbar->thumbNeedsRepaint());
    scrollbar->setNeedsPaintInvalidation(NoPart);
    EXPECT_TRUE(scrollbar->trackNeedsRepaint());
    EXPECT_TRUE(scrollbar->thumbNeedsRepaint());

    scrollbar->clearTrackNeedsRepaint();
    scrollbar->clearThumbNeedsRepaint();
    EXPECT_FALSE(scrollbar->trackNeedsRepaint());
    EXPECT_FALSE(scrollbar->thumbNeedsRepaint());
    scrollbar->setNeedsPaintInvalidation(ThumbPart);
    EXPECT_TRUE(scrollbar->trackNeedsRepaint());
    EXPECT_TRUE(scrollbar->thumbNeedsRepaint());

    // When not all parts are repainted on invalidation,
    // setNeedsPaintInvalidation sets repaint bits only on the requested parts.
    EXPECT_CALL(theme, shouldRepaintAllPartsOnInvalidation()).WillRepeatedly(Return(false));
    scrollbar->clearTrackNeedsRepaint();
    scrollbar->clearThumbNeedsRepaint();
    EXPECT_FALSE(scrollbar->trackNeedsRepaint());
    EXPECT_FALSE(scrollbar->thumbNeedsRepaint());
    scrollbar->setNeedsPaintInvalidation(ThumbPart);
    EXPECT_FALSE(scrollbar->trackNeedsRepaint());
    EXPECT_TRUE(scrollbar->thumbNeedsRepaint());

    // Forced GC in order to finalize objects depending on the mock object.
    ThreadHeap::collectAllGarbage();
}

TEST_F(ScrollableAreaTest, ScrollbarGraphicsLayerInvalidation)
{
    ScrollbarTheme::setMockScrollbarsEnabled(true);
    MockScrollableArea* scrollableArea = MockScrollableArea::create(IntPoint(0, 100));
    FakeGraphicsLayerClient graphicsLayerClient;
    graphicsLayerClient.setIsTrackingPaintInvalidations(true);
    FakeGraphicsLayer graphicsLayer(&graphicsLayerClient);
    graphicsLayer.setDrawsContent(true);
    graphicsLayer.setSize(FloatSize(111, 222));

    EXPECT_CALL(*scrollableArea, layerForHorizontalScrollbar()).WillRepeatedly(Return(&graphicsLayer));

    Scrollbar* scrollbar = Scrollbar::create(scrollableArea, HorizontalScrollbar, RegularScrollbar, nullptr);
    graphicsLayer.resetTrackedPaintInvalidations();
    scrollbar->setNeedsPaintInvalidation(NoPart);
    EXPECT_TRUE(graphicsLayer.hasTrackedPaintInvalidations());

    // Forced GC in order to finalize objects depending on the mock object.
    ThreadHeap::collectAllGarbage();
}

TEST_F(ScrollableAreaTest, InvalidatesNonCompositedScrollbarsWhenThumbMoves)
{
    ScrollbarThemeWithMockInvalidation theme;
    MockScrollableArea* scrollableArea = MockScrollableArea::create(IntPoint(100, 100));
    Scrollbar* horizontalScrollbar = Scrollbar::createForTesting(scrollableArea, HorizontalScrollbar, RegularScrollbar, &theme);
    Scrollbar* verticalScrollbar = Scrollbar::createForTesting(scrollableArea, VerticalScrollbar, RegularScrollbar, &theme);
    EXPECT_CALL(*scrollableArea, horizontalScrollbar()).WillRepeatedly(Return(horizontalScrollbar));
    EXPECT_CALL(*scrollableArea, verticalScrollbar()).WillRepeatedly(Return(verticalScrollbar));

    // Regardless of whether the theme invalidates any parts, non-composited
    // scrollbars have to be repainted if the thumb moves.
    EXPECT_CALL(*scrollableArea, layerForHorizontalScrollbar()).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(*scrollableArea, layerForVerticalScrollbar()).WillRepeatedly(Return(nullptr));
    ASSERT_FALSE(scrollableArea->hasLayerForVerticalScrollbar());
    ASSERT_FALSE(scrollableArea->hasLayerForHorizontalScrollbar());
    EXPECT_CALL(theme, shouldRepaintAllPartsOnInvalidation()).WillRepeatedly(Return(false));
    EXPECT_CALL(theme, invalidateOnThumbPositionChange(_, _, _)).WillRepeatedly(Return(NoPart));

    // A scroll in each direction should only invalidate one scrollbar.
    scrollableArea->setScrollPosition(DoublePoint(0, 50), ProgrammaticScroll);
    EXPECT_FALSE(scrollableArea->horizontalScrollbarNeedsPaintInvalidation());
    EXPECT_TRUE(scrollableArea->verticalScrollbarNeedsPaintInvalidation());
    scrollableArea->clearNeedsPaintInvalidationForScrollControls();
    scrollableArea->setScrollPosition(DoublePoint(50, 50), ProgrammaticScroll);
    EXPECT_TRUE(scrollableArea->horizontalScrollbarNeedsPaintInvalidation());
    EXPECT_FALSE(scrollableArea->verticalScrollbarNeedsPaintInvalidation());
    scrollableArea->clearNeedsPaintInvalidationForScrollControls();

    // Forced GC in order to finalize objects depending on the mock object.
    ThreadHeap::collectAllGarbage();
}

TEST_F(ScrollableAreaTest, InvalidatesCompositedScrollbarsIfPartsNeedRepaint)
{
    ScrollbarThemeWithMockInvalidation theme;
    MockScrollableArea* scrollableArea = MockScrollableArea::create(IntPoint(100, 100));
    Scrollbar* horizontalScrollbar = Scrollbar::createForTesting(scrollableArea, HorizontalScrollbar, RegularScrollbar, &theme);
    horizontalScrollbar->clearTrackNeedsRepaint();
    horizontalScrollbar->clearThumbNeedsRepaint();
    Scrollbar* verticalScrollbar = Scrollbar::createForTesting(scrollableArea, VerticalScrollbar, RegularScrollbar, &theme);
    verticalScrollbar->clearTrackNeedsRepaint();
    verticalScrollbar->clearThumbNeedsRepaint();
    EXPECT_CALL(*scrollableArea, horizontalScrollbar()).WillRepeatedly(Return(horizontalScrollbar));
    EXPECT_CALL(*scrollableArea, verticalScrollbar()).WillRepeatedly(Return(verticalScrollbar));

    // Composited scrollbars only need repainting when parts become invalid
    // (e.g. if the track changes appearance when the thumb reaches the end).
    FakeGraphicsLayerClient graphicsLayerClient;
    graphicsLayerClient.setIsTrackingPaintInvalidations(true);
    FakeGraphicsLayer layerForHorizontalScrollbar(&graphicsLayerClient);
    layerForHorizontalScrollbar.setDrawsContent(true);
    layerForHorizontalScrollbar.setSize(FloatSize(10, 10));
    FakeGraphicsLayer layerForVerticalScrollbar(&graphicsLayerClient);
    layerForVerticalScrollbar.setDrawsContent(true);
    layerForVerticalScrollbar.setSize(FloatSize(10, 10));
    EXPECT_CALL(*scrollableArea, layerForHorizontalScrollbar()).WillRepeatedly(Return(&layerForHorizontalScrollbar));
    EXPECT_CALL(*scrollableArea, layerForVerticalScrollbar()).WillRepeatedly(Return(&layerForVerticalScrollbar));
    ASSERT_TRUE(scrollableArea->hasLayerForHorizontalScrollbar());
    ASSERT_TRUE(scrollableArea->hasLayerForVerticalScrollbar());
    EXPECT_CALL(theme, shouldRepaintAllPartsOnInvalidation()).WillRepeatedly(Return(false));

    // First, we'll scroll horizontally, and the theme will require repainting
    // the back button (i.e. the track).
    EXPECT_CALL(theme, invalidateOnThumbPositionChange(_, _, _)).WillOnce(Return(BackButtonStartPart));
    scrollableArea->setScrollPosition(DoublePoint(50, 0), ProgrammaticScroll);
    EXPECT_TRUE(layerForHorizontalScrollbar.hasTrackedPaintInvalidations());
    EXPECT_FALSE(layerForVerticalScrollbar.hasTrackedPaintInvalidations());
    EXPECT_TRUE(horizontalScrollbar->trackNeedsRepaint());
    EXPECT_FALSE(horizontalScrollbar->thumbNeedsRepaint());
    layerForHorizontalScrollbar.resetTrackedPaintInvalidations();
    horizontalScrollbar->clearTrackNeedsRepaint();

    // Next, we'll scroll vertically, but invalidate the thumb.
    EXPECT_CALL(theme, invalidateOnThumbPositionChange(_, _, _)).WillOnce(Return(ThumbPart));
    scrollableArea->setScrollPosition(DoublePoint(50, 50), ProgrammaticScroll);
    EXPECT_FALSE(layerForHorizontalScrollbar.hasTrackedPaintInvalidations());
    EXPECT_TRUE(layerForVerticalScrollbar.hasTrackedPaintInvalidations());
    EXPECT_FALSE(verticalScrollbar->trackNeedsRepaint());
    EXPECT_TRUE(verticalScrollbar->thumbNeedsRepaint());
    layerForVerticalScrollbar.resetTrackedPaintInvalidations();
    verticalScrollbar->clearThumbNeedsRepaint();

    // Next we'll scroll in both, but the thumb position moving requires no
    // invalidations. Nonetheless the GraphicsLayer should be invalidated,
    // because we still need to update the underlying layer (though no
    // rasterization will be required).
    EXPECT_CALL(theme, invalidateOnThumbPositionChange(_, _, _)).Times(2).WillRepeatedly(Return(NoPart));
    scrollableArea->setScrollPosition(DoublePoint(70, 70), ProgrammaticScroll);
    EXPECT_TRUE(layerForHorizontalScrollbar.hasTrackedPaintInvalidations());
    EXPECT_TRUE(layerForVerticalScrollbar.hasTrackedPaintInvalidations());
    EXPECT_FALSE(horizontalScrollbar->trackNeedsRepaint());
    EXPECT_FALSE(horizontalScrollbar->thumbNeedsRepaint());
    EXPECT_FALSE(verticalScrollbar->trackNeedsRepaint());
    EXPECT_FALSE(verticalScrollbar->thumbNeedsRepaint());

    // Forced GC in order to finalize objects depending on the mock object.
    ThreadHeap::collectAllGarbage();
}

TEST_F(ScrollableAreaTest, RecalculatesScrollbarOverlayIfBackgroundChanges)
{
    MockScrollableArea* scrollableArea = MockScrollableArea::create(IntPoint(0, 100));

    EXPECT_EQ(ScrollbarOverlayStyleDefault, scrollableArea->getScrollbarOverlayStyle());
    scrollableArea->recalculateScrollbarOverlayStyle(Color(34, 85, 51));
    EXPECT_EQ(ScrollbarOverlayStyleLight, scrollableArea->getScrollbarOverlayStyle());
    scrollableArea->recalculateScrollbarOverlayStyle(Color(236, 143, 185));
    EXPECT_EQ(ScrollbarOverlayStyleDefault, scrollableArea->getScrollbarOverlayStyle());
}

} // namespace blink
