// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScrollbarTestSuite_h
#define ScrollbarTestSuite_h

#include "platform/heap/GarbageCollected.h"
#include "platform/scroll/ScrollableArea.h"
#include "platform/scroll/Scrollbar.h"
#include "platform/scroll/ScrollbarThemeMock.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

class MockScrollableArea : public GarbageCollectedFinalized<MockScrollableArea>, public ScrollableArea {
    USING_GARBAGE_COLLECTED_MIXIN(MockScrollableArea);

public:
    static MockScrollableArea* create()
    {
        return new MockScrollableArea();
    }

    static MockScrollableArea* create(const IntPoint& maximumScrollPosition)
    {
        MockScrollableArea* mock = create();
        mock->setMaximumScrollPosition(maximumScrollPosition);
        return mock;
    }

    MOCK_CONST_METHOD0(visualRectForScrollbarParts, LayoutRect());
    MOCK_CONST_METHOD0(isActive, bool());
    MOCK_CONST_METHOD1(scrollSize, int(ScrollbarOrientation));
    MOCK_CONST_METHOD0(isScrollCornerVisible, bool());
    MOCK_CONST_METHOD0(scrollCornerRect, IntRect());
    MOCK_CONST_METHOD0(horizontalScrollbar, Scrollbar*());
    MOCK_CONST_METHOD0(verticalScrollbar, Scrollbar*());
    MOCK_CONST_METHOD0(enclosingScrollableArea, ScrollableArea*());
    MOCK_CONST_METHOD1(visibleContentRect, IntRect(IncludeScrollbarsInRect));
    MOCK_CONST_METHOD0(contentsSize, IntSize());
    MOCK_CONST_METHOD0(scrollableAreaBoundingBox, IntRect());
    MOCK_CONST_METHOD0(layerForHorizontalScrollbar, GraphicsLayer*());
    MOCK_CONST_METHOD0(layerForVerticalScrollbar, GraphicsLayer*());

    bool userInputScrollable(ScrollbarOrientation) const override { return true; }
    bool scrollbarsCanBeActive() const override { return true; }
    bool shouldPlaceVerticalScrollbarOnLeft() const override { return false; }
    void setScrollOffset(const DoublePoint& offset, ScrollType) override { m_scrollPosition = flooredIntPoint(offset).shrunkTo(m_maximumScrollPosition); }
    IntPoint scrollPosition() const override { return m_scrollPosition; }
    IntPoint minimumScrollPosition() const override { return IntPoint(); }
    IntPoint maximumScrollPosition() const override { return m_maximumScrollPosition; }
    int visibleHeight() const override { return 768; }
    int visibleWidth() const override { return 1024; }
    bool scrollAnimatorEnabled() const override { return false; }
    int pageStep(ScrollbarOrientation) const override { return 0; }
    void scrollControlWasSetNeedsPaintInvalidation() {}

    using ScrollableArea::horizontalScrollbarNeedsPaintInvalidation;
    using ScrollableArea::verticalScrollbarNeedsPaintInvalidation;
    using ScrollableArea::clearNeedsPaintInvalidationForScrollControls;

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        ScrollableArea::trace(visitor);
    }

private:
    void setMaximumScrollPosition(const IntPoint& maximumScrollPosition)
    {
        m_maximumScrollPosition = maximumScrollPosition;
    }

    explicit MockScrollableArea()
        : m_maximumScrollPosition(IntPoint(0, 100))
    {
    }

    IntPoint m_scrollPosition;
    IntPoint m_maximumScrollPosition;
};

class ScrollbarTestSuite : public testing::Test {
public:
    ScrollbarTestSuite() {}

    void SetUp() override
    {
        TestingPlatformSupport::Config config;
        config.compositorSupport = Platform::current()->compositorSupport();
        m_fakePlatform = wrapUnique(new TestingPlatformSupportWithMockScheduler(config));
    }

    void TearDown() override
    {
        m_fakePlatform = nullptr;
    }

private:
    std::unique_ptr<TestingPlatformSupportWithMockScheduler> m_fakePlatform;
};

} // namespace blink

#endif
