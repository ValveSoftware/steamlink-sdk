// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/commands/InsertListCommand.h"

#include "core/dom/ParentNode.h"
#include "core/dom/Text.h"
#include "core/editing/EditingTestBase.h"
#include "core/editing/FrameSelection.h"

namespace blink {

class InsertListCommandTest : public EditingTestBase {};

TEST_F(InsertListCommandTest, ShouldCleanlyRemoveSpuriousTextNode) {
  // Needs to be editable to use InsertListCommand.
  document().setDesignMode("on");
  // Set up the condition:
  // * Selection is a range, to go down into
  //   InsertListCommand::listifyParagraph.
  // * The selection needs to have a sibling list to go down into
  //   InsertListCommand::mergeWithNeighboringLists.
  // * Should be the same type (ordered list) to go into
  //   CompositeEditCommand::mergeIdenticalElements.
  // * Should have no actual children to fail the listChildNode check
  //   in InsertListCommand::doApplyForSingleParagraph.
  // * There needs to be an extra text node to trigger its removal in
  //   CompositeEditCommand::mergeIdenticalElements.
  // The removeNode is what updates document lifecycle to VisualUpdatePending
  // and makes FrameView::needsLayout return true.
  setBodyContent("\nd\n<ol>");
  Text* emptyText = document().createTextNode("");
  document().body()->insertBefore(emptyText, document().body()->firstChild());
  updateAllLifecyclePhases();
  document().frame()->selection().setSelection(
      SelectionInDOMTree::Builder()
          .collapse(Position(document().body(), 0))
          .extend(Position(document().body(), 2))
          .build());

  InsertListCommand* command =
      InsertListCommand::create(document(), InsertListCommand::OrderedList);
  // This should not DCHECK.
  EXPECT_TRUE(command->apply())
      << "The insert ordered list command should have succeeded";
  EXPECT_EQ("<ol><li>d</li></ol>", document().body()->innerHTML());
}
}
