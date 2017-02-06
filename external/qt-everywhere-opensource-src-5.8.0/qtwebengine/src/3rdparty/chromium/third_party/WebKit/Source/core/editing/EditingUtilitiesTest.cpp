// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/EditingUtilities.h"

#include "core/dom/StaticNodeList.h"
#include "core/editing/EditingTestBase.h"

namespace blink {

class EditingUtilitiesTest : public EditingTestBase {
};

TEST_F(EditingUtilitiesTest, directionOfEnclosingBlock)
{
    const char* bodyContent = "<p id='host'><b id='one'></b><b id='two'>22</b></p>";
    const char* shadowContent = "<content select=#two></content><p dir=rtl><content select=#one></content><p>";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent, "host");
    Node* one = document().getElementById("one");

    EXPECT_EQ(LTR, directionOfEnclosingBlock(Position(one, 0)));
    EXPECT_EQ(RTL, directionOfEnclosingBlock(PositionInFlatTree(one, 0)));
}

TEST_F(EditingUtilitiesTest, firstEditablePositionAfterPositionInRoot)
{
    const char* bodyContent = "<p id='host' contenteditable><b id='one'>1</b><b id='two'>22</b></p>";
    const char* shadowContent = "<content select=#two></content><content select=#one></content><b id='three'>333</b>";
    setBodyContent(bodyContent);
    ShadowRoot* shadowRoot = setShadowContent(shadowContent, "host");
    Element* host = document().getElementById("host");
    Node* one = document().getElementById("one");
    Node* two = document().getElementById("two");
    Node* three = shadowRoot->getElementById("three");

    EXPECT_EQ(Position(one, 0), firstEditablePositionAfterPositionInRoot(Position(one, 0), *host));
    EXPECT_EQ(Position(one->firstChild(), 0), firstEditableVisiblePositionAfterPositionInRoot(Position(one, 0), *host).deepEquivalent());

    EXPECT_EQ(PositionInFlatTree(one, 0), firstEditablePositionAfterPositionInRoot(PositionInFlatTree(one, 0), *host));
    EXPECT_EQ(PositionInFlatTree(two->firstChild(), 2), firstEditableVisiblePositionAfterPositionInRoot(PositionInFlatTree(one, 0), *host).deepEquivalent());

    EXPECT_EQ(Position::firstPositionInNode(host), firstEditablePositionAfterPositionInRoot(Position(three, 0), *host));
    EXPECT_EQ(Position(one->firstChild(), 0), firstEditableVisiblePositionAfterPositionInRoot(Position(three, 0), *host).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree::afterNode(host), firstEditablePositionAfterPositionInRoot(PositionInFlatTree(three, 0), *host));
    EXPECT_EQ(PositionInFlatTree::lastPositionInNode(host), firstEditableVisiblePositionAfterPositionInRoot(PositionInFlatTree(three, 0), *host).deepEquivalent());
}

TEST_F(EditingUtilitiesTest, enclosingBlock)
{
    const char* bodyContent = "<p id='host'><b id='one'>11</b></p>";
    const char* shadowContent = "<content select=#two></content><div id='three'><content select=#one></content></div>";
    setBodyContent(bodyContent);
    ShadowRoot* shadowRoot = setShadowContent(shadowContent, "host");
    Node* host = document().getElementById("host");
    Node* one = document().getElementById("one");
    Node* three = shadowRoot->getElementById("three");

    EXPECT_EQ(host, enclosingBlock(Position(one, 0), CannotCrossEditingBoundary));
    EXPECT_EQ(three, enclosingBlock(PositionInFlatTree(one, 0), CannotCrossEditingBoundary));
}

TEST_F(EditingUtilitiesTest, enclosingNodeOfType)
{
    const char* bodyContent = "<p id='host'><b id='one'>11</b></p>";
    const char* shadowContent = "<content select=#two></content><div id='three'><content select=#one></div></content>";
    setBodyContent(bodyContent);
    ShadowRoot* shadowRoot = setShadowContent(shadowContent, "host");
    Node* host = document().getElementById("host");
    Node* one = document().getElementById("one");
    Node* three = shadowRoot->getElementById("three");

    EXPECT_EQ(host, enclosingNodeOfType(Position(one, 0), isEnclosingBlock));
    EXPECT_EQ(three, enclosingNodeOfType(PositionInFlatTree(one, 0), isEnclosingBlock));
}

TEST_F(EditingUtilitiesTest, isEditablePositionWithTable)
{
    // We would like to have below DOM tree without HTML, HEAD and BODY element.
    //   <table id=table><caption>foo</caption></table>
    // However, |setBodyContent()| automatically creates HTML, HEAD and BODY
    // element. So, we build DOM tree manually.
    // Note: This is unusual HTML taken from http://crbug.com/574230
    Element* table = document().createElement("table", ASSERT_NO_EXCEPTION);
    table->setInnerHTML("<caption>foo</caption>", ASSERT_NO_EXCEPTION);
    while (document().firstChild())
        document().firstChild()->remove();
    document().appendChild(table);
    document().setDesignMode("on");
    updateAllLifecyclePhases();

    EXPECT_FALSE(isEditablePosition(Position(table, 0)));
}

TEST_F(EditingUtilitiesTest, tableElementJustBefore)
{
    const char* bodyContent = "<div contenteditable id=host><table id=table><tr><td>1</td></tr></table><b id=two>22</b></div>";
    const char* shadowContent = "<content select=#two></content><content select=#table></content>";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent, "host");
    Node* host = document().getElementById("host");
    Node* table = document().getElementById("table");

    EXPECT_EQ(table, tableElementJustBefore(VisiblePosition::afterNode(table)));
    EXPECT_EQ(table, tableElementJustBefore(VisiblePositionInFlatTree::afterNode(table)));

    EXPECT_EQ(table, tableElementJustBefore(VisiblePosition::lastPositionInNode(table)));
    EXPECT_EQ(table, tableElementJustBefore(createVisiblePosition(PositionInFlatTree::lastPositionInNode(table))));

    EXPECT_EQ(nullptr, tableElementJustBefore(createVisiblePosition(Position(host, 2))));
    EXPECT_EQ(table, tableElementJustBefore(createVisiblePosition(PositionInFlatTree(host, 2))));

    EXPECT_EQ(nullptr, tableElementJustBefore(VisiblePosition::afterNode(host)));
    EXPECT_EQ(nullptr, tableElementJustBefore(VisiblePositionInFlatTree::afterNode(host)));

    EXPECT_EQ(nullptr, tableElementJustBefore(VisiblePosition::lastPositionInNode(host)));
    EXPECT_EQ(table, tableElementJustBefore(createVisiblePosition(PositionInFlatTree::lastPositionInNode(host))));
}

TEST_F(EditingUtilitiesTest, lastEditablePositionBeforePositionInRoot)
{
    const char* bodyContent = "<p id='host' contenteditable><b id='one'>1</b><b id='two'>22</b></p>";
    const char* shadowContent = "<content select=#two></content><content select=#one></content><b id='three'>333</b>";
    setBodyContent(bodyContent);
    ShadowRoot* shadowRoot = setShadowContent(shadowContent, "host");
    Element* host = document().getElementById("host");
    Node* one = document().getElementById("one");
    Node* two = document().getElementById("two");
    Node* three = shadowRoot->getElementById("three");

    EXPECT_EQ(Position(one, 0), lastEditablePositionBeforePositionInRoot(Position(one, 0), *host));
    EXPECT_EQ(Position(one->firstChild(), 0), lastEditableVisiblePositionBeforePositionInRoot(Position(one, 0), *host).deepEquivalent());

    EXPECT_EQ(PositionInFlatTree(one, 0), lastEditablePositionBeforePositionInRoot(PositionInFlatTree(one, 0), *host));
    EXPECT_EQ(PositionInFlatTree(two->firstChild(), 2), lastEditableVisiblePositionBeforePositionInRoot(PositionInFlatTree(one, 0), *host).deepEquivalent());

    EXPECT_EQ(Position::firstPositionInNode(host), lastEditablePositionBeforePositionInRoot(Position(three, 0), *host));
    EXPECT_EQ(Position(one->firstChild(), 0), lastEditableVisiblePositionBeforePositionInRoot(Position(three, 0), *host).deepEquivalent());
    EXPECT_EQ(PositionInFlatTree::firstPositionInNode(host), lastEditablePositionBeforePositionInRoot(PositionInFlatTree(three, 0), *host));
    EXPECT_EQ(PositionInFlatTree(two->firstChild(), 0), lastEditableVisiblePositionBeforePositionInRoot(PositionInFlatTree(three, 0), *host).deepEquivalent());
}

TEST_F(EditingUtilitiesTest, NextNodeIndex)
{
    const char* bodyContent = "<p id='host'>00<b id='one'>11</b><b id='two'>22</b>33</p>";
    const char* shadowContent = "<content select=#two></content><content select=#one></content>";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent, "host");
    Node* host = document().getElementById("host");
    Node* two = document().getElementById("two");

    EXPECT_EQ(Position(host, 3), nextPositionOf(Position(two, 2), PositionMoveType::GraphemeCluster));
    EXPECT_EQ(PositionInFlatTree(host, 1), nextPositionOf(PositionInFlatTree(two, 2), PositionMoveType::GraphemeCluster));
}

TEST_F(EditingUtilitiesTest, NextVisuallyDistinctCandidate)
{
    const char* bodyContent = "<p id='host'>00<b id='one'>11</b><b id='two'>22</b><b id='three'>33</b></p>";
    const char* shadowContent = "<content select=#two></content><content select=#one></content><content select=#three></content>";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent, "host");
    Node* one = document().getElementById("one");
    Node* two = document().getElementById("two");
    Node* three = document().getElementById("three");

    EXPECT_EQ(Position(two->firstChild(), 1), nextVisuallyDistinctCandidate(Position(one, 1)));
    EXPECT_EQ(PositionInFlatTree(three->firstChild(), 1), nextVisuallyDistinctCandidate(PositionInFlatTree(one, 1)));
}

TEST_F(EditingUtilitiesTest, AreaIdenticalElements)
{
    setBodyContent("<style>li:nth-child(even) { -webkit-user-modify: read-write; }</style><ul><li>first item</li><li>second item</li><li class=foo>third</li><li>fourth</li></ul>");
    StaticElementList* items = document().querySelectorAll("li", ASSERT_NO_EXCEPTION);
    DCHECK_EQ(items->length(), 4u);

    EXPECT_FALSE(areIdenticalElements(*items->item(0)->firstChild(), *items->item(1)->firstChild()))
        << "Can't merge non-elements.  e.g. Text nodes";

    // Compare a LI and a UL.
    EXPECT_FALSE(areIdenticalElements(*items->item(0), *items->item(0)->parentNode()))
        << "Can't merge different tag names.";

    EXPECT_FALSE(areIdenticalElements(*items->item(0), *items->item(2)))
        << "Can't merge a element with no attributes and another element with an attribute.";

    // We can't use contenteditable attribute to make editability difference
    // because the hasEquivalentAttributes check is done earier.
    EXPECT_FALSE(areIdenticalElements(*items->item(0), *items->item(1)))
        << "Can't merge non-editable nodes.";

    EXPECT_TRUE(areIdenticalElements(*items->item(1), *items->item(3)));
}

TEST_F(EditingUtilitiesTest, uncheckedPreviousNextOffset_FirstLetter)
{
    setBodyContent("<style>p::first-letter {color:red;}</style><p id='target'>abc</p>");
    Node* node = document().getElementById("target")->firstChild();
    EXPECT_EQ(2, previousGraphemeBoundaryOf(node, 3));
    EXPECT_EQ(1, previousGraphemeBoundaryOf(node, 2));
    EXPECT_EQ(0, previousGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(1, nextGraphemeBoundaryOf(node, 0));
    EXPECT_EQ(2, nextGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(3, nextGraphemeBoundaryOf(node, 2));

    updateAllLifecyclePhases();
    EXPECT_NE(nullptr, node->layoutObject());
    EXPECT_EQ(2, previousGraphemeBoundaryOf(node, 3));
    EXPECT_EQ(1, previousGraphemeBoundaryOf(node, 2));
    EXPECT_EQ(0, previousGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(1, nextGraphemeBoundaryOf(node, 0));
    EXPECT_EQ(2, nextGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(3, nextGraphemeBoundaryOf(node, 2));
}

TEST_F(EditingUtilitiesTest, uncheckedPreviousNextOffset_textTransform)
{
    setBodyContent("<style>p {text-transform:uppercase}</style><p id='target'>abc</p>");
    Node* node = document().getElementById("target")->firstChild();
    EXPECT_EQ(2, previousGraphemeBoundaryOf(node, 3));
    EXPECT_EQ(1, previousGraphemeBoundaryOf(node, 2));
    EXPECT_EQ(0, previousGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(1, nextGraphemeBoundaryOf(node, 0));
    EXPECT_EQ(2, nextGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(3, nextGraphemeBoundaryOf(node, 2));

    updateAllLifecyclePhases();
    EXPECT_NE(nullptr, node->layoutObject());
    EXPECT_EQ(2, previousGraphemeBoundaryOf(node, 3));
    EXPECT_EQ(1, previousGraphemeBoundaryOf(node, 2));
    EXPECT_EQ(0, previousGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(1, nextGraphemeBoundaryOf(node, 0));
    EXPECT_EQ(2, nextGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(3, nextGraphemeBoundaryOf(node, 2));
}

// Following breaking rules come from http://unicode.org/reports/tr29/
// Note that some of rules are in draft. Also see
// http://www.unicode.org/reports/tr29/proposed.html
TEST_F(EditingUtilitiesTest, uncheckedPreviousNextOffset)
{
    // GB1: Break at the start of text.
    setBodyContent("<p id='target'>a</p>");
    Node* node = document().getElementById("target")->firstChild();
    EXPECT_EQ(0, previousGraphemeBoundaryOf(node, 1));

    // GB2: Break at the end of text.
    setBodyContent("<p id='target'>a</p>");
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(1, nextGraphemeBoundaryOf(node, 0));

    // GB3: Do not break between CR and LF.
    setBodyContent("<p id='target'>a&#x0D;&#x0A;b</p>");
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(3, previousGraphemeBoundaryOf(node, 4));
    EXPECT_EQ(1, previousGraphemeBoundaryOf(node, 3));
    EXPECT_EQ(0, previousGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(1, nextGraphemeBoundaryOf(node, 0));
    EXPECT_EQ(3, nextGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(4, nextGraphemeBoundaryOf(node, 3));

    // GB4,GB5: Break before and after CR/LF/Control.
    setBodyContent("<p id='target'>a&#x0D;b</p>"); // CR
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(2, previousGraphemeBoundaryOf(node, 3));
    EXPECT_EQ(1, previousGraphemeBoundaryOf(node, 2));
    EXPECT_EQ(0, previousGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(1, nextGraphemeBoundaryOf(node, 0));
    EXPECT_EQ(2, nextGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(3, nextGraphemeBoundaryOf(node, 2));
    setBodyContent("<p id='target'>a&#x0A;b</p>"); // LF
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(2, previousGraphemeBoundaryOf(node, 3));
    EXPECT_EQ(1, previousGraphemeBoundaryOf(node, 2));
    EXPECT_EQ(0, previousGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(1, nextGraphemeBoundaryOf(node, 0));
    EXPECT_EQ(2, nextGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(3, nextGraphemeBoundaryOf(node, 2));
    setBodyContent("<p id='target'>a&#xAD;b</p>"); // U+00AD(SOFT HYPHEN) has Control property.
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(2, previousGraphemeBoundaryOf(node, 3));
    EXPECT_EQ(1, previousGraphemeBoundaryOf(node, 2));
    EXPECT_EQ(0, previousGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(1, nextGraphemeBoundaryOf(node, 0));
    EXPECT_EQ(2, nextGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(3, nextGraphemeBoundaryOf(node, 2));

    // GB6: Don't break Hangul sequence.
    const std::string L = "&#x1100;"; // U+1100 (HANGUL CHOSEONG KIYEOK) has L property.
    const std::string V = "&#x1160;"; // U+1160 (HANGUL JUNGSEONG FILLER) has V property.
    const std::string LV = "&#xAC00;"; // U+AC00 (HANGUL SYLLABLE GA) has LV property.
    const std::string LVT = "&#xAC01;"; // U+AC01 (HANGUL SYLLABLE GAG) has LVT property.
    const std::string T = "&#x11A8;"; // U+11A8 (HANGUL JONGSEONG KIYEOK) has T property.
    setBodyContent("<p id='target'>a" + L + L + "b</p>"); // L x L
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(3, previousGraphemeBoundaryOf(node, 4));
    EXPECT_EQ(1, previousGraphemeBoundaryOf(node, 3));
    EXPECT_EQ(0, previousGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(1, nextGraphemeBoundaryOf(node, 0));
    EXPECT_EQ(3, nextGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(4, nextGraphemeBoundaryOf(node, 3));
    setBodyContent("<p id='target'>a" + L + V +"b</p>"); // L x V
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(3, previousGraphemeBoundaryOf(node, 4));
    EXPECT_EQ(1, previousGraphemeBoundaryOf(node, 3));
    EXPECT_EQ(0, previousGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(1, nextGraphemeBoundaryOf(node, 0));
    EXPECT_EQ(3, nextGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(4, nextGraphemeBoundaryOf(node, 3));
    setBodyContent("<p id='target'>a" + L + LV + "b</p>"); // L x LV
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(3, previousGraphemeBoundaryOf(node, 4));
    EXPECT_EQ(1, previousGraphemeBoundaryOf(node, 3));
    EXPECT_EQ(0, previousGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(1, nextGraphemeBoundaryOf(node, 0));
    EXPECT_EQ(3, nextGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(4, nextGraphemeBoundaryOf(node, 3));
    setBodyContent("<p id='target'>a" + L + LVT + "b</p>"); // L x LVT
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(3, previousGraphemeBoundaryOf(node, 4));
    EXPECT_EQ(1, previousGraphemeBoundaryOf(node, 3));
    EXPECT_EQ(0, previousGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(1, nextGraphemeBoundaryOf(node, 0));
    EXPECT_EQ(3, nextGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(4, nextGraphemeBoundaryOf(node, 3));

    // GB7: Don't break Hangul sequence.
    setBodyContent("<p id='target'>a" + LV + V + "b</p>"); // LV x V
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(3, previousGraphemeBoundaryOf(node, 4));
    EXPECT_EQ(1, previousGraphemeBoundaryOf(node, 3));
    EXPECT_EQ(0, previousGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(1, nextGraphemeBoundaryOf(node, 0));
    EXPECT_EQ(3, nextGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(4, nextGraphemeBoundaryOf(node, 3));
    setBodyContent("<p id='target'>a" + LV + T + "b</p>"); // LV x T
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(3, previousGraphemeBoundaryOf(node, 4));
    EXPECT_EQ(1, previousGraphemeBoundaryOf(node, 3));
    EXPECT_EQ(0, previousGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(1, nextGraphemeBoundaryOf(node, 0));
    EXPECT_EQ(3, nextGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(4, nextGraphemeBoundaryOf(node, 3));
    setBodyContent("<p id='target'>a" + V + V + "b</p>"); // V x V
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(3, previousGraphemeBoundaryOf(node, 4));
    EXPECT_EQ(1, previousGraphemeBoundaryOf(node, 3));
    EXPECT_EQ(0, previousGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(1, nextGraphemeBoundaryOf(node, 0));
    EXPECT_EQ(3, nextGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(4, nextGraphemeBoundaryOf(node, 3));
    setBodyContent("<p id='target'>a" + V + T + "b</p>"); // V x T
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(3, previousGraphemeBoundaryOf(node, 4));
    EXPECT_EQ(1, previousGraphemeBoundaryOf(node, 3));
    EXPECT_EQ(0, previousGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(1, nextGraphemeBoundaryOf(node, 0));
    EXPECT_EQ(3, nextGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(4, nextGraphemeBoundaryOf(node, 3));

    // GB8: Don't break Hangul sequence.
    setBodyContent("<p id='target'>a" + LVT + T + "b</p>"); // LVT x T
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(3, previousGraphemeBoundaryOf(node, 4));
    EXPECT_EQ(1, previousGraphemeBoundaryOf(node, 3));
    EXPECT_EQ(0, previousGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(1, nextGraphemeBoundaryOf(node, 0));
    EXPECT_EQ(3, nextGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(4, nextGraphemeBoundaryOf(node, 3));
    setBodyContent("<p id='target'>a" + T + T + "b</p>"); // T x T
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(3, previousGraphemeBoundaryOf(node, 4));
    EXPECT_EQ(1, previousGraphemeBoundaryOf(node, 3));
    EXPECT_EQ(0, previousGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(1, nextGraphemeBoundaryOf(node, 0));
    EXPECT_EQ(3, nextGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(4, nextGraphemeBoundaryOf(node, 3));

    // Break other Hangul syllable combination. See test of GB999.

    // GB8a: Don't break between regional indicator if there are even numbered regional indicator symbols before.
    // U+1F1FA is REGIONAL INDICATOR SYMBOL LETTER U.
    // U+1F1F8 is REGIONAL INDICATOR SYMBOL LETTER S.
    const std::string flag = "&#x1F1FA;&#x1F1F8;"; // US flag.
    setBodyContent("<p id='target'>" + flag + flag + flag + flag + "a</p>"); // ^(RI RI)* RI x RI
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(16, previousGraphemeBoundaryOf(node, 17));
    EXPECT_EQ(12, previousGraphemeBoundaryOf(node, 16));
    EXPECT_EQ(8, previousGraphemeBoundaryOf(node, 12));
    EXPECT_EQ(4, previousGraphemeBoundaryOf(node, 8));
    EXPECT_EQ(0, previousGraphemeBoundaryOf(node, 4));
    EXPECT_EQ(4, nextGraphemeBoundaryOf(node, 0));
    EXPECT_EQ(8, nextGraphemeBoundaryOf(node, 4));
    EXPECT_EQ(12, nextGraphemeBoundaryOf(node, 8));
    EXPECT_EQ(16, nextGraphemeBoundaryOf(node, 12));
    EXPECT_EQ(17, nextGraphemeBoundaryOf(node, 16));

    // GB8c: Don't break between regional indicator if there are even numbered regional indicator symbols before.
    setBodyContent("<p id='target'>a" + flag + flag + flag + flag + "b</p>"); // [^RI] (RI RI)* RI x RI
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(17, previousGraphemeBoundaryOf(node, 18));
    EXPECT_EQ(13, previousGraphemeBoundaryOf(node, 17));
    EXPECT_EQ(9, previousGraphemeBoundaryOf(node, 13));
    EXPECT_EQ(5, previousGraphemeBoundaryOf(node, 9));
    EXPECT_EQ(1, previousGraphemeBoundaryOf(node, 5));
    EXPECT_EQ(0, previousGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(1, nextGraphemeBoundaryOf(node, 0));
    EXPECT_EQ(5, nextGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(9, nextGraphemeBoundaryOf(node, 5));
    EXPECT_EQ(13, nextGraphemeBoundaryOf(node, 9));
    EXPECT_EQ(17, nextGraphemeBoundaryOf(node, 13));
    EXPECT_EQ(18, nextGraphemeBoundaryOf(node, 17));

    // GB8c: Break if there is an odd number of regional indicator symbols before.
    setBodyContent("<p id='target'>a" + flag + flag + flag + flag + "&#x1F1F8;b</p>"); // RI ÷ RI
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(19, previousGraphemeBoundaryOf(node, 20));
    EXPECT_EQ(17, previousGraphemeBoundaryOf(node, 19));
    EXPECT_EQ(13, previousGraphemeBoundaryOf(node, 17));
    EXPECT_EQ(9, previousGraphemeBoundaryOf(node, 13));
    EXPECT_EQ(5, previousGraphemeBoundaryOf(node, 9));
    EXPECT_EQ(1, previousGraphemeBoundaryOf(node, 5));
    EXPECT_EQ(0, previousGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(1, nextGraphemeBoundaryOf(node, 0));
    EXPECT_EQ(5, nextGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(9, nextGraphemeBoundaryOf(node, 5));
    EXPECT_EQ(13, nextGraphemeBoundaryOf(node, 9));
    EXPECT_EQ(17, nextGraphemeBoundaryOf(node, 13));
    EXPECT_EQ(19, nextGraphemeBoundaryOf(node, 17));
    EXPECT_EQ(20, nextGraphemeBoundaryOf(node, 19));

    // GB9: Do not break before extending characters or ZWJ.
    // U+0300(COMBINING GRAVE ACCENT) has Extend property.
    setBodyContent("<p id='target'>a&#x0300;b</p>"); // x Extend
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(2, previousGraphemeBoundaryOf(node, 3));
    EXPECT_EQ(0, previousGraphemeBoundaryOf(node, 2));
    EXPECT_EQ(2, nextGraphemeBoundaryOf(node, 0));
    EXPECT_EQ(3, nextGraphemeBoundaryOf(node, 2));
    // U+200D is ZERO WIDTH JOINER.
    setBodyContent("<p id='target'>a&#x200D;b</p>"); // x ZWJ
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(2, previousGraphemeBoundaryOf(node, 3));
    EXPECT_EQ(0, previousGraphemeBoundaryOf(node, 2));
    EXPECT_EQ(2, nextGraphemeBoundaryOf(node, 0));
    EXPECT_EQ(3, nextGraphemeBoundaryOf(node, 2));

    // GB9a: Do not break before SpacingMarks.
    // U+0903(DEVANAGARI SIGN VISARGA) has SpacingMark property.
    setBodyContent("<p id='target'>a&#x0903;b</p>"); // x SpacingMark
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(2, previousGraphemeBoundaryOf(node, 3));
    EXPECT_EQ(0, previousGraphemeBoundaryOf(node, 2));
    EXPECT_EQ(2, nextGraphemeBoundaryOf(node, 0));
    EXPECT_EQ(3, nextGraphemeBoundaryOf(node, 2));

    // GB9b: Do not break after Prepend.
    // TODO(nona): Introduce Prepend test case once ICU grabs Unicode 9.0.

    // For https://bugs.webkit.org/show_bug.cgi?id=24342
    // The break should happens after Thai character.
    setBodyContent("<p id='target'>a&#x0E40;b</p>");
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(2, previousGraphemeBoundaryOf(node, 3));
    EXPECT_EQ(1, previousGraphemeBoundaryOf(node, 2));
    EXPECT_EQ(0, previousGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(1, nextGraphemeBoundaryOf(node, 0));
    EXPECT_EQ(2, nextGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(3, nextGraphemeBoundaryOf(node, 2));

    // Blink customization: Don't break before Japanese half-width katakana voiced marks.
    setBodyContent("<p id='target'>a&#xFF76;&#xFF9E;b</p>");
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(3, previousGraphemeBoundaryOf(node, 4));
    EXPECT_EQ(1, previousGraphemeBoundaryOf(node, 3));
    EXPECT_EQ(0, previousGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(1, nextGraphemeBoundaryOf(node, 0));
    EXPECT_EQ(3, nextGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(4, nextGraphemeBoundaryOf(node, 3));

    // Additional rule for IndicSyllabicCategory=Virama: Do not break after that.
    // See http://www.unicode.org/Public/9.0.0/ucd/IndicSyllabicCategory-9.0.0d2.txt
    // U+0905 is DEVANAGARI LETTER A. This has Extend property.
    // U+094D is DEVANAGARI SIGN VIRAMA. This has Virama property.
    // U+0915 is DEVANAGARI LETTER KA.
    setBodyContent("<p id='target'>a&#x0905;&#x094D;&#x0915;b</p>");
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(4, previousGraphemeBoundaryOf(node, 5));
    EXPECT_EQ(1, previousGraphemeBoundaryOf(node, 4));
    EXPECT_EQ(0, previousGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(1, nextGraphemeBoundaryOf(node, 0));
    EXPECT_EQ(4, nextGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(5, nextGraphemeBoundaryOf(node, 4));
    // U+0E01 is THAI CHARACTER KO KAI
    // U+0E3A is THAI CHARACTER PHINTHU
    // Should break after U+0E3A since U+0E3A has Virama property but not listed in
    // IndicSyllabicCategory=Virama.
    setBodyContent("<p id='target'>a&#x0E01;&#x0E3A;&#x0E01;b</p>");
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(4, previousGraphemeBoundaryOf(node, 5));
    EXPECT_EQ(3, previousGraphemeBoundaryOf(node, 4));
    EXPECT_EQ(1, previousGraphemeBoundaryOf(node, 3));
    EXPECT_EQ(0, previousGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(1, nextGraphemeBoundaryOf(node, 0));
    EXPECT_EQ(3, nextGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(4, nextGraphemeBoundaryOf(node, 3));
    EXPECT_EQ(5, nextGraphemeBoundaryOf(node, 4));

    // GB10: Do not break within emoji modifier.
    // U+1F385(FATHER CHRISTMAS) has E_Base property.
    // U+1F3FB(EMOJI MODIFIER FITZPATRICK TYPE-1-2) has E_Modifier property.
    setBodyContent("<p id='target'>a&#x1F385;&#x1F3FB;b</p>"); // E_Base x E_Modifier
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(5, previousGraphemeBoundaryOf(node, 6));
    EXPECT_EQ(1, previousGraphemeBoundaryOf(node, 5));
    EXPECT_EQ(0, previousGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(1, nextGraphemeBoundaryOf(node, 0));
    EXPECT_EQ(5, nextGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(6, nextGraphemeBoundaryOf(node, 5));
    // U+1F466(BOY) has EBG property.
    setBodyContent("<p id='target'>a&#x1F466;&#x1F3FB;b</p>"); // EBG x E_Modifier
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(5, previousGraphemeBoundaryOf(node, 6));
    EXPECT_EQ(1, previousGraphemeBoundaryOf(node, 5));
    EXPECT_EQ(0, previousGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(1, nextGraphemeBoundaryOf(node, 0));
    EXPECT_EQ(5, nextGraphemeBoundaryOf(node, 1));
    EXPECT_EQ(6, nextGraphemeBoundaryOf(node, 5));

    // GB11: Do not break within ZWJ emoji sequence.
    // U+2764(HEAVY BLACK HEART) has Glue_After_Zwj property.
    setBodyContent("<p id='target'>a&#x200D;&#x2764;b</p>"); // ZWJ x Glue_After_Zwj
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(3, previousGraphemeBoundaryOf(node, 4));
    EXPECT_EQ(0, previousGraphemeBoundaryOf(node, 3));
    EXPECT_EQ(3, nextGraphemeBoundaryOf(node, 0));
    EXPECT_EQ(4, nextGraphemeBoundaryOf(node, 3));
    setBodyContent("<p id='target'>a&#x200D;&#x1F466;b</p>"); // ZWJ x EBG
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(4, previousGraphemeBoundaryOf(node, 5));
    EXPECT_EQ(0, previousGraphemeBoundaryOf(node, 4));
    EXPECT_EQ(4, nextGraphemeBoundaryOf(node, 0));
    EXPECT_EQ(5, nextGraphemeBoundaryOf(node, 4));

    // Not only Glue_After_ZWJ or EBG but also other emoji shouldn't break
    // before ZWJ.
    // U+1F5FA(WORLD MAP) doesn't have either Glue_After_Zwj or EBG but has
    // Emoji property.
    setBodyContent("<p id='target'>&#x200D;&#x1F5FA;</p>");
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(0, previousGraphemeBoundaryOf(node, 3));
    EXPECT_EQ(3, nextGraphemeBoundaryOf(node, 0));

    // GB999: Otherwise break everywhere.
    // Breaks between Hangul syllable except for GB6, GB7, GB8.
    setBodyContent("<p id='target'>" + L + T + "</p>");
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(1, previousGraphemeBoundaryOf(node, 2));
    EXPECT_EQ(1, nextGraphemeBoundaryOf(node, 0));
    setBodyContent("<p id='target'>" + V + L + "</p>");
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(1, previousGraphemeBoundaryOf(node, 2));
    EXPECT_EQ(1, nextGraphemeBoundaryOf(node, 0));
    setBodyContent("<p id='target'>" + V + LV + "</p>");
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(1, previousGraphemeBoundaryOf(node, 2));
    EXPECT_EQ(1, nextGraphemeBoundaryOf(node, 0));
    setBodyContent("<p id='target'>" + V + LVT + "</p>");
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(1, previousGraphemeBoundaryOf(node, 2));
    EXPECT_EQ(1, nextGraphemeBoundaryOf(node, 0));
    setBodyContent("<p id='target'>" + LV + L + "</p>");
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(1, previousGraphemeBoundaryOf(node, 2));
    EXPECT_EQ(1, nextGraphemeBoundaryOf(node, 0));
    setBodyContent("<p id='target'>" + LV + LV + "</p>");
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(1, previousGraphemeBoundaryOf(node, 2));
    EXPECT_EQ(1, nextGraphemeBoundaryOf(node, 0));
    setBodyContent("<p id='target'>" + LV + LVT + "</p>");
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(1, previousGraphemeBoundaryOf(node, 2));
    EXPECT_EQ(1, nextGraphemeBoundaryOf(node, 0));
    setBodyContent("<p id='target'>" + LVT + L + "</p>");
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(1, previousGraphemeBoundaryOf(node, 2));
    EXPECT_EQ(1, nextGraphemeBoundaryOf(node, 0));
    setBodyContent("<p id='target'>" + LVT + V + "</p>");
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(1, previousGraphemeBoundaryOf(node, 2));
    EXPECT_EQ(1, nextGraphemeBoundaryOf(node, 0));
    setBodyContent("<p id='target'>" + LVT + LV + "</p>");
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(1, previousGraphemeBoundaryOf(node, 2));
    EXPECT_EQ(1, nextGraphemeBoundaryOf(node, 0));
    setBodyContent("<p id='target'>" + LVT + LVT + "</p>");
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(1, previousGraphemeBoundaryOf(node, 2));
    EXPECT_EQ(1, nextGraphemeBoundaryOf(node, 0));
    setBodyContent("<p id='target'>" + T + L + "</p>");
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(1, previousGraphemeBoundaryOf(node, 2));
    EXPECT_EQ(1, nextGraphemeBoundaryOf(node, 0));
    setBodyContent("<p id='target'>" + T + V + "</p>");
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(1, previousGraphemeBoundaryOf(node, 2));
    EXPECT_EQ(1, nextGraphemeBoundaryOf(node, 0));
    setBodyContent("<p id='target'>" + T + LV + "</p>");
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(1, previousGraphemeBoundaryOf(node, 2));
    EXPECT_EQ(1, nextGraphemeBoundaryOf(node, 0));
    setBodyContent("<p id='target'>" + T + LVT + "</p>");
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(1, previousGraphemeBoundaryOf(node, 2));
    EXPECT_EQ(1, nextGraphemeBoundaryOf(node, 0));

    // For GB10, if base emoji character is not E_Base or EBG, break happens before E_Modifier.
    setBodyContent("<p id='target'>a&#x1F3FB;</p>");
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(1, previousGraphemeBoundaryOf(node, 3));
    EXPECT_EQ(1, nextGraphemeBoundaryOf(node, 0));
    // U+1F5FA(WORLD MAP) doesn't have either E_Base or EBG property.
    setBodyContent("<p id='target'>&#x1F5FA;&#x1F3FB;</p>");
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(2, previousGraphemeBoundaryOf(node, 4));
    EXPECT_EQ(2, nextGraphemeBoundaryOf(node, 0));

    // For GB11, if trailing character is not Glue_After_Zwj or EBG, break happens after ZWJ.
    // U+1F5FA(WORLD MAP) doesn't have either Glue_After_Zwj or EBG.
    setBodyContent("<p id='target'>&#x200D;a</p>");
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(1, previousGraphemeBoundaryOf(node, 2));
    EXPECT_EQ(1, nextGraphemeBoundaryOf(node, 0));
}

TEST_F(EditingUtilitiesTest, previousPositionOf_Backspace)
{
    // BMP characters. Only one code point should be deleted.
    setBodyContent("<p id='target'>abc</p>");
    Node* node = document().getElementById("target")->firstChild();
    EXPECT_EQ(Position(node, 2), previousPositionOf(Position(node, 3), PositionMoveType::BackwardDeletion));
    EXPECT_EQ(Position(node, 1), previousPositionOf(Position(node, 2), PositionMoveType::BackwardDeletion));
    EXPECT_EQ(Position(node, 0), previousPositionOf(Position(node, 1), PositionMoveType::BackwardDeletion));
}

TEST_F(EditingUtilitiesTest, previousPositionOf_Backspace_FirstLetter)
{

    setBodyContent("<style>p::first-letter {color:red;}</style><p id='target'>abc</p>");
    Node* node = document().getElementById("target")->firstChild();
    EXPECT_EQ(Position(node, 2), previousPositionOf(Position(node, 3), PositionMoveType::BackwardDeletion));
    EXPECT_EQ(Position(node, 1), previousPositionOf(Position(node, 2), PositionMoveType::BackwardDeletion));
    EXPECT_EQ(Position(node, 0), previousPositionOf(Position(node, 1), PositionMoveType::BackwardDeletion));

    setBodyContent("<style>p::first-letter {color:red;}</style><p id='target'>(a)bc</p>");
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(Position(node, 4), previousPositionOf(Position(node, 5), PositionMoveType::BackwardDeletion));
    EXPECT_EQ(Position(node, 3), previousPositionOf(Position(node, 4), PositionMoveType::BackwardDeletion));
    EXPECT_EQ(Position(node, 2), previousPositionOf(Position(node, 3), PositionMoveType::BackwardDeletion));
    EXPECT_EQ(Position(node, 1), previousPositionOf(Position(node, 2), PositionMoveType::BackwardDeletion));
    EXPECT_EQ(Position(node, 0), previousPositionOf(Position(node, 1), PositionMoveType::BackwardDeletion));
}

TEST_F(EditingUtilitiesTest, previousPositionOf_Backspace_TextTransform)
{
    // Uppercase of &#x00DF; will be transformed to SS.
    setBodyContent("<style>p {text-transform:uppercase}</style><p id='target'>&#x00DF;abc</p>");
    Node* node = document().getElementById("target")->firstChild();
    EXPECT_EQ(Position(node, 3), previousPositionOf(Position(node, 4), PositionMoveType::BackwardDeletion));
    EXPECT_EQ(Position(node, 2), previousPositionOf(Position(node, 3), PositionMoveType::BackwardDeletion));
    EXPECT_EQ(Position(node, 1), previousPositionOf(Position(node, 2), PositionMoveType::BackwardDeletion));
    EXPECT_EQ(Position(node, 0), previousPositionOf(Position(node, 1), PositionMoveType::BackwardDeletion));
}

TEST_F(EditingUtilitiesTest, previousPositionOf_Backspace_SurrogatePairs)
{
    // Supplementary plane characters. Only one code point should be deleted.
    // &#x1F441; is EYE.
    setBodyContent("<p id='target'>&#x1F441;&#x1F441;&#x1F441;</p>");
    Node* node = document().getElementById("target")->firstChild();
    EXPECT_EQ(Position(node, 4), previousPositionOf(Position(node, 6), PositionMoveType::BackwardDeletion));
    EXPECT_EQ(Position(node, 2), previousPositionOf(Position(node, 4), PositionMoveType::BackwardDeletion));
    EXPECT_EQ(Position(node, 0), previousPositionOf(Position(node, 2), PositionMoveType::BackwardDeletion));

    // BMP and Supplementary plane case.
    setBodyContent("<p id='target'>&#x1F441;a&#x1F441;a</p>");
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(Position(node, 5), previousPositionOf(Position(node, 6), PositionMoveType::BackwardDeletion));
    EXPECT_EQ(Position(node, 3), previousPositionOf(Position(node, 5), PositionMoveType::BackwardDeletion));
    EXPECT_EQ(Position(node, 2), previousPositionOf(Position(node, 3), PositionMoveType::BackwardDeletion));
    EXPECT_EQ(Position(node, 0), previousPositionOf(Position(node, 2), PositionMoveType::BackwardDeletion));

    // Edge case: broken surrogate pairs.
    setBodyContent("<p id='target'>&#xD83D;</p>"); // &#xD83D; is unpaired lead surrogate.
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(Position(node, 0), previousPositionOf(Position(node, 1), PositionMoveType::BackwardDeletion));

    setBodyContent("<p id='target'>&#x1F441;&#xD83D;&#x1F441;</p>"); // &#xD83D; is unpaired lead surrogate.
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(Position(node, 3), previousPositionOf(Position(node, 5), PositionMoveType::BackwardDeletion));
    EXPECT_EQ(Position(node, 2), previousPositionOf(Position(node, 3), PositionMoveType::BackwardDeletion));
    EXPECT_EQ(Position(node, 0), previousPositionOf(Position(node, 2), PositionMoveType::BackwardDeletion));

    setBodyContent("<p id='target'>a&#xD83D;a</p>"); // &#xD83D; is unpaired lead surrogate.
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(Position(node, 2), previousPositionOf(Position(node, 3), PositionMoveType::BackwardDeletion));
    EXPECT_EQ(Position(node, 1), previousPositionOf(Position(node, 2), PositionMoveType::BackwardDeletion));
    EXPECT_EQ(Position(node, 0), previousPositionOf(Position(node, 1), PositionMoveType::BackwardDeletion));

    setBodyContent("<p id='target'>&#xDC41;</p>"); // &#xDC41; is unpaired trail surrogate.
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(Position(node, 0), previousPositionOf(Position(node, 1), PositionMoveType::BackwardDeletion));

    setBodyContent("<p id='target'>&#x1F441;&#xDC41;&#x1F441;</p>"); // &#xDC41; is unpaired trail surrogate.
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(Position(node, 3), previousPositionOf(Position(node, 5), PositionMoveType::BackwardDeletion));
    EXPECT_EQ(Position(node, 2), previousPositionOf(Position(node, 3), PositionMoveType::BackwardDeletion));
    EXPECT_EQ(Position(node, 0), previousPositionOf(Position(node, 2), PositionMoveType::BackwardDeletion));

    setBodyContent("<p id='target'>a&#xDC41;a</p>"); // &#xDC41; is unpaired trail surrogate.
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(Position(node, 2), previousPositionOf(Position(node, 3), PositionMoveType::BackwardDeletion));
    EXPECT_EQ(Position(node, 1), previousPositionOf(Position(node, 2), PositionMoveType::BackwardDeletion));
    EXPECT_EQ(Position(node, 0), previousPositionOf(Position(node, 1), PositionMoveType::BackwardDeletion));

    // Edge case: specify middle of surrogate pairs.
    setBodyContent("<p id='target'>&#x1F441;&#x1F441;&#x1F441</p>");
    node = document().getElementById("target")->firstChild();
    EXPECT_EQ(Position(node, 4), previousPositionOf(Position(node, 5), PositionMoveType::BackwardDeletion));
    EXPECT_EQ(Position(node, 2), previousPositionOf(Position(node, 3), PositionMoveType::BackwardDeletion));
    EXPECT_EQ(Position(node, 0), previousPositionOf(Position(node, 1), PositionMoveType::BackwardDeletion));
}

} // namespace blink
