// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/spellcheck/SpellChecker.h"

#include "core/editing/EditingTestBase.h"
#include "core/editing/Editor.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLInputElement.h"
#include "core/testing/DummyPageHolder.h"

namespace blink {

class SpellCheckerTest : public EditingTestBase {
 protected:
  int layoutCount() const { return page().frameView().layoutCount(); }
  DummyPageHolder& page() const { return dummyPageHolder(); }

  void forceLayout();
};

void SpellCheckerTest::forceLayout() {
  FrameView& frameView = page().frameView();
  IntRect frameRect = frameView.frameRect();
  frameRect.setWidth(frameRect.width() + 1);
  frameRect.setHeight(frameRect.height() + 1);
  page().frameView().setFrameRect(frameRect);
  document().updateStyleAndLayoutIgnorePendingStylesheets();
}

TEST_F(SpellCheckerTest, AdvanceToNextMisspellingWithEmptyInputNoCrash) {
  setBodyContent("<input placeholder='placeholder'>abc");
  updateAllLifecyclePhases();
  Element* input = document().querySelector("input");
  input->focus();
  // Do not crash in AdvanceToNextMisspelling command.
  EXPECT_TRUE(
      document().frame()->editor().executeCommand("AdvanceToNextMisspelling"));
}

TEST_F(SpellCheckerTest, SpellCheckDoesNotCauseUpdateLayout) {
  setBodyContent("<input>");
  HTMLInputElement* input =
      toHTMLInputElement(document().querySelector("input"));
  input->focus();
  input->setValue("Hello, input field");
  document().updateStyleAndLayout();
  VisibleSelection oldSelection = document().frame()->selection().selection();

  Position newPosition(input->innerEditorElement()->firstChild(), 3);
  VisibleSelection newSelection = createVisibleSelection(
      SelectionInDOMTree::Builder().collapse(newPosition).build());
  document().frame()->selection().setSelection(
      newSelection, FrameSelection::CloseTyping |
                        FrameSelection::ClearTypingStyle |
                        FrameSelection::DoNotUpdateAppearance);
  ASSERT_EQ(3, input->selectionStart());

  Persistent<SpellChecker> spellChecker(SpellChecker::create(page().frame()));
  forceLayout();
  int startCount = layoutCount();
  spellChecker->respondToChangedSelection(
      oldSelection.start(),
      FrameSelection::CloseTyping | FrameSelection::ClearTypingStyle);
  EXPECT_EQ(startCount, layoutCount());
}

}  // namespace blink
