// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/FocusController.h"

#include "core/dom/shadow/ShadowRootInit.h"
#include "core/html/HTMLElement.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

class FocusControllerTest : public testing::Test {
 public:
  Document& document() const { return m_pageHolder->document(); }
  FocusController& focusController() const {
    return document().page()->focusController();
  }

 private:
  void SetUp() override { m_pageHolder = DummyPageHolder::create(); }

  std::unique_ptr<DummyPageHolder> m_pageHolder;
};

TEST_F(FocusControllerTest, SetInitialFocus) {
  document().body()->setInnerHTML("<input><textarea>");
  Element* input = toElement(document().body()->firstChild());
  // Set sequential focus navigation point before the initial focus.
  input->focus();
  input->blur();
  focusController().setInitialFocus(WebFocusTypeForward);
  EXPECT_EQ(input, document().focusedElement())
      << "We should ignore sequential focus navigation starting point in "
         "setInitialFocus().";
}

TEST_F(FocusControllerTest, DoNotCrash1) {
  document().body()->setInnerHTML(
      "<div id='host'></div>This test is for crbug.com/609012<p id='target' "
      "tabindex='0'></p>");
  // <div> with shadow root
  Element* host = toElement(document().body()->firstChild());
  ShadowRootInit init;
  init.setMode("open");
  host->attachShadow(ScriptState::forMainWorld(document().frame()), init,
                     ASSERT_NO_EXCEPTION);
  // "This test is for crbug.com/609012"
  Node* text = host->nextSibling();
  // <p>
  Element* target = toElement(text->nextSibling());

  // Set sequential focus navigation point at text node.
  document().setSequentialFocusNavigationStartingPoint(text);

  focusController().advanceFocus(WebFocusTypeForward);
  EXPECT_EQ(target, document().focusedElement())
      << "This should not hit assertion and finish properly.";
}

TEST_F(FocusControllerTest, DoNotCrash2) {
  document().body()->setInnerHTML(
      "<p id='target' tabindex='0'></p>This test is for crbug.com/609012<div "
      "id='host'></div>");
  // <p>
  Element* target = toElement(document().body()->firstChild());
  // "This test is for crbug.com/609012"
  Node* text = target->nextSibling();
  // <div> with shadow root
  Element* host = toElement(text->nextSibling());
  ShadowRootInit init;
  init.setMode("open");
  host->attachShadow(ScriptState::forMainWorld(document().frame()), init,
                     ASSERT_NO_EXCEPTION);

  // Set sequential focus navigation point at text node.
  document().setSequentialFocusNavigationStartingPoint(text);

  focusController().advanceFocus(WebFocusTypeBackward);
  EXPECT_EQ(target, document().focusedElement())
      << "This should not hit assertion and finish properly.";
}

}  // namespace blink
