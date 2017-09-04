// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ExceptionState.h"
#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/QualifiedName.h"
#include "core/editing/EditingTestBase.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/Position.h"
#include "core/editing/SelectionTemplate.h"
#include "core/editing/VisibleSelection.h"
#include "core/editing/commands/FormatBlockCommand.h"
#include "core/editing/commands/IndentOutdentCommand.h"
#include "core/html/HTMLHeadElement.h"

#include <memory>

namespace blink {

class ApplyBlockElementCommandTest : public EditingTestBase {};

// This is a regression test for https://crbug.com/639534
TEST_F(ApplyBlockElementCommandTest, selectionCrossingOverBody) {
  document().head()->insertAdjacentHTML(
      "afterbegin",
      "<style> .CLASS13 { -webkit-user-modify: read-write; }</style></head>",
      ASSERT_NO_EXCEPTION);
  document().body()->insertAdjacentHTML(
      "afterbegin",
      "\n<pre><var id='va' class='CLASS13'>\nC\n</var></pre><input />",
      ASSERT_NO_EXCEPTION);
  document().body()->insertAdjacentText("beforebegin", "foo",
                                        ASSERT_NO_EXCEPTION);

  document().body()->setContentEditable("false", ASSERT_NO_EXCEPTION);
  document().setDesignMode("on");
  document().updateStyleAndLayoutIgnorePendingStylesheets();
  selection().setSelection(
      SelectionInDOMTree::Builder()
          .setBaseAndExtent(
              Position(document().documentElement(), 1),
              Position(document().getElementById("va")->firstChild(), 2))
          .build());

  FormatBlockCommand* command =
      FormatBlockCommand::create(document(), HTMLNames::footerTag);
  command->apply();

  EXPECT_EQ(
      "<body contenteditable=\"false\">\n"
      "<pre><var id=\"va\" class=\"CLASS13\">\nC\n</var></pre><input></body>",
      document().documentElement()->innerHTML());
}

// This is a regression test for https://crbug.com/660801
TEST_F(ApplyBlockElementCommandTest, visibilityChangeDuringCommand) {
  document().head()->insertAdjacentHTML(
      "afterbegin", "<style>li:first-child { visibility:visible; }</style>",
      ASSERT_NO_EXCEPTION);
  setBodyContent("<ul style='visibility:hidden'><li>xyz</li></ul>");
  document().setDesignMode("on");

  updateAllLifecyclePhases();
  selection().setSelection(
      SelectionInDOMTree::Builder()
          .collapse(Position(document().querySelector("li"), 0))
          .build());

  IndentOutdentCommand* command =
      IndentOutdentCommand::create(document(), IndentOutdentCommand::Indent);
  command->apply();

  EXPECT_EQ(
      "<head><style>li:first-child { visibility:visible; }</style></head>"
      "<body><ul style=\"visibility:hidden\"><ul></ul><li>xyz</li></ul></body>",
      document().documentElement()->innerHTML());
}

}  // namespace blink
