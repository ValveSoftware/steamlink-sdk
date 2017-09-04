// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/forms/OptionList.h"

#include "core/html/HTMLDocument.h"
#include "core/html/HTMLOptionElement.h"
#include "core/html/HTMLSelectElement.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

AtomicString id(const HTMLOptionElement* option) {
  return option->fastGetAttribute(HTMLNames::idAttr);
}

}  // namespace

class OptionListTest : public ::testing::Test {
 protected:
  void SetUp() override {
    HTMLDocument* document = HTMLDocument::create();
    HTMLSelectElement* select = HTMLSelectElement::create(*document);
    document->appendChild(select);
    m_select = select;
  }
  HTMLSelectElement& select() const { return *m_select; }

 private:
  Persistent<HTMLSelectElement> m_select;
};

TEST_F(OptionListTest, Empty) {
  OptionList list = select().optionList();
  EXPECT_EQ(list.end(), list.begin())
      << "OptionList should iterate over empty SELECT successfully";
}

TEST_F(OptionListTest, OptionOnly) {
  select().setInnerHTML(
      "text<input><option id=o1></option><input><option "
      "id=o2></option><input>");
  HTMLElement* div = toHTMLElement(select().document().createElement("div"));
  div->setInnerHTML("<option id=o3></option>");
  select().appendChild(div);
  OptionList list = select().optionList();
  OptionList::Iterator iter = list.begin();
  EXPECT_EQ("o1", id(*iter));
  ++iter;
  EXPECT_EQ("o2", id(*iter));
  ++iter;
  // No "o3" because it's in DIV.
  EXPECT_EQ(list.end(), iter);
}

TEST_F(OptionListTest, Optgroup) {
  select().setInnerHTML(
      "<optgroup><option id=g11></option><option id=g12></option></optgroup>"
      "<optgroup><option id=g21></option></optgroup>"
      "<optgroup></optgroup>"
      "<option id=o1></option>"
      "<optgroup><option id=g41></option></optgroup>");
  OptionList list = select().optionList();
  OptionList::Iterator iter = list.begin();
  EXPECT_EQ("g11", id(*iter));
  ++iter;
  EXPECT_EQ("g12", id(*iter));
  ++iter;
  EXPECT_EQ("g21", id(*iter));
  ++iter;
  EXPECT_EQ("o1", id(*iter));
  ++iter;
  EXPECT_EQ("g41", id(*iter));
  ++iter;
  EXPECT_EQ(list.end(), iter);

  toHTMLElement(select().firstChild())
      ->setInnerHTML(
          "<optgroup><option id=gg11></option></optgroup>"
          "<option id=g11></option>");
  list = select().optionList();
  iter = list.begin();
  EXPECT_EQ("g11", id(*iter)) << "Nested OPTGROUP should be ignored.";
}

}  // naemespace blink
