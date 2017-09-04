// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSStyleSheet.h"
#include "core/css/StyleRule.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/parser/CSSParser.h"
#include "core/css/parser/CSSParserMode.h"
#include "core/frame/FrameHost.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/heap/Heap.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/WTFString.h"

namespace blink {

class CSSLazyParsingTest : public testing::Test {
 public:
  bool hasParsedProperties(StyleRule* rule) {
    return rule->hasParsedProperties();
  }

  StyleRule* ruleAt(StyleSheetContents* sheet, size_t index) {
    return toStyleRule(sheet->childRules()[index]);
  }
};

TEST_F(CSSLazyParsingTest, Simple) {
  CSSParserContext context(HTMLStandardMode, nullptr);
  StyleSheetContents* styleSheet = StyleSheetContents::create(context);

  String sheetText = "body { background-color: red; }";
  CSSParser::parseSheet(context, styleSheet, sheetText, true /* lazy parse */);
  StyleRule* rule = ruleAt(styleSheet, 0);
  EXPECT_FALSE(hasParsedProperties(rule));
  rule->properties();
  EXPECT_TRUE(hasParsedProperties(rule));
}

// Avoiding lazy parsing for trivially empty blocks helps us perform the
// shouldConsiderForMatchingRules optimization.
TEST_F(CSSLazyParsingTest, DontLazyParseEmpty) {
  CSSParserContext context(HTMLStandardMode, nullptr);
  StyleSheetContents* styleSheet = StyleSheetContents::create(context);

  String sheetText = "body {  }";
  CSSParser::parseSheet(context, styleSheet, sheetText, true /* lazy parse */);
  StyleRule* rule = ruleAt(styleSheet, 0);
  EXPECT_TRUE(hasParsedProperties(rule));
  EXPECT_FALSE(
      rule->shouldConsiderForMatchingRules(false /* includeEmptyRules */));
}

// Avoid parsing rules with ::before or ::after to avoid causing
// collectFeatures() when we trigger parsing for attr();
TEST_F(CSSLazyParsingTest, DontLazyParseBeforeAfter) {
  CSSParserContext context(HTMLStandardMode, nullptr);
  StyleSheetContents* styleSheet = StyleSheetContents::create(context);

  String sheetText =
      "p::before { content: 'foo' } p .class::after { content: 'bar' } ";
  CSSParser::parseSheet(context, styleSheet, sheetText, true /* lazy parse */);

  EXPECT_TRUE(hasParsedProperties(ruleAt(styleSheet, 0)));
  EXPECT_TRUE(hasParsedProperties(ruleAt(styleSheet, 1)));
}

// Test for crbug.com/664115 where |shouldConsiderForMatchingRules| would flip
// from returning false to true if the lazy property was parsed. This is a
// dangerous API because callers will expect the set of matching rules to be
// identical if the stylesheet is not mutated.
TEST_F(CSSLazyParsingTest, ShouldConsiderForMatchingRulesDoesntChange1) {
  CSSParserContext context(HTMLStandardMode, nullptr);
  StyleSheetContents* styleSheet = StyleSheetContents::create(context);

  String sheetText = "p::first-letter { ,badness, } ";
  CSSParser::parseSheet(context, styleSheet, sheetText, true /* lazy parse */);

  StyleRule* rule = ruleAt(styleSheet, 0);
  EXPECT_FALSE(hasParsedProperties(rule));
  EXPECT_TRUE(
      rule->shouldConsiderForMatchingRules(false /* includeEmptyRules */));

  // Parse the rule.
  rule->properties();

  // Now, we should still consider this for matching rules even if it is empty.
  EXPECT_TRUE(hasParsedProperties(rule));
  EXPECT_TRUE(
      rule->shouldConsiderForMatchingRules(false /* includeEmptyRules */));
}

// Test the same thing as above, with a property that does not get lazy parsed,
// to ensure that we perform the optimization where possible.
TEST_F(CSSLazyParsingTest, ShouldConsiderForMatchingRulesSimple) {
  CSSParserContext context(HTMLStandardMode, nullptr);
  StyleSheetContents* styleSheet = StyleSheetContents::create(context);

  String sheetText = "p::before { ,badness, } ";
  CSSParser::parseSheet(context, styleSheet, sheetText, true /* lazy parse */);

  StyleRule* rule = ruleAt(styleSheet, 0);
  EXPECT_TRUE(hasParsedProperties(rule));
  EXPECT_FALSE(
      rule->shouldConsiderForMatchingRules(false /* includeEmptyRules */));
}

// Regression test for crbug.com/660290 where we change the underlying owning
// document from the StyleSheetContents without changing the UseCounter. This
// test ensures that the new UseCounter is used when doing new parsing work.
TEST_F(CSSLazyParsingTest, ChangeDocuments) {
  std::unique_ptr<DummyPageHolder> dummyHolder =
      DummyPageHolder::create(IntSize(500, 500));
  CSSParserContext context(HTMLStandardMode,
                           UseCounter::getFrom(&dummyHolder->document()));
  StyleSheetContents* styleSheet = StyleSheetContents::create(context);
  CSSStyleSheet* sheet =
      CSSStyleSheet::create(styleSheet, dummyHolder->document());
  DCHECK(sheet);

  String sheetText = "body { background-color: red; } p { color: orange;  }";
  CSSParser::parseSheet(context, styleSheet, sheetText, true /* lazy parse */);

  // Parse the first property set with the first document as owner.
  StyleRule* rule = ruleAt(styleSheet, 0);
  EXPECT_FALSE(hasParsedProperties(rule));
  rule->properties();
  EXPECT_TRUE(hasParsedProperties(rule));

  EXPECT_EQ(&dummyHolder->document(), styleSheet->singleOwnerDocument());

  // Change owner document.
  styleSheet->unregisterClient(sheet);
  std::unique_ptr<DummyPageHolder> dummyHolder2 =
      DummyPageHolder::create(IntSize(500, 500));
  CSSStyleSheet::create(styleSheet, dummyHolder2->document());

  EXPECT_EQ(&dummyHolder2->document(), styleSheet->singleOwnerDocument());

  // Parse the second property set with the second document as owner.
  StyleRule* rule2 = ruleAt(styleSheet, 1);
  EXPECT_FALSE(hasParsedProperties(rule2));
  rule2->properties();
  EXPECT_TRUE(hasParsedProperties(rule2));

  UseCounter& useCounter1 = dummyHolder->document().frameHost()->useCounter();
  UseCounter& useCounter2 = dummyHolder2->document().frameHost()->useCounter();
  EXPECT_TRUE(useCounter1.isCounted(CSSPropertyBackgroundColor));
  EXPECT_TRUE(useCounter2.isCounted(CSSPropertyColor));
  EXPECT_FALSE(useCounter2.isCounted(CSSPropertyBackgroundColor));
  EXPECT_FALSE(useCounter1.isCounted(CSSPropertyColor));
}

}  // namespace blink
