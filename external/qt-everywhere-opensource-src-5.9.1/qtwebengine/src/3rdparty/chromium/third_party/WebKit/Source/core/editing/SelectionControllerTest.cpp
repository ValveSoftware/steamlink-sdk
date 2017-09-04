// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/SelectionController.h"

#include "core/editing/EditingTestBase.h"
#include "core/editing/FrameSelection.h"
#include "core/input/EventHandler.h"

namespace blink {

class SelectionControllerTest : public EditingTestBase {
 protected:
  SelectionControllerTest() = default;

  const VisibleSelection& visibleSelectionInDOMTree() const {
    return selection().selection();
  }

  const VisibleSelectionInFlatTree& visibleSelectionInFlatTree() const {
    return selection().selectionInFlatTree();
  }

  void setNonDirectionalSelectionIfNeeded(const VisibleSelectionInFlatTree&,
                                          TextGranularity);

 private:
  DISALLOW_COPY_AND_ASSIGN(SelectionControllerTest);
};

void SelectionControllerTest::setNonDirectionalSelectionIfNeeded(
    const VisibleSelectionInFlatTree& newSelection,
    TextGranularity granularity) {
  frame()
      .eventHandler()
      .selectionController()
      .setNonDirectionalSelectionIfNeeded(
          newSelection, granularity, SelectionController::DoNotAdjustEndpoints);
}

TEST_F(SelectionControllerTest, setNonDirectionalSelectionIfNeeded) {
  const char* bodyContent = "<span id=top>top</span><span id=host></span>";
  const char* shadowContent = "<span id=bottom>bottom</span>";
  setBodyContent(bodyContent);
  ShadowRoot* shadowRoot = setShadowContent(shadowContent, "host");

  Node* top = document().getElementById("top")->firstChild();
  Node* bottom = shadowRoot->getElementById("bottom")->firstChild();
  Node* host = document().getElementById("host");

  // top to bottom
  setNonDirectionalSelectionIfNeeded(
      createVisibleSelection(SelectionInFlatTree::Builder()
                                 .collapse(PositionInFlatTree(top, 1))
                                 .extend(PositionInFlatTree(bottom, 3))
                                 .build()),
      CharacterGranularity);
  EXPECT_EQ(Position(top, 1), visibleSelectionInDOMTree().base());
  EXPECT_EQ(Position::beforeNode(host), visibleSelectionInDOMTree().extent());
  EXPECT_EQ(Position(top, 1), visibleSelectionInDOMTree().start());
  EXPECT_EQ(Position(top, 3), visibleSelectionInDOMTree().end());

  EXPECT_EQ(PositionInFlatTree(top, 1), visibleSelectionInFlatTree().base());
  EXPECT_EQ(PositionInFlatTree(bottom, 3),
            visibleSelectionInFlatTree().extent());
  EXPECT_EQ(PositionInFlatTree(top, 1), visibleSelectionInFlatTree().start());
  EXPECT_EQ(PositionInFlatTree(bottom, 3), visibleSelectionInFlatTree().end());

  // bottom to top
  setNonDirectionalSelectionIfNeeded(
      createVisibleSelection(SelectionInFlatTree::Builder()
                                 .collapse(PositionInFlatTree(bottom, 3))
                                 .extend(PositionInFlatTree(top, 1))
                                 .build()),
      CharacterGranularity);
  EXPECT_EQ(Position(bottom, 3), visibleSelectionInDOMTree().base());
  EXPECT_EQ(Position::beforeNode(bottom->parentNode()),
            visibleSelectionInDOMTree().extent());
  EXPECT_EQ(Position(bottom, 0), visibleSelectionInDOMTree().start());
  EXPECT_EQ(Position(bottom, 3), visibleSelectionInDOMTree().end());

  EXPECT_EQ(PositionInFlatTree(bottom, 3), visibleSelectionInFlatTree().base());
  EXPECT_EQ(PositionInFlatTree(top, 1), visibleSelectionInFlatTree().extent());
  EXPECT_EQ(PositionInFlatTree(top, 1), visibleSelectionInFlatTree().start());
  EXPECT_EQ(PositionInFlatTree(bottom, 3), visibleSelectionInFlatTree().end());
}

}  // namespace blink
