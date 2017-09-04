// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/EditingStyle.h"

#include "core/css/StylePropertySet.h"
#include "core/dom/Document.h"
#include "core/editing/EditingTestBase.h"
#include "core/html/HTMLBodyElement.h"
#include "core/html/HTMLDivElement.h"
#include "core/html/HTMLHeadElement.h"
#include "core/html/HTMLHtmlElement.h"

namespace blink {

class EditingStyleTest : public EditingTestBase {};

TEST_F(EditingStyleTest, mergeInlineStyleOfElement) {
  setBodyContent(
      "<span id=s1 style='--A:var(---B)'>1</span>"
      "<span id=s2 style='float:var(--C)'>2</span>");
  updateAllLifecyclePhases();

  EditingStyle* editingStyle =
      EditingStyle::create(toHTMLElement(document().getElementById("s2")));
  editingStyle->mergeInlineStyleOfElement(
      toHTMLElement(document().getElementById("s1")),
      EditingStyle::OverrideValues);

  EXPECT_FALSE(editingStyle->style()->hasProperty(CSSPropertyFloat))
      << "Don't merge a property with unresolved value";
  EXPECT_EQ("var(---B)",
            editingStyle->style()->getPropertyValue(AtomicString("--A")))
      << "Keep unresolved value on merging style";
}

}  // namespace blink
