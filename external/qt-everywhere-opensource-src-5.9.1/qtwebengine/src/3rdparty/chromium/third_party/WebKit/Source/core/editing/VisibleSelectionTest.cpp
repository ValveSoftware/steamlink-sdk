// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/VisibleSelection.h"

#include "core/dom/Range.h"
#include "core/editing/EditingTestBase.h"
#include "core/editing/SelectionAdjuster.h"

#define LOREM_IPSUM                                                            \
  "Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod "  \
  "tempor "                                                                    \
  "incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, "     \
  "quis nostrud "                                                              \
  "exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. "     \
  "Duis aute irure "                                                           \
  "dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat "    \
  "nulla pariatur."                                                            \
  "Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia " \
  "deserunt "                                                                  \
  "mollit anim id est laborum."

namespace blink {

class VisibleSelectionTest : public EditingTestBase {
 protected:
  // Helper function to set the VisibleSelection base/extent.
  template <typename Strategy>
  void setSelection(VisibleSelectionTemplate<Strategy>& selection, int base) {
    setSelection(selection, base, base);
  }

  // Helper function to set the VisibleSelection base/extent.
  template <typename Strategy>
  void setSelection(VisibleSelectionTemplate<Strategy>& selection,
                    int base,
                    int extend) {
    Node* node = document().body()->firstChild();
    selection = createVisibleSelection(
        typename SelectionTemplate<Strategy>::Builder(selection.asSelection())
            .collapse(PositionTemplate<Strategy>(node, base))
            .extend(PositionTemplate<Strategy>(node, extend))
            .build());
  }
};

static void testFlatTreePositionsToEqualToDOMTreePositions(
    const VisibleSelection& selection,
    const VisibleSelectionInFlatTree& selectionInFlatTree) {
  // Since DOM tree positions can't be map to flat tree version, e.g.
  // shadow root, not distributed node, we map a position in flat tree
  // to DOM tree position.
  EXPECT_EQ(selection.start(),
            toPositionInDOMTree(selectionInFlatTree.start()));
  EXPECT_EQ(selection.end(), toPositionInDOMTree(selectionInFlatTree.end()));
  EXPECT_EQ(selection.base(), toPositionInDOMTree(selectionInFlatTree.base()));
  EXPECT_EQ(selection.extent(),
            toPositionInDOMTree(selectionInFlatTree.extent()));
}

template <typename Strategy>
VisibleSelectionTemplate<Strategy> expandUsingGranularity(
    const VisibleSelectionTemplate<Strategy>& selection,
    TextGranularity granularity) {
  return createVisibleSelection(
      typename SelectionTemplate<Strategy>::Builder()
          .setBaseAndExtent(selection.base(), selection.extent())
          .setGranularity(granularity)
          .build());
}

TEST_F(VisibleSelectionTest, expandUsingGranularity) {
  const char* bodyContent =
      "<span id=host><a id=one>1</a><a id=two>22</a></span>";
  const char* shadowContent =
      "<p><b id=three>333</b><content select=#two></content><b "
      "id=four>4444</b><span id=space>  </span><content "
      "select=#one></content><b id=five>55555</b></p>";
  setBodyContent(bodyContent);
  ShadowRoot* shadowRoot = setShadowContent(shadowContent, "host");

  Node* one = document().getElementById("one")->firstChild();
  Node* two = document().getElementById("two")->firstChild();
  Node* three = shadowRoot->getElementById("three")->firstChild();
  Node* four = shadowRoot->getElementById("four")->firstChild();
  Node* five = shadowRoot->getElementById("five")->firstChild();

  VisibleSelection selection;
  VisibleSelectionInFlatTree selectionInFlatTree;

  // From a position at distributed node
  selection = createVisibleSelection(
      SelectionInDOMTree::Builder().collapse(Position(one, 1)).build());
  selection = expandUsingGranularity(selection, WordGranularity);
  selectionInFlatTree =
      createVisibleSelection(SelectionInFlatTree::Builder()
                                 .collapse(PositionInFlatTree(one, 1))
                                 .build());
  selectionInFlatTree =
      expandUsingGranularity(selectionInFlatTree, WordGranularity);

  EXPECT_EQ(Position(one, 1), selection.base());
  EXPECT_EQ(Position(one, 1), selection.extent());
  EXPECT_EQ(Position(one, 0), selection.start());
  EXPECT_EQ(Position(two, 2), selection.end());

  EXPECT_EQ(PositionInFlatTree(one, 1), selectionInFlatTree.base());
  EXPECT_EQ(PositionInFlatTree(one, 1), selectionInFlatTree.extent());
  EXPECT_EQ(PositionInFlatTree(one, 0), selectionInFlatTree.start());
  EXPECT_EQ(PositionInFlatTree(five, 5), selectionInFlatTree.end());

  // From a position at distributed node
  selection = createVisibleSelection(
      SelectionInDOMTree::Builder().collapse(Position(two, 1)).build());
  selection = expandUsingGranularity(selection, WordGranularity);
  selectionInFlatTree =
      createVisibleSelection(SelectionInFlatTree::Builder()
                                 .collapse(PositionInFlatTree(two, 1))
                                 .build());
  selectionInFlatTree =
      expandUsingGranularity(selectionInFlatTree, WordGranularity);

  EXPECT_EQ(Position(two, 1), selection.base());
  EXPECT_EQ(Position(two, 1), selection.extent());
  EXPECT_EQ(Position(one, 0), selection.start());
  EXPECT_EQ(Position(two, 2), selection.end());

  EXPECT_EQ(PositionInFlatTree(two, 1), selectionInFlatTree.base());
  EXPECT_EQ(PositionInFlatTree(two, 1), selectionInFlatTree.extent());
  EXPECT_EQ(PositionInFlatTree(three, 0), selectionInFlatTree.start());
  EXPECT_EQ(PositionInFlatTree(four, 4), selectionInFlatTree.end());

  // From a position at node in shadow tree
  selection = createVisibleSelection(
      SelectionInDOMTree::Builder().collapse(Position(three, 1)).build());
  selection = expandUsingGranularity(selection, WordGranularity);
  selectionInFlatTree =
      createVisibleSelection(SelectionInFlatTree::Builder()
                                 .collapse(PositionInFlatTree(three, 1))
                                 .build());
  selectionInFlatTree =
      expandUsingGranularity(selectionInFlatTree, WordGranularity);

  EXPECT_EQ(Position(three, 1), selection.base());
  EXPECT_EQ(Position(three, 1), selection.extent());
  EXPECT_EQ(Position(three, 0), selection.start());
  EXPECT_EQ(Position(four, 4), selection.end());

  EXPECT_EQ(PositionInFlatTree(three, 1), selectionInFlatTree.base());
  EXPECT_EQ(PositionInFlatTree(three, 1), selectionInFlatTree.extent());
  EXPECT_EQ(PositionInFlatTree(three, 0), selectionInFlatTree.start());
  EXPECT_EQ(PositionInFlatTree(four, 4), selectionInFlatTree.end());

  // From a position at node in shadow tree
  selection = createVisibleSelection(
      SelectionInDOMTree::Builder().collapse(Position(four, 1)).build());
  selection = expandUsingGranularity(selection, WordGranularity);
  selectionInFlatTree =
      createVisibleSelection(SelectionInFlatTree::Builder()
                                 .collapse(PositionInFlatTree(four, 1))
                                 .build());
  selectionInFlatTree =
      expandUsingGranularity(selectionInFlatTree, WordGranularity);

  EXPECT_EQ(Position(four, 1), selection.base());
  EXPECT_EQ(Position(four, 1), selection.extent());
  EXPECT_EQ(Position(three, 0), selection.start());
  EXPECT_EQ(Position(four, 4), selection.end());

  EXPECT_EQ(PositionInFlatTree(four, 1), selectionInFlatTree.base());
  EXPECT_EQ(PositionInFlatTree(four, 1), selectionInFlatTree.extent());
  EXPECT_EQ(PositionInFlatTree(three, 0), selectionInFlatTree.start());
  EXPECT_EQ(PositionInFlatTree(four, 4), selectionInFlatTree.end());

  // From a position at node in shadow tree
  selection = createVisibleSelection(
      SelectionInDOMTree::Builder().collapse(Position(five, 1)).build());
  selection = expandUsingGranularity(selection, WordGranularity);
  selectionInFlatTree =
      createVisibleSelection(SelectionInFlatTree::Builder()
                                 .collapse(PositionInFlatTree(five, 1))
                                 .build());
  selectionInFlatTree =
      expandUsingGranularity(selectionInFlatTree, WordGranularity);

  EXPECT_EQ(Position(five, 1), selection.base());
  EXPECT_EQ(Position(five, 1), selection.extent());
  EXPECT_EQ(Position(five, 0), selection.start());
  EXPECT_EQ(Position(five, 5), selection.end());

  EXPECT_EQ(PositionInFlatTree(five, 1), selectionInFlatTree.base());
  EXPECT_EQ(PositionInFlatTree(five, 1), selectionInFlatTree.extent());
  EXPECT_EQ(PositionInFlatTree(one, 0), selectionInFlatTree.start());
  EXPECT_EQ(PositionInFlatTree(five, 5), selectionInFlatTree.end());
}

TEST_F(VisibleSelectionTest, Initialisation) {
  setBodyContent(LOREM_IPSUM);

  VisibleSelection selection;
  VisibleSelectionInFlatTree selectionInFlatTree;
  setSelection(selection, 0);
  setSelection(selectionInFlatTree, 0);

  EXPECT_FALSE(selection.isNone());
  EXPECT_FALSE(selectionInFlatTree.isNone());
  EXPECT_TRUE(selection.isCaret());
  EXPECT_TRUE(selectionInFlatTree.isCaret());

  Range* range = firstRangeOf(selection);
  EXPECT_EQ(0, range->startOffset());
  EXPECT_EQ(0, range->endOffset());
  EXPECT_EQ("", range->text());
  testFlatTreePositionsToEqualToDOMTreePositions(selection,
                                                 selectionInFlatTree);

  const VisibleSelection noSelection =
      createVisibleSelection(SelectionInDOMTree::Builder().build());
  EXPECT_EQ(NoSelection, noSelection.getSelectionType());
}

TEST_F(VisibleSelectionTest, ShadowCrossing) {
  const char* bodyContent =
      "<p id='host'>00<b id='one'>11</b><b id='two'>22</b>33</p>";
  const char* shadowContent =
      "<a><span id='s4'>44</span><content select=#two></content><span "
      "id='s5'>55</span><content select=#one></content><span "
      "id='s6'>66</span></a>";
  setBodyContent(bodyContent);
  ShadowRoot* shadowRoot = setShadowContent(shadowContent, "host");

  Element* body = document().body();
  Element* host = body->querySelector("#host");
  Element* one = body->querySelector("#one");
  Element* six = shadowRoot->querySelector("#s6");

  VisibleSelection selection = createVisibleSelection(
      SelectionInDOMTree::Builder()
          .collapse(Position::firstPositionInNode(one))
          .extend(Position::lastPositionInNode(shadowRoot))
          .build());
  VisibleSelectionInFlatTree selectionInFlatTree = createVisibleSelection(
      SelectionInFlatTree::Builder()
          .collapse(PositionInFlatTree::firstPositionInNode(one))
          .extend(PositionInFlatTree::lastPositionInNode(host))
          .build());

  EXPECT_EQ(Position(host, PositionAnchorType::BeforeAnchor),
            selection.start());
  EXPECT_EQ(Position(one->firstChild(), 0), selection.end());
  EXPECT_EQ(PositionInFlatTree(one->firstChild(), 0),
            selectionInFlatTree.start());
  EXPECT_EQ(PositionInFlatTree(six->firstChild(), 2),
            selectionInFlatTree.end());
}

TEST_F(VisibleSelectionTest, ShadowV0DistributedNodes) {
  const char* bodyContent =
      "<p id='host'>00<b id='one'>11</b><b id='two'>22</b>33</p>";
  const char* shadowContent =
      "<a><span id='s4'>44</span><content select=#two></content><span "
      "id='s5'>55</span><content select=#one></content><span "
      "id='s6'>66</span></a>";
  setBodyContent(bodyContent);
  ShadowRoot* shadowRoot = setShadowContent(shadowContent, "host");

  Element* body = document().body();
  Element* one = body->querySelector("#one");
  Element* two = body->querySelector("#two");
  Element* five = shadowRoot->querySelector("#s5");

  VisibleSelection selection =
      createVisibleSelection(SelectionInDOMTree::Builder()
                                 .collapse(Position::firstPositionInNode(one))
                                 .extend(Position::lastPositionInNode(two))
                                 .build());
  VisibleSelectionInFlatTree selectionInFlatTree = createVisibleSelection(
      SelectionInFlatTree::Builder()
          .collapse(PositionInFlatTree::firstPositionInNode(one))
          .extend(PositionInFlatTree::lastPositionInNode(two))
          .build());

  EXPECT_EQ(Position(one->firstChild(), 0), selection.start());
  EXPECT_EQ(Position(two->firstChild(), 2), selection.end());
  EXPECT_EQ(PositionInFlatTree(five->firstChild(), 0),
            selectionInFlatTree.start());
  EXPECT_EQ(PositionInFlatTree(five->firstChild(), 2),
            selectionInFlatTree.end());
}

TEST_F(VisibleSelectionTest, ShadowNested) {
  const char* bodyContent =
      "<p id='host'>00<b id='one'>11</b><b id='two'>22</b>33</p>";
  const char* shadowContent =
      "<a><span id='s4'>44</span><content select=#two></content><span "
      "id='s5'>55</span><content select=#one></content><span "
      "id='s6'>66</span></a>";
  const char* shadowContent2 =
      "<span id='s7'>77</span><content></content><span id='s8'>88</span>";
  setBodyContent(bodyContent);
  ShadowRoot* shadowRoot = setShadowContent(shadowContent, "host");
  ShadowRoot* shadowRoot2 = createShadowRootForElementWithIDAndSetInnerHTML(
      *shadowRoot, "s5", shadowContent2);

  // Flat tree is something like below:
  //  <p id="host">
  //    <span id="s4">44</span>
  //    <b id="two">22</b>
  //    <span id="s5"><span id="s7">77>55</span id="s8">88</span>
  //    <b id="one">11</b>
  //    <span id="s6">66</span>
  //  </p>
  Element* body = document().body();
  Element* host = body->querySelector("#host");
  Element* one = body->querySelector("#one");
  Element* eight = shadowRoot2->querySelector("#s8");

  VisibleSelection selection = createVisibleSelection(
      SelectionInDOMTree::Builder()
          .collapse(Position::firstPositionInNode(one))
          .extend(Position::lastPositionInNode(shadowRoot2))
          .build());
  VisibleSelectionInFlatTree selectionInFlatTree = createVisibleSelection(
      SelectionInFlatTree::Builder()
          .collapse(PositionInFlatTree::firstPositionInNode(one))
          .extend(PositionInFlatTree::afterNode(eight))
          .build());

  EXPECT_EQ(Position(host, PositionAnchorType::BeforeAnchor),
            selection.start());
  EXPECT_EQ(Position(one->firstChild(), 0), selection.end());
  EXPECT_EQ(PositionInFlatTree(eight->firstChild(), 2),
            selectionInFlatTree.start());
  EXPECT_EQ(PositionInFlatTree(eight->firstChild(), 2),
            selectionInFlatTree.end());
}

TEST_F(VisibleSelectionTest, WordGranularity) {
  setBodyContent(LOREM_IPSUM);

  VisibleSelection selection;
  VisibleSelectionInFlatTree selectionInFlatTree;

  // Beginning of a word.
  {
    setSelection(selection, 0);
    setSelection(selectionInFlatTree, 0);
    selection = expandUsingGranularity(selection, WordGranularity);
    selectionInFlatTree =
        expandUsingGranularity(selectionInFlatTree, WordGranularity);

    Range* range = firstRangeOf(selection);
    EXPECT_EQ(0, range->startOffset());
    EXPECT_EQ(5, range->endOffset());
    EXPECT_EQ("Lorem", range->text());
    testFlatTreePositionsToEqualToDOMTreePositions(selection,
                                                   selectionInFlatTree);
  }

  // Middle of a word.
  {
    setSelection(selection, 8);
    setSelection(selectionInFlatTree, 8);
    selection = expandUsingGranularity(selection, WordGranularity);
    selectionInFlatTree =
        expandUsingGranularity(selectionInFlatTree, WordGranularity);

    Range* range = firstRangeOf(selection);
    EXPECT_EQ(6, range->startOffset());
    EXPECT_EQ(11, range->endOffset());
    EXPECT_EQ("ipsum", range->text());
    testFlatTreePositionsToEqualToDOMTreePositions(selection,
                                                   selectionInFlatTree);
  }

  // End of a word.
  // FIXME: that sounds buggy, we might want to select the word _before_ instead
  // of the space...
  {
    setSelection(selection, 5);
    setSelection(selectionInFlatTree, 5);
    selection = expandUsingGranularity(selection, WordGranularity);
    selectionInFlatTree =
        expandUsingGranularity(selectionInFlatTree, WordGranularity);

    Range* range = firstRangeOf(selection);
    EXPECT_EQ(5, range->startOffset());
    EXPECT_EQ(6, range->endOffset());
    EXPECT_EQ(" ", range->text());
    testFlatTreePositionsToEqualToDOMTreePositions(selection,
                                                   selectionInFlatTree);
  }

  // Before comma.
  // FIXME: that sounds buggy, we might want to select the word _before_ instead
  // of the comma.
  {
    setSelection(selection, 26);
    setSelection(selectionInFlatTree, 26);
    selection = expandUsingGranularity(selection, WordGranularity);
    selectionInFlatTree =
        expandUsingGranularity(selectionInFlatTree, WordGranularity);

    Range* range = firstRangeOf(selection);
    EXPECT_EQ(26, range->startOffset());
    EXPECT_EQ(27, range->endOffset());
    EXPECT_EQ(",", range->text());
    testFlatTreePositionsToEqualToDOMTreePositions(selection,
                                                   selectionInFlatTree);
  }

  // After comma.
  {
    setSelection(selection, 27);
    setSelection(selectionInFlatTree, 27);
    selection = expandUsingGranularity(selection, WordGranularity);
    selectionInFlatTree =
        expandUsingGranularity(selectionInFlatTree, WordGranularity);

    Range* range = firstRangeOf(selection);
    EXPECT_EQ(27, range->startOffset());
    EXPECT_EQ(28, range->endOffset());
    EXPECT_EQ(" ", range->text());
    testFlatTreePositionsToEqualToDOMTreePositions(selection,
                                                   selectionInFlatTree);
  }

  // When selecting part of a word.
  {
    setSelection(selection, 0, 1);
    setSelection(selectionInFlatTree, 0, 1);
    selection = expandUsingGranularity(selection, WordGranularity);
    selectionInFlatTree =
        expandUsingGranularity(selectionInFlatTree, WordGranularity);

    Range* range = firstRangeOf(selection);
    EXPECT_EQ(0, range->startOffset());
    EXPECT_EQ(5, range->endOffset());
    EXPECT_EQ("Lorem", range->text());
    testFlatTreePositionsToEqualToDOMTreePositions(selection,
                                                   selectionInFlatTree);
  }

  // When selecting part of two words.
  {
    setSelection(selection, 2, 8);
    setSelection(selectionInFlatTree, 2, 8);
    selection = expandUsingGranularity(selection, WordGranularity);
    selectionInFlatTree =
        expandUsingGranularity(selectionInFlatTree, WordGranularity);

    Range* range = firstRangeOf(selection);
    EXPECT_EQ(0, range->startOffset());
    EXPECT_EQ(11, range->endOffset());
    EXPECT_EQ("Lorem ipsum", range->text());
    testFlatTreePositionsToEqualToDOMTreePositions(selection,
                                                   selectionInFlatTree);
  }
}

// This is for crbug.com/627783, simulating restoring selection
// in undo stack.
TEST_F(VisibleSelectionTest, updateIfNeededWithShadowHost) {
  setBodyContent("<div id=host></div><div id=sample>foo</div>");
  setShadowContent("<content>", "host");
  Element* sample = document().getElementById("sample");

  // Simulates saving selection in undo stack.
  VisibleSelection selection =
      createVisibleSelection(SelectionInDOMTree::Builder()
                                 .collapse(Position(sample->firstChild(), 0))
                                 .build());
  EXPECT_EQ(Position(sample->firstChild(), 0), selection.start());

  // Simulates modifying DOM tree to invalidate distribution.
  Element* host = document().getElementById("host");
  host->appendChild(sample);
  document().updateStyleAndLayout();

  // Simulates to restore selection from undo stack.
  selection.updateIfNeeded();
  EXPECT_EQ(Position(sample->firstChild(), 0), selection.start());

  VisibleSelectionInFlatTree selectionInFlatTree;
  SelectionAdjuster::adjustSelectionInFlatTree(&selectionInFlatTree, selection);
  EXPECT_EQ(PositionInFlatTree(sample->firstChild(), 0),
            selectionInFlatTree.start());
}

}  // namespace blink
