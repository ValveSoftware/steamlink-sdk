// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/VisibleUnits.h"

#include "core/dom/Text.h"
#include "core/editing/EditingTestBase.h"
#include "core/editing/VisiblePosition.h"
#include "core/html/HTMLTextFormControlElement.h"
#include "core/layout/LayoutTextFragment.h"
#include "core/layout/line/InlineBox.h"
#include <ostream> // NOLINT

namespace blink {

namespace {

PositionWithAffinity positionWithAffinityInDOMTree(Node& anchor, int offset, TextAffinity affinity = TextAffinity::Downstream)
{
    return PositionWithAffinity(canonicalPositionOf(Position(&anchor, offset)), affinity);
}

VisiblePosition createVisiblePositionInDOMTree(Node& anchor, int offset, TextAffinity affinity = TextAffinity::Downstream)
{
    return createVisiblePosition(Position(&anchor, offset), affinity);
}

PositionInFlatTreeWithAffinity positionWithAffinityInFlatTree(Node& anchor, int offset, TextAffinity affinity = TextAffinity::Downstream)
{
    return PositionInFlatTreeWithAffinity(canonicalPositionOf(PositionInFlatTree(&anchor, offset)), affinity);
}

VisiblePositionInFlatTree createVisiblePositionInFlatTree(Node& anchor, int offset, TextAffinity affinity = TextAffinity::Downstream)
{
    return createVisiblePosition(PositionInFlatTree(&anchor, offset), affinity);
}

} // namespace

std::ostream& operator<<(std::ostream& ostream, const InlineBoxPosition& inlineBoxPosition)
{
    if (!inlineBoxPosition.inlineBox)
        return ostream << "null";
    return ostream << inlineBoxPosition.inlineBox->getLineLayoutItem().node() << "@" << inlineBoxPosition.offsetInBox;
}

class VisibleUnitsTest : public EditingTestBase {
};

TEST_F(VisibleUnitsTest, absoluteCaretBoundsOf)
{
    const char* bodyContent = "<p id='host'><b id='one'>11</b><b id='two'>22</b></p>";
    const char* shadowContent = "<div><content select=#two></content><content select=#one></content></div>";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent, "host");

    Element* body = document().body();
    Element* one = body->querySelector("#one", ASSERT_NO_EXCEPTION);

    IntRect boundsInDOMTree = absoluteCaretBoundsOf(createVisiblePosition(Position(one, 0)));
    IntRect boundsInFlatTree = absoluteCaretBoundsOf(createVisiblePosition(PositionInFlatTree(one, 0)));

    EXPECT_FALSE(boundsInDOMTree.isEmpty());
    EXPECT_EQ(boundsInDOMTree, boundsInFlatTree);
}

TEST_F(VisibleUnitsTest, associatedLayoutObjectOfFirstLetterPunctuations)
{
    const char* bodyContent = "<style>p:first-letter {color:red;}</style><p id=sample>(a)bc</p>";
    setBodyContent(bodyContent);

    Node* sample = document().getElementById("sample");
    Node* text = sample->firstChild();

    LayoutTextFragment* layoutObject0 = toLayoutTextFragment(associatedLayoutObjectOf(*text, 0));
    EXPECT_FALSE(layoutObject0->isRemainingTextLayoutObject());

    LayoutTextFragment* layoutObject1 = toLayoutTextFragment(associatedLayoutObjectOf(*text, 1));
    EXPECT_EQ(layoutObject0, layoutObject1) << "A character 'a' should be part of first letter.";

    LayoutTextFragment* layoutObject2 = toLayoutTextFragment(associatedLayoutObjectOf(*text, 2));
    EXPECT_EQ(layoutObject0, layoutObject2) << "close parenthesis should be part of first letter.";

    LayoutTextFragment* layoutObject3 = toLayoutTextFragment(associatedLayoutObjectOf(*text, 3));
    EXPECT_TRUE(layoutObject3->isRemainingTextLayoutObject());
}

TEST_F(VisibleUnitsTest, associatedLayoutObjectOfFirstLetterSplit)
{
    const char* bodyContent = "<style>p:first-letter {color:red;}</style><p id=sample>abc</p>";
    setBodyContent(bodyContent);

    Node* sample = document().getElementById("sample");
    Node* firstLetter = sample->firstChild();
    // Split "abc" into "a" "bc"
    toText(firstLetter)->splitText(1, ASSERT_NO_EXCEPTION);
    updateAllLifecyclePhases();

    LayoutTextFragment* layoutObject0 = toLayoutTextFragment(associatedLayoutObjectOf(*firstLetter, 0));
    EXPECT_FALSE(layoutObject0->isRemainingTextLayoutObject());

    LayoutTextFragment* layoutObject1 = toLayoutTextFragment(associatedLayoutObjectOf(*firstLetter, 1));
    EXPECT_EQ(layoutObject0, layoutObject1);
}

TEST_F(VisibleUnitsTest, associatedLayoutObjectOfFirstLetterWithTrailingWhitespace)
{
    const char* bodyContent = "<style>div:first-letter {color:red;}</style><div id=sample>a\n <div></div></div>";
    setBodyContent(bodyContent);

    Node* sample = document().getElementById("sample");
    Node* text = sample->firstChild();

    LayoutTextFragment* layoutObject0 = toLayoutTextFragment(associatedLayoutObjectOf(*text, 0));
    EXPECT_FALSE(layoutObject0->isRemainingTextLayoutObject());

    LayoutTextFragment* layoutObject1 = toLayoutTextFragment(associatedLayoutObjectOf(*text, 1));
    EXPECT_TRUE(layoutObject1->isRemainingTextLayoutObject());

    LayoutTextFragment* layoutObject2 = toLayoutTextFragment(associatedLayoutObjectOf(*text, 2));
    EXPECT_EQ(layoutObject1, layoutObject2);
}

TEST_F(VisibleUnitsTest, caretMinOffset)
{
    const char* bodyContent = "<p id=one>one</p>";
    setBodyContent(bodyContent);

    Element* one = document().getElementById("one");

    EXPECT_EQ(0, caretMinOffset(one->firstChild()));
}

TEST_F(VisibleUnitsTest, caretMinOffsetWithFirstLetter)
{
    const char* bodyContent = "<style>#one:first-letter { font-size: 200%; }</style><p id=one>one</p>";
    setBodyContent(bodyContent);

    Element* one = document().getElementById("one");

    EXPECT_EQ(0, caretMinOffset(one->firstChild()));
}

TEST_F(VisibleUnitsTest, characterAfter)
{
    const char* bodyContent = "<p id='host'><b id='one'>1</b><b id='two'>22</b></p><b id='three'>333</b>";
    const char* shadowContent = "<b id='four'>4444</b><content select=#two></content><content select=#one></content><b id='five'>5555</b>";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent, "host");

    Element* one = document().getElementById("one");
    Element* two = document().getElementById("two");

    EXPECT_EQ('2', characterAfter(createVisiblePositionInDOMTree(*one->firstChild(), 1)));
    EXPECT_EQ('5', characterAfter(createVisiblePositionInFlatTree(*one->firstChild(), 1)));

    EXPECT_EQ(0, characterAfter(createVisiblePositionInDOMTree(*two->firstChild(), 2)));
    EXPECT_EQ('1', characterAfter(createVisiblePositionInFlatTree(*two->firstChild(), 2)));
}

TEST_F(VisibleUnitsTest, canonicalPositionOfWithHTMLHtmlElement)
{
    const char* bodyContent = "<html><div id=one contenteditable>1</div><span id=two contenteditable=false>22</span><span id=three contenteditable=false>333</span><span id=four contenteditable=false>333</span></html>";
    setBodyContent(bodyContent);

    Node* one = document().querySelector("#one", ASSERT_NO_EXCEPTION);
    Node* two = document().querySelector("#two", ASSERT_NO_EXCEPTION);
    Node* three = document().querySelector("#three", ASSERT_NO_EXCEPTION);
    Node* four = document().querySelector("#four", ASSERT_NO_EXCEPTION);
    Element* html = document().createElement("html", ASSERT_NO_EXCEPTION);
    // Move two, three and four into second html element.
    html->appendChild(two);
    html->appendChild(three);
    html->appendChild(four);
    one->appendChild(html);
    updateAllLifecyclePhases();

    EXPECT_EQ(Position(), canonicalPositionOf(Position(document().documentElement(), 0)));

    EXPECT_EQ(Position(one->firstChild(), 0), canonicalPositionOf(Position(one, 0)));
    EXPECT_EQ(Position(one->firstChild(), 1), canonicalPositionOf(Position(one, 1)));

    EXPECT_EQ(Position(one->firstChild(), 0), canonicalPositionOf(Position(one->firstChild(), 0)));
    EXPECT_EQ(Position(one->firstChild(), 1), canonicalPositionOf(Position(one->firstChild(), 1)));

    EXPECT_EQ(Position(html, 0), canonicalPositionOf(Position(html, 0)));
    EXPECT_EQ(Position(html, 1), canonicalPositionOf(Position(html, 1)));
    EXPECT_EQ(Position(html, 2), canonicalPositionOf(Position(html, 2)));

    EXPECT_EQ(Position(two->firstChild(), 0), canonicalPositionOf(Position(two, 0)));
    EXPECT_EQ(Position(two->firstChild(), 2), canonicalPositionOf(Position(two, 1)));
}

TEST_F(VisibleUnitsTest, characterBefore)
{
    const char* bodyContent = "<p id=host><b id=one>1</b><b id=two>22</b></p><b id=three>333</b>";
    const char* shadowContent = "<b id=four>4444</b><content select=#two></content><content select=#one></content><b id=five>5555</b>";
    setBodyContent(bodyContent);
    ShadowRoot* shadowRoot = setShadowContent(shadowContent, "host");

    Node* one = document().getElementById("one")->firstChild();
    Node* two = document().getElementById("two")->firstChild();
    Node* five = shadowRoot->getElementById("five")->firstChild();

    EXPECT_EQ(0, characterBefore(createVisiblePositionInDOMTree(*one, 0)));
    EXPECT_EQ('2', characterBefore(createVisiblePositionInFlatTree(*one, 0)));

    EXPECT_EQ('1', characterBefore(createVisiblePositionInDOMTree(*one, 1)));
    EXPECT_EQ('1', characterBefore(createVisiblePositionInFlatTree(*one, 1)));

    EXPECT_EQ('1', characterBefore(createVisiblePositionInDOMTree(*two, 0)));
    EXPECT_EQ('4', characterBefore(createVisiblePositionInFlatTree(*two, 0)));

    EXPECT_EQ('4', characterBefore(createVisiblePositionInDOMTree(*five, 0)));
    EXPECT_EQ('1', characterBefore(createVisiblePositionInFlatTree(*five, 0)));
}

TEST_F(VisibleUnitsTest, computeInlineBoxPosition)
{
    const char* bodyContent = "<p id=host><b id=one>1</b><b id=two>22</b></p><b id=three>333</b>";
    const char* shadowContent = "<b id=four>4444</b><content select=#two></content><content select=#one></content><b id=five>5555</b>";
    setBodyContent(bodyContent);
    ShadowRoot* shadowRoot = setShadowContent(shadowContent, "host");

    Node* one = document().getElementById("one")->firstChild();
    Node* two = document().getElementById("two")->firstChild();
    Node* three = document().getElementById("three")->firstChild();
    Node* four = shadowRoot->getElementById("four")->firstChild();
    Node* five = shadowRoot->getElementById("five")->firstChild();

    EXPECT_EQ(computeInlineBoxPosition(PositionInFlatTree(one, 0), TextAffinity::Downstream), computeInlineBoxPosition(createVisiblePositionInFlatTree(*one, 0)));
    EXPECT_EQ(computeInlineBoxPosition(PositionInFlatTree(one, 1), TextAffinity::Downstream), computeInlineBoxPosition(createVisiblePositionInFlatTree(*one, 1)));

    EXPECT_EQ(computeInlineBoxPosition(PositionInFlatTree(two, 0), TextAffinity::Downstream), computeInlineBoxPosition(createVisiblePositionInFlatTree(*two, 0)));
    EXPECT_EQ(computeInlineBoxPosition(PositionInFlatTree(two, 1), TextAffinity::Downstream), computeInlineBoxPosition(createVisiblePositionInFlatTree(*two, 1)));
    EXPECT_EQ(computeInlineBoxPosition(PositionInFlatTree(two, 2), TextAffinity::Downstream), computeInlineBoxPosition(createVisiblePositionInFlatTree(*two, 2)));

    EXPECT_EQ(computeInlineBoxPosition(PositionInFlatTree(three, 0), TextAffinity::Downstream), computeInlineBoxPosition(createVisiblePositionInFlatTree(*three, 0)));
    EXPECT_EQ(computeInlineBoxPosition(PositionInFlatTree(three, 1), TextAffinity::Downstream), computeInlineBoxPosition(createVisiblePositionInFlatTree(*three, 1)));

    EXPECT_EQ(computeInlineBoxPosition(PositionInFlatTree(four, 0), TextAffinity::Downstream), computeInlineBoxPosition(createVisiblePositionInFlatTree(*four, 0)));
    EXPECT_EQ(computeInlineBoxPosition(PositionInFlatTree(four, 1), TextAffinity::Downstream), computeInlineBoxPosition(createVisiblePositionInFlatTree(*four, 1)));

    EXPECT_EQ(computeInlineBoxPosition(PositionInFlatTree(five, 0), TextAffinity::Downstream), computeInlineBoxPosition(createVisiblePositionInFlatTree(*five, 0)));
    EXPECT_EQ(computeInlineBoxPosition(PositionInFlatTree(five, 1), TextAffinity::Downstream), computeInlineBoxPosition(createVisiblePositionInFlatTree(*five, 1)));
}

TEST_F(VisibleUnitsTest, endOfDocument)
{
    const char* bodyContent = "<a id=host><b id=one>1</b><b id=two>22</b></a>";
    const char* shadowContent = "<p><content select=#two></content></p><p><content select=#one></content></p>";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent, "host");

    Element* one = document().getElementById("one");
    Element* two = document().getElementById("two");

    EXPECT_EQ(Position(two->firstChild(), 2), endOfDocument(createVisiblePositionInDOMTree(*one->firstChild(), 0)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(one->firstChild(), 1), endOfDocument(createVisiblePositionInFlatTree(*one->firstChild(), 0)).deepEquivalent());

    EXPECT_EQ(Position(two->firstChild(), 2), endOfDocument(createVisiblePositionInDOMTree(*two->firstChild(), 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(one->firstChild(), 1), endOfDocument(createVisiblePositionInFlatTree(*two->firstChild(), 1)).deepEquivalent());
}

TEST_F(VisibleUnitsTest, endOfLine)
{
    const char* bodyContent = "<a id=host><b id=one>11</b><b id=two>22</b></a><i id=three>333</i><i id=four>4444</i><br>";
    const char* shadowContent = "<div><u id=five>55555</u><content select=#two></content><br><u id=six>666666</u><br><content select=#one></content><u id=seven>7777777</u></div>";
    setBodyContent(bodyContent);
    ShadowRoot* shadowRoot = setShadowContent(shadowContent, "host");

    Node* one = document().getElementById("one")->firstChild();
    Node* two = document().getElementById("two")->firstChild();
    Node* three = document().getElementById("three")->firstChild();
    Node* four = document().getElementById("four")->firstChild();
    Node* five = shadowRoot->getElementById("five")->firstChild();
    Node* six = shadowRoot->getElementById("six")->firstChild();
    Node* seven = shadowRoot->getElementById("seven")->firstChild();

    EXPECT_EQ(Position(seven, 7), endOfLine(createVisiblePositionInDOMTree(*one, 0)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(seven, 7), endOfLine(createVisiblePositionInFlatTree(*one, 0)).deepEquivalent());

    EXPECT_EQ(Position(seven, 7), endOfLine(createVisiblePositionInDOMTree(*one, 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(seven, 7), endOfLine(createVisiblePositionInFlatTree(*one, 1)).deepEquivalent());

    EXPECT_EQ(Position(seven, 7), endOfLine(createVisiblePositionInDOMTree(*two, 0)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(two, 2), endOfLine(createVisiblePositionInFlatTree(*two, 0)).deepEquivalent());

    // TODO(yosin) endOfLine(two, 1) -> (five, 5) is a broken result. We keep
    // it as a marker for future change.
    EXPECT_EQ(Position(five, 5), endOfLine(createVisiblePositionInDOMTree(*two, 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(two, 2), endOfLine(createVisiblePositionInFlatTree(*two, 1)).deepEquivalent());

    EXPECT_EQ(Position(five, 5), endOfLine(createVisiblePositionInDOMTree(*three, 0)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(four, 4), endOfLine(createVisiblePositionInFlatTree(*three, 1)).deepEquivalent());

    EXPECT_EQ(Position(four, 4), endOfLine(createVisiblePositionInDOMTree(*four, 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(four, 4), endOfLine(createVisiblePositionInFlatTree(*four, 1)).deepEquivalent());

    EXPECT_EQ(Position(five, 5), endOfLine(createVisiblePositionInDOMTree(*five, 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(two, 2), endOfLine(createVisiblePositionInFlatTree(*five, 1)).deepEquivalent());

    EXPECT_EQ(Position(six, 6), endOfLine(createVisiblePositionInDOMTree(*six, 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(six, 6), endOfLine(createVisiblePositionInFlatTree(*six, 1)).deepEquivalent());

    EXPECT_EQ(Position(seven, 7), endOfLine(createVisiblePositionInDOMTree(*seven, 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(seven, 7), endOfLine(createVisiblePositionInFlatTree(*seven, 1)).deepEquivalent());
}

TEST_F(VisibleUnitsTest, endOfParagraphFirstLetter)
{
    setBodyContent("<style>div::first-letter { color: red }</style><div id=sample>1ab\nde</div>");

    Node* sample = document().getElementById("sample");
    Node* text = sample->firstChild();

    EXPECT_EQ(Position(text, 6), endOfParagraph(createVisiblePositionInDOMTree(*text, 0)).deepEquivalent());
    EXPECT_EQ(Position(text, 6), endOfParagraph(createVisiblePositionInDOMTree(*text, 1)).deepEquivalent());
    EXPECT_EQ(Position(text, 6), endOfParagraph(createVisiblePositionInDOMTree(*text, 2)).deepEquivalent());
    EXPECT_EQ(Position(text, 6), endOfParagraph(createVisiblePositionInDOMTree(*text, 3)).deepEquivalent());
    EXPECT_EQ(Position(text, 6), endOfParagraph(createVisiblePositionInDOMTree(*text, 4)).deepEquivalent());
    EXPECT_EQ(Position(text, 6), endOfParagraph(createVisiblePositionInDOMTree(*text, 5)).deepEquivalent());
    EXPECT_EQ(Position(text, 6), endOfParagraph(createVisiblePositionInDOMTree(*text, 6)).deepEquivalent());
}

TEST_F(VisibleUnitsTest, endOfParagraphFirstLetterPre)
{
    setBodyContent("<style>pre::first-letter { color: red }</style><pre id=sample>1ab\nde</pre>");

    Node* sample = document().getElementById("sample");
    Node* text = sample->firstChild();

    EXPECT_EQ(Position(text, 3), endOfParagraph(createVisiblePositionInDOMTree(*text, 0)).deepEquivalent());
    EXPECT_EQ(Position(text, 3), endOfParagraph(createVisiblePositionInDOMTree(*text, 1)).deepEquivalent());
    EXPECT_EQ(Position(text, 3), endOfParagraph(createVisiblePositionInDOMTree(*text, 2)).deepEquivalent());
    EXPECT_EQ(Position(text, 6), endOfParagraph(createVisiblePositionInDOMTree(*text, 3)).deepEquivalent());
    EXPECT_EQ(Position(text, 6), endOfParagraph(createVisiblePositionInDOMTree(*text, 4)).deepEquivalent());
    EXPECT_EQ(Position(text, 6), endOfParagraph(createVisiblePositionInDOMTree(*text, 5)).deepEquivalent());
    EXPECT_EQ(Position(text, 6), endOfParagraph(createVisiblePositionInDOMTree(*text, 6)).deepEquivalent());
}

TEST_F(VisibleUnitsTest, endOfParagraphShadow)
{
    const char* bodyContent = "<a id=host><b id=one>1</b><b id=two>22</b></a><b id=three>333</b>";
    const char* shadowContent = "<p><content select=#two></content></p><p><content select=#one></content></p>";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent, "host");

    Element* one = document().getElementById("one");
    Element* two = document().getElementById("two");
    Element* three = document().getElementById("three");

    EXPECT_EQ(Position(three->firstChild(), 3), endOfParagraph(createVisiblePositionInDOMTree(*one->firstChild(), 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(one->firstChild(), 1), endOfParagraph(createVisiblePositionInFlatTree(*one->firstChild(), 1)).deepEquivalent());

    EXPECT_EQ(Position(three->firstChild(), 3), endOfParagraph(createVisiblePositionInDOMTree(*two->firstChild(), 2)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(two->firstChild(), 2), endOfParagraph(createVisiblePositionInFlatTree(*two->firstChild(), 2)).deepEquivalent());
}

TEST_F(VisibleUnitsTest, endOfParagraphSimple)
{
    setBodyContent("<div id=sample>1ab\nde</div>");

    Node* sample = document().getElementById("sample");
    Node* text = sample->firstChild();

    EXPECT_EQ(Position(text, 6), endOfParagraph(createVisiblePositionInDOMTree(*text, 0)).deepEquivalent());
    EXPECT_EQ(Position(text, 6), endOfParagraph(createVisiblePositionInDOMTree(*text, 1)).deepEquivalent());
    EXPECT_EQ(Position(text, 6), endOfParagraph(createVisiblePositionInDOMTree(*text, 2)).deepEquivalent());
    EXPECT_EQ(Position(text, 6), endOfParagraph(createVisiblePositionInDOMTree(*text, 3)).deepEquivalent());
    EXPECT_EQ(Position(text, 6), endOfParagraph(createVisiblePositionInDOMTree(*text, 4)).deepEquivalent());
    EXPECT_EQ(Position(text, 6), endOfParagraph(createVisiblePositionInDOMTree(*text, 5)).deepEquivalent());
    EXPECT_EQ(Position(text, 6), endOfParagraph(createVisiblePositionInDOMTree(*text, 6)).deepEquivalent());
}

TEST_F(VisibleUnitsTest, endOfParagraphSimplePre)
{
    setBodyContent("<pre id=sample>1ab\nde</pre>");

    Node* sample = document().getElementById("sample");
    Node* text = sample->firstChild();

    EXPECT_EQ(Position(text, 3), endOfParagraph(createVisiblePositionInDOMTree(*text, 0)).deepEquivalent());
    EXPECT_EQ(Position(text, 3), endOfParagraph(createVisiblePositionInDOMTree(*text, 1)).deepEquivalent());
    EXPECT_EQ(Position(text, 3), endOfParagraph(createVisiblePositionInDOMTree(*text, 2)).deepEquivalent());
    EXPECT_EQ(Position(text, 3), endOfParagraph(createVisiblePositionInDOMTree(*text, 3)).deepEquivalent());
    EXPECT_EQ(Position(text, 6), endOfParagraph(createVisiblePositionInDOMTree(*text, 4)).deepEquivalent());
    EXPECT_EQ(Position(text, 6), endOfParagraph(createVisiblePositionInDOMTree(*text, 5)).deepEquivalent());
    EXPECT_EQ(Position(text, 6), endOfParagraph(createVisiblePositionInDOMTree(*text, 6)).deepEquivalent());
}

TEST_F(VisibleUnitsTest, endOfSentence)
{
    const char* bodyContent = "<a id=host><b id=one>1</b><b id=two>22</b></a>";
    const char* shadowContent = "<p><i id=three>333</i> <content select=#two></content> <content select=#one></content> <i id=four>4444</i></p>";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent, "host");
    ShadowRoot* shadowRoot = setShadowContent(shadowContent, "host");

    Node* one = document().getElementById("one")->firstChild();
    Node* two = document().getElementById("two")->firstChild();
    Node* three = shadowRoot->getElementById("three")->firstChild();
    Node* four = shadowRoot->getElementById("four")->firstChild();

    EXPECT_EQ(Position(two, 2), endOfSentence(createVisiblePositionInDOMTree(*one, 0)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(four, 4), endOfSentence(createVisiblePositionInFlatTree(*one, 0)).deepEquivalent());

    EXPECT_EQ(Position(two, 2), endOfSentence(createVisiblePositionInDOMTree(*one, 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(four, 4), endOfSentence(createVisiblePositionInFlatTree(*one, 1)).deepEquivalent());

    EXPECT_EQ(Position(two, 2), endOfSentence(createVisiblePositionInDOMTree(*two, 0)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(four, 4), endOfSentence(createVisiblePositionInFlatTree(*two, 0)).deepEquivalent());

    EXPECT_EQ(Position(two, 2), endOfSentence(createVisiblePositionInDOMTree(*two, 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(four, 4), endOfSentence(createVisiblePositionInFlatTree(*two, 1)).deepEquivalent());

    EXPECT_EQ(Position(four, 4), endOfSentence(createVisiblePositionInDOMTree(*three, 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(four, 4), endOfSentence(createVisiblePositionInFlatTree(*three, 1)).deepEquivalent());

    EXPECT_EQ(Position(four, 4), endOfSentence(createVisiblePositionInDOMTree(*four, 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(four, 4), endOfSentence(createVisiblePositionInFlatTree(*four, 1)).deepEquivalent());
}

TEST_F(VisibleUnitsTest, endOfWord)
{
    const char* bodyContent = "<a id=host><b id=one>1</b> <b id=two>22</b></a><i id=three>333</i>";
    const char* shadowContent = "<p><u id=four>44444</u><content select=#two></content><span id=space> </span><content select=#one></content><u id=five>55555</u></p>";
    setBodyContent(bodyContent);
    ShadowRoot* shadowRoot = setShadowContent(shadowContent, "host");

    Node* one = document().getElementById("one")->firstChild();
    Node* two = document().getElementById("two")->firstChild();
    Node* three = document().getElementById("three")->firstChild();
    Node* four = shadowRoot->getElementById("four")->firstChild();
    Node* five = shadowRoot->getElementById("five")->firstChild();

    EXPECT_EQ(Position(three, 3), endOfWord(createVisiblePositionInDOMTree(*one, 0)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(five, 5), endOfWord(createVisiblePositionInFlatTree(*one, 0)).deepEquivalent());

    EXPECT_EQ(Position(three, 3), endOfWord(createVisiblePositionInDOMTree(*one, 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(five, 5), endOfWord(createVisiblePositionInFlatTree(*one, 1)).deepEquivalent());

    EXPECT_EQ(Position(three, 3), endOfWord(createVisiblePositionInDOMTree(*two, 0)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(two, 2), endOfWord(createVisiblePositionInFlatTree(*two, 0)).deepEquivalent());

    EXPECT_EQ(Position(three, 3), endOfWord(createVisiblePositionInDOMTree(*two, 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(two, 2), endOfWord(createVisiblePositionInFlatTree(*two, 1)).deepEquivalent());

    EXPECT_EQ(Position(three, 3), endOfWord(createVisiblePositionInDOMTree(*three, 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(three, 3), endOfWord(createVisiblePositionInFlatTree(*three, 1)).deepEquivalent());

    EXPECT_EQ(Position(four, 5), endOfWord(createVisiblePositionInDOMTree(*four, 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(two, 2), endOfWord(createVisiblePositionInFlatTree(*four, 1)).deepEquivalent());

    EXPECT_EQ(Position(five, 5), endOfWord(createVisiblePositionInDOMTree(*five, 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(five, 5), endOfWord(createVisiblePositionInFlatTree(*five, 1)).deepEquivalent());
}

TEST_F(VisibleUnitsTest, isEndOfEditableOrNonEditableContent)
{
    const char* bodyContent = "<a id=host><b id=one contenteditable>1</b><b id=two>22</b></a>";
    const char* shadowContent = "<content select=#two></content></p><p><content select=#one></content>";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent, "host");

    Element* one = document().getElementById("one");
    Element* two = document().getElementById("two");

    EXPECT_FALSE(isEndOfEditableOrNonEditableContent(createVisiblePositionInDOMTree(*one->firstChild(), 1)));
    EXPECT_TRUE(isEndOfEditableOrNonEditableContent(createVisiblePositionInFlatTree(*one->firstChild(), 1)));

    EXPECT_TRUE(isEndOfEditableOrNonEditableContent(createVisiblePositionInDOMTree(*two->firstChild(), 2)));
    EXPECT_FALSE(isEndOfEditableOrNonEditableContent(createVisiblePositionInFlatTree(*two->firstChild(), 2)));
}

TEST_F(VisibleUnitsTest, isEndOfEditableOrNonEditableContentWithInput)
{
    const char* bodyContent = "<input id=sample value=ab>cde";
    setBodyContent(bodyContent);

    Node* text = toHTMLTextFormControlElement(document().getElementById("sample"))->innerEditorElement()->firstChild();

    EXPECT_FALSE(isEndOfEditableOrNonEditableContent(createVisiblePositionInDOMTree(*text, 0)));
    EXPECT_FALSE(isEndOfEditableOrNonEditableContent(createVisiblePositionInFlatTree(*text, 0)));

    EXPECT_FALSE(isEndOfEditableOrNonEditableContent(createVisiblePositionInDOMTree(*text, 1)));
    EXPECT_FALSE(isEndOfEditableOrNonEditableContent(createVisiblePositionInFlatTree(*text, 1)));

    EXPECT_TRUE(isEndOfEditableOrNonEditableContent(createVisiblePositionInDOMTree(*text, 2)));
    EXPECT_TRUE(isEndOfEditableOrNonEditableContent(createVisiblePositionInFlatTree(*text, 2)));
}

TEST_F(VisibleUnitsTest, isEndOfLine)
{
    const char* bodyContent = "<a id=host><b id=one>11</b><b id=two>22</b></a><i id=three>333</i><i id=four>4444</i><br>";
    const char* shadowContent = "<div><u id=five>55555</u><content select=#two></content><br><u id=six>666666</u><br><content select=#one></content><u id=seven>7777777</u></div>";
    setBodyContent(bodyContent);
    ShadowRoot* shadowRoot = setShadowContent(shadowContent, "host");

    Node* one = document().getElementById("one")->firstChild();
    Node* two = document().getElementById("two")->firstChild();
    Node* three = document().getElementById("three")->firstChild();
    Node* four = document().getElementById("four")->firstChild();
    Node* five = shadowRoot->getElementById("five")->firstChild();
    Node* six = shadowRoot->getElementById("six")->firstChild();
    Node* seven = shadowRoot->getElementById("seven")->firstChild();

    EXPECT_FALSE(isEndOfLine(createVisiblePositionInDOMTree(*one, 0)));
    EXPECT_FALSE(isEndOfLine(createVisiblePositionInFlatTree(*one, 0)));

    EXPECT_FALSE(isEndOfLine(createVisiblePositionInDOMTree(*one, 1)));
    EXPECT_FALSE(isEndOfLine(createVisiblePositionInFlatTree(*one, 1)));

    EXPECT_FALSE(isEndOfLine(createVisiblePositionInDOMTree(*two, 2)));
    EXPECT_TRUE(isEndOfLine(createVisiblePositionInFlatTree(*two, 2)));

    EXPECT_FALSE(isEndOfLine(createVisiblePositionInDOMTree(*three, 3)));
    EXPECT_FALSE(isEndOfLine(createVisiblePositionInFlatTree(*three, 3)));

    EXPECT_TRUE(isEndOfLine(createVisiblePositionInDOMTree(*four, 4)));
    EXPECT_TRUE(isEndOfLine(createVisiblePositionInFlatTree(*four, 4)));

    EXPECT_TRUE(isEndOfLine(createVisiblePositionInDOMTree(*five, 5)));
    EXPECT_FALSE(isEndOfLine(createVisiblePositionInFlatTree(*five, 5)));

    EXPECT_TRUE(isEndOfLine(createVisiblePositionInDOMTree(*six, 6)));
    EXPECT_TRUE(isEndOfLine(createVisiblePositionInFlatTree(*six, 6)));

    EXPECT_TRUE(isEndOfLine(createVisiblePositionInDOMTree(*seven, 7)));
    EXPECT_TRUE(isEndOfLine(createVisiblePositionInFlatTree(*seven, 7)));
}

TEST_F(VisibleUnitsTest, isLogicalEndOfLine)
{
    const char* bodyContent = "<a id=host><b id=one>11</b><b id=two>22</b></a><i id=three>333</i><i id=four>4444</i><br>";
    const char* shadowContent = "<div><u id=five>55555</u><content select=#two></content><br><u id=six>666666</u><br><content select=#one></content><u id=seven>7777777</u></div>";
    setBodyContent(bodyContent);
    ShadowRoot* shadowRoot = setShadowContent(shadowContent, "host");

    Node* one = document().getElementById("one")->firstChild();
    Node* two = document().getElementById("two")->firstChild();
    Node* three = document().getElementById("three")->firstChild();
    Node* four = document().getElementById("four")->firstChild();
    Node* five = shadowRoot->getElementById("five")->firstChild();
    Node* six = shadowRoot->getElementById("six")->firstChild();
    Node* seven = shadowRoot->getElementById("seven")->firstChild();

    EXPECT_FALSE(isLogicalEndOfLine(createVisiblePositionInDOMTree(*one, 0)));
    EXPECT_FALSE(isLogicalEndOfLine(createVisiblePositionInFlatTree(*one, 0)));

    EXPECT_FALSE(isLogicalEndOfLine(createVisiblePositionInDOMTree(*one, 1)));
    EXPECT_FALSE(isLogicalEndOfLine(createVisiblePositionInFlatTree(*one, 1)));

    EXPECT_FALSE(isLogicalEndOfLine(createVisiblePositionInDOMTree(*two, 2)));
    EXPECT_TRUE(isLogicalEndOfLine(createVisiblePositionInFlatTree(*two, 2)));

    EXPECT_FALSE(isLogicalEndOfLine(createVisiblePositionInDOMTree(*three, 3)));
    EXPECT_FALSE(isLogicalEndOfLine(createVisiblePositionInFlatTree(*three, 3)));

    EXPECT_TRUE(isLogicalEndOfLine(createVisiblePositionInDOMTree(*four, 4)));
    EXPECT_TRUE(isLogicalEndOfLine(createVisiblePositionInFlatTree(*four, 4)));

    EXPECT_TRUE(isLogicalEndOfLine(createVisiblePositionInDOMTree(*five, 5)));
    EXPECT_FALSE(isLogicalEndOfLine(createVisiblePositionInFlatTree(*five, 5)));

    EXPECT_TRUE(isLogicalEndOfLine(createVisiblePositionInDOMTree(*six, 6)));
    EXPECT_TRUE(isLogicalEndOfLine(createVisiblePositionInFlatTree(*six, 6)));

    EXPECT_TRUE(isLogicalEndOfLine(createVisiblePositionInDOMTree(*seven, 7)));
    EXPECT_TRUE(isLogicalEndOfLine(createVisiblePositionInFlatTree(*seven, 7)));
}

TEST_F(VisibleUnitsTest, inSameLine)
{
    const char* bodyContent = "<p id='host'>00<b id='one'>11</b><b id='two'>22</b>33</p>";
    const char* shadowContent = "<div><span id='s4'>44</span><content select=#two></content><br><span id='s5'>55</span><br><content select=#one></content><span id='s6'>66</span></div>";
    setBodyContent(bodyContent);
    ShadowRoot* shadowRoot = setShadowContent(shadowContent, "host");

    Element* body = document().body();
    Element* one = body->querySelector("#one", ASSERT_NO_EXCEPTION);
    Element* two = body->querySelector("#two", ASSERT_NO_EXCEPTION);
    Element* four = shadowRoot->querySelector("#s4", ASSERT_NO_EXCEPTION);
    Element* five = shadowRoot->querySelector("#s5", ASSERT_NO_EXCEPTION);

    EXPECT_TRUE(inSameLine(positionWithAffinityInDOMTree(*one, 0), positionWithAffinityInDOMTree(*two, 0)));
    EXPECT_TRUE(inSameLine(positionWithAffinityInDOMTree(*one->firstChild(), 0), positionWithAffinityInDOMTree(*two->firstChild(), 0)));
    EXPECT_FALSE(inSameLine(positionWithAffinityInDOMTree(*one->firstChild(), 0), positionWithAffinityInDOMTree(*five->firstChild(), 0)));
    EXPECT_FALSE(inSameLine(positionWithAffinityInDOMTree(*two->firstChild(), 0), positionWithAffinityInDOMTree(*four->firstChild(), 0)));

    EXPECT_TRUE(inSameLine(createVisiblePositionInDOMTree(*one, 0), createVisiblePositionInDOMTree(*two, 0)));
    EXPECT_TRUE(inSameLine(createVisiblePositionInDOMTree(*one->firstChild(), 0), createVisiblePositionInDOMTree(*two->firstChild(), 0)));
    EXPECT_FALSE(inSameLine(createVisiblePositionInDOMTree(*one->firstChild(), 0), createVisiblePositionInDOMTree(*five->firstChild(), 0)));
    EXPECT_FALSE(inSameLine(createVisiblePositionInDOMTree(*two->firstChild(), 0), createVisiblePositionInDOMTree(*four->firstChild(), 0)));

    EXPECT_FALSE(inSameLine(positionWithAffinityInFlatTree(*one, 0), positionWithAffinityInFlatTree(*two, 0)));
    EXPECT_FALSE(inSameLine(positionWithAffinityInFlatTree(*one->firstChild(), 0), positionWithAffinityInFlatTree(*two->firstChild(), 0)));
    EXPECT_FALSE(inSameLine(positionWithAffinityInFlatTree(*one->firstChild(), 0), positionWithAffinityInFlatTree(*five->firstChild(), 0)));
    EXPECT_TRUE(inSameLine(positionWithAffinityInFlatTree(*two->firstChild(), 0), positionWithAffinityInFlatTree(*four->firstChild(), 0)));

    EXPECT_FALSE(inSameLine(createVisiblePositionInFlatTree(*one, 0), createVisiblePositionInFlatTree(*two, 0)));
    EXPECT_FALSE(inSameLine(createVisiblePositionInFlatTree(*one->firstChild(), 0), createVisiblePositionInFlatTree(*two->firstChild(), 0)));
    EXPECT_FALSE(inSameLine(createVisiblePositionInFlatTree(*one->firstChild(), 0), createVisiblePositionInFlatTree(*five->firstChild(), 0)));
    EXPECT_TRUE(inSameLine(createVisiblePositionInFlatTree(*two->firstChild(), 0), createVisiblePositionInFlatTree(*four->firstChild(), 0)));
}

TEST_F(VisibleUnitsTest, isEndOfParagraph)
{
    const char* bodyContent = "<a id=host><b id=one>1</b><b id=two>22</b></a><b id=three>333</b>";
    const char* shadowContent = "<p><content select=#two></content></p><p><content select=#one></content></p>";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent, "host");

    Node* one = document().getElementById("one")->firstChild();
    Node* two = document().getElementById("two")->firstChild();
    Node* three = document().getElementById("three")->firstChild();

    EXPECT_FALSE(isEndOfParagraph(createVisiblePositionInDOMTree(*one, 0)));
    EXPECT_FALSE(isEndOfParagraph(createVisiblePositionInFlatTree(*one, 0)));

    EXPECT_FALSE(isEndOfParagraph(createVisiblePositionInDOMTree(*one, 1)));
    EXPECT_TRUE(isEndOfParagraph(createVisiblePositionInFlatTree(*one, 1)));

    EXPECT_FALSE(isEndOfParagraph(createVisiblePositionInDOMTree(*two, 2)));
    EXPECT_TRUE(isEndOfParagraph(createVisiblePositionInFlatTree(*two, 2)));

    EXPECT_FALSE(isEndOfParagraph(createVisiblePositionInDOMTree(*three, 0)));
    EXPECT_FALSE(isEndOfParagraph(createVisiblePositionInFlatTree(*three, 0)));

    EXPECT_TRUE(isEndOfParagraph(createVisiblePositionInDOMTree(*three, 3)));
    EXPECT_TRUE(isEndOfParagraph(createVisiblePositionInFlatTree(*three, 3)));
}

TEST_F(VisibleUnitsTest, isStartOfLine)
{
    const char* bodyContent = "<a id=host><b id=one>11</b><b id=two>22</b></a><i id=three>333</i><i id=four>4444</i><br>";
    const char* shadowContent = "<div><u id=five>55555</u><content select=#two></content><br><u id=six>666666</u><br><content select=#one></content><u id=seven>7777777</u></div>";
    setBodyContent(bodyContent);
    ShadowRoot* shadowRoot = setShadowContent(shadowContent, "host");

    Node* one = document().getElementById("one")->firstChild();
    Node* two = document().getElementById("two")->firstChild();
    Node* three = document().getElementById("three")->firstChild();
    Node* four = document().getElementById("four")->firstChild();
    Node* five = shadowRoot->getElementById("five")->firstChild();
    Node* six = shadowRoot->getElementById("six")->firstChild();
    Node* seven = shadowRoot->getElementById("seven")->firstChild();

    EXPECT_TRUE(isStartOfLine(createVisiblePositionInDOMTree(*one, 0)));
    EXPECT_TRUE(isStartOfLine(createVisiblePositionInFlatTree(*one, 0)));

    EXPECT_FALSE(isStartOfLine(createVisiblePositionInDOMTree(*one, 1)));
    EXPECT_FALSE(isStartOfLine(createVisiblePositionInFlatTree(*one, 1)));

    EXPECT_FALSE(isStartOfLine(createVisiblePositionInDOMTree(*two, 0)));
    EXPECT_FALSE(isStartOfLine(createVisiblePositionInFlatTree(*two, 0)));

    EXPECT_FALSE(isStartOfLine(createVisiblePositionInDOMTree(*three, 0)));
    EXPECT_TRUE(isStartOfLine(createVisiblePositionInFlatTree(*three, 0)));

    EXPECT_FALSE(isStartOfLine(createVisiblePositionInDOMTree(*four, 0)));
    EXPECT_FALSE(isStartOfLine(createVisiblePositionInFlatTree(*four, 0)));

    EXPECT_TRUE(isStartOfLine(createVisiblePositionInDOMTree(*five, 0)));
    EXPECT_TRUE(isStartOfLine(createVisiblePositionInFlatTree(*five, 0)));

    EXPECT_TRUE(isStartOfLine(createVisiblePositionInDOMTree(*six, 0)));
    EXPECT_TRUE(isStartOfLine(createVisiblePositionInFlatTree(*six, 0)));

    EXPECT_FALSE(isStartOfLine(createVisiblePositionInDOMTree(*seven, 0)));
    EXPECT_FALSE(isStartOfLine(createVisiblePositionInFlatTree(*seven, 0)));
}

TEST_F(VisibleUnitsTest, isStartOfParagraph)
{
    const char* bodyContent = "<b id=zero>0</b><a id=host><b id=one>1</b><b id=two>22</b></a><b id=three>333</b>";
    const char* shadowContent = "<p><content select=#two></content></p><p><content select=#one></content></p>";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent, "host");

    Node* zero = document().getElementById("zero")->firstChild();
    Node* one = document().getElementById("one")->firstChild();
    Node* two = document().getElementById("two")->firstChild();
    Node* three = document().getElementById("three")->firstChild();

    EXPECT_TRUE(isStartOfParagraph(createVisiblePositionInDOMTree(*zero, 0)));
    EXPECT_TRUE(isStartOfParagraph(createVisiblePositionInFlatTree(*zero, 0)));

    EXPECT_FALSE(isStartOfParagraph(createVisiblePositionInDOMTree(*one, 0)));
    EXPECT_TRUE(isStartOfParagraph(createVisiblePositionInFlatTree(*one, 0)));

    EXPECT_FALSE(isStartOfParagraph(createVisiblePositionInDOMTree(*one, 1)));
    EXPECT_FALSE(isStartOfParagraph(createVisiblePositionInFlatTree(*one, 1)));

    EXPECT_FALSE(isStartOfParagraph(createVisiblePositionInDOMTree(*two, 0)));
    EXPECT_TRUE(isStartOfParagraph(createVisiblePositionInFlatTree(*two, 0)));

    EXPECT_FALSE(isStartOfParagraph(createVisiblePositionInDOMTree(*three, 0)));
    EXPECT_TRUE(isStartOfParagraph(createVisiblePositionInFlatTree(*three, 0)));
}

TEST_F(VisibleUnitsTest, isVisuallyEquivalentCandidateWithHTMLHtmlElement)
{
    const char* bodyContent = "<html><div id=one contenteditable>1</div><span id=two contenteditable=false>22</span><span id=three contenteditable=false>333</span><span id=four contenteditable=false>333</span></html>";
    setBodyContent(bodyContent);

    Node* one = document().querySelector("#one", ASSERT_NO_EXCEPTION);
    Node* two = document().querySelector("#two", ASSERT_NO_EXCEPTION);
    Node* three = document().querySelector("#three", ASSERT_NO_EXCEPTION);
    Node* four = document().querySelector("#four", ASSERT_NO_EXCEPTION);
    Element* html = document().createElement("html", ASSERT_NO_EXCEPTION);
    // Move two, three and four into second html element.
    html->appendChild(two);
    html->appendChild(three);
    html->appendChild(four);
    one->appendChild(html);
    updateAllLifecyclePhases();

    EXPECT_FALSE(isVisuallyEquivalentCandidate(Position(document().documentElement(), 0)));

    EXPECT_FALSE(isVisuallyEquivalentCandidate(Position(one, 0)));
    EXPECT_FALSE(isVisuallyEquivalentCandidate(Position(one, 1)));

    EXPECT_TRUE(isVisuallyEquivalentCandidate(Position(one->firstChild(), 0)));
    EXPECT_TRUE(isVisuallyEquivalentCandidate(Position(one->firstChild(), 1)));

    EXPECT_TRUE(isVisuallyEquivalentCandidate(Position(html, 0)));
    EXPECT_TRUE(isVisuallyEquivalentCandidate(Position(html, 1)));
    EXPECT_TRUE(isVisuallyEquivalentCandidate(Position(html, 2)));

    EXPECT_FALSE(isVisuallyEquivalentCandidate(Position(two, 0)));
    EXPECT_FALSE(isVisuallyEquivalentCandidate(Position(two, 1)));
}

TEST_F(VisibleUnitsTest, isVisuallyEquivalentCandidateWithDocument)
{
    updateAllLifecyclePhases();

    EXPECT_FALSE(isVisuallyEquivalentCandidate(Position(&document(), 0)));
}

TEST_F(VisibleUnitsTest, leftPositionOf)
{
    const char* bodyContent = "<b id=zero>0</b><p id=host><b id=one>1</b><b id=two>22</b></p><b id=three>333</b>";
    const char* shadowContent = "<b id=four>4444</b><content select=#two></content><content select=#one></content><b id=five>55555</b>";
    setBodyContent(bodyContent);
    ShadowRoot* shadowRoot = setShadowContent(shadowContent, "host");

    Element* one = document().getElementById("one");
    Element* two = document().getElementById("two");
    Element* three = document().getElementById("three");
    Element* four = shadowRoot->getElementById("four");
    Element* five = shadowRoot->getElementById("five");

    EXPECT_EQ(Position(two->firstChild(), 1), leftPositionOf(createVisiblePosition(Position(one, 0))).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(two->firstChild(), 1), leftPositionOf(createVisiblePosition(PositionInFlatTree(one, 0))).deepEquivalent());

    EXPECT_EQ(Position(one->firstChild(), 0), leftPositionOf(createVisiblePosition(Position(two, 0))).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(four->firstChild(), 3), leftPositionOf(createVisiblePosition(PositionInFlatTree(two, 0))).deepEquivalent());

    EXPECT_EQ(Position(two->firstChild(), 2), leftPositionOf(createVisiblePosition(Position(three, 0))).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(five->firstChild(), 5), leftPositionOf(createVisiblePosition(PositionInFlatTree(three, 0))).deepEquivalent());
}

TEST_F(VisibleUnitsTest, localCaretRectOfPosition)
{
    const char* bodyContent = "<p id='host'><b id='one'>1</b></p><b id='two'>22</b>";
    const char* shadowContent = "<b id='two'>22</b><content select=#one></content><b id='three'>333</b>";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent, "host");

    Element* one = document().getElementById("one");

    LayoutObject* layoutObjectFromDOMTree;
    LayoutRect layoutRectFromDOMTree = localCaretRectOfPosition(Position(one->firstChild(), 0), layoutObjectFromDOMTree);

    LayoutObject* layoutObjectFromFlatTree;
    LayoutRect layoutRectFromFlatTree = localCaretRectOfPosition(PositionInFlatTree(one->firstChild(), 0), layoutObjectFromFlatTree);

    EXPECT_TRUE(layoutObjectFromDOMTree);
    EXPECT_FALSE(layoutRectFromDOMTree.isEmpty());
    EXPECT_EQ(layoutObjectFromDOMTree, layoutObjectFromFlatTree);
    EXPECT_EQ(layoutRectFromDOMTree, layoutRectFromFlatTree);
}

TEST_F(VisibleUnitsTest, logicalEndOfLine)
{
    const char* bodyContent = "<a id=host><b id=one>11</b><b id=two>22</b></a><i id=three>333</i><i id=four>4444</i><br>";
    const char* shadowContent = "<div><u id=five>55555</u><content select=#two></content><br><u id=six>666666</u><br><content select=#one></content><u id=seven>7777777</u></div>";
    setBodyContent(bodyContent);
    ShadowRoot* shadowRoot = setShadowContent(shadowContent, "host");

    Node* one = document().getElementById("one")->firstChild();
    Node* two = document().getElementById("two")->firstChild();
    Node* three = document().getElementById("three")->firstChild();
    Node* four = document().getElementById("four")->firstChild();
    Node* five = shadowRoot->getElementById("five")->firstChild();
    Node* six = shadowRoot->getElementById("six")->firstChild();
    Node* seven = shadowRoot->getElementById("seven")->firstChild();

    EXPECT_EQ(Position(seven, 7), logicalEndOfLine(createVisiblePositionInDOMTree(*one, 0)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(seven, 7), logicalEndOfLine(createVisiblePositionInFlatTree(*one, 0)).deepEquivalent());

    EXPECT_EQ(Position(seven, 7), logicalEndOfLine(createVisiblePositionInDOMTree(*one, 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(seven, 7), logicalEndOfLine(createVisiblePositionInFlatTree(*one, 1)).deepEquivalent());

    EXPECT_EQ(Position(seven, 7), logicalEndOfLine(createVisiblePositionInDOMTree(*two, 0)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(two, 2), logicalEndOfLine(createVisiblePositionInFlatTree(*two, 0)).deepEquivalent());

    // TODO(yosin) logicalEndOfLine(two, 1) -> (five, 5) is a broken result. We keep
    // it as a marker for future change.
    EXPECT_EQ(Position(five, 5), logicalEndOfLine(createVisiblePositionInDOMTree(*two, 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(two, 2), logicalEndOfLine(createVisiblePositionInFlatTree(*two, 1)).deepEquivalent());

    EXPECT_EQ(Position(five, 5), logicalEndOfLine(createVisiblePositionInDOMTree(*three, 0)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(four, 4), logicalEndOfLine(createVisiblePositionInFlatTree(*three, 1)).deepEquivalent());

    EXPECT_EQ(Position(four, 4), logicalEndOfLine(createVisiblePositionInDOMTree(*four, 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(four, 4), logicalEndOfLine(createVisiblePositionInFlatTree(*four, 1)).deepEquivalent());

    EXPECT_EQ(Position(five, 5), logicalEndOfLine(createVisiblePositionInDOMTree(*five, 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(two, 2), logicalEndOfLine(createVisiblePositionInFlatTree(*five, 1)).deepEquivalent());

    EXPECT_EQ(Position(six, 6), logicalEndOfLine(createVisiblePositionInDOMTree(*six, 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(six, 6), logicalEndOfLine(createVisiblePositionInFlatTree(*six, 1)).deepEquivalent());

    EXPECT_EQ(Position(seven, 7), logicalEndOfLine(createVisiblePositionInDOMTree(*seven, 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(seven, 7), logicalEndOfLine(createVisiblePositionInFlatTree(*seven, 1)).deepEquivalent());
}

TEST_F(VisibleUnitsTest, logicalStartOfLine)
{
    const char* bodyContent = "<a id=host><b id=one>11</b><b id=two>22</b></a><i id=three>333</i><i id=four>4444</i><br>";
    const char* shadowContent = "<div><u id=five>55555</u><content select=#two></content><br><u id=six>666666</u><br><content select=#one></content><u id=seven>7777777</u></div>";
    setBodyContent(bodyContent);
    ShadowRoot* shadowRoot = setShadowContent(shadowContent, "host");

    Node* one = document().getElementById("one")->firstChild();
    Node* two = document().getElementById("two")->firstChild();
    Node* three = document().getElementById("three")->firstChild();
    Node* four = document().getElementById("four")->firstChild();
    Node* five = shadowRoot->getElementById("five")->firstChild();
    Node* six = shadowRoot->getElementById("six")->firstChild();
    Node* seven = shadowRoot->getElementById("seven")->firstChild();

    EXPECT_EQ(Position(one, 0), logicalStartOfLine(createVisiblePositionInDOMTree(*one, 0)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(one, 0), logicalStartOfLine(createVisiblePositionInFlatTree(*one, 0)).deepEquivalent());

    EXPECT_EQ(Position(one, 0), logicalStartOfLine(createVisiblePositionInDOMTree(*one, 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(one, 0), logicalStartOfLine(createVisiblePositionInFlatTree(*one, 1)).deepEquivalent());

    EXPECT_EQ(Position(one, 0), logicalStartOfLine(createVisiblePositionInDOMTree(*two, 0)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(five, 0), logicalStartOfLine(createVisiblePositionInFlatTree(*two, 0)).deepEquivalent());

    EXPECT_EQ(Position(five, 0), logicalStartOfLine(createVisiblePositionInDOMTree(*two, 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(five, 0), logicalStartOfLine(createVisiblePositionInFlatTree(*two, 1)).deepEquivalent());

    EXPECT_EQ(Position(five, 0), logicalStartOfLine(createVisiblePositionInDOMTree(*three, 0)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(three, 0), logicalStartOfLine(createVisiblePositionInFlatTree(*three, 1)).deepEquivalent());

    // TODO(yosin) logicalStartOfLine(four, 1) -> (two, 2) is a broken result.
    // We keep it as a marker for future change.
    EXPECT_EQ(Position(two, 2), logicalStartOfLine(createVisiblePositionInDOMTree(*four, 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(three, 0), logicalStartOfLine(createVisiblePositionInFlatTree(*four, 1)).deepEquivalent());

    EXPECT_EQ(Position(five, 0), logicalStartOfLine(createVisiblePositionInDOMTree(*five, 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(five, 0), logicalStartOfLine(createVisiblePositionInFlatTree(*five, 1)).deepEquivalent());

    EXPECT_EQ(Position(six, 0), logicalStartOfLine(createVisiblePositionInDOMTree(*six, 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(six, 0), logicalStartOfLine(createVisiblePositionInFlatTree(*six, 1)).deepEquivalent());

    EXPECT_EQ(Position(one, 0), logicalStartOfLine(createVisiblePositionInDOMTree(*seven, 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(one, 0), logicalStartOfLine(createVisiblePositionInFlatTree(*seven, 1)).deepEquivalent());
}

TEST_F(VisibleUnitsTest, mostBackwardCaretPositionAfterAnchor)
{
    const char* bodyContent = "<p id='host'><b id='one'>1</b></p><b id='two'>22</b>";
    const char* shadowContent = "<b id='two'>22</b><content select=#one></content><b id='three'>333</b>";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent, "host");

    Element* host = document().getElementById("host");

    EXPECT_EQ(Position::lastPositionInNode(host), mostForwardCaretPosition(Position::afterNode(host)));
    EXPECT_EQ(PositionInFlatTree::lastPositionInNode(host), mostForwardCaretPosition(PositionInFlatTree::afterNode(host)));
}

TEST_F(VisibleUnitsTest, mostBackwardCaretPositionFirstLetter)
{
    // Note: first-letter pseudo element contains letter and punctuations.
    const char* bodyContent = "<style>p:first-letter {color:red;}</style><p id=sample> (2)45 </p>";
    setBodyContent(bodyContent);

    Node* sample = document().getElementById("sample")->firstChild();

    EXPECT_EQ(Position(sample->parentNode(), 0), mostBackwardCaretPosition(Position(sample, 0)));
    EXPECT_EQ(Position(sample->parentNode(), 0), mostBackwardCaretPosition(Position(sample, 1)));
    EXPECT_EQ(Position(sample, 2), mostBackwardCaretPosition(Position(sample, 2)));
    EXPECT_EQ(Position(sample, 3), mostBackwardCaretPosition(Position(sample, 3)));
    EXPECT_EQ(Position(sample, 4), mostBackwardCaretPosition(Position(sample, 4)));
    EXPECT_EQ(Position(sample, 5), mostBackwardCaretPosition(Position(sample, 5)));
    EXPECT_EQ(Position(sample, 6), mostBackwardCaretPosition(Position(sample, 6)));
    EXPECT_EQ(Position(sample, 6), mostBackwardCaretPosition(Position(sample, 7)));
    EXPECT_EQ(Position(sample, 6), mostBackwardCaretPosition(Position::lastPositionInNode(sample->parentNode())));
    EXPECT_EQ(Position(sample, 6), mostBackwardCaretPosition(Position::afterNode(sample->parentNode())));
    EXPECT_EQ(Position::lastPositionInNode(document().body()), mostBackwardCaretPosition(Position::lastPositionInNode(document().body())));
}

TEST_F(VisibleUnitsTest, mostBackwardCaretPositionFirstLetterSplit)
{
    const char* bodyContent = "<style>p:first-letter {color:red;}</style><p id=sample>abc</p>";
    setBodyContent(bodyContent);

    Node* sample = document().getElementById("sample");
    Node* firstLetter = sample->firstChild();
    // Split "abc" into "a" "bc"
    Text* remaining = toText(firstLetter)->splitText(1, ASSERT_NO_EXCEPTION);
    updateAllLifecyclePhases();

    EXPECT_EQ(Position(sample, 0), mostBackwardCaretPosition(Position(firstLetter, 0)));
    EXPECT_EQ(Position(firstLetter, 1), mostBackwardCaretPosition(Position(firstLetter, 1)));
    EXPECT_EQ(Position(firstLetter, 1), mostBackwardCaretPosition(Position(remaining, 0)));
    EXPECT_EQ(Position(remaining, 1), mostBackwardCaretPosition(Position(remaining, 1)));
    EXPECT_EQ(Position(remaining, 2), mostBackwardCaretPosition(Position(remaining, 2)));
    EXPECT_EQ(Position(remaining, 2), mostBackwardCaretPosition(Position::lastPositionInNode(sample)));
    EXPECT_EQ(Position(remaining, 2), mostBackwardCaretPosition(Position::afterNode(sample)));
}

TEST_F(VisibleUnitsTest, mostForwardCaretPositionAfterAnchor)
{
    const char* bodyContent = "<p id='host'><b id='one'>1</b></p>";
    const char* shadowContent = "<b id='two'>22</b><content select=#one></content><b id='three'>333</b>";
    setBodyContent(bodyContent);
    ShadowRoot* shadowRoot = setShadowContent(shadowContent, "host");
    updateAllLifecyclePhases();

    Element* host = document().getElementById("host");
    Element* one = document().getElementById("one");
    Element* three = shadowRoot->getElementById("three");

    EXPECT_EQ(Position(one->firstChild(), 1), mostBackwardCaretPosition(Position::afterNode(host)));
    EXPECT_EQ(PositionInFlatTree(three->firstChild(), 3), mostBackwardCaretPosition(PositionInFlatTree::afterNode(host)));
}

TEST_F(VisibleUnitsTest, mostForwardCaretPositionFirstLetter)
{
    // Note: first-letter pseudo element contains letter and punctuations.
    const char* bodyContent = "<style>p:first-letter {color:red;}</style><p id=sample> (2)45 </p>";
    setBodyContent(bodyContent);

    Node* sample = document().getElementById("sample")->firstChild();

    EXPECT_EQ(Position(document().body(), 0), mostForwardCaretPosition(Position::firstPositionInNode(document().body())));
    EXPECT_EQ(Position(sample, 1), mostForwardCaretPosition(Position::beforeNode(sample->parentNode())));
    EXPECT_EQ(Position(sample, 1), mostForwardCaretPosition(Position::firstPositionInNode(sample->parentNode())));
    EXPECT_EQ(Position(sample, 1), mostForwardCaretPosition(Position(sample, 0)));
    EXPECT_EQ(Position(sample, 1), mostForwardCaretPosition(Position(sample, 1)));
    EXPECT_EQ(Position(sample, 2), mostForwardCaretPosition(Position(sample, 2)));
    EXPECT_EQ(Position(sample, 3), mostForwardCaretPosition(Position(sample, 3)));
    EXPECT_EQ(Position(sample, 4), mostForwardCaretPosition(Position(sample, 4)));
    EXPECT_EQ(Position(sample, 5), mostForwardCaretPosition(Position(sample, 5)));
    EXPECT_EQ(Position(sample, 7), mostForwardCaretPosition(Position(sample, 6)));
    EXPECT_EQ(Position(sample, 7), mostForwardCaretPosition(Position(sample, 7)));
}

TEST_F(VisibleUnitsTest, nextPositionOf)
{
    const char* bodyContent = "<b id=zero>0</b><p id=host><b id=one>1</b><b id=two>22</b></p><b id=three>333</b>";
    const char* shadowContent = "<b id=four>4444</b><content select=#two></content><content select=#one></content><b id=five>55555</b>";
    setBodyContent(bodyContent);
    ShadowRoot* shadowRoot = setShadowContent(shadowContent, "host");

    Element* zero = document().getElementById("zero");
    Element* one = document().getElementById("one");
    Element* two = document().getElementById("two");
    Element* three = document().getElementById("three");
    Element* four = shadowRoot->getElementById("four");
    Element* five = shadowRoot->getElementById("five");

    EXPECT_EQ(Position(one->firstChild(), 0), nextPositionOf(createVisiblePosition(Position(zero, 1))).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(four->firstChild(), 0), nextPositionOf(createVisiblePosition(PositionInFlatTree(zero, 1))).deepEquivalent());

    EXPECT_EQ(Position(one->firstChild(), 1), nextPositionOf(createVisiblePosition(Position(one, 0))).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(one->firstChild(), 1), nextPositionOf(createVisiblePosition(PositionInFlatTree(one, 0))).deepEquivalent());

    EXPECT_EQ(Position(two->firstChild(), 1), nextPositionOf(createVisiblePosition(Position(one, 1))).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(five->firstChild(), 1), nextPositionOf(createVisiblePosition(PositionInFlatTree(one, 1))).deepEquivalent());

    EXPECT_EQ(Position(three->firstChild(), 0), nextPositionOf(createVisiblePosition(Position(two, 2))).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(one->firstChild(), 1), nextPositionOf(createVisiblePosition(PositionInFlatTree(two, 2))).deepEquivalent());
}

TEST_F(VisibleUnitsTest, previousPositionOf)
{
    const char* bodyContent = "<b id=zero>0</b><p id=host><b id=one>1</b><b id=two>22</b></p><b id=three>333</b>";
    const char* shadowContent = "<b id=four>4444</b><content select=#two></content><content select=#one></content><b id=five>55555</b>";
    setBodyContent(bodyContent);
    ShadowRoot* shadowRoot = setShadowContent(shadowContent, "host");

    Node* zero = document().getElementById("zero")->firstChild();
    Node* one = document().getElementById("one")->firstChild();
    Node* two = document().getElementById("two")->firstChild();
    Node* three = document().getElementById("three")->firstChild();
    Node* four = shadowRoot->getElementById("four")->firstChild();
    Node* five = shadowRoot->getElementById("five")->firstChild();

    EXPECT_EQ(Position(zero, 0), previousPositionOf(createVisiblePosition(Position(zero, 1))).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(zero, 0), previousPositionOf(createVisiblePosition(PositionInFlatTree(zero, 1))).deepEquivalent());

    EXPECT_EQ(Position(zero, 1), previousPositionOf(createVisiblePosition(Position(one, 0))).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(two, 1), previousPositionOf(createVisiblePosition(PositionInFlatTree(one, 0))).deepEquivalent());

    EXPECT_EQ(Position(one, 0), previousPositionOf(createVisiblePosition(Position(one, 1))).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(two, 2), previousPositionOf(createVisiblePosition(PositionInFlatTree(one, 1))).deepEquivalent());

    EXPECT_EQ(Position(one, 0), previousPositionOf(createVisiblePosition(Position(two, 0))).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(four, 3), previousPositionOf(createVisiblePosition(PositionInFlatTree(two, 0))).deepEquivalent());

    // DOM tree to shadow tree
    EXPECT_EQ(Position(two, 2), previousPositionOf(createVisiblePosition(Position(three, 0))).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(five, 5), previousPositionOf(createVisiblePosition(PositionInFlatTree(three, 0))).deepEquivalent());

    // Shadow tree to DOM tree
    EXPECT_EQ(Position(), previousPositionOf(createVisiblePosition(Position(four, 0))).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(zero, 1), previousPositionOf(createVisiblePosition(PositionInFlatTree(four, 0))).deepEquivalent());

    // Note: Canonicalization maps (five, 0) to (four, 4) in DOM tree and
    // (one, 1) in flat tree.
    EXPECT_EQ(Position(four, 4), previousPositionOf(createVisiblePosition(Position(five, 1))).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(one, 1), previousPositionOf(createVisiblePosition(PositionInFlatTree(five, 1))).deepEquivalent());
}

TEST_F(VisibleUnitsTest, previousPositionOfOneCharPerLine)
{
    const char* bodyContent = "<div id=sample style='font-size: 500px'>A&#x714a;&#xfa67;</div>";
    setBodyContent(bodyContent);

    Node* sample = document().getElementById("sample")->firstChild();

    // In case of each line has one character, VisiblePosition are:
    // [C,Dn]   [C,Up]  [B, Dn]   [B, Up]
    //  A        A       A         A|
    //  B        B|     |B         B
    // |C        C       C         C
    EXPECT_EQ(PositionWithAffinity(Position(sample, 1)), previousPositionOf(createVisiblePosition(Position(sample, 2))).toPositionWithAffinity());
    EXPECT_EQ(PositionWithAffinity(Position(sample, 1)), previousPositionOf(createVisiblePosition(Position(sample, 2), TextAffinity::Upstream)).toPositionWithAffinity());
}

TEST_F(VisibleUnitsTest, rendersInDifferentPositionAfterAnchor)
{
    const char* bodyContent = "<p id='sample'>00</p>";
    setBodyContent(bodyContent);
    Element* sample = document().getElementById("sample");

    EXPECT_FALSE(rendersInDifferentPosition(Position(), Position()));
    EXPECT_FALSE(rendersInDifferentPosition(Position(), Position::afterNode(sample)))
        << "if one of position is null, the reuslt is false.";
    EXPECT_FALSE(rendersInDifferentPosition(Position::afterNode(sample), Position(sample, 1)));
    EXPECT_FALSE(rendersInDifferentPosition(Position::lastPositionInNode(sample), Position(sample, 1)));
}

TEST_F(VisibleUnitsTest, rendersInDifferentPositionAfterAnchorWithHidden)
{
    const char* bodyContent = "<p><span id=one>11</span><span id=two style='display:none'>  </span></p>";
    setBodyContent(bodyContent);
    Element* one = document().getElementById("one");
    Element* two = document().getElementById("two");

    EXPECT_TRUE(rendersInDifferentPosition(Position::lastPositionInNode(one), Position(two, 0)))
        << "two doesn't have layout object";
}

TEST_F(VisibleUnitsTest, rendersInDifferentPositionAfterAnchorWithDifferentLayoutObjects)
{
    const char* bodyContent = "<p><span id=one>11</span><span id=two>  </span></p>";
    setBodyContent(bodyContent);
    Element* one = document().getElementById("one");
    Element* two = document().getElementById("two");

    EXPECT_FALSE(rendersInDifferentPosition(Position::lastPositionInNode(one), Position(two, 0)));
    EXPECT_FALSE(rendersInDifferentPosition(Position::lastPositionInNode(one), Position(two, 1)))
        << "width of two is zero since contents is collapsed whitespaces";
}

TEST_F(VisibleUnitsTest, renderedOffset)
{
    const char* bodyContent = "<div contenteditable><span id='sample1'>1</span><span id='sample2'>22</span></div>";
    setBodyContent(bodyContent);
    Element* sample1 = document().getElementById("sample1");
    Element* sample2 = document().getElementById("sample2");

    EXPECT_FALSE(rendersInDifferentPosition(Position::afterNode(sample1->firstChild()), Position(sample2->firstChild(), 0)));
    EXPECT_FALSE(rendersInDifferentPosition(Position::lastPositionInNode(sample1->firstChild()), Position(sample2->firstChild(), 0)));
}

TEST_F(VisibleUnitsTest, rightPositionOf)
{
    const char* bodyContent = "<b id=zero>0</b><p id=host><b id=one>1</b><b id=two>22</b></p><b id=three>333</b>";
    const char* shadowContent = "<p id=four>4444</p><content select=#two></content><content select=#one></content><p id=five>55555</p>";
    setBodyContent(bodyContent);
    ShadowRoot* shadowRoot = setShadowContent(shadowContent, "host");

    Node* one = document().getElementById("one")->firstChild();
    Node* two = document().getElementById("two")->firstChild();
    Node* three = document().getElementById("three")->firstChild();
    Node* four = shadowRoot->getElementById("four")->firstChild();
    Node* five = shadowRoot->getElementById("five")->firstChild();

    EXPECT_EQ(Position(), rightPositionOf(createVisiblePosition(Position(one, 1))).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(five, 0), rightPositionOf(createVisiblePosition(PositionInFlatTree(one, 1))).deepEquivalent());

    EXPECT_EQ(Position(one, 1), rightPositionOf(createVisiblePosition(Position(two, 2))).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(one, 1), rightPositionOf(createVisiblePosition(PositionInFlatTree(two, 2))).deepEquivalent());

    EXPECT_EQ(Position(five, 0), rightPositionOf(createVisiblePosition(Position(four, 4))).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(two, 0), rightPositionOf(createVisiblePosition(PositionInFlatTree(four, 4))).deepEquivalent());

    EXPECT_EQ(Position(), rightPositionOf(createVisiblePosition(Position(five, 5))).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(three, 0), rightPositionOf(createVisiblePosition(PositionInFlatTree(five, 5))).deepEquivalent());
}

TEST_F(VisibleUnitsTest, startOfDocument)
{
    const char* bodyContent = "<a id=host><b id=one>1</b><b id=two>22</b></a>";
    const char* shadowContent = "<p><content select=#two></content></p><p><content select=#one></content></p>";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent, "host");

    Node* one = document().getElementById("one")->firstChild();
    Node* two = document().getElementById("two")->firstChild();

    EXPECT_EQ(Position(one, 0), startOfDocument(createVisiblePositionInDOMTree(*one, 0)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(two, 0), startOfDocument(createVisiblePositionInFlatTree(*one, 0)).deepEquivalent());

    EXPECT_EQ(Position(one, 0), startOfDocument(createVisiblePositionInDOMTree(*two, 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(two, 0), startOfDocument(createVisiblePositionInFlatTree(*two, 1)).deepEquivalent());
}

TEST_F(VisibleUnitsTest, startOfLine)
{
    const char* bodyContent = "<a id=host><b id=one>11</b><b id=two>22</b></a><i id=three>333</i><i id=four>4444</i><br>";
    const char* shadowContent = "<div><u id=five>55555</u><content select=#two></content><br><u id=six>666666</u><br><content select=#one></content><u id=seven>7777777</u></div>";
    setBodyContent(bodyContent);
    ShadowRoot* shadowRoot = setShadowContent(shadowContent, "host");

    Node* one = document().getElementById("one")->firstChild();
    Node* two = document().getElementById("two")->firstChild();
    Node* three = document().getElementById("three")->firstChild();
    Node* four = document().getElementById("four")->firstChild();
    Node* five = shadowRoot->getElementById("five")->firstChild();
    Node* six = shadowRoot->getElementById("six")->firstChild();
    Node* seven = shadowRoot->getElementById("seven")->firstChild();

    EXPECT_EQ(Position(one, 0), startOfLine(createVisiblePositionInDOMTree(*one, 0)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(one, 0), startOfLine(createVisiblePositionInFlatTree(*one, 0)).deepEquivalent());

    EXPECT_EQ(Position(one, 0), startOfLine(createVisiblePositionInDOMTree(*one, 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(one, 0), startOfLine(createVisiblePositionInFlatTree(*one, 1)).deepEquivalent());

    EXPECT_EQ(Position(one, 0), startOfLine(createVisiblePositionInDOMTree(*two, 0)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(five, 0), startOfLine(createVisiblePositionInFlatTree(*two, 0)).deepEquivalent());

    EXPECT_EQ(Position(five, 0), startOfLine(createVisiblePositionInDOMTree(*two, 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(five, 0), startOfLine(createVisiblePositionInFlatTree(*two, 1)).deepEquivalent());

    EXPECT_EQ(Position(five, 0), startOfLine(createVisiblePositionInDOMTree(*three, 0)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(three, 0), startOfLine(createVisiblePositionInFlatTree(*three, 1)).deepEquivalent());

    // TODO(yosin) startOfLine(four, 1) -> (two, 2) is a broken result. We keep
    // it as a marker for future change.
    EXPECT_EQ(Position(two, 2), startOfLine(createVisiblePositionInDOMTree(*four, 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(three, 0), startOfLine(createVisiblePositionInFlatTree(*four, 1)).deepEquivalent());

    EXPECT_EQ(Position(five, 0), startOfLine(createVisiblePositionInDOMTree(*five, 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(five, 0), startOfLine(createVisiblePositionInFlatTree(*five, 1)).deepEquivalent());

    EXPECT_EQ(Position(six, 0), startOfLine(createVisiblePositionInDOMTree(*six, 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(six, 0), startOfLine(createVisiblePositionInFlatTree(*six, 1)).deepEquivalent());

    EXPECT_EQ(Position(one, 0), startOfLine(createVisiblePositionInDOMTree(*seven, 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(one, 0), startOfLine(createVisiblePositionInFlatTree(*seven, 1)).deepEquivalent());
}

TEST_F(VisibleUnitsTest, startOfParagraph)
{
    const char* bodyContent = "<b id=zero>0</b><a id=host><b id=one>1</b><b id=two>22</b></a><b id=three>333</b>";
    const char* shadowContent = "<p><content select=#two></content></p><p><content select=#one></content></p>";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent, "host");

    Node* zero = document().getElementById("zero")->firstChild();
    Node* one = document().getElementById("one")->firstChild();
    Node* two = document().getElementById("two")->firstChild();
    Node* three = document().getElementById("three")->firstChild();

    EXPECT_EQ(Position(zero, 0), startOfParagraph(createVisiblePositionInDOMTree(*one, 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(one, 0), startOfParagraph(createVisiblePositionInFlatTree(*one, 1)).deepEquivalent());

    EXPECT_EQ(Position(zero, 0), startOfParagraph(createVisiblePositionInDOMTree(*two, 2)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(two, 0), startOfParagraph(createVisiblePositionInFlatTree(*two, 2)).deepEquivalent());

    // DOM version of |startOfParagraph()| doesn't take account contents in
    // shadow tree.
    EXPECT_EQ(Position(zero, 0), startOfParagraph(createVisiblePositionInDOMTree(*three, 2)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(three, 0), startOfParagraph(createVisiblePositionInFlatTree(*three, 2)).deepEquivalent());

    // crbug.com/563777. startOfParagraph() unexpectedly returned a null
    // position with nested editable <BODY>s.
    Element* root = document().documentElement();
    root->setInnerHTML("<style>* { display:inline-table; }</style><body contenteditable=true><svg><svg><foreignObject>abc<svg></svg></foreignObject></svg></svg></body>", ASSERT_NO_EXCEPTION);
    Element* oldBody = document().body();
    root->setInnerHTML("<body contenteditable=true><svg><foreignObject><style>def</style>", ASSERT_NO_EXCEPTION);
    DCHECK_NE(oldBody, document().body());
    Node* foreignObject = document().body()->firstChild()->firstChild();
    foreignObject->insertBefore(oldBody, foreignObject->firstChild());
    Node* styleText = foreignObject->lastChild()->firstChild();
    DCHECK(styleText->isTextNode()) << styleText;
    updateAllLifecyclePhases();

    EXPECT_FALSE(startOfParagraph(createVisiblePosition(Position(styleText, 0))).isNull());
}

TEST_F(VisibleUnitsTest, startOfSentence)
{
    const char* bodyContent = "<a id=host><b id=one>1</b><b id=two>22</b></a>";
    const char* shadowContent = "<p><i id=three>333</i> <content select=#two></content> <content select=#one></content> <i id=four>4444</i></p>";
    setBodyContent(bodyContent);
    ShadowRoot* shadowRoot = setShadowContent(shadowContent, "host");

    Node* one = document().getElementById("one")->firstChild();
    Node* two = document().getElementById("two")->firstChild();
    Node* three = shadowRoot->getElementById("three")->firstChild();
    Node* four = shadowRoot->getElementById("four")->firstChild();

    EXPECT_EQ(Position(one, 0), startOfSentence(createVisiblePositionInDOMTree(*one, 0)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(three, 0), startOfSentence(createVisiblePositionInFlatTree(*one, 0)).deepEquivalent());

    EXPECT_EQ(Position(one, 0), startOfSentence(createVisiblePositionInDOMTree(*one, 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(three, 0), startOfSentence(createVisiblePositionInFlatTree(*one, 1)).deepEquivalent());

    EXPECT_EQ(Position(one, 0), startOfSentence(createVisiblePositionInDOMTree(*two, 0)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(three, 0), startOfSentence(createVisiblePositionInFlatTree(*two, 0)).deepEquivalent());

    EXPECT_EQ(Position(one, 0), startOfSentence(createVisiblePositionInDOMTree(*two, 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(three, 0), startOfSentence(createVisiblePositionInFlatTree(*two, 1)).deepEquivalent());

    EXPECT_EQ(Position(three, 0), startOfSentence(createVisiblePositionInDOMTree(*three, 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(three, 0), startOfSentence(createVisiblePositionInFlatTree(*three, 1)).deepEquivalent());

    EXPECT_EQ(Position(three, 0), startOfSentence(createVisiblePositionInDOMTree(*four, 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(three, 0), startOfSentence(createVisiblePositionInFlatTree(*four, 1)).deepEquivalent());
}

TEST_F(VisibleUnitsTest, startOfWord)
{
    const char* bodyContent = "<a id=host><b id=one>1</b> <b id=two>22</b></a><i id=three>333</i>";
    const char* shadowContent = "<p><u id=four>44444</u><content select=#two></content><span id=space> </span><content select=#one></content><u id=five>55555</u></p>";
    setBodyContent(bodyContent);
    ShadowRoot* shadowRoot = setShadowContent(shadowContent, "host");

    Node* one = document().getElementById("one")->firstChild();
    Node* two = document().getElementById("two")->firstChild();
    Node* three = document().getElementById("three")->firstChild();
    Node* four = shadowRoot->getElementById("four")->firstChild();
    Node* five = shadowRoot->getElementById("five")->firstChild();
    Node* space = shadowRoot->getElementById("space")->firstChild();

    EXPECT_EQ(Position(one, 0), startOfWord(createVisiblePositionInDOMTree(*one, 0)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(space, 1), startOfWord(createVisiblePositionInFlatTree(*one, 0)).deepEquivalent());

    EXPECT_EQ(Position(one, 0), startOfWord(createVisiblePositionInDOMTree(*one, 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(space, 1), startOfWord(createVisiblePositionInFlatTree(*one, 1)).deepEquivalent());

    EXPECT_EQ(Position(one, 0), startOfWord(createVisiblePositionInDOMTree(*two, 0)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(four, 0), startOfWord(createVisiblePositionInFlatTree(*two, 0)).deepEquivalent());

    EXPECT_EQ(Position(one, 0), startOfWord(createVisiblePositionInDOMTree(*two, 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(four, 0), startOfWord(createVisiblePositionInFlatTree(*two, 1)).deepEquivalent());

    EXPECT_EQ(Position(one, 0), startOfWord(createVisiblePositionInDOMTree(*three, 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(space, 1), startOfWord(createVisiblePositionInFlatTree(*three, 1)).deepEquivalent());

    EXPECT_EQ(Position(four, 0), startOfWord(createVisiblePositionInDOMTree(*four, 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(four, 0), startOfWord(createVisiblePositionInFlatTree(*four, 1)).deepEquivalent());

    EXPECT_EQ(Position(space, 1), startOfWord(createVisiblePositionInDOMTree(*five, 1)).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree(space, 1), startOfWord(createVisiblePositionInFlatTree(*five, 1)).deepEquivalent());
}

TEST_F(VisibleUnitsTest, endsOfNodeAreVisuallyDistinctPositionsWithInvisibleChild)
{
    // Repro case of crbug.com/582247
    const char* bodyContent = "<button> </button><script>document.designMode = 'on'</script>";
    setBodyContent(bodyContent);

    Node* button = document().querySelector("button", ASSERT_NO_EXCEPTION);
    EXPECT_TRUE(endsOfNodeAreVisuallyDistinctPositions(button));
}

TEST_F(VisibleUnitsTest, endsOfNodeAreVisuallyDistinctPositionsWithEmptyLayoutChild)
{
    // Repro case of crbug.com/584030
    const char* bodyContent = "<button><rt><script>document.designMode = 'on'</script></rt></button>";
    setBodyContent(bodyContent);

    Node* button = document().querySelector("button", ASSERT_NO_EXCEPTION);
    EXPECT_TRUE(endsOfNodeAreVisuallyDistinctPositions(button));
}

} // namespace blink
