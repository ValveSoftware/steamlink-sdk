// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/FrameSelection.h"

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Text.h"
#include "core/editing/EditingTestBase.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLBodyElement.h"
#include "core/html/HTMLDocument.h"
#include "core/layout/LayoutView.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/PaintLayer.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/graphics/paint/PaintController.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/StdLibExtras.h"
#include <memory>

namespace blink {

class FrameSelectionTest : public EditingTestBase {
protected:
    void setSelection(const VisibleSelection&);
    FrameSelection& selection() const;
    const VisibleSelection& visibleSelectionInDOMTree() const { return selection().selection(); }
    const VisibleSelectionInFlatTree& visibleSelectionInFlatTree() const { return selection().selectionInFlatTree(); }

    Text* appendTextNode(const String& data);
    int layoutCount() const { return dummyPageHolder().frameView().layoutCount(); }

    bool shouldPaintCaretForTesting() const { return selection().shouldPaintCaretForTesting(); }
    bool isPreviousCaretDirtyForTesting() const { return selection().isPreviousCaretDirtyForTesting(); }

private:
    Persistent<Text> m_textNode;
};

void FrameSelectionTest::setSelection(const VisibleSelection& newSelection)
{
    dummyPageHolder().frame().selection().setSelection(newSelection);
}

FrameSelection& FrameSelectionTest::selection() const
{
    return dummyPageHolder().frame().selection();
}

Text* FrameSelectionTest::appendTextNode(const String& data)
{
    Text* text = document().createTextNode(data);
    document().body()->appendChild(text);
    return text;
}

TEST_F(FrameSelectionTest, SetValidSelection)
{
    Text* text = appendTextNode("Hello, World!");
    VisibleSelection validSelection(Position(text, 0), Position(text, 5));
    EXPECT_FALSE(validSelection.isNone());
    setSelection(validSelection);
    EXPECT_FALSE(selection().isNone());
}

TEST_F(FrameSelectionTest, InvalidateCaretRect)
{
    Text* text = appendTextNode("Hello, World!");
    document().view()->updateAllLifecyclePhases();

    VisibleSelection validSelection(Position(text, 0), Position(text, 0));
    setSelection(validSelection);
    selection().setCaretRectNeedsUpdate();
    EXPECT_TRUE(selection().isCaretBoundsDirty());
    selection().invalidateCaretRect();
    EXPECT_FALSE(selection().isCaretBoundsDirty());

    document().body()->removeChild(text);
    document().updateStyleAndLayoutIgnorePendingStylesheets();
    selection().setCaretRectNeedsUpdate();
    EXPECT_TRUE(selection().isCaretBoundsDirty());
    selection().invalidateCaretRect();
    EXPECT_FALSE(selection().isCaretBoundsDirty());
}

TEST_F(FrameSelectionTest, PaintCaretShouldNotLayout)
{
    Text* text = appendTextNode("Hello, World!");
    document().view()->updateAllLifecyclePhases();

    document().body()->setContentEditable("true", ASSERT_NO_EXCEPTION);
    document().body()->focus();
    EXPECT_TRUE(document().body()->focused());

    VisibleSelection validSelection(Position(text, 0), Position(text, 0));
    selection().setCaretVisible(true);
    setSelection(validSelection);
    EXPECT_TRUE(selection().isCaret());
    EXPECT_TRUE(shouldPaintCaretForTesting());

    int startCount = layoutCount();
    {
        // To force layout in next updateLayout calling, widen view.
        FrameView& frameView = dummyPageHolder().frameView();
        IntRect frameRect = frameView.frameRect();
        frameRect.setWidth(frameRect.width() + 1);
        frameRect.setHeight(frameRect.height() + 1);
        dummyPageHolder().frameView().setFrameRect(frameRect);
    }
    std::unique_ptr<PaintController> paintController = PaintController::create();
    {
        GraphicsContext context(*paintController);
        selection().paintCaret(context, LayoutPoint());
    }
    paintController->commitNewDisplayItems();
    EXPECT_EQ(startCount, layoutCount());
}

TEST_F(FrameSelectionTest, InvalidatePreviousCaretAfterRemovingLastCharacter)
{
    Text* text = appendTextNode("Hello, World!");
    document().view()->updateAllLifecyclePhases();

    document().body()->setContentEditable("true", ASSERT_NO_EXCEPTION);
    document().body()->focus();
    EXPECT_TRUE(document().body()->focused());

    selection().setCaretVisible(true);
    EXPECT_TRUE(selection().isCaret());
    EXPECT_TRUE(shouldPaintCaretForTesting());

    // Simulate to type "Hello, World!".
    DisableCompositingQueryAsserts disabler;
    selection().moveTo(createVisiblePosition(selection().end(), selection().affinity()), NotUserTriggered);
    selection().setCaretRectNeedsUpdate();
    EXPECT_TRUE(selection().isCaretBoundsDirty());
    EXPECT_FALSE(isPreviousCaretDirtyForTesting());
    selection().invalidateCaretRect();
    EXPECT_FALSE(selection().isCaretBoundsDirty());
    EXPECT_TRUE(isPreviousCaretDirtyForTesting());

    // Simulate to remove all except for "H".
    text->replaceWholeText("H");
    selection().moveTo(createVisiblePosition(selection().end(), selection().affinity()), NotUserTriggered);
    selection().setCaretRectNeedsUpdate();
    EXPECT_TRUE(selection().isCaretBoundsDirty());
    // "H" remains so early previousCaret invalidation isn't needed.
    EXPECT_TRUE(isPreviousCaretDirtyForTesting());
    selection().invalidateCaretRect();
    EXPECT_FALSE(selection().isCaretBoundsDirty());
    EXPECT_TRUE(isPreviousCaretDirtyForTesting());

    // Simulate to remove the last character.
    document().body()->removeChild(text);
    // This line is the objective of this test.
    // As removing the last character, early previousCaret invalidation is executed.
    EXPECT_FALSE(isPreviousCaretDirtyForTesting());
    document().updateStyleAndLayoutIgnorePendingStylesheets();
    selection().setCaretRectNeedsUpdate();
    EXPECT_TRUE(selection().isCaretBoundsDirty());
    EXPECT_FALSE(isPreviousCaretDirtyForTesting());
    selection().invalidateCaretRect();
    EXPECT_FALSE(selection().isCaretBoundsDirty());
    EXPECT_TRUE(isPreviousCaretDirtyForTesting());
}

#define EXPECT_EQ_SELECTED_TEXT(text) \
    EXPECT_EQ(text, WebString(selection().selectedText()).utf8())

TEST_F(FrameSelectionTest, SelectWordAroundPosition)
{
    // "Foo Bar  Baz,"
    Text* text = appendTextNode("Foo Bar&nbsp;&nbsp;Baz,");
    // "Fo|o Bar  Baz,"
    EXPECT_TRUE(selection().selectWordAroundPosition(createVisiblePosition(Position(text, 2))));
    EXPECT_EQ_SELECTED_TEXT("Foo");
    // "Foo| Bar  Baz,"
    EXPECT_TRUE(selection().selectWordAroundPosition(createVisiblePosition(Position(text, 3))));
    EXPECT_EQ_SELECTED_TEXT("Foo");
    // "Foo Bar | Baz,"
    EXPECT_FALSE(selection().selectWordAroundPosition(createVisiblePosition(Position(text, 13))));
    // "Foo Bar  Baz|,"
    EXPECT_TRUE(selection().selectWordAroundPosition(createVisiblePosition(Position(text, 22))));
    EXPECT_EQ_SELECTED_TEXT("Baz");
}

TEST_F(FrameSelectionTest, ModifyExtendWithFlatTree)
{
    setBodyContent("<span id=host></span>one");
    setShadowContent("two<content></content>", "host");
    Element* host = document().getElementById("host");
    Node* const two = FlatTreeTraversal::firstChild(*host);
    // Select "two" for selection in DOM tree
    // Select "twoone" for selection in Flat tree
    selection().setSelection(VisibleSelectionInFlatTree(PositionInFlatTree(host, 0), PositionInFlatTree(document().body(), 2)));
    selection().modify(FrameSelection::AlterationExtend, DirectionForward, WordGranularity);
    EXPECT_EQ(Position(two, 0), visibleSelectionInDOMTree().start());
    EXPECT_EQ(Position(two, 3), visibleSelectionInDOMTree().end());
    EXPECT_EQ(PositionInFlatTree(two, 0), visibleSelectionInFlatTree().start());
    EXPECT_EQ(PositionInFlatTree(two, 3), visibleSelectionInFlatTree().end());
}

TEST_F(FrameSelectionTest, MoveRangeSelectionTest)
{
    // "Foo Bar Baz,"
    Text* text = appendTextNode("Foo Bar Baz,");
    // Itinitializes with "Foo B|a>r Baz," (| means start and > means end).
    selection().setSelection(VisibleSelection(Position(text, 5), Position(text, 6)));
    EXPECT_EQ_SELECTED_TEXT("a");

    // "Foo B|ar B>az," with the Character granularity.
    selection().moveRangeSelection(createVisiblePosition(Position(text, 5)), createVisiblePosition(Position(text, 9)), CharacterGranularity);
    EXPECT_EQ_SELECTED_TEXT("ar B");
    // "Foo B|ar B>az," with the Word granularity.
    selection().moveRangeSelection(createVisiblePosition(Position(text, 5)), createVisiblePosition(Position(text, 9)), WordGranularity);
    EXPECT_EQ_SELECTED_TEXT("Bar Baz");
    // "Fo<o B|ar Baz," with the Character granularity.
    selection().moveRangeSelection(createVisiblePosition(Position(text, 5)), createVisiblePosition(Position(text, 2)), CharacterGranularity);
    EXPECT_EQ_SELECTED_TEXT("o B");
    // "Fo<o B|ar Baz," with the Word granularity.
    selection().moveRangeSelection(createVisiblePosition(Position(text, 5)), createVisiblePosition(Position(text, 2)), WordGranularity);
    EXPECT_EQ_SELECTED_TEXT("Foo Bar");
}

TEST_F(FrameSelectionTest, setNonDirectionalSelectionIfNeeded)
{
    const char* bodyContent = "<span id=top>top</span><span id=host></span>";
    const char* shadowContent = "<span id=bottom>bottom</span>";
    setBodyContent(bodyContent);
    ShadowRoot* shadowRoot = setShadowContent(shadowContent, "host");

    Node* top = document().getElementById("top")->firstChild();
    Node* bottom = shadowRoot->getElementById("bottom")->firstChild();
    Node* host = document().getElementById("host");

    // top to bottom
    selection().setNonDirectionalSelectionIfNeeded(VisibleSelectionInFlatTree(PositionInFlatTree(top, 1), PositionInFlatTree(bottom, 3)), CharacterGranularity);
    EXPECT_EQ(Position(top, 1), visibleSelectionInDOMTree().base());
    EXPECT_EQ(Position::beforeNode(host), visibleSelectionInDOMTree().extent());
    EXPECT_EQ(Position(top, 1), visibleSelectionInDOMTree().start());
    EXPECT_EQ(Position(top, 3), visibleSelectionInDOMTree().end());

    EXPECT_EQ(PositionInFlatTree(top, 1), visibleSelectionInFlatTree().base());
    EXPECT_EQ(PositionInFlatTree(bottom, 3), visibleSelectionInFlatTree().extent());
    EXPECT_EQ(PositionInFlatTree(top, 1), visibleSelectionInFlatTree().start());
    EXPECT_EQ(PositionInFlatTree(bottom, 3), visibleSelectionInFlatTree().end());

    // bottom to top
    selection().setNonDirectionalSelectionIfNeeded(VisibleSelectionInFlatTree(PositionInFlatTree(bottom, 3), PositionInFlatTree(top, 1)), CharacterGranularity);
    EXPECT_EQ(Position(bottom, 3), visibleSelectionInDOMTree().base());
    EXPECT_EQ(Position::beforeNode(bottom->parentNode()), visibleSelectionInDOMTree().extent());
    EXPECT_EQ(Position(bottom, 0), visibleSelectionInDOMTree().start());
    EXPECT_EQ(Position(bottom, 3), visibleSelectionInDOMTree().end());

    EXPECT_EQ(PositionInFlatTree(bottom, 3), visibleSelectionInFlatTree().base());
    EXPECT_EQ(PositionInFlatTree(top, 1), visibleSelectionInFlatTree().extent());
    EXPECT_EQ(PositionInFlatTree(top, 1), visibleSelectionInFlatTree().start());
    EXPECT_EQ(PositionInFlatTree(bottom, 3), visibleSelectionInFlatTree().end());
}

TEST_F(FrameSelectionTest, SelectAllWithUnselectableRoot)
{
    Element* select = document().createElement("select", ASSERT_NO_EXCEPTION);
    document().replaceChild(select, document().documentElement());
    selection().selectAll();
    EXPECT_TRUE(selection().isNone()) << "Nothing should be selected if the content of the documentElement is not selctable.";
}

} // namespace blink
