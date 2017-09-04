// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/LayoutLocale.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(LayoutLocaleTest, Get) {
  LayoutLocale::clearForTesting();

  EXPECT_EQ(nullptr, LayoutLocale::get(nullAtom));

  EXPECT_EQ(emptyAtom, LayoutLocale::get(emptyAtom)->localeString());

  EXPECT_STRCASEEQ("en-us",
                   LayoutLocale::get("en-us")->localeString().ascii().data());
  EXPECT_STRCASEEQ("ja-jp",
                   LayoutLocale::get("ja-jp")->localeString().ascii().data());

  LayoutLocale::clearForTesting();
}

TEST(LayoutLocaleTest, GetCaseInsensitive) {
  const LayoutLocale* enUS = LayoutLocale::get("en-us");
  EXPECT_EQ(enUS, LayoutLocale::get("en-US"));
}

TEST(LayoutLocaleTest, ScriptTest) {
  // Test combinations of BCP 47 locales.
  // https://tools.ietf.org/html/bcp47
  struct {
    const char* locale;
    UScriptCode script;
    bool hasScriptForHan;
    UScriptCode scriptForHan;
  } tests[] = {
      {"en-US", USCRIPT_LATIN},

      // Common lang-script.
      {"en-Latn", USCRIPT_LATIN},
      {"ar-Arab", USCRIPT_ARABIC},

      // Common lang-region in East Asia.
      {"ja-JP", USCRIPT_KATAKANA_OR_HIRAGANA, true},
      {"ko-KR", USCRIPT_HANGUL, true},
      {"zh", USCRIPT_SIMPLIFIED_HAN, true},
      {"zh-CN", USCRIPT_SIMPLIFIED_HAN, true},
      {"zh-HK", USCRIPT_TRADITIONAL_HAN, true},
      {"zh-MO", USCRIPT_TRADITIONAL_HAN, true},
      {"zh-SG", USCRIPT_SIMPLIFIED_HAN, true},
      {"zh-TW", USCRIPT_TRADITIONAL_HAN, true},

      // Encompassed languages within the Chinese macrolanguage.
      // Both "lang" and "lang-extlang" should work.
      {"nan", USCRIPT_TRADITIONAL_HAN, true},
      {"wuu", USCRIPT_SIMPLIFIED_HAN, true},
      {"yue", USCRIPT_TRADITIONAL_HAN, true},
      {"zh-nan", USCRIPT_TRADITIONAL_HAN, true},
      {"zh-wuu", USCRIPT_SIMPLIFIED_HAN, true},
      {"zh-yue", USCRIPT_TRADITIONAL_HAN, true},

      // Script has priority over other subtags.
      {"zh-Hant", USCRIPT_TRADITIONAL_HAN, true},
      {"en-Hans", USCRIPT_SIMPLIFIED_HAN, true},
      {"en-Hant", USCRIPT_TRADITIONAL_HAN, true},
      {"en-Hans-TW", USCRIPT_SIMPLIFIED_HAN, true},
      {"en-Hant-CN", USCRIPT_TRADITIONAL_HAN, true},
      {"wuu-Hant", USCRIPT_TRADITIONAL_HAN, true},
      {"yue-Hans", USCRIPT_SIMPLIFIED_HAN, true},
      {"zh-wuu-Hant", USCRIPT_TRADITIONAL_HAN, true},
      {"zh-yue-Hans", USCRIPT_SIMPLIFIED_HAN, true},

      // Lang has priority over region.
      // icu::Locale::getDefault() returns other combinations if, for instnace,
      // English Windows with the display language set to Japanese.
      {"ja", USCRIPT_KATAKANA_OR_HIRAGANA, true},
      {"ja-US", USCRIPT_KATAKANA_OR_HIRAGANA, true},
      {"ko", USCRIPT_HANGUL, true},
      {"ko-US", USCRIPT_HANGUL, true},
      {"wuu-TW", USCRIPT_SIMPLIFIED_HAN, true},
      {"yue-CN", USCRIPT_TRADITIONAL_HAN, true},
      {"zh-wuu-TW", USCRIPT_SIMPLIFIED_HAN, true},
      {"zh-yue-CN", USCRIPT_TRADITIONAL_HAN, true},

      // Region should not affect script, but it can influence scriptForHan.
      {"en-CN", USCRIPT_LATIN, false},
      {"en-HK", USCRIPT_LATIN, true, USCRIPT_TRADITIONAL_HAN},
      {"en-MO", USCRIPT_LATIN, true, USCRIPT_TRADITIONAL_HAN},
      {"en-SG", USCRIPT_LATIN, false},
      {"en-TW", USCRIPT_LATIN, true, USCRIPT_TRADITIONAL_HAN},
      {"en-JP", USCRIPT_LATIN, true, USCRIPT_KATAKANA_OR_HIRAGANA},
      {"en-KR", USCRIPT_LATIN, true, USCRIPT_HANGUL},

      // Multiple regions are invalid, but it can still give hints for the font
      // selection.
      {"en-US-JP", USCRIPT_LATIN, true, USCRIPT_KATAKANA_OR_HIRAGANA},
  };

  for (const auto& test : tests) {
    RefPtr<LayoutLocale> locale = LayoutLocale::createForTesting(test.locale);
    EXPECT_EQ(test.script, locale->script()) << test.locale;
    EXPECT_EQ(test.hasScriptForHan, locale->hasScriptForHan()) << test.locale;
    if (!test.hasScriptForHan)
      EXPECT_EQ(USCRIPT_SIMPLIFIED_HAN, locale->scriptForHan()) << test.locale;
    else if (test.scriptForHan)
      EXPECT_EQ(test.scriptForHan, locale->scriptForHan()) << test.locale;
    else
      EXPECT_EQ(test.script, locale->scriptForHan()) << test.locale;
  }
}

}  // namespace blink
