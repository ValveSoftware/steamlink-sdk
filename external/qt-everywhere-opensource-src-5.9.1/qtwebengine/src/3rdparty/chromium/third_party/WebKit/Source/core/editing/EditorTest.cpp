// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/Editor.h"

#include "core/dom/Document.h"
#include "core/editing/EditingTestBase.h"
#include "core/html/HTMLBodyElement.h"
#include "core/html/HTMLDivElement.h"
#include "core/html/HTMLHeadElement.h"
#include "core/html/HTMLHtmlElement.h"

namespace blink {

class EditorTest : public EditingTestBase {
 protected:
  void makeDocumentEmpty();
};

void EditorTest::makeDocumentEmpty() {
  while (document().firstChild())
    document().removeChild(document().firstChild());
}

TEST_F(EditorTest, tidyUpHTMLStructureFromBody) {
  Element* body = HTMLBodyElement::create(document());
  makeDocumentEmpty();
  document().setDesignMode("on");
  document().appendChild(body);
  Editor::tidyUpHTMLStructure(document());

  EXPECT_TRUE(isHTMLHtmlElement(document().documentElement()));
  EXPECT_EQ(body, document().body());
  EXPECT_EQ(document().documentElement(), body->parentNode());
}

TEST_F(EditorTest, tidyUpHTMLStructureFromDiv) {
  Element* div = HTMLDivElement::create(document());
  makeDocumentEmpty();
  document().setDesignMode("on");
  document().appendChild(div);
  Editor::tidyUpHTMLStructure(document());

  EXPECT_TRUE(isHTMLHtmlElement(document().documentElement()));
  EXPECT_TRUE(isHTMLBodyElement(document().body()));
  EXPECT_EQ(document().body(), div->parentNode());
}

TEST_F(EditorTest, tidyUpHTMLStructureFromHead) {
  Element* head = HTMLHeadElement::create(document());
  makeDocumentEmpty();
  document().setDesignMode("on");
  document().appendChild(head);
  Editor::tidyUpHTMLStructure(document());

  EXPECT_TRUE(isHTMLHtmlElement(document().documentElement()));
  EXPECT_TRUE(isHTMLBodyElement(document().body()));
  EXPECT_EQ(document().documentElement(), head->parentNode());
}

}  // namespace blink
