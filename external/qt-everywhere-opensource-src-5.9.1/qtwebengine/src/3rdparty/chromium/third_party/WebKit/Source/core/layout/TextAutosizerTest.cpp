// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutTestHelper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class TextAutosizerTest : public RenderingTest {
 private:
  void SetUp() override {
    RenderingTest::SetUp();
    document().settings()->setTextAutosizingEnabled(true);
    document().settings()->setTextAutosizingWindowSizeOverride(
        IntSize(320, 480));
  }
};

TEST_F(TextAutosizerTest, SimpleParagraph) {
  setBodyInnerHTML(
      "<style>"
      "  html { font-size: 16px; }"
      "  body { width: 800px; margin: 0; overflow-y: hidden; }"
      "</style>"
      "<div id='autosized'>"
      "  Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do"
      "  eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim"
      "  ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut"
      "  aliquip ex ea commodo consequat. Duis aute irure dolor in"
      "  reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla"
      "  pariatur. Excepteur sint occaecat cupidatat non proident, sunt in"
      "  culpa qui officia deserunt mollit anim id est laborum."
      "</div>");
  Element* autosized = document().getElementById("autosized");
  EXPECT_FLOAT_EQ(16.f,
                  autosized->layoutObject()->style()->specifiedFontSize());
  // (specified font-size = 16px) * (viewport width = 800px) /
  // (window width = 320px) = 40px.
  EXPECT_FLOAT_EQ(40.f, autosized->layoutObject()->style()->computedFontSize());
}

TEST_F(TextAutosizerTest, TextSizeAdjustDisablesAutosizing) {
  setBodyInnerHTML(
      "<style>"
      "  html { font-size: 16px; }"
      "  body { width: 800px; margin: 0; overflow-y: hidden; }"
      "</style>"
      "<div id='textSizeAdjustAuto' style='text-size-adjust: auto;'>"
      "  Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do"
      "  eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim"
      "  ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut"
      "  aliquip ex ea commodo consequat. Duis aute irure dolor in"
      "  reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla"
      "  pariatur. Excepteur sint occaecat cupidatat non proident, sunt in"
      "  culpa qui officia deserunt mollit anim id est laborum."
      "</div>"
      "<div id='textSizeAdjustNone' style='text-size-adjust: none;'>"
      "  Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do"
      "  eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim"
      "  ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut"
      "  aliquip ex ea commodo consequat. Duis aute irure dolor in"
      "  reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla"
      "  pariatur. Excepteur sint occaecat cupidatat non proident, sunt in"
      "  culpa qui officia deserunt mollit anim id est laborum."
      "</div>"
      "<div id='textSizeAdjust100' style='text-size-adjust: 100%;'>"
      "  Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do"
      "  eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim"
      "  ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut"
      "  aliquip ex ea commodo consequat. Duis aute irure dolor in"
      "  reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla"
      "  pariatur. Excepteur sint occaecat cupidatat non proident, sunt in"
      "  culpa qui officia deserunt mollit anim id est laborum."
      "</div>");
  LayoutObject* textSizeAdjustAuto =
      document().getElementById("textSizeAdjustAuto")->layoutObject();
  EXPECT_FLOAT_EQ(16.f, textSizeAdjustAuto->style()->specifiedFontSize());
  EXPECT_FLOAT_EQ(40.f, textSizeAdjustAuto->style()->computedFontSize());
  LayoutObject* textSizeAdjustNone =
      document().getElementById("textSizeAdjustNone")->layoutObject();
  EXPECT_FLOAT_EQ(16.f, textSizeAdjustNone->style()->specifiedFontSize());
  EXPECT_FLOAT_EQ(16.f, textSizeAdjustNone->style()->computedFontSize());
  LayoutObject* textSizeAdjust100 =
      document().getElementById("textSizeAdjust100")->layoutObject();
  EXPECT_FLOAT_EQ(16.f, textSizeAdjust100->style()->specifiedFontSize());
  EXPECT_FLOAT_EQ(16.f, textSizeAdjust100->style()->computedFontSize());
}

TEST_F(TextAutosizerTest, ParagraphWithChangingTextSizeAdjustment) {
  setBodyInnerHTML(
      "<style>"
      "  html { font-size: 16px; }"
      "  body { width: 800px; margin: 0; overflow-y: hidden; }"
      "  .none { text-size-adjust: none; }"
      "  .small { text-size-adjust: 50%; }"
      "  .large { text-size-adjust: 150%; }"
      "</style>"
      "<div id='autosized'>"
      "  Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do"
      "  eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim"
      "  ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut"
      "  aliquip ex ea commodo consequat. Duis aute irure dolor in"
      "  reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla"
      "  pariatur. Excepteur sint occaecat cupidatat non proident, sunt in"
      "  culpa qui officia deserunt mollit anim id est laborum."
      "</div>");
  Element* autosizedDiv = document().getElementById("autosized");
  EXPECT_FLOAT_EQ(16.f,
                  autosizedDiv->layoutObject()->style()->specifiedFontSize());
  EXPECT_FLOAT_EQ(40.f,
                  autosizedDiv->layoutObject()->style()->computedFontSize());

  autosizedDiv->setAttribute(HTMLNames::classAttr, "none");
  document().view()->updateAllLifecyclePhases();
  EXPECT_FLOAT_EQ(16.f,
                  autosizedDiv->layoutObject()->style()->specifiedFontSize());
  EXPECT_FLOAT_EQ(16.f,
                  autosizedDiv->layoutObject()->style()->computedFontSize());

  autosizedDiv->setAttribute(HTMLNames::classAttr, "small");
  document().view()->updateAllLifecyclePhases();
  EXPECT_FLOAT_EQ(16.f,
                  autosizedDiv->layoutObject()->style()->specifiedFontSize());
  EXPECT_FLOAT_EQ(8.f,
                  autosizedDiv->layoutObject()->style()->computedFontSize());

  autosizedDiv->setAttribute(HTMLNames::classAttr, "large");
  document().view()->updateAllLifecyclePhases();
  EXPECT_FLOAT_EQ(16.f,
                  autosizedDiv->layoutObject()->style()->specifiedFontSize());
  EXPECT_FLOAT_EQ(24.f,
                  autosizedDiv->layoutObject()->style()->computedFontSize());

  autosizedDiv->removeAttribute(HTMLNames::classAttr);
  document().view()->updateAllLifecyclePhases();
  EXPECT_FLOAT_EQ(16.f,
                  autosizedDiv->layoutObject()->style()->specifiedFontSize());
  EXPECT_FLOAT_EQ(40.f,
                  autosizedDiv->layoutObject()->style()->computedFontSize());
}

TEST_F(TextAutosizerTest, ZeroTextSizeAdjustment) {
  setBodyInnerHTML(
      "<style>"
      "  html { font-size: 16px; }"
      "  body { width: 800px; margin: 0; overflow-y: hidden; }"
      "</style>"
      "<div id='textSizeAdjustZero' style='text-size-adjust: 0%;'>"
      "  Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do"
      "  eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim"
      "  ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut"
      "  aliquip ex ea commodo consequat. Duis aute irure dolor in"
      "  reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla"
      "  pariatur. Excepteur sint occaecat cupidatat non proident, sunt in"
      "  culpa qui officia deserunt mollit anim id est laborum."
      "</div>");
  LayoutObject* textSizeAdjustZero =
      document().getElementById("textSizeAdjustZero")->layoutObject();
  EXPECT_FLOAT_EQ(16.f, textSizeAdjustZero->style()->specifiedFontSize());
  EXPECT_FLOAT_EQ(0.f, textSizeAdjustZero->style()->computedFontSize());
}

TEST_F(TextAutosizerTest, NegativeTextSizeAdjustment) {
  setBodyInnerHTML(
      "<style>"
      "  html { font-size: 16px; }"
      "  body { width: 800px; margin: 0; overflow-y: hidden; }"
      "</style>"
      // Negative values should be treated as auto.
      "<div id='textSizeAdjustNegative' style='text-size-adjust: -10%;'>"
      "  Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do"
      "  eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim"
      "  ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut"
      "  aliquip ex ea commodo consequat. Duis aute irure dolor in"
      "  reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla"
      "  pariatur. Excepteur sint occaecat cupidatat non proident, sunt in"
      "  culpa qui officia deserunt mollit anim id est laborum."
      "</div>");
  LayoutObject* textSizeAdjustNegative =
      document().getElementById("textSizeAdjustNegative")->layoutObject();
  EXPECT_FLOAT_EQ(16.f, textSizeAdjustNegative->style()->specifiedFontSize());
  EXPECT_FLOAT_EQ(40.f, textSizeAdjustNegative->style()->computedFontSize());
}

TEST_F(TextAutosizerTest, TextSizeAdjustmentPixelUnits) {
  setBodyInnerHTML(
      "<style>"
      "  html { font-size: 16px; }"
      "  body { width: 800px; margin: 0; overflow-y: hidden; }"
      "</style>"
      // Non-percentage values should be treated as auto.
      "<div id='textSizeAdjustPixels' style='text-size-adjust: 0.1px;'>"
      "  Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do"
      "  eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim"
      "  ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut"
      "  aliquip ex ea commodo consequat. Duis aute irure dolor in"
      "  reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla"
      "  pariatur. Excepteur sint occaecat cupidatat non proident, sunt in"
      "  culpa qui officia deserunt mollit anim id est laborum."
      "</div>");
  LayoutObject* textSizeAdjustPixels =
      document().getElementById("textSizeAdjustPixels")->layoutObject();
  EXPECT_FLOAT_EQ(16.f, textSizeAdjustPixels->style()->specifiedFontSize());
  EXPECT_FLOAT_EQ(40.f, textSizeAdjustPixels->style()->computedFontSize());
}

TEST_F(TextAutosizerTest, NestedTextSizeAdjust) {
  setBodyInnerHTML(
      "<style>"
      "  html { font-size: 16px; }"
      "  body { width: 800px; margin: 0; overflow-y: hidden; }"
      "</style>"
      "<div id='textSizeAdjustA' style='text-size-adjust: 47%;'>"
      "  Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do"
      "  eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim"
      "  ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut"
      "  aliquip ex ea commodo consequat. Duis aute irure dolor in"
      "  reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla"
      "  pariatur. Excepteur sint occaecat cupidatat non proident, sunt in"
      "  culpa qui officia deserunt mollit anim id est laborum."
      "  <div id='textSizeAdjustB' style='text-size-adjust: 53%;'>"
      "    Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do"
      "    eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim"
      "    ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut"
      "    aliquip ex ea commodo consequat. Duis aute irure dolor in"
      "    reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla"
      "    pariatur. Excepteur sint occaecat cupidatat non proident, sunt in"
      "    culpa qui officia deserunt mollit anim id est laborum."
      "  </div>"
      "</div>");
  LayoutObject* textSizeAdjustA =
      document().getElementById("textSizeAdjustA")->layoutObject();
  EXPECT_FLOAT_EQ(16.f, textSizeAdjustA->style()->specifiedFontSize());
  // 16px * 47% = 7.52
  EXPECT_FLOAT_EQ(7.52f, textSizeAdjustA->style()->computedFontSize());
  LayoutObject* textSizeAdjustB =
      document().getElementById("textSizeAdjustB")->layoutObject();
  EXPECT_FLOAT_EQ(16.f, textSizeAdjustB->style()->specifiedFontSize());
  // 16px * 53% = 8.48
  EXPECT_FLOAT_EQ(8.48f, textSizeAdjustB->style()->computedFontSize());
}

TEST_F(TextAutosizerTest, PrefixedTextSizeAdjustIsAlias) {
  setBodyInnerHTML(
      "<style>"
      "  html { font-size: 16px; }"
      "  body { width: 800px; margin: 0; overflow-y: hidden; }"
      "</style>"
      "<div id='textSizeAdjust' style='-webkit-text-size-adjust: 50%;'>"
      "  Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do"
      "  eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim"
      "  ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut"
      "  aliquip ex ea commodo consequat. Duis aute irure dolor in"
      "  reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla"
      "  pariatur. Excepteur sint occaecat cupidatat non proident, sunt in"
      "  culpa qui officia deserunt mollit anim id est laborum."
      "</div>");
  LayoutObject* textSizeAdjust =
      document().getElementById("textSizeAdjust")->layoutObject();
  EXPECT_FLOAT_EQ(16.f, textSizeAdjust->style()->specifiedFontSize());
  EXPECT_FLOAT_EQ(8.f, textSizeAdjust->style()->computedFontSize());
  EXPECT_FLOAT_EQ(.5f,
                  textSizeAdjust->style()->getTextSizeAdjust().multiplier());
}

TEST_F(TextAutosizerTest, AccessibilityFontScaleFactor) {
  document().settings()->setAccessibilityFontScaleFactor(1.5);
  setBodyInnerHTML(
      "<style>"
      "  html { font-size: 16px; }"
      "  body { width: 800px; margin: 0; overflow-y: hidden; }"
      "</style>"
      "<div id='autosized'>"
      "  Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do"
      "  eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim"
      "  ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut"
      "  aliquip ex ea commodo consequat. Duis aute irure dolor in"
      "  reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla"
      "  pariatur. Excepteur sint occaecat cupidatat non proident, sunt in"
      "  culpa qui officia deserunt mollit anim id est laborum."
      "</div>");
  Element* autosized = document().getElementById("autosized");
  EXPECT_FLOAT_EQ(16.f,
                  autosized->layoutObject()->style()->specifiedFontSize());
  // 1.5 * (specified font-size = 16px) * (viewport width = 800px) /
  // (window width = 320px) = 60px.
  EXPECT_FLOAT_EQ(60.f, autosized->layoutObject()->style()->computedFontSize());
}

TEST_F(TextAutosizerTest, AccessibilityFontScaleFactorWithTextSizeAdjustNone) {
  document().settings()->setAccessibilityFontScaleFactor(1.5);
  setBodyInnerHTML(
      "<style>"
      "  html { font-size: 16px; }"
      "  body { width: 800px; margin: 0; overflow-y: hidden; }"
      "  #autosized { width: 400px; text-size-adjust: 100%; }"
      "  #notAutosized { width: 100px; text-size-adjust: 100%; }"
      "</style>"
      "<div id='autosized'>"
      "  Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do"
      "  eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim"
      "  ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut"
      "  aliquip ex ea commodo consequat. Duis aute irure dolor in"
      "  reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla"
      "  pariatur. Excepteur sint occaecat cupidatat non proident, sunt in"
      "  culpa qui officia deserunt mollit anim id est laborum."
      "</div>"
      "<div id='notAutosized'>"
      "  Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do"
      "  eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim"
      "  ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut"
      "  aliquip ex ea commodo consequat. Duis aute irure dolor in"
      "  reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla"
      "  pariatur. Excepteur sint occaecat cupidatat non proident, sunt in"
      "  culpa qui officia deserunt mollit anim id est laborum."
      "</div>");
  Element* autosized = document().getElementById("autosized");
  EXPECT_FLOAT_EQ(16.f,
                  autosized->layoutObject()->style()->specifiedFontSize());
  // 1.5 * (specified font-size = 16px) = 24px.
  EXPECT_FLOAT_EQ(24.f, autosized->layoutObject()->style()->computedFontSize());

  // Because this does not autosize (due to the width), no accessibility font
  // scale factor should be applied.
  Element* notAutosized = document().getElementById("notAutosized");
  EXPECT_FLOAT_EQ(16.f,
                  notAutosized->layoutObject()->style()->specifiedFontSize());
  // specified font-size = 16px.
  EXPECT_FLOAT_EQ(16.f,
                  notAutosized->layoutObject()->style()->computedFontSize());
}

TEST_F(TextAutosizerTest, ChangingAccessibilityFontScaleFactor) {
  document().settings()->setAccessibilityFontScaleFactor(1);
  setBodyInnerHTML(
      "<style>"
      "  html { font-size: 16px; }"
      "  body { width: 800px; margin: 0; overflow-y: hidden; }"
      "</style>"
      "<div id='autosized'>"
      "  Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do"
      "  eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim"
      "  ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut"
      "  aliquip ex ea commodo consequat. Duis aute irure dolor in"
      "  reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla"
      "  pariatur. Excepteur sint occaecat cupidatat non proident, sunt in"
      "  culpa qui officia deserunt mollit anim id est laborum."
      "</div>");
  Element* autosized = document().getElementById("autosized");
  EXPECT_FLOAT_EQ(16.f,
                  autosized->layoutObject()->style()->specifiedFontSize());
  // 1.0 * (specified font-size = 16px) * (viewport width = 800px) /
  // (window width = 320px) = 40px.
  EXPECT_FLOAT_EQ(40.f, autosized->layoutObject()->style()->computedFontSize());

  document().settings()->setAccessibilityFontScaleFactor(2);
  document().view()->updateAllLifecyclePhases();

  EXPECT_FLOAT_EQ(16.f,
                  autosized->layoutObject()->style()->specifiedFontSize());
  // 2.0 * (specified font-size = 16px) * (viewport width = 800px) /
  // (window width = 320px) = 80px.
  EXPECT_FLOAT_EQ(80.f, autosized->layoutObject()->style()->computedFontSize());
}

TEST_F(TextAutosizerTest, TextSizeAdjustDoesNotDisableAccessibility) {
  document().settings()->setAccessibilityFontScaleFactor(1.5);
  setBodyInnerHTML(
      "<style>"
      "  html { font-size: 16px; }"
      "  body { width: 800px; margin: 0; overflow-y: hidden; }"
      "</style>"
      "<div id='textSizeAdjustNone' style='text-size-adjust: none;'>"
      "  Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do"
      "  eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim"
      "  ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut"
      "  aliquip ex ea commodo consequat. Duis aute irure dolor in"
      "  reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla"
      "  pariatur. Excepteur sint occaecat cupidatat non proident, sunt in"
      "  culpa qui officia deserunt mollit anim id est laborum."
      "</div>"
      "<div id='textSizeAdjustDouble' style='text-size-adjust: 200%;'>"
      "  Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do"
      "  eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim"
      "  ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut"
      "  aliquip ex ea commodo consequat. Duis aute irure dolor in"
      "  reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla"
      "  pariatur. Excepteur sint occaecat cupidatat non proident, sunt in"
      "  culpa qui officia deserunt mollit anim id est laborum."
      "</div>");
  Element* textSizeAdjustNone = document().getElementById("textSizeAdjustNone");
  EXPECT_FLOAT_EQ(
      16.f, textSizeAdjustNone->layoutObject()->style()->specifiedFontSize());
  // 1.5 * (specified font-size = 16px) = 24px.
  EXPECT_FLOAT_EQ(
      24.f, textSizeAdjustNone->layoutObject()->style()->computedFontSize());

  Element* textSizeAdjustDouble =
      document().getElementById("textSizeAdjustDouble");
  EXPECT_FLOAT_EQ(
      16.f, textSizeAdjustDouble->layoutObject()->style()->specifiedFontSize());
  // 1.5 * (specified font-size = 16px) * (text size adjustment = 2) = 48px.
  EXPECT_FLOAT_EQ(
      48.f, textSizeAdjustDouble->layoutObject()->style()->computedFontSize());

  // Changing the accessibility font scale factor should change the adjusted
  // size.
  document().settings()->setAccessibilityFontScaleFactor(2);
  document().view()->updateAllLifecyclePhases();

  EXPECT_FLOAT_EQ(
      16.f, textSizeAdjustNone->layoutObject()->style()->specifiedFontSize());
  // 2.0 * (specified font-size = 16px) = 32px.
  EXPECT_FLOAT_EQ(
      32.f, textSizeAdjustNone->layoutObject()->style()->computedFontSize());

  EXPECT_FLOAT_EQ(
      16.f, textSizeAdjustDouble->layoutObject()->style()->specifiedFontSize());
  // 2.0 * (specified font-size = 16px) * (text size adjustment = 2) = 64px.
  EXPECT_FLOAT_EQ(
      64.f, textSizeAdjustDouble->layoutObject()->style()->computedFontSize());
}

// https://crbug.com/646237
TEST_F(TextAutosizerTest, DISABLED_TextSizeAdjustWithoutNeedingAutosizing) {
  document().settings()->setTextAutosizingWindowSizeOverride(IntSize(800, 600));
  setBodyInnerHTML(
      "<style>"
      "  html { font-size: 16px; }"
      "  body { width: 800px; margin: 0; overflow-y: hidden; }"
      "</style>"
      "<div id='textSizeAdjust' style='text-size-adjust: 150%;'>"
      "  Text"
      "</div>");

  LayoutObject* textSizeAdjust =
      document().getElementById("textSizeAdjust")->layoutObject();
  EXPECT_FLOAT_EQ(16.f, textSizeAdjust->style()->specifiedFontSize());
  EXPECT_FLOAT_EQ(24.f, textSizeAdjust->style()->computedFontSize());
  EXPECT_FLOAT_EQ(1.5f,
                  textSizeAdjust->style()->getTextSizeAdjust().multiplier());
}

TEST_F(TextAutosizerTest, DeviceScaleAdjustmentWithViewport) {
  setBodyInnerHTML(
      "<meta name='viewport' content='width=800'>"
      "<style>"
      "  html { font-size: 16px; }"
      "  body { width: 800px; margin: 0; overflow-y: hidden; }"
      "</style>"
      "<div id='autosized'>"
      "  Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do"
      "  eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim"
      "  ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut"
      "  aliquip ex ea commodo consequat. Duis aute irure dolor in"
      "  reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla"
      "  pariatur. Excepteur sint occaecat cupidatat non proident, sunt in"
      "  culpa qui officia deserunt mollit anim id est laborum."
      "</div>");

  document().settings()->setViewportMetaEnabled(true);
  document().settings()->setDeviceScaleAdjustment(1.5f);
  document().view()->updateAllLifecyclePhases();

  Element* autosized = document().getElementById("autosized");
  EXPECT_FLOAT_EQ(16.f,
                  autosized->layoutObject()->style()->specifiedFontSize());
  // (specified font-size = 16px) * (viewport width = 800px) /
  // (window width = 320px) = 40px.
  // The device scale adjustment of 1.5 is ignored.
  EXPECT_FLOAT_EQ(40.f, autosized->layoutObject()->style()->computedFontSize());

  document().settings()->setViewportMetaEnabled(false);
  document().view()->updateAllLifecyclePhases();

  autosized = document().getElementById("autosized");
  EXPECT_FLOAT_EQ(16.f,
                  autosized->layoutObject()->style()->specifiedFontSize());
  // (device scale adjustment = 1.5) * (specified font-size = 16px) *
  // (viewport width = 800px) / (window width = 320px) = 60px.
  EXPECT_FLOAT_EQ(60.f, autosized->layoutObject()->style()->computedFontSize());
}

}  // namespace blink
