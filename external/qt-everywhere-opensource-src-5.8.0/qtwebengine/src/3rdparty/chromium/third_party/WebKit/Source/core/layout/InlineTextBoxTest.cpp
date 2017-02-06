// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/line/InlineTextBox.h"

#include "core/layout/LayoutTestHelper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class InlineTextBoxTest : public RenderingTest {
};

class TestInlineTextBox : public InlineTextBox {
public:
    TestInlineTextBox(LineLayoutItem item)
        : InlineTextBox(item, 0, 0)
    {
        setHasVirtualLogicalHeight();
    }

    static TestInlineTextBox* create(Document& document, const String& string)
    {
        Text* node = document.createTextNode(string);
        LayoutText* text = new LayoutText(node, string.impl());
        text->setStyle(ComputedStyle::create());
        return new TestInlineTextBox(LineLayoutItem(text));
    }

    LayoutUnit virtualLogicalHeight() const override
    {
        return m_logicalHeight;
    }

    void setLogicalFrameRect(const LayoutRect& rect)
    {
        setX(rect.x());
        setY(rect.y());
        setLogicalWidth(rect.width());
        m_logicalHeight = rect.height();
    }

private:
    LayoutUnit m_logicalHeight;
};

static void moveAndTest(InlineTextBox* box, const LayoutSize& move, LayoutRect& frame, LayoutRect& overflow)
{
    box->move(move);
    frame.move(move);
    overflow.move(move);
    ASSERT_EQ(frame, box->logicalFrameRect());
    ASSERT_EQ(overflow, box->logicalOverflowRect());
}

TEST_F(InlineTextBoxTest, LogicalOverflowRect)
{
    // Setup a TestInlineTextBox.
    TestInlineTextBox* box = TestInlineTextBox::create(document(), "");

    // Initially, logicalOverflowRect() should be the same as logicalFrameRect().
    LayoutRect frame(5, 20, 100, 200);
    LayoutRect overflow = frame;
    box->setLogicalFrameRect(frame);
    ASSERT_EQ(frame, box->logicalFrameRect());
    ASSERT_EQ(overflow, box->logicalOverflowRect());

    // Ensure it's movable and the rects are correct.
    LayoutSize move(10, 10);
    moveAndTest(box, move, frame, overflow);

    // Ensure clearKnownToHaveNoOverflow() doesn't change either rects.
    box->clearKnownToHaveNoOverflow();
    ASSERT_EQ(frame, box->logicalFrameRect());
    ASSERT_EQ(overflow, box->logicalOverflowRect());

    // Ensure it's still movable correctly when !knownToHaveNoOverflow().
    moveAndTest(box, move, frame, overflow);

    // Let it have different logicalOverflowRect() than logicalFrameRect().
    overflow.expand(LayoutSize(10, 10));
    box->setLogicalOverflowRect(overflow);
    ASSERT_EQ(frame, box->logicalFrameRect());
    ASSERT_EQ(overflow, box->logicalOverflowRect());

    // Ensure it's still movable correctly.
    moveAndTest(box, move, frame, overflow);
}

} // namespace blink
