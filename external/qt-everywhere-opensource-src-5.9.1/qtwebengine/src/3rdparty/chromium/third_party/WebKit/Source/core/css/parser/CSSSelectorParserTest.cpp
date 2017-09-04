// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSSelectorParser.h"

#include "core/css/CSSSelectorList.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/parser/CSSTokenizer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

typedef struct {
  const char* input;
  const int a;
  const int b;
} ANPlusBTestCase;

TEST(CSSSelectorParserTest, ValidANPlusB) {
  ANPlusBTestCase testCases[] = {
      {"odd", 2, 1},
      {"OdD", 2, 1},
      {"even", 2, 0},
      {"EveN", 2, 0},
      {"0", 0, 0},
      {"8", 0, 8},
      {"+12", 0, 12},
      {"-14", 0, -14},

      {"0n", 0, 0},
      {"16N", 16, 0},
      {"-19n", -19, 0},
      {"+23n", 23, 0},
      {"n", 1, 0},
      {"N", 1, 0},
      {"+n", 1, 0},
      {"-n", -1, 0},
      {"-N", -1, 0},

      {"6n-3", 6, -3},
      {"-26N-33", -26, -33},
      {"n-18", 1, -18},
      {"+N-5", 1, -5},
      {"-n-7", -1, -7},

      {"0n+0", 0, 0},
      {"10n+5", 10, 5},
      {"10N +5", 10, 5},
      {"10n -5", 10, -5},
      {"N+6", 1, 6},
      {"n +6", 1, 6},
      {"+n -7", 1, -7},
      {"-N -8", -1, -8},
      {"-n+9", -1, 9},

      {"33N- 22", 33, -22},
      {"+n- 25", 1, -25},
      {"N- 46", 1, -46},
      {"n- 0", 1, 0},
      {"-N- 951", -1, -951},
      {"-n- 951", -1, -951},

      {"29N + 77", 29, 77},
      {"29n - 77", 29, -77},
      {"+n + 61", 1, 61},
      {"+N - 63", 1, -63},
      {"+n/**/- 48", 1, -48},
      {"-n + 81", -1, 81},
      {"-N - 88", -1, -88},
  };

  for (auto testCase : testCases) {
    SCOPED_TRACE(testCase.input);

    std::pair<int, int> ab;
    CSSTokenizer tokenizer(testCase.input);
    CSSParserTokenRange range = tokenizer.tokenRange();
    bool passed = CSSSelectorParser::consumeANPlusB(range, ab);
    EXPECT_TRUE(passed);
    EXPECT_EQ(ab.first, testCase.a);
    EXPECT_EQ(ab.second, testCase.b);
  }
}

TEST(CSSSelectorParserTest, InvalidANPlusB) {
  // Some of these have token range prefixes which are valid <an+b> and could
  // in theory be valid in consumeANPlusB, but this behaviour isn't needed
  // anywhere and not implemented.
  const char* testCases[] = {
      " odd",     "+ n",     "3m+4",  "12n--34",  "12n- -34",
      "12n- +34", "23n-+43", "10n 5", "10n + +5", "10n + -5",
  };

  for (auto testCase : testCases) {
    SCOPED_TRACE(testCase);

    std::pair<int, int> ab;
    CSSTokenizer tokenizer(testCase);
    CSSParserTokenRange range = tokenizer.tokenRange();
    bool passed = CSSSelectorParser::consumeANPlusB(range, ab);
    EXPECT_FALSE(passed);
  }
}

TEST(CSSSelectorParserTest, ShadowDomPseudoInCompound) {
  const char* testCases[][2] = {{"::shadow", "::shadow"},
                                {".a::shadow", ".a::shadow"},
                                {"::content", "::content"},
                                {".a::content", ".a::content"},
                                {"::content.a", "::content.a"},
                                {"::content.a.b", "::content.a.b"},
                                {".a::content.b", ".a::content.b"},
                                {"::content:not(#id)", "::content:not(#id)"}};

  for (auto testCase : testCases) {
    SCOPED_TRACE(testCase[0]);
    CSSTokenizer tokenizer(testCase[0]);
    CSSParserTokenRange range = tokenizer.tokenRange();
    CSSSelectorList list = CSSSelectorParser::parseSelector(
        range, CSSParserContext(HTMLStandardMode, nullptr), nullptr);
    EXPECT_STREQ(testCase[1], list.selectorsText().ascii().data());
  }
}

TEST(CSSSelectorParserTest, PseudoElementsInCompoundLists) {
  const char* testCases[] = {":not(::before)",
                             ":not(::content)",
                             ":not(::shadow)",
                             ":host(::before)",
                             ":host(::content)",
                             ":host(::shadow)",
                             ":host-context(::before)",
                             ":host-context(::content)",
                             ":host-context(::shadow)",
                             ":-webkit-any(::after, ::before)",
                             ":-webkit-any(::content, span)",
                             ":-webkit-any(div, ::shadow)"};

  for (auto testCase : testCases) {
    CSSTokenizer tokenizer(testCase);
    CSSParserTokenRange range = tokenizer.tokenRange();
    CSSSelectorList list = CSSSelectorParser::parseSelector(
        range, CSSParserContext(HTMLStandardMode, nullptr), nullptr);
    EXPECT_FALSE(list.isValid());
  }
}

TEST(CSSSelectorParserTest, ValidSimpleAfterPseudoElementInCompound) {
  const char* testCases[] = {
      "::-webkit-volume-slider:hover", "::selection:window-inactive",
      "::-webkit-scrollbar:disabled", "::-webkit-volume-slider:not(:hover)",
      "::-webkit-scrollbar:not(:horizontal)"};

  for (auto testCase : testCases) {
    CSSTokenizer tokenizer(testCase);
    CSSParserTokenRange range = tokenizer.tokenRange();
    CSSSelectorList list = CSSSelectorParser::parseSelector(
        range, CSSParserContext(HTMLStandardMode, nullptr), nullptr);
    EXPECT_TRUE(list.isValid());
  }
}

TEST(CSSSelectorParserTest, InvalidSimpleAfterPseudoElementInCompound) {
  const char* testCases[] = {
      "::before#id",
      "::after:hover",
      ".class::content::before",
      "::shadow.class",
      "::selection:window-inactive::before",
      "::-webkit-volume-slider.class",
      "::before:not(.a)",
      "::shadow:not(::after)",
      "::-webkit-scrollbar:vertical:not(:first-child)",
      "video::-webkit-media-text-track-region-container.scrolling",
      "div ::before.a"};

  for (auto testCase : testCases) {
    CSSTokenizer tokenizer(testCase);
    CSSParserTokenRange range = tokenizer.tokenRange();
    CSSSelectorList list = CSSSelectorParser::parseSelector(
        range, CSSParserContext(HTMLStandardMode, nullptr), nullptr);
    EXPECT_FALSE(list.isValid());
  }
}

TEST(CSSSelectorParserTest, WorkaroundForInvalidCustomPseudoInUAStyle) {
  // See crbug.com/578131
  const char* testCases[] = {
      "video::-webkit-media-text-track-region-container.scrolling",
      "input[type=\"range\" i]::-webkit-media-slider-container > div"};

  for (auto testCase : testCases) {
    CSSTokenizer tokenizer(testCase);
    CSSParserTokenRange range = tokenizer.tokenRange();
    CSSSelectorList list = CSSSelectorParser::parseSelector(
        range, CSSParserContext(UASheetMode, nullptr), nullptr);
    EXPECT_TRUE(list.isValid());
  }
}

TEST(CSSSelectorParserTest, ValidPseudoElementInNonRightmostCompound) {
  const char* testCases[] = {"::content *", "::shadow *",
                             "::content div::before",
                             "::shadow ::first-letter"};

  for (auto testCase : testCases) {
    CSSTokenizer tokenizer(testCase);
    CSSParserTokenRange range = tokenizer.tokenRange();
    CSSSelectorList list = CSSSelectorParser::parseSelector(
        range, CSSParserContext(HTMLStandardMode, nullptr), nullptr);
    EXPECT_TRUE(list.isValid());
  }
}

TEST(CSSSelectorParserTest, InvalidPseudoElementInNonRightmostCompound) {
  const char* testCases[] = {"::-webkit-volume-slider *", "::before *",
                             "::-webkit-scrollbar *", "::cue *",
                             "::selection *"};

  for (auto testCase : testCases) {
    CSSTokenizer tokenizer(testCase);
    CSSParserTokenRange range = tokenizer.tokenRange();
    CSSSelectorList list = CSSSelectorParser::parseSelector(
        range, CSSParserContext(HTMLStandardMode, nullptr), nullptr);
    EXPECT_FALSE(list.isValid());
  }
}

TEST(CSSSelectorParserTest, UnresolvedNamespacePrefix) {
  const char* testCases[] = {"ns|div", "div ns|div", "div ns|div "};

  CSSParserContext context(HTMLStandardMode, nullptr);
  StyleSheetContents* sheet = StyleSheetContents::create(context);

  for (auto testCase : testCases) {
    CSSTokenizer tokenizer(testCase);
    CSSParserTokenRange range = tokenizer.tokenRange();
    CSSSelectorList list =
        CSSSelectorParser::parseSelector(range, context, sheet);
    EXPECT_FALSE(list.isValid());
  }
}

TEST(CSSSelectorParserTest, SerializedUniversal) {
  const char* testCases[][2] = {
      {"*::-webkit-volume-slider", "::-webkit-volume-slider"},
      {"*::cue(i)", "::cue(i)"},
      {"*::shadow", "::shadow"},
      {"*:host-context(.x)", "*:host-context(.x)"},
      {"*:host", "*:host"},
      {"|*::-webkit-volume-slider", "|*::-webkit-volume-slider"},
      {"|*::cue(i)", "|*::cue(i)"},
      {"|*::shadow", "|*::shadow"},
      {"*|*::-webkit-volume-slider", "::-webkit-volume-slider"},
      {"*|*::cue(i)", "::cue(i)"},
      {"*|*::shadow", "::shadow"},
      {"ns|*::-webkit-volume-slider", "ns|*::-webkit-volume-slider"},
      {"ns|*::cue(i)", "ns|*::cue(i)"},
      {"ns|*::shadow", "ns|*::shadow"}};

  CSSParserContext context(HTMLStandardMode, nullptr);
  StyleSheetContents* sheet = StyleSheetContents::create(context);
  sheet->parserAddNamespace("ns", "http://ns.org");

  for (auto testCase : testCases) {
    SCOPED_TRACE(testCase[0]);
    CSSTokenizer tokenizer(testCase[0]);
    CSSParserTokenRange range = tokenizer.tokenRange();
    CSSSelectorList list =
        CSSSelectorParser::parseSelector(range, context, sheet);
    EXPECT_TRUE(list.isValid());
    EXPECT_STREQ(testCase[1], list.selectorsText().ascii().data());
  }
}

TEST(CSSSelectorParserTest, InvalidDescendantCombinatorInDynamicProfile) {
  const char* testCases[] = {"div >>>> span", "div >>> span", "div >> span"};

  CSSParserContext context(HTMLStandardMode, nullptr,
                           CSSParserContext::DynamicProfile);
  StyleSheetContents* sheet = StyleSheetContents::create(context);

  for (auto testCase : testCases) {
    SCOPED_TRACE(testCase);
    CSSTokenizer tokenizer(testCase);
    CSSParserTokenRange range = tokenizer.tokenRange();
    CSSSelectorList list =
        CSSSelectorParser::parseSelector(range, context, sheet);
    EXPECT_FALSE(list.isValid());
  }
}

TEST(CSSSelectorParserTest, InvalidDescendantCombinatorInStaticProfile) {
  const char* testCases[] = {"div >>>> span", "div >> span", "div >> > span",
                             "div > >> span", "div > > > span"};

  CSSParserContext context(HTMLStandardMode, nullptr,
                           CSSParserContext::StaticProfile);
  StyleSheetContents* sheet = StyleSheetContents::create(context);

  for (auto testCase : testCases) {
    SCOPED_TRACE(testCase);
    CSSTokenizer tokenizer(testCase);
    CSSParserTokenRange range = tokenizer.tokenRange();
    CSSSelectorList list =
        CSSSelectorParser::parseSelector(range, context, sheet);
    EXPECT_FALSE(list.isValid());
  }
}

TEST(CSSSelectorParserTest, ShadowPiercingCombinatorInStaticProfile) {
  const char* testCases[][2] = {{"div >>> span", "div >>> span"},
                                {"div >>/**/> span", "div >>> span"},
                                {"div >/**/>> span", "div >>> span"},
                                {"div >/**/>/**/> span", "div >>> span"}};

  CSSParserContext context(HTMLStandardMode, nullptr,
                           CSSParserContext::StaticProfile);
  StyleSheetContents* sheet = StyleSheetContents::create(context);

  for (auto testCase : testCases) {
    SCOPED_TRACE(testCase[0]);
    CSSTokenizer tokenizer(testCase[0]);
    CSSParserTokenRange range = tokenizer.tokenRange();
    CSSSelectorList list =
        CSSSelectorParser::parseSelector(range, context, sheet);
    EXPECT_TRUE(list.isValid());
    EXPECT_STREQ(testCase[1], list.selectorsText().ascii().data());
  }
}

}  // namespace blink
