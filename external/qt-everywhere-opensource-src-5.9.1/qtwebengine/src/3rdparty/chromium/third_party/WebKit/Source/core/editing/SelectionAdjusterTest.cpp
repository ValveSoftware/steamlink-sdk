// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/SelectionAdjuster.h"

#include "core/editing/EditingTestBase.h"

namespace blink {

class SelectionAdjusterTest : public EditingTestBase {};

TEST_F(SelectionAdjusterTest, adjustSelectionInFlatTree) {
  setBodyContent("<div id=sample>foo</div>");
  VisibleSelectionInFlatTree selectionInFlatTree;

  Node* const sample = document().getElementById("sample");
  Node* const foo = sample->firstChild();
  // Select "foo"
  VisibleSelection selection =
      createVisibleSelection(SelectionInDOMTree::Builder()
                                 .collapse(Position(foo, 0))
                                 .extend(Position(foo, 3))
                                 .build());
  SelectionAdjuster::adjustSelectionInFlatTree(&selectionInFlatTree, selection);
  EXPECT_EQ(PositionInFlatTree(foo, 0), selectionInFlatTree.start());
  EXPECT_EQ(PositionInFlatTree(foo, 3), selectionInFlatTree.end());
}

TEST_F(SelectionAdjusterTest, adjustSelectionInDOMTree) {
  setBodyContent("<div id=sample>foo</div>");
  VisibleSelection selection;

  Node* const sample = document().getElementById("sample");
  Node* const foo = sample->firstChild();
  // Select "foo"
  VisibleSelectionInFlatTree selectionInFlatTree =
      createVisibleSelection(SelectionInFlatTree::Builder()
                                 .collapse(PositionInFlatTree(foo, 0))
                                 .extend(PositionInFlatTree(foo, 3))
                                 .build());
  SelectionAdjuster::adjustSelectionInDOMTree(&selection, selectionInFlatTree);
  EXPECT_EQ(Position(foo, 0), selection.start());
  EXPECT_EQ(Position(foo, 3), selection.end());
}

}  // namespace blink
