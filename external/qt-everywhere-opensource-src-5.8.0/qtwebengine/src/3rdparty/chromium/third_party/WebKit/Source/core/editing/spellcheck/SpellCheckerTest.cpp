// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/spellcheck/SpellChecker.h"

#include "core/editing/EditingTestBase.h"
#include "core/editing/Editor.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"

namespace blink {

class SpellCheckerTest : public EditingTestBase {
};

TEST_F(SpellCheckerTest, AdvanceToNextMisspellingWithEmptyInputNoCrash)
{
    setBodyContent("<input placeholder='placeholder'>abc");
    updateAllLifecyclePhases();
    Element* input = document().querySelector("input", ASSERT_NO_EXCEPTION);
    input->focus();
    document().settings()->setUnifiedTextCheckerEnabled(true);
    // Do not crash in AdvanceToNextMisspelling command.
    EXPECT_TRUE(document().frame()->editor().executeCommand("AdvanceToNextMisspelling"));
    document().settings()->setUnifiedTextCheckerEnabled(false);
}

} // namespace blink
