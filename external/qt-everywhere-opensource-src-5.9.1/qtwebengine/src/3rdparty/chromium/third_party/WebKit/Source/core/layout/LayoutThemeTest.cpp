// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutTheme.h"

#include "core/dom/Document.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLElement.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "core/style/ComputedStyle.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/graphics/Color.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

class LayoutThemeTest : public ::testing::Test {
 protected:
  void SetUp() override;
  Document& document() const { return *m_document; }
  void setHtmlInnerHTML(const char* htmlContent);

 private:
  std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
  Persistent<Document> m_document;
};

void LayoutThemeTest::SetUp() {
  m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
  m_document = &m_dummyPageHolder->document();
  ASSERT(m_document);
}

void LayoutThemeTest::setHtmlInnerHTML(const char* htmlContent) {
  document().documentElement()->setInnerHTML(String::fromUTF8(htmlContent));
  document().view()->updateAllLifecyclePhases();
}

inline Color outlineColor(Element* element) {
  return element->computedStyle()->visitedDependentColor(
      CSSPropertyOutlineColor);
}

inline EBorderStyle outlineStyle(Element* element) {
  return element->computedStyle()->outlineStyle();
}

TEST_F(LayoutThemeTest, ChangeFocusRingColor) {
  setHtmlInnerHTML("<span id=span tabIndex=0>Span</span>");

  Element* span = document().getElementById(AtomicString("span"));
  EXPECT_NE(nullptr, span);
  EXPECT_NE(nullptr, span->layoutObject());

  Color customColor = makeRGB(123, 145, 167);

  // Checking unfocused style.
  EXPECT_EQ(BorderStyleNone, outlineStyle(span));
  EXPECT_NE(customColor, outlineColor(span));

  // Do focus.
  document().page()->focusController().setActive(true);
  document().page()->focusController().setFocused(true);
  span->focus();
  document().view()->updateAllLifecyclePhases();

  // Checking focused style.
  EXPECT_NE(BorderStyleNone, outlineStyle(span));
  EXPECT_NE(customColor, outlineColor(span));

  // Change focus ring color.
  LayoutTheme::theme().setCustomFocusRingColor(customColor);
  Page::platformColorsChanged();
  document().view()->updateAllLifecyclePhases();

  // Check that the focus ring color is updated.
  EXPECT_NE(BorderStyleNone, outlineStyle(span));
  EXPECT_EQ(customColor, outlineColor(span));
}

TEST_F(LayoutThemeTest, FormatMediaTime) {
  struct {
    float time;
    float duration;
    String expectedResult;
  } tests[] = {
      {1, 1, "0:01"},        {1, 15, "0:01"},        {1, 600, "0:01"},
      {1, 3600, "00:01"},    {1, 7200, "000:01"},    {15, 15, "0:15"},
      {15, 600, "0:15"},     {15, 3600, "00:15"},    {15, 7200, "000:15"},
      {600, 600, "10:00"},   {600, 3600, "10:00"},   {600, 7200, "010:00"},
      {3600, 3600, "60:00"}, {3600, 7200, "060:00"}, {7200, 7200, "120:00"},
  };

  for (const auto& testcase : tests) {
    EXPECT_EQ(testcase.expectedResult,
              LayoutTheme::theme().formatMediaControlsCurrentTime(
                  testcase.time, testcase.duration));
  }
}

}  // namespace blink
