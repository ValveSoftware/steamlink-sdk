// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/editing/VisibleSelection.h"

#include "core/dom/Document.h"
#include "core/dom/Range.h"
#include "core/dom/Text.h"
#include "core/html/HTMLElement.h"
#include "core/testing/DummyPageHolder.h"
#include <gtest/gtest.h>

#define LOREM_IPSUM \
    "Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod tempor " \
    "incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud " \
    "exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure " \
    "dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur." \
    "Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt " \
    "mollit anim id est laborum."

namespace WebCore {

class VisibleSelectionTest : public ::testing::Test {
protected:
    virtual void SetUp() OVERRIDE;

    // Oilpan: wrapper object needed to be able to trace VisibleSelection.
    class VisibleSelectionWrapper : public NoBaseWillBeGarbageCollectedFinalized<VisibleSelectionWrapper> {
    public:
        void trace(Visitor* visitor)
        {
            visitor->trace(m_selection);
        }

        VisibleSelection m_selection;
    };

    Document& document() const { return m_dummyPageHolder->document(); }
    Text* textNode() const { return m_textNode.get(); }
    VisibleSelection& selection() { return m_wrap->m_selection; }

    // Helper function to set the VisibleSelection base/extent.
    void setSelection(int base) { setSelection(base, base); }

    // Helper function to set the VisibleSelection base/extent.
    void setSelection(int base, int extend)
    {
        m_wrap->m_selection.setBase(Position(textNode(), base));
        m_wrap->m_selection.setExtent(Position(textNode(), extend));
    }

private:
    OwnPtr<DummyPageHolder> m_dummyPageHolder;
    RefPtrWillBePersistent<Text> m_textNode;
    OwnPtrWillBePersistent<VisibleSelectionWrapper> m_wrap;
};

void WebCore::VisibleSelectionTest::SetUp()
{
    m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
    m_textNode = document().createTextNode(LOREM_IPSUM);
    m_wrap = adoptPtrWillBeNoop(new VisibleSelectionWrapper());
    document().body()->appendChild(m_textNode.get());
}

} // namespace WebCore

namespace {

using namespace WebCore;

TEST_F(VisibleSelectionTest, Initialisation)
{
    setSelection(0);

    EXPECT_FALSE(selection().isNone());
    EXPECT_TRUE(selection().isCaret());

    RefPtrWillBeRawPtr<Range> range = selection().firstRange();
    EXPECT_EQ(0, range->startOffset());
    EXPECT_EQ(0, range->endOffset());
    EXPECT_EQ("", range->text());
}

TEST_F(VisibleSelectionTest, WordGranularity)
{
    // Beginning of a word.
    {
        setSelection(0);
        selection().expandUsingGranularity(WordGranularity);

        RefPtrWillBeRawPtr<Range> range = selection().firstRange();
        EXPECT_EQ(0, range->startOffset());
        EXPECT_EQ(5, range->endOffset());
        EXPECT_EQ("Lorem", range->text());
    }

    // Middle of a word.
    {
        setSelection(8);
        selection().expandUsingGranularity(WordGranularity);

        RefPtrWillBeRawPtr<Range> range = selection().firstRange();
        EXPECT_EQ(6, range->startOffset());
        EXPECT_EQ(11, range->endOffset());
        EXPECT_EQ("ipsum", range->text());
    }

    // End of a word.
    // FIXME: that sounds buggy, we might want to select the word _before_ instead
    // of the space...
    {
        setSelection(5);
        selection().expandUsingGranularity(WordGranularity);

        RefPtrWillBeRawPtr<Range> range = selection().firstRange();
        EXPECT_EQ(5, range->startOffset());
        EXPECT_EQ(6, range->endOffset());
        EXPECT_EQ(" ", range->text());
    }

    // Before comma.
    // FIXME: that sounds buggy, we might want to select the word _before_ instead
    // of the comma.
    {
        setSelection(26);
        selection().expandUsingGranularity(WordGranularity);

        RefPtrWillBeRawPtr<Range> range = selection().firstRange();
        EXPECT_EQ(26, range->startOffset());
        EXPECT_EQ(27, range->endOffset());
        EXPECT_EQ(",", range->text());
    }

    // After comma.
    {
        setSelection(27);
        selection().expandUsingGranularity(WordGranularity);

        RefPtrWillBeRawPtr<Range> range = selection().firstRange();
        EXPECT_EQ(27, range->startOffset());
        EXPECT_EQ(28, range->endOffset());
        EXPECT_EQ(" ", range->text());
    }

    // When selecting part of a word.
    {
        setSelection(0, 1);
        selection().expandUsingGranularity(WordGranularity);

        RefPtrWillBeRawPtr<Range> range = selection().firstRange();
        EXPECT_EQ(0, range->startOffset());
        EXPECT_EQ(5, range->endOffset());
        EXPECT_EQ("Lorem", range->text());
    }

    // When selecting part of two words.
    {
        setSelection(2, 8);
        selection().expandUsingGranularity(WordGranularity);

        RefPtrWillBeRawPtr<Range> range = selection().firstRange();
        EXPECT_EQ(0, range->startOffset());
        EXPECT_EQ(11, range->endOffset());
        EXPECT_EQ("Lorem ipsum", range->text());
    }
}

} // namespace WebCore
