// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/Text.h"

#include "core/dom/Range.h"
#include "core/editing/EditingTestBase.h"
#include "core/html/HTMLPreElement.h"
#include "core/layout/LayoutText.h"

namespace blink {

// TODO(xiaochengh): Use a new testing base class.
class TextTest : public EditingTestBase {
};

TEST_F(TextTest, SetDataToChangeFirstLetterTextNode)
{
    setBodyContent("<style>pre::first-letter {color:red;}</style><pre id=sample>a<span>b</span></pre>");

    Node* sample = document().getElementById("sample");
    Text* text = toText(sample->firstChild());
    text->setData(" ");
    updateAllLifecyclePhases();

    EXPECT_FALSE(text->layoutObject()->isTextFragment());
}

TEST_F(TextTest, RemoveFirstLetterPseudoElementWhenNoLetter)
{
    setBodyContent("<style>*::first-letter{font:icon;}</style><pre>AB\n</pre>");

    Element* pre = document().querySelector("pre", ASSERT_NO_EXCEPTION);
    Text* text = toText(pre->firstChild());

    Range* range = Range::create(document(), text, 0, text, 2);
    range->deleteContents(ASSERT_NO_EXCEPTION);
    updateAllLifecyclePhases();

    EXPECT_FALSE(text->layoutObject()->isTextFragment());
}

} // namespace blink
