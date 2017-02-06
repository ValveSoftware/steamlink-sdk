// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/text/LocaleToScriptMapping.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/WTFString.h"

namespace blink {

TEST(LocaleToScriptMappingTest, scriptCodeForHanFromLocaleTest)
{
    struct {
        const char* locale;
        UScriptCode script;
    } tests[] = {
        { "ja-JP", USCRIPT_KATAKANA_OR_HIRAGANA },
        { "ko-KR", USCRIPT_HANGUL },
        { "zh-CN", USCRIPT_SIMPLIFIED_HAN },
        { "zh-TW", USCRIPT_TRADITIONAL_HAN },
        { "zh-HK", USCRIPT_TRADITIONAL_HAN },

        // icu::Locale::getDefault() returns other combinations if, for instnace,
        // English Windows with the display language set to Japanese.
        { "ja", USCRIPT_KATAKANA_OR_HIRAGANA },
        { "ja-US", USCRIPT_KATAKANA_OR_HIRAGANA },
        { "ko", USCRIPT_HANGUL },
        { "ko-US", USCRIPT_HANGUL },

        { "zh-hant", USCRIPT_TRADITIONAL_HAN },

        { "en-CN", USCRIPT_SIMPLIFIED_HAN },
        { "en-JP", USCRIPT_KATAKANA_OR_HIRAGANA },
        { "en-KR", USCRIPT_HANGUL },
        { "en-HK", USCRIPT_TRADITIONAL_HAN },
        { "en-TW", USCRIPT_TRADITIONAL_HAN },

        { "en-HanS", USCRIPT_SIMPLIFIED_HAN },
        { "en-HanT", USCRIPT_TRADITIONAL_HAN },

        { "en-HanS-JP", USCRIPT_SIMPLIFIED_HAN },
        { "en-HanT-JP", USCRIPT_TRADITIONAL_HAN },
        { "en-US-JP", USCRIPT_KATAKANA_OR_HIRAGANA },

        { "en-US", USCRIPT_COMMON },
    };

    for (auto test : tests) {
        String str(test.locale);
        EXPECT_EQ(test.script, scriptCodeForHanFromLocale(str));

        str = str.replace('-', '_');
        EXPECT_EQ(test.script, scriptCodeForHanFromLocale(str, '_'));
    }
}

} // namespace blink
