// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/StyleSheetContents.h"

#include "core/css/CSSTestHelper.h"
#include "core/css/parser/CSSParser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(StyleSheetContentsTest, InsertMediaRule) {
  CSSParserContext context(HTMLStandardMode, nullptr);

  StyleSheetContents* styleSheet = StyleSheetContents::create(context);
  styleSheet->parseString("@namespace ns url(test);");
  EXPECT_EQ(1U, styleSheet->ruleCount());

  styleSheet->setMutable();
  styleSheet->wrapperInsertRule(
      CSSParser::parseRule(context, styleSheet,
                           "@media all { div { color: pink } }"),
      0);
  EXPECT_EQ(1U, styleSheet->ruleCount());
  EXPECT_TRUE(styleSheet->hasMediaQueries());

  styleSheet->wrapperInsertRule(
      CSSParser::parseRule(context, styleSheet,
                           "@media all { div { color: green } }"),
      1);
  EXPECT_EQ(2U, styleSheet->ruleCount());
  EXPECT_TRUE(styleSheet->hasMediaQueries());
}

TEST(StyleSheetContentsTest, InsertFontFaceRule) {
  CSSParserContext context(HTMLStandardMode, nullptr);

  StyleSheetContents* styleSheet = StyleSheetContents::create(context);
  styleSheet->parseString("@namespace ns url(test);");
  EXPECT_EQ(1U, styleSheet->ruleCount());

  styleSheet->setMutable();
  styleSheet->wrapperInsertRule(
      CSSParser::parseRule(context, styleSheet,
                           "@font-face { font-family: a }"),
      0);
  EXPECT_EQ(1U, styleSheet->ruleCount());
  EXPECT_TRUE(styleSheet->hasFontFaceRule());

  styleSheet->wrapperInsertRule(
      CSSParser::parseRule(context, styleSheet,
                           "@font-face { font-family: b }"),
      1);
  EXPECT_EQ(2U, styleSheet->ruleCount());
  EXPECT_TRUE(styleSheet->hasFontFaceRule());
}

TEST(StyleSheetContentsTest, HasViewportRule) {
  CSSParserContext context(HTMLStandardMode, nullptr);

  StyleSheetContents* styleSheet = StyleSheetContents::create(context);
  styleSheet->parseString("@viewport { width: 200px}");
  EXPECT_EQ(1U, styleSheet->ruleCount());
  EXPECT_TRUE(styleSheet->hasViewportRule());
}

TEST(StyleSheetContentsTest, HasViewportRuleAfterInsertion) {
  CSSParserContext context(HTMLStandardMode, nullptr);

  StyleSheetContents* styleSheet = StyleSheetContents::create(context);
  styleSheet->parseString("body { color: pink }");
  EXPECT_EQ(1U, styleSheet->ruleCount());
  EXPECT_FALSE(styleSheet->hasViewportRule());

  styleSheet->setMutable();
  styleSheet->wrapperInsertRule(
      CSSParser::parseRule(context, styleSheet, "@viewport { width: 200px }"),
      0);
  EXPECT_EQ(2U, styleSheet->ruleCount());
  EXPECT_TRUE(styleSheet->hasViewportRule());
}

TEST(StyleSheetContentsTest, HasViewportRuleAfterInsertionIntoMediaRule) {
  CSSParserContext context(HTMLStandardMode, nullptr);

  StyleSheetContents* styleSheet = StyleSheetContents::create(context);
  styleSheet->parseString("@media {}");
  ASSERT_EQ(1U, styleSheet->ruleCount());
  EXPECT_FALSE(styleSheet->hasViewportRule());

  StyleRuleMedia* mediaRule = toStyleRuleMedia(styleSheet->ruleAt(0));
  styleSheet->setMutable();
  mediaRule->wrapperInsertRule(
      0,
      CSSParser::parseRule(context, styleSheet, "@viewport { width: 200px }"));
  EXPECT_EQ(1U, mediaRule->childRules().size());
  EXPECT_TRUE(styleSheet->hasViewportRule());
}

}  // namespace blink
