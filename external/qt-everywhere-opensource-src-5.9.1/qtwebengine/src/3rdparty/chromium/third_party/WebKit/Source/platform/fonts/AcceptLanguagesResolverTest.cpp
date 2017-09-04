// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/AcceptLanguagesResolver.h"

#include "platform/LayoutLocale.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(AcceptLanguagesResolverTest, AcceptLanguagesChanged) {
  struct {
    const char* acceptLanguages;
    UScriptCode script;
    const char* locale;
  } tests[] = {
      // Non-Han script cases.
      {nullptr, USCRIPT_COMMON, nullptr},
      {"", USCRIPT_COMMON, nullptr},
      {"en-US", USCRIPT_COMMON, nullptr},
      {",en-US", USCRIPT_COMMON, nullptr},

      // Single value cases.
      {"ja-JP", USCRIPT_KATAKANA_OR_HIRAGANA, "ja-jp"},
      {"ko-KR", USCRIPT_HANGUL, "ko-kr"},
      {"zh-CN", USCRIPT_SIMPLIFIED_HAN, "zh-Hans"},
      {"zh-HK", USCRIPT_TRADITIONAL_HAN, "zh-Hant"},
      {"zh-TW", USCRIPT_TRADITIONAL_HAN, "zh-Hant"},

      // Language only.
      {"ja", USCRIPT_KATAKANA_OR_HIRAGANA, "ja-jp"},
      {"ko", USCRIPT_HANGUL, "ko-kr"},
      {"zh", USCRIPT_SIMPLIFIED_HAN, "zh-Hans"},

      // Unusual combinations.
      {"en-JP", USCRIPT_KATAKANA_OR_HIRAGANA, "ja-jp"},

      // Han scripts not in the first item.
      {"en-US,ja-JP", USCRIPT_KATAKANA_OR_HIRAGANA, "ja-jp"},
      {"en-US,en-JP", USCRIPT_KATAKANA_OR_HIRAGANA, "ja-jp"},

      // Multiple Han scripts. The first one wins.
      {"ja-JP,zh-CN", USCRIPT_KATAKANA_OR_HIRAGANA, "ja-jp"},
      {"zh-TW,ja-JP", USCRIPT_TRADITIONAL_HAN, "zh-Hant"},
  };

  for (const auto& test : tests) {
    const LayoutLocale* locale =
        AcceptLanguagesResolver::localeForHanFromAcceptLanguages(
            test.acceptLanguages);

    if (test.script == USCRIPT_COMMON) {
      EXPECT_EQ(nullptr, locale) << test.acceptLanguages;
      continue;
    }

    ASSERT_NE(nullptr, locale) << test.acceptLanguages;
    EXPECT_EQ(test.script, locale->scriptForHan()) << test.acceptLanguages;
    EXPECT_STRCASEEQ(test.locale, locale->localeForHanForSkFontMgr())
        << test.acceptLanguages;
  }
}

}  // namespace blink
