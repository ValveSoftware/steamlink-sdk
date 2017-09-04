// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLInputElement.h"

#include "core/dom/Document.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/VisualViewport.h"
#include "core/html/HTMLBodyElement.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLHtmlElement.h"
#include "core/html/HTMLOptionElement.h"
#include "core/html/forms/DateTimeChooser.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

class HTMLInputElementTest : public testing::Test {
 protected:
  Document& document() { return m_pageHolder->document(); }
  HTMLInputElement& testElement() {
    Element* element = document().getElementById("test");
    DCHECK(element);
    return toHTMLInputElement(*element);
  }

 private:
  void SetUp() override {
    m_pageHolder = DummyPageHolder::create(IntSize(800, 600));
  }

  std::unique_ptr<DummyPageHolder> m_pageHolder;
};

TEST_F(HTMLInputElementTest, FilteredDataListOptionsNoList) {
  document().documentElement()->setInnerHTML("<input id=test>");
  EXPECT_TRUE(testElement().filteredDataListOptions().isEmpty());

  document().documentElement()->setInnerHTML(
      "<input id=test list=dl1><datalist id=dl1></datalist>");
  EXPECT_TRUE(testElement().filteredDataListOptions().isEmpty());
}

TEST_F(HTMLInputElementTest, FilteredDataListOptionsContain) {
  document().documentElement()->setInnerHTML(
      "<input id=test value=BC list=dl2>"
      "<datalist id=dl2>"
      "<option>AbC DEF</option>"
      "<option>VAX</option>"
      "<option value=ghi>abc</option>"  // Match to label, not value.
      "</datalist>");
  auto options = testElement().filteredDataListOptions();
  EXPECT_EQ(2u, options.size());
  EXPECT_EQ("AbC DEF", options[0]->value().utf8());
  EXPECT_EQ("ghi", options[1]->value().utf8());

  document().documentElement()->setInnerHTML(
      "<input id=test value=i list=dl2>"
      "<datalist id=dl2>"
      "<option>I</option>"
      "<option>&#x0130;</option>"  // LATIN CAPITAL LETTER I WITH DOT ABOVE
      "<option>&#xFF49;</option>"  // FULLWIDTH LATIN SMALL LETTER I
      "</datalist>");
  options = testElement().filteredDataListOptions();
  EXPECT_EQ(2u, options.size());
  EXPECT_EQ("I", options[0]->value().utf8());
  EXPECT_EQ(0x0130, options[1]->value()[0]);
}

TEST_F(HTMLInputElementTest, FilteredDataListOptionsForMultipleEmail) {
  document().documentElement()->setInnerHTML(
      "<input id=test value='foo@example.com, tkent' list=dl3 type=email "
      "multiple>"
      "<datalist id=dl3>"
      "<option>keishi@chromium.org</option>"
      "<option>tkent@chromium.org</option>"
      "</datalist>");
  auto options = testElement().filteredDataListOptions();
  EXPECT_EQ(1u, options.size());
  EXPECT_EQ("tkent@chromium.org", options[0]->value().utf8());
}

TEST_F(HTMLInputElementTest, create) {
  HTMLInputElement* input = HTMLInputElement::create(
      document(), nullptr, /* createdByParser */ false);
  EXPECT_NE(nullptr, input->userAgentShadowRoot());

  input =
      HTMLInputElement::create(document(), nullptr, /* createdByParser */ true);
  EXPECT_EQ(nullptr, input->userAgentShadowRoot());
  input->parserSetAttributes(Vector<Attribute>());
  EXPECT_NE(nullptr, input->userAgentShadowRoot());
}

TEST_F(HTMLInputElementTest, NoAssertWhenMovedInNewDocument) {
  Document* documentWithoutFrame = Document::create();
  EXPECT_EQ(nullptr, documentWithoutFrame->frameHost());
  HTMLHtmlElement* html = HTMLHtmlElement::create(*documentWithoutFrame);
  html->appendChild(HTMLBodyElement::create(*documentWithoutFrame));

  // Create an input element with type "range" inside a document without frame.
  toHTMLBodyElement(html->firstChild())->setInnerHTML("<input type='range' />");
  documentWithoutFrame->appendChild(html);

  std::unique_ptr<DummyPageHolder> pageHolder = DummyPageHolder::create();
  auto& document = pageHolder->document();
  EXPECT_NE(nullptr, document.frameHost());

  // Put the input element inside a document with frame.
  document.body()->appendChild(documentWithoutFrame->body()->firstChild());

  // Remove the input element and all refs to it so it gets deleted before the
  // document.
  // The assert in |EventHandlerRegistry::updateEventHandlerTargets()| should
  // not be triggered.
  document.body()->removeChild(document.body()->firstChild());
}

TEST_F(HTMLInputElementTest, DefaultToolTip) {
  HTMLInputElement* inputWithoutForm =
      HTMLInputElement::create(document(), nullptr, false);
  inputWithoutForm->setBooleanAttribute(HTMLNames::requiredAttr, true);
  document().body()->appendChild(inputWithoutForm);
  EXPECT_EQ("<<ValidationValueMissing>>", inputWithoutForm->defaultToolTip());

  HTMLFormElement* form = HTMLFormElement::create(document());
  document().body()->appendChild(form);
  HTMLInputElement* inputWithForm =
      HTMLInputElement::create(document(), nullptr, false);
  inputWithForm->setBooleanAttribute(HTMLNames::requiredAttr, true);
  form->appendChild(inputWithForm);
  EXPECT_EQ("<<ValidationValueMissing>>", inputWithForm->defaultToolTip());

  form->setBooleanAttribute(HTMLNames::novalidateAttr, true);
  EXPECT_EQ(String(), inputWithForm->defaultToolTip());
}

// crbug.com/589838
TEST_F(HTMLInputElementTest, ImageTypeCrash) {
  HTMLInputElement* input =
      HTMLInputElement::create(document(), nullptr, false);
  input->setAttribute(HTMLNames::typeAttr, "image");
  input->ensureFallbackContent();
  // Make sure ensurePrimaryContent() recreates UA shadow tree, and updating
  // |value| doesn't crash.
  input->ensurePrimaryContent();
  input->setAttribute(HTMLNames::valueAttr, "aaa");
}

TEST_F(HTMLInputElementTest, DateTimeChooserSizeParamRespectsScale) {
  document().view()->frame().host()->visualViewport().setScale(2.f);
  document().body()->setInnerHTML(
      "<input type='date' style='width:200px;height:50px' />");
  document().view()->updateAllLifecyclePhases();
  HTMLInputElement* input = toHTMLInputElement(document().body()->firstChild());

  DateTimeChooserParameters params;
  bool success = input->setupDateTimeChooserParameters(params);
  EXPECT_TRUE(success);
  EXPECT_EQ("date", params.type);
  EXPECT_EQ(IntRect(16, 16, 400, 100), params.anchorRectInScreen);
}

}  // namespace blink
