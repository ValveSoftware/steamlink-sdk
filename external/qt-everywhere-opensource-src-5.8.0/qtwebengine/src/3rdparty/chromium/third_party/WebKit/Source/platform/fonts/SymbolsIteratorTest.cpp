// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/SymbolsIterator.h"

#include "platform/Logging.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <string>

namespace blink {

struct TestRun {
    std::string text;
    FontFallbackPriority fontFallbackPriority;
};

struct ExpectedRun {
    unsigned limit;
    FontFallbackPriority fontFallbackPriority;

    ExpectedRun(unsigned theLimit, FontFallbackPriority theFontFallbackPriority)
        : limit(theLimit)
        , fontFallbackPriority(theFontFallbackPriority)
    {
    }
};

class SymbolsIteratorTest : public testing::Test {
protected:
    void CheckRuns(const Vector<TestRun>& runs)
    {
        String text(emptyString16Bit());
        Vector<ExpectedRun> expect;
        for (auto& run : runs) {
            text.append(String::fromUTF8(run.text.c_str()));
            expect.append(ExpectedRun(text.length(), run.fontFallbackPriority));
        }
        SymbolsIterator symbolsIterator(text.characters16(), text.length());
        VerifyRuns(&symbolsIterator, expect);
    }

    void VerifyRuns(SymbolsIterator* symbolsIterator,
        const Vector<ExpectedRun>& expect)
    {
        unsigned limit;
        FontFallbackPriority fontFallbackPriority;
        unsigned long runCount = 0;
        while (symbolsIterator->consume(&limit, &fontFallbackPriority)) {
            ASSERT_LT(runCount, expect.size());
            ASSERT_EQ(expect[runCount].limit, limit);
            ASSERT_EQ(expect[runCount].fontFallbackPriority, fontFallbackPriority);
            ++runCount;
        }
        ASSERT_EQ(expect.size(), runCount);
    }
};

// Some of our compilers cannot initialize a vector from an array yet.
#define DECLARE_RUNSVECTOR(...)                     \
    static const TestRun runsArray[] = __VA_ARGS__; \
    Vector<TestRun> runs; \
    runs.append(runsArray, sizeof(runsArray) / sizeof(*runsArray));

#define CHECK_RUNS(...)              \
    DECLARE_RUNSVECTOR(__VA_ARGS__); \
    CheckRuns(runs);


TEST_F(SymbolsIteratorTest, Empty)
{
    String empty(emptyString16Bit());
    SymbolsIterator symbolsIterator(empty.characters16(), empty.length());
    unsigned limit = 0;
    FontFallbackPriority symbolsFont = FontFallbackPriority::Invalid;
    ASSERT(!symbolsIterator.consume(&limit, &symbolsFont));
    ASSERT_EQ(limit, 0u);
    ASSERT_EQ(symbolsFont, FontFallbackPriority::Invalid);
}

TEST_F(SymbolsIteratorTest, Space)
{
    CHECK_RUNS({ { " ", FontFallbackPriority::Text } });
}

TEST_F(SymbolsIteratorTest, Latin)
{
    CHECK_RUNS({ { "Aa", FontFallbackPriority::Text } });
}

TEST_F(SymbolsIteratorTest, LatinColorEmojiTextEmoji)
{
    CHECK_RUNS({ { "a", FontFallbackPriority::Text },
        { "⌚", FontFallbackPriority::EmojiEmoji },
        { "☎", FontFallbackPriority::EmojiText } });
}

TEST_F(SymbolsIteratorTest, IgnoreVSInMath)
{
    CHECK_RUNS({ { "⊆⊇⊈\xEF\xB8\x8E⊙⊚⊚", FontFallbackPriority::Math } });
}

TEST_F(SymbolsIteratorTest, IgnoreVS15InText)
{
    CHECK_RUNS({ { "abcdef\xEF\xB8\x8Eghji", FontFallbackPriority::Text } });
}

TEST_F(SymbolsIteratorTest, IgnoreVS16InText)
{
    CHECK_RUNS({ { "abcdef\xEF\xB8\x8Fghji", FontFallbackPriority::Text } });
}

TEST_F(SymbolsIteratorTest, NumbersAndHashNormalAndEmoji)
{
    CHECK_RUNS({ { "0123456789#", FontFallbackPriority::Text },
        { "0⃣1⃣2⃣3⃣4⃣5⃣6⃣7⃣8⃣9⃣#⃣", FontFallbackPriority::EmojiEmoji },
        { "0123456789#", FontFallbackPriority::Text } });
}


TEST_F(SymbolsIteratorTest, SingleFlag)
{
    CHECK_RUNS({ { "🇺", FontFallbackPriority::Text } });
}

// TODO: Perhaps check for invalid country indicator combinations?

TEST_F(SymbolsIteratorTest, FlagsVsNonFlags)
{
    CHECK_RUNS({ { "🇺🇸🇸", FontFallbackPriority::EmojiEmoji }, // "US"
        { "abc", FontFallbackPriority::Text },
        { "🇺🇸", FontFallbackPriority::EmojiEmoji },
        { "a🇿", FontFallbackPriority::Text } });
}


TEST_F(SymbolsIteratorTest, EmojiVS15)
{
    // A VS15 after the anchor must trigger text display.
    CHECK_RUNS({ { "⚓\xEF\xB8\x8E", FontFallbackPriority::EmojiText },
        { "⛵", FontFallbackPriority::EmojiEmoji } });
}

TEST_F(SymbolsIteratorTest, EmojiZWSSequences)
{
    CHECK_RUNS({ { "👩‍👩‍👧‍👦👩‍❤️‍💋‍👨", FontFallbackPriority::EmojiEmoji },
        { "abcd", FontFallbackPriority::Text },
        { "👩‍👩‍", FontFallbackPriority::EmojiEmoji },
        { "efgh", FontFallbackPriority::Text } });
}

TEST_F(SymbolsIteratorTest, AllEmojiZWSSequences)
{
    CHECK_RUNS({ { "💏👩‍❤️‍💋‍👨👨‍❤️‍💋‍👨👩‍❤️‍💋‍👩💑👩‍❤️‍👨👨‍❤️‍👨👩‍❤️"
        "‍👩👪👨‍👩‍👦👨‍👩‍👧👨‍👩‍👧‍👦👨‍👩‍👦‍👦👨‍👩‍👧‍👧👨‍👨‍👦👨‍👨‍👧👨‍👨‍👧‍👦👨‍👨‍👦‍👦👨‍👨‍👧‍👧"
        "👩‍👩‍👦👩‍👩‍👧👩‍👩‍👧‍👦👩‍👩‍👦‍👦👩‍👩‍👧‍👧👁‍🗨",
        FontFallbackPriority::EmojiEmoji } });
}

TEST_F(SymbolsIteratorTest, EyeSpeechBubble)
{
    CHECK_RUNS({ { "👁‍🗨", FontFallbackPriority::EmojiEmoji } });
}

TEST_F(SymbolsIteratorTest, Modifier)
{
    CHECK_RUNS({ { "👶🏿", FontFallbackPriority::EmojiEmoji } });
}

TEST_F(SymbolsIteratorTest, DingbatsMiscSymbolsModifier)
{
    CHECK_RUNS({ { "⛹🏻✍🏻✊🏼", FontFallbackPriority::EmojiEmoji } });
}

TEST_F(SymbolsIteratorTest, ExtraZWJPrefix)
{
    CHECK_RUNS({ { "\xE2\x80\x8D", FontFallbackPriority::Text },
        { "\xF0\x9F\x91\xA9\xE2\x80\x8D\xE2"
        "\x9D\xA4\xEF\xB8\x8F\xE2\x80\x8D"
        "\xF0\x9F\x92\x8B\xE2\x80\x8D\xF0\x9F\x91\xA8",
        FontFallbackPriority::EmojiEmoji } });
}

TEST_F(SymbolsIteratorTest, Arrows)
{
    CHECK_RUNS({ { "x", FontFallbackPriority::Text },
        { "→←", FontFallbackPriority::Math },
        { "x", FontFallbackPriority::Text },
        { "←↑↓→", FontFallbackPriority::Math } });
}

} // namespace blink
