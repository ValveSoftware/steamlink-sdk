// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/commands/ReplaceSelectionCommand.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentFragment.h"
#include "core/dom/ParserContentPolicy.h"
#include "core/editing/EditingTestBase.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/Position.h"
#include "core/editing/VisibleSelection.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <memory>

namespace blink {

class ReplaceSelectionCommandTest : public EditingTestBase {};

// This is a regression test for https://crbug.com/619131
TEST_F(ReplaceSelectionCommandTest, pastingEmptySpan) {
  document().setDesignMode("on");
  setBodyContent("foo");

  LocalFrame* frame = document().frame();
  frame->selection().setSelection(SelectionInDOMTree::Builder()
                                      .collapse(Position(document().body(), 0))
                                      .build());

  DocumentFragment* fragment = document().createDocumentFragment();
  fragment->appendChild(document().createElement("span"));

  // |options| are taken from |Editor::replaceSelectionWithFragment()| with
  // |selectReplacement| and |smartReplace|.
  ReplaceSelectionCommand::CommandOptions options =
      ReplaceSelectionCommand::PreventNesting |
      ReplaceSelectionCommand::SanitizeFragment |
      ReplaceSelectionCommand::SelectReplacement |
      ReplaceSelectionCommand::SmartReplace;
  ReplaceSelectionCommand* command =
      ReplaceSelectionCommand::create(document(), fragment, options);

  EXPECT_TRUE(command->apply()) << "the replace command should have succeeded";
  EXPECT_EQ("foo", document().body()->innerHTML()) << "no DOM tree mutation";
}

// This is a regression test for https://crbug.com/121163
TEST_F(ReplaceSelectionCommandTest, styleTagsInPastedHeadIncludedInContent) {
  document().setDesignMode("on");
  updateAllLifecyclePhases();
  dummyPageHolder().frame().selection().setSelection(
      SelectionInDOMTree::Builder()
          .collapse(Position(document().body(), 0))
          .build());

  DocumentFragment* fragment = document().createDocumentFragment();
  fragment->parseHTML(
      "<head><style>foo { bar: baz; }</style></head>"
      "<body><p>Text</p></body>",
      document().documentElement(), DisallowScriptingAndPluginContent);

  ReplaceSelectionCommand::CommandOptions options = 0;
  ReplaceSelectionCommand* command =
      ReplaceSelectionCommand::create(document(), fragment, options);
  EXPECT_TRUE(command->apply()) << "the replace command should have succeeded";

  EXPECT_EQ(
      "<head><style>foo { bar: baz; }</style></head>"
      "<body><p>Text</p></body>",
      document().body()->innerHTML())
      << "the STYLE and P elements should have been pasted into the body "
      << "of the document";
}

}  // namespace blink
