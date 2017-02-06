// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/StaticRange.h"

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/dom/Element.h"
#include "core/dom/NodeList.h"
#include "core/dom/Range.h"
#include "core/dom/Text.h"
#include "core/html/HTMLBodyElement.h"
#include "core/html/HTMLDocument.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLHtmlElement.h"
#include "platform/heap/Handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/Compiler.h"
#include "wtf/RefPtr.h"
#include "wtf/text/AtomicString.h"

namespace blink {

class StaticRangeTest : public testing::Test {
protected:
    void SetUp() override;

    HTMLDocument& document() const;

private:
    Persistent<HTMLDocument> m_document;
};

void StaticRangeTest::SetUp()
{
    m_document = HTMLDocument::create();
    HTMLHtmlElement* html = HTMLHtmlElement::create(*m_document);
    html->appendChild(HTMLBodyElement::create(*m_document));
    m_document->appendChild(html);
}

HTMLDocument& StaticRangeTest::document() const
{
    return *m_document;
}

TEST_F(StaticRangeTest, SplitTextNodeRangeWithinText)
{
    document().body()->setInnerHTML("1234", ASSERT_NO_EXCEPTION);
    Text* oldText = toText(document().body()->firstChild());

    StaticRange* staticRange04 = StaticRange::create(document(), oldText, 0, oldText, 4);
    StaticRange* staticRange02 = StaticRange::create(document(), oldText, 0, oldText, 2);
    StaticRange* staticRange22 = StaticRange::create(document(), oldText, 2, oldText, 2);
    StaticRange* staticRange24 = StaticRange::create(document(), oldText, 2, oldText, 4);

    Range* range04 = staticRange04->toRange(ASSERT_NO_EXCEPTION);
    Range* range02 = staticRange02->toRange(ASSERT_NO_EXCEPTION);
    Range* range22 = staticRange22->toRange(ASSERT_NO_EXCEPTION);
    Range* range24 = staticRange24->toRange(ASSERT_NO_EXCEPTION);

    oldText->splitText(2, ASSERT_NO_EXCEPTION);
    Text* newText = toText(oldText->nextSibling());

    // Range should mutate.
    EXPECT_TRUE(range04->boundaryPointsValid());
    EXPECT_EQ(oldText, range04->startContainer());
    EXPECT_EQ(0, range04->startOffset());
    EXPECT_EQ(newText, range04->endContainer());
    EXPECT_EQ(2, range04->endOffset());

    EXPECT_TRUE(range02->boundaryPointsValid());
    EXPECT_EQ(oldText, range02->startContainer());
    EXPECT_EQ(0, range02->startOffset());
    EXPECT_EQ(oldText, range02->endContainer());
    EXPECT_EQ(2, range02->endOffset());

    // Our implementation always moves the boundary point at the separation point to the end of the original text node.
    EXPECT_TRUE(range22->boundaryPointsValid());
    EXPECT_EQ(oldText, range22->startContainer());
    EXPECT_EQ(2, range22->startOffset());
    EXPECT_EQ(oldText, range22->endContainer());
    EXPECT_EQ(2, range22->endOffset());

    EXPECT_TRUE(range24->boundaryPointsValid());
    EXPECT_EQ(oldText, range24->startContainer());
    EXPECT_EQ(2, range24->startOffset());
    EXPECT_EQ(newText, range24->endContainer());
    EXPECT_EQ(2, range24->endOffset());

    // StaticRange shouldn't mutate.
    EXPECT_EQ(oldText, staticRange04->startContainer());
    EXPECT_EQ(0, staticRange04->startOffset());
    EXPECT_EQ(oldText, staticRange04->endContainer());
    EXPECT_EQ(4, staticRange04->endOffset());

    EXPECT_EQ(oldText, staticRange02->startContainer());
    EXPECT_EQ(0, staticRange02->startOffset());
    EXPECT_EQ(oldText, staticRange02->endContainer());
    EXPECT_EQ(2, staticRange02->endOffset());

    EXPECT_EQ(oldText, staticRange22->startContainer());
    EXPECT_EQ(2, staticRange22->startOffset());
    EXPECT_EQ(oldText, staticRange22->endContainer());
    EXPECT_EQ(2, staticRange22->endOffset());

    EXPECT_EQ(oldText, staticRange24->startContainer());
    EXPECT_EQ(2, staticRange24->startOffset());
    EXPECT_EQ(oldText, staticRange24->endContainer());
    EXPECT_EQ(4, staticRange24->endOffset());
}

TEST_F(StaticRangeTest, SplitTextNodeRangeOutsideText)
{
    document().body()->setInnerHTML("<span id=\"outer\">0<span id=\"inner-left\">1</span>SPLITME<span id=\"inner-right\">2</span>3</span>", ASSERT_NO_EXCEPTION);

    Element* outer = document().getElementById(AtomicString::fromUTF8("outer"));
    Element* innerLeft = document().getElementById(AtomicString::fromUTF8("inner-left"));
    Element* innerRight = document().getElementById(AtomicString::fromUTF8("inner-right"));
    Text* oldText = toText(outer->childNodes()->item(2));

    StaticRange* staticRangeOuterOutside = StaticRange::create(document(), outer, 0, outer, 5);
    StaticRange* staticRangeOuterInside = StaticRange::create(document(), outer, 1, outer, 4);
    StaticRange* staticRangeOuterSurroundingText = StaticRange::create(document(), outer, 2, outer, 3);
    StaticRange* staticRangeInnerLeft = StaticRange::create(document(), innerLeft, 0, innerLeft, 1);
    StaticRange* staticRangeInnerRight = StaticRange::create(document(), innerRight, 0, innerRight, 1);
    StaticRange* staticRangeFromTextToMiddleOfElement = StaticRange::create(document(), oldText, 6, outer, 3);

    Range* rangeOuterOutside = staticRangeOuterOutside->toRange(ASSERT_NO_EXCEPTION);
    Range* rangeOuterInside = staticRangeOuterInside->toRange(ASSERT_NO_EXCEPTION);
    Range* rangeOuterSurroundingText = staticRangeOuterSurroundingText->toRange(ASSERT_NO_EXCEPTION);
    Range* rangeInnerLeft = staticRangeInnerLeft->toRange(ASSERT_NO_EXCEPTION);
    Range* rangeInnerRight = staticRangeInnerRight->toRange(ASSERT_NO_EXCEPTION);
    Range* rangeFromTextToMiddleOfElement = staticRangeFromTextToMiddleOfElement->toRange(ASSERT_NO_EXCEPTION);

    oldText->splitText(3, ASSERT_NO_EXCEPTION);
    Text* newText = toText(oldText->nextSibling());

    // Range should mutate.
    EXPECT_TRUE(rangeOuterOutside->boundaryPointsValid());
    EXPECT_EQ(outer, rangeOuterOutside->startContainer());
    EXPECT_EQ(0, rangeOuterOutside->startOffset());
    EXPECT_EQ(outer, rangeOuterOutside->endContainer());
    EXPECT_EQ(6, rangeOuterOutside->endOffset()); // Increased by 1 since a new node is inserted.

    EXPECT_TRUE(rangeOuterInside->boundaryPointsValid());
    EXPECT_EQ(outer, rangeOuterInside->startContainer());
    EXPECT_EQ(1, rangeOuterInside->startOffset());
    EXPECT_EQ(outer, rangeOuterInside->endContainer());
    EXPECT_EQ(5, rangeOuterInside->endOffset());

    EXPECT_TRUE(rangeOuterSurroundingText->boundaryPointsValid());
    EXPECT_EQ(outer, rangeOuterSurroundingText->startContainer());
    EXPECT_EQ(2, rangeOuterSurroundingText->startOffset());
    EXPECT_EQ(outer, rangeOuterSurroundingText->endContainer());
    EXPECT_EQ(4, rangeOuterSurroundingText->endOffset());

    EXPECT_TRUE(rangeInnerLeft->boundaryPointsValid());
    EXPECT_EQ(innerLeft, rangeInnerLeft->startContainer());
    EXPECT_EQ(0, rangeInnerLeft->startOffset());
    EXPECT_EQ(innerLeft, rangeInnerLeft->endContainer());
    EXPECT_EQ(1, rangeInnerLeft->endOffset());

    EXPECT_TRUE(rangeInnerRight->boundaryPointsValid());
    EXPECT_EQ(innerRight, rangeInnerRight->startContainer());
    EXPECT_EQ(0, rangeInnerRight->startOffset());
    EXPECT_EQ(innerRight, rangeInnerRight->endContainer());
    EXPECT_EQ(1, rangeInnerRight->endOffset());

    EXPECT_TRUE(rangeFromTextToMiddleOfElement->boundaryPointsValid());
    EXPECT_EQ(newText, rangeFromTextToMiddleOfElement->startContainer());
    EXPECT_EQ(3, rangeFromTextToMiddleOfElement->startOffset());
    EXPECT_EQ(outer, rangeFromTextToMiddleOfElement->endContainer());
    EXPECT_EQ(4, rangeFromTextToMiddleOfElement->endOffset());

    // StaticRange shouldn't mutate.
    EXPECT_EQ(outer, staticRangeOuterOutside->startContainer());
    EXPECT_EQ(0, staticRangeOuterOutside->startOffset());
    EXPECT_EQ(outer, staticRangeOuterOutside->endContainer());
    EXPECT_EQ(5, staticRangeOuterOutside->endOffset());

    EXPECT_EQ(outer, staticRangeOuterInside->startContainer());
    EXPECT_EQ(1, staticRangeOuterInside->startOffset());
    EXPECT_EQ(outer, staticRangeOuterInside->endContainer());
    EXPECT_EQ(4, staticRangeOuterInside->endOffset());

    EXPECT_EQ(outer, staticRangeOuterSurroundingText->startContainer());
    EXPECT_EQ(2, staticRangeOuterSurroundingText->startOffset());
    EXPECT_EQ(outer, staticRangeOuterSurroundingText->endContainer());
    EXPECT_EQ(3, staticRangeOuterSurroundingText->endOffset());

    EXPECT_EQ(innerLeft, staticRangeInnerLeft->startContainer());
    EXPECT_EQ(0, staticRangeInnerLeft->startOffset());
    EXPECT_EQ(innerLeft, staticRangeInnerLeft->endContainer());
    EXPECT_EQ(1, staticRangeInnerLeft->endOffset());

    EXPECT_EQ(innerRight, staticRangeInnerRight->startContainer());
    EXPECT_EQ(0, staticRangeInnerRight->startOffset());
    EXPECT_EQ(innerRight, staticRangeInnerRight->endContainer());
    EXPECT_EQ(1, staticRangeInnerRight->endOffset());

    EXPECT_EQ(oldText, staticRangeFromTextToMiddleOfElement->startContainer());
    EXPECT_EQ(6, staticRangeFromTextToMiddleOfElement->startOffset());
    EXPECT_EQ(outer, staticRangeFromTextToMiddleOfElement->endContainer());
    EXPECT_EQ(3, staticRangeFromTextToMiddleOfElement->endOffset());
}

TEST_F(StaticRangeTest, InvalidToRange)
{
    document().body()->setInnerHTML("1234", ASSERT_NO_EXCEPTION);
    Text* oldText = toText(document().body()->firstChild());

    StaticRange* staticRange04 = StaticRange::create(document(), oldText, 0, oldText, 4);

    // Valid StaticRange.
    staticRange04->toRange(ASSERT_NO_EXCEPTION);

    oldText->splitText(2, ASSERT_NO_EXCEPTION);
    // StaticRange shouldn't mutate, endOffset() become invalid after splitText().
    EXPECT_EQ(oldText, staticRange04->startContainer());
    EXPECT_EQ(0, staticRange04->startOffset());
    EXPECT_EQ(oldText, staticRange04->endContainer());
    EXPECT_EQ(4, staticRange04->endOffset());

    // Invalid StaticRange.
    TrackExceptionState exceptionState;
    staticRange04->toRange(exceptionState);
    EXPECT_TRUE(exceptionState.hadException());
}

} // namespace blink
