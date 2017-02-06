// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutTestHelper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class TextAutosizerTest : public RenderingTest {
private:
    void SetUp() override
    {
        RenderingTest::SetUp();
        document().settings()->setTextAutosizingEnabled(true);
        document().settings()->setTextAutosizingWindowSizeOverride(IntSize(320, 480));
    }
};

TEST_F(TextAutosizerTest, SimpleParagraph)
{
    setBodyInnerHTML(
        "<meta name='viewport' content='width=800'>"
        "<style>"
        "    html { font-size: 16px; }"
        "    body { width: 800px; margin: 0; overflow-y: hidden; }"
        "</style>"
        "<div id='autosized'>"
        "    Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod tempor"
        "    incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud"
        "    exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure"
        "    dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur."
        "    Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt"
        "    mollit anim id est laborum."
        "</div>");
    Element* autosized = document().getElementById("autosized");
    EXPECT_FLOAT_EQ(16.f, autosized->layoutObject()->style()->specifiedFontSize());
    // (specified font-size = 16px) * (viewport width = 800px) / (window width = 320px) = 40px.
    EXPECT_FLOAT_EQ(40.f, autosized->layoutObject()->style()->computedFontSize());
}

TEST_F(TextAutosizerTest, DISABLED_TextSizeAdjustDisablesAutosizing)
{
    setBodyInnerHTML(
        "<meta name='viewport' content='width=800'>"
        "<style>"
        "    html { font-size: 16px; }"
        "    body { width: 800px; margin: 0; overflow-y: hidden; }"
        "</style>"
        "<div id='textSizeAdjustAuto' style='text-size-adjust: auto;'>"
        "    Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod tempor"
        "    incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud"
        "    exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure"
        "    dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur."
        "    Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt"
        "    mollit anim id est laborum."
        "</div>"
        "<div id='textSizeAdjustNone' style='text-size-adjust: none;'>"
        "    Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod tempor"
        "    incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud"
        "    exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure"
        "    dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur."
        "    Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt"
        "    mollit anim id est laborum."
        "</div>"
        "<div id='textSizeAdjust100' style='text-size-adjust: 100%;'>"
        "    Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod tempor"
        "    incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud"
        "    exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure"
        "    dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur."
        "    Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt"
        "    mollit anim id est laborum."
        "</div>");
    LayoutObject* textSizeAdjustAuto = document().getElementById("textSizeAdjustAuto")->layoutObject();
    EXPECT_FLOAT_EQ(16.f, textSizeAdjustAuto->style()->specifiedFontSize());
    EXPECT_FLOAT_EQ(40.f, textSizeAdjustAuto->style()->computedFontSize());
    LayoutObject* textSizeAdjustNone = document().getElementById("textSizeAdjustNone")->layoutObject();
    EXPECT_FLOAT_EQ(16.f, textSizeAdjustNone->style()->specifiedFontSize());
    EXPECT_FLOAT_EQ(16.f, textSizeAdjustNone->style()->computedFontSize());
    LayoutObject* textSizeAdjust100 = document().getElementById("textSizeAdjust100")->layoutObject();
    EXPECT_FLOAT_EQ(16.f, textSizeAdjust100->style()->specifiedFontSize());
    EXPECT_FLOAT_EQ(16.f, textSizeAdjust100->style()->computedFontSize());
}

TEST_F(TextAutosizerTest, DISABLED_ParagraphWithChangingTextSizeAdjustment)
{
    setBodyInnerHTML(
        "<meta name='viewport' content='width=800'>"
        "<style>"
        "    html { font-size: 16px; }"
        "    body { width: 800px; margin: 0; overflow-y: hidden; }"
        "    .none { text-size-adjust: none; }"
        "    .small { text-size-adjust: 50%; }"
        "    .large { text-size-adjust: 150%; }"
        "</style>"
        "<div id='autosized'>"
        "    Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod tempor"
        "    incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud"
        "    exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure"
        "    dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur."
        "    Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt"
        "    mollit anim id est laborum."
        "</div>");
    Element* autosizedDiv = document().getElementById("autosized");
    EXPECT_FLOAT_EQ(16.f, autosizedDiv->layoutObject()->style()->specifiedFontSize());
    EXPECT_FLOAT_EQ(40.f, autosizedDiv->layoutObject()->style()->computedFontSize());

    autosizedDiv->setAttribute(HTMLNames::classAttr, "none");
    document().view()->updateAllLifecyclePhases();
    EXPECT_FLOAT_EQ(16.f, autosizedDiv->layoutObject()->style()->specifiedFontSize());
    EXPECT_FLOAT_EQ(16.f, autosizedDiv->layoutObject()->style()->computedFontSize());

    autosizedDiv->setAttribute(HTMLNames::classAttr, "small");
    document().view()->updateAllLifecyclePhases();
    EXPECT_FLOAT_EQ(16.f, autosizedDiv->layoutObject()->style()->specifiedFontSize());
    EXPECT_FLOAT_EQ(8.f, autosizedDiv->layoutObject()->style()->computedFontSize());

    autosizedDiv->setAttribute(HTMLNames::classAttr, "large");
    document().view()->updateAllLifecyclePhases();
    EXPECT_FLOAT_EQ(16.f, autosizedDiv->layoutObject()->style()->specifiedFontSize());
    EXPECT_FLOAT_EQ(24.f, autosizedDiv->layoutObject()->style()->computedFontSize());

    autosizedDiv->removeAttribute(HTMLNames::classAttr);
    document().view()->updateAllLifecyclePhases();
    EXPECT_FLOAT_EQ(16.f, autosizedDiv->layoutObject()->style()->specifiedFontSize());
    EXPECT_FLOAT_EQ(40.f, autosizedDiv->layoutObject()->style()->computedFontSize());
}

TEST_F(TextAutosizerTest, DISABLED_ZeroTextSizeAdjustment)
{
    setBodyInnerHTML(
        "<meta name='viewport' content='width=800'>"
        "<style>"
        "    html { font-size: 16px; }"
        "    body { width: 800px; margin: 0; overflow-y: hidden; }"
        "</style>"
        "<div id='textSizeAdjustZero' style='text-size-adjust: 0%;'>"
        "    Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod tempor"
        "    incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud"
        "    exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure"
        "    dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur."
        "    Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt"
        "    mollit anim id est laborum."
        "</div>");
    LayoutObject* textSizeAdjustZero = document().getElementById("textSizeAdjustZero")->layoutObject();
    EXPECT_FLOAT_EQ(16.f, textSizeAdjustZero->style()->specifiedFontSize());
    EXPECT_FLOAT_EQ(0.f, textSizeAdjustZero->style()->computedFontSize());
}

TEST_F(TextAutosizerTest, NegativeTextSizeAdjustment)
{
    setBodyInnerHTML(
        "<meta name='viewport' content='width=800'>"
        "<style>"
        "    html { font-size: 16px; }"
        "    body { width: 800px; margin: 0; overflow-y: hidden; }"
        "</style>"
        // Negative values should be treated as auto.
        "<div id='textSizeAdjustNegative' style='text-size-adjust: -10%;'>"
        "    Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod tempor"
        "    incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud"
        "    exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure"
        "    dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur."
        "    Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt"
        "    mollit anim id est laborum."
        "</div>");
    LayoutObject* textSizeAdjustNegative = document().getElementById("textSizeAdjustNegative")->layoutObject();
    EXPECT_FLOAT_EQ(16.f, textSizeAdjustNegative->style()->specifiedFontSize());
    EXPECT_FLOAT_EQ(40.f, textSizeAdjustNegative->style()->computedFontSize());
}

TEST_F(TextAutosizerTest, TextSizeAdjustmentPixelUnits)
{
    setBodyInnerHTML(
        "<meta name='viewport' content='width=800'>"
        "<style>"
        "    html { font-size: 16px; }"
        "    body { width: 800px; margin: 0; overflow-y: hidden; }"
        "</style>"
        // Non-percentage values should be treated as auto.
        "<div id='textSizeAdjustPixels' style='text-size-adjust: 0.1px;'>"
        "    Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod tempor"
        "    incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud"
        "    exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure"
        "    dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur."
        "    Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt"
        "    mollit anim id est laborum."
        "</div>");
    LayoutObject* textSizeAdjustPixels = document().getElementById("textSizeAdjustPixels")->layoutObject();
    EXPECT_FLOAT_EQ(16.f, textSizeAdjustPixels->style()->specifiedFontSize());
    EXPECT_FLOAT_EQ(40.f, textSizeAdjustPixels->style()->computedFontSize());
}

TEST_F(TextAutosizerTest, DISABLED_NestedTextSizeAdjust)
{
    setBodyInnerHTML(
        "<meta name='viewport' content='width=800'>"
        "<style>"
        "    html { font-size: 16px; }"
        "    body { width: 800px; margin: 0; overflow-y: hidden; }"
        "</style>"
        "<div id='textSizeAdjustA' style='text-size-adjust: 47%;'>"
        "    Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod tempor"
        "    incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud"
        "    exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure"
        "    dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur."
        "    Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt"
        "    mollit anim id est laborum."
        "    <div id='textSizeAdjustB' style='text-size-adjust: 53%;'>"
        "        Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod tempor"
        "        incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud"
        "        exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute"
        "        irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla"
        "        pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui"
        "        officia deserunt mollit anim id est laborum."
        "    </div>"
        "</div>");
    LayoutObject* textSizeAdjustA = document().getElementById("textSizeAdjustA")->layoutObject();
    EXPECT_FLOAT_EQ(16.f, textSizeAdjustA->style()->specifiedFontSize());
    // 16px * 47% = 7.52
    EXPECT_FLOAT_EQ(7.52f, textSizeAdjustA->style()->computedFontSize());
    LayoutObject* textSizeAdjustB = document().getElementById("textSizeAdjustB")->layoutObject();
    EXPECT_FLOAT_EQ(16.f, textSizeAdjustB->style()->specifiedFontSize());
    // 16px * 53% = 8.48
    EXPECT_FLOAT_EQ(8.48f, textSizeAdjustB->style()->computedFontSize());
}

TEST_F(TextAutosizerTest, DISABLED_PrefixedTextSizeAdjustIsAlias)
{
    setBodyInnerHTML(
        "<meta name='viewport' content='width=800'>"
        "<style>"
        "    html { font-size: 16px; }"
        "    body { width: 800px; margin: 0; overflow-y: hidden; }"
        "</style>"
        "<div id='textSizeAdjust' style='-webkit-text-size-adjust: 50%;'>"
        "    Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod tempor"
        "    incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud"
        "    exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure"
        "    dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur."
        "    Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt"
        "    mollit anim id est laborum."
        "</div>");
    LayoutObject* textSizeAdjust = document().getElementById("textSizeAdjust")->layoutObject();
    EXPECT_FLOAT_EQ(16.f, textSizeAdjust->style()->specifiedFontSize());
    EXPECT_FLOAT_EQ(8.f, textSizeAdjust->style()->computedFontSize());
    EXPECT_FLOAT_EQ(.5f, textSizeAdjust->style()->getTextSizeAdjust().multiplier());
}

} // namespace blink
