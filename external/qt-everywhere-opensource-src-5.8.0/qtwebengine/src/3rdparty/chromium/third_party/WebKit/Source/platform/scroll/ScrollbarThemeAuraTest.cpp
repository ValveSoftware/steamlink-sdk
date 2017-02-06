// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scroll/ScrollbarThemeAura.h"

#include "platform/scroll/ScrollbarTestSuite.h"

namespace blink {

using testing::Return;

namespace {

class ScrollbarThemeAuraButtonOverride final : public ScrollbarThemeAura {
public:
    ScrollbarThemeAuraButtonOverride()
        : m_hasScrollbarButtons(true)
    {
    }

    void setHasScrollbarButtons(bool value)
    {
        m_hasScrollbarButtons = value;
    }

    bool hasScrollbarButtons(ScrollbarOrientation unused) const override
    {
        return m_hasScrollbarButtons;
    }

private:
    bool m_hasScrollbarButtons;
};

} // namespace

class ScrollbarThemeAuraTest : public ScrollbarTestSuite {
};

TEST_F(ScrollbarThemeAuraTest, ButtonSizeHorizontal)
{
    MockScrollableArea* mockScrollableArea = MockScrollableArea::create();
    ScrollbarThemeMock mockTheme;
    Scrollbar* scrollbar = Scrollbar::createForTesting(
        mockScrollableArea, HorizontalScrollbar, RegularScrollbar, &mockTheme);
    ScrollbarThemeAuraButtonOverride theme;

    IntRect scrollbarSizeNormalDimensions(11, 22, 444, 66);
    scrollbar->setFrameRect(scrollbarSizeNormalDimensions);
    IntSize size1 = theme.buttonSize(*scrollbar);
    EXPECT_EQ(66, size1.width());
    EXPECT_EQ(66, size1.height());

    IntRect scrollbarSizeSquashedDimensions(11, 22, 444, 666);
    scrollbar->setFrameRect(scrollbarSizeSquashedDimensions);
    IntSize size2 = theme.buttonSize(*scrollbar);
    EXPECT_EQ(222, size2.width());
    EXPECT_EQ(666, size2.height());

    ThreadHeap::collectAllGarbage();
}

TEST_F(ScrollbarThemeAuraTest, ButtonSizeVertical)
{
    MockScrollableArea* mockScrollableArea = MockScrollableArea::create();
    ScrollbarThemeMock mockTheme;
    Scrollbar* scrollbar = Scrollbar::createForTesting(
        mockScrollableArea, VerticalScrollbar, RegularScrollbar, &mockTheme);
    ScrollbarThemeAuraButtonOverride theme;

    IntRect scrollbarSizeNormalDimensions(11, 22, 44, 666);
    scrollbar->setFrameRect(scrollbarSizeNormalDimensions);
    IntSize size1 = theme.buttonSize(*scrollbar);
    EXPECT_EQ(44, size1.width());
    EXPECT_EQ(44, size1.height());

    IntRect scrollbarSizeSquashedDimensions(11, 22, 444, 666);
    scrollbar->setFrameRect(scrollbarSizeSquashedDimensions);
    IntSize size2 = theme.buttonSize(*scrollbar);
    EXPECT_EQ(444, size2.width());
    EXPECT_EQ(333, size2.height());

    ThreadHeap::collectAllGarbage();
}

TEST_F(ScrollbarThemeAuraTest, NoButtonsReturnsSize0)
{
    MockScrollableArea* mockScrollableArea = MockScrollableArea::create();
    ScrollbarThemeMock mockTheme;
    Scrollbar* scrollbar = Scrollbar::createForTesting(
        mockScrollableArea, VerticalScrollbar, RegularScrollbar, &mockTheme);
    ScrollbarThemeAuraButtonOverride theme;
    theme.setHasScrollbarButtons(false);

    scrollbar->setFrameRect(IntRect(1, 2, 3, 4));
    IntSize size = theme.buttonSize(*scrollbar);
    EXPECT_EQ(0, size.width());
    EXPECT_EQ(0, size.height());

    ThreadHeap::collectAllGarbage();
}

} // namespace blink
