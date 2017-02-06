// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/shaping/RunSegmenter.h"

#include "platform/Logging.h"
#include "platform/fonts/OrientationIterator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/Assertions.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"
#include <string>

namespace blink {


struct SegmenterTestRun {
    std::string text;
    UScriptCode script;
    OrientationIterator::RenderOrientation renderOrientation;
    FontFallbackPriority fontFallbackPriority;
};

struct SegmenterExpectedRun {
    unsigned start;
    unsigned limit;
    UScriptCode script;
    OrientationIterator::RenderOrientation renderOrientation;
    FontFallbackPriority fontFallbackPriority;

    SegmenterExpectedRun(unsigned theStart,
        unsigned theLimit,
        UScriptCode theScript,
        OrientationIterator::RenderOrientation theRenderOrientation,
        FontFallbackPriority theFontFallbackPriority)
        : start(theStart)
        , limit(theLimit)
        , script(theScript)
        , renderOrientation(theRenderOrientation)
        , fontFallbackPriority(theFontFallbackPriority)
    {
    }
};

class RunSegmenterTest : public testing::Test {
protected:
    void CheckRuns(const Vector<SegmenterTestRun>& runs, FontOrientation orientation)
    {
        String text(emptyString16Bit());
        Vector<SegmenterExpectedRun> expect;
        for (auto& run : runs) {
            unsigned lengthBefore = text.length();
            text.append(String::fromUTF8(run.text.c_str()));
            expect.append(SegmenterExpectedRun(lengthBefore, text.length(), run.script, run.renderOrientation, run.fontFallbackPriority));
        }
        RunSegmenter runSegmenter(text.characters16(), text.length(), orientation);
        VerifyRuns(&runSegmenter, expect);
    }

    void VerifyRuns(RunSegmenter* runSegmenter,
        const Vector<SegmenterExpectedRun>& expect)
    {
        RunSegmenter::RunSegmenterRange segmenterRange;
        unsigned long runCount = 0;
        while (runSegmenter->consume(&segmenterRange)) {
            ASSERT_LT(runCount, expect.size());
            ASSERT_EQ(expect[runCount].start, segmenterRange.start);
            ASSERT_EQ(expect[runCount].limit, segmenterRange.end);
            ASSERT_EQ(expect[runCount].script, segmenterRange.script);
            ASSERT_EQ(expect[runCount].renderOrientation, segmenterRange.renderOrientation);
            ASSERT_EQ(expect[runCount].fontFallbackPriority, segmenterRange.fontFallbackPriority);
            ++runCount;
        }
        ASSERT_EQ(expect.size(), runCount);
    }
};

// Some of our compilers cannot initialize a vector from an array yet.
#define DECLARE_RUNSVECTOR(...)                              \
    static const SegmenterTestRun runsArray[] = __VA_ARGS__; \
    Vector<SegmenterTestRun> runs; \
    runs.append(runsArray, sizeof(runsArray) / sizeof(*runsArray));

#define CHECK_RUNS_MIXED(...)    \
    DECLARE_RUNSVECTOR(__VA_ARGS__); \
    CheckRuns(runs, FontOrientation::VerticalMixed);

#define CHECK_RUNS_HORIZONTAL(...)    \
    DECLARE_RUNSVECTOR(__VA_ARGS__); \
    CheckRuns(runs, FontOrientation::Horizontal);

TEST_F(RunSegmenterTest, Empty)
{
    String empty(emptyString16Bit());
    RunSegmenter::RunSegmenterRange segmenterRange = { 0, 0, USCRIPT_INVALID_CODE, OrientationIterator::OrientationKeep };
    RunSegmenter runSegmenter(empty.characters16(), empty.length(), FontOrientation::VerticalMixed);
    ASSERT(!runSegmenter.consume(&segmenterRange));
    ASSERT_EQ(segmenterRange.start, 0u);
    ASSERT_EQ(segmenterRange.end, 0u);
    ASSERT_EQ(segmenterRange.script, USCRIPT_INVALID_CODE);
    ASSERT_EQ(segmenterRange.renderOrientation, OrientationIterator::OrientationKeep);
    ASSERT_EQ(segmenterRange.fontFallbackPriority, FontFallbackPriority::Text);
}

TEST_F(RunSegmenterTest, LatinPunctuationSideways)
{
    CHECK_RUNS_MIXED({ { "Abc.;?Xyz", USCRIPT_LATIN, OrientationIterator::OrientationRotateSideways, FontFallbackPriority::Text } });
}

TEST_F(RunSegmenterTest, OneSpace)
{
    CHECK_RUNS_MIXED({ { " ", USCRIPT_COMMON, OrientationIterator::OrientationRotateSideways, FontFallbackPriority::Text } });
}

TEST_F(RunSegmenterTest, ArabicHangul)
{
    CHECK_RUNS_MIXED({ { "نص", USCRIPT_ARABIC, OrientationIterator::OrientationRotateSideways, FontFallbackPriority::Text },
        { "키스의", USCRIPT_HANGUL, OrientationIterator::OrientationKeep, FontFallbackPriority::Text } });
}

TEST_F(RunSegmenterTest, JapaneseHindiEmojiMix)
{
    CHECK_RUNS_MIXED({ { "百家姓", USCRIPT_HAN, OrientationIterator::OrientationKeep, FontFallbackPriority::Text },
        { "ऋषियों", USCRIPT_DEVANAGARI, OrientationIterator::OrientationRotateSideways, FontFallbackPriority::Text },
        { "🌱🌲🌳🌴", USCRIPT_DEVANAGARI, OrientationIterator::OrientationKeep, FontFallbackPriority::EmojiEmoji },
        { "百家姓", USCRIPT_HAN, OrientationIterator::OrientationKeep, FontFallbackPriority::Text },
        { "🌱🌲", USCRIPT_HAN, OrientationIterator::OrientationKeep, FontFallbackPriority::EmojiEmoji } });
}

TEST_F(RunSegmenterTest, HangulSpace)
{
    CHECK_RUNS_MIXED({ { "키스의", USCRIPT_HANGUL, OrientationIterator::OrientationKeep, FontFallbackPriority::Text },
        { " ", USCRIPT_HANGUL, OrientationIterator::OrientationRotateSideways, FontFallbackPriority::Text },
        { "고유조건은", USCRIPT_HANGUL, OrientationIterator::OrientationKeep, FontFallbackPriority::Text } });
}

TEST_F(RunSegmenterTest, TechnicalCommonUpright)
{
    CHECK_RUNS_MIXED({ { "⌀⌁⌂", USCRIPT_COMMON, OrientationIterator::OrientationKeep, FontFallbackPriority::Math } });
}

TEST_F(RunSegmenterTest, PunctuationCommonSideways)
{
    CHECK_RUNS_MIXED({ { ".…¡", USCRIPT_COMMON, OrientationIterator::OrientationRotateSideways, FontFallbackPriority::Text } });
}

TEST_F(RunSegmenterTest, JapanesePunctuationMixedInside)
{
    CHECK_RUNS_MIXED({ { "いろはに", USCRIPT_HIRAGANA, OrientationIterator::OrientationKeep, FontFallbackPriority::Text },
        { ".…¡", USCRIPT_HIRAGANA, OrientationIterator::OrientationRotateSideways, FontFallbackPriority::Text },
        { "ほへと", USCRIPT_HIRAGANA, OrientationIterator::OrientationKeep, FontFallbackPriority::Text } });
}

TEST_F(RunSegmenterTest, JapanesePunctuationMixedInsideHorizontal)
{
    CHECK_RUNS_HORIZONTAL({ { "いろはに.…¡ほへと", USCRIPT_HIRAGANA, OrientationIterator::OrientationKeep, FontFallbackPriority::Text }});
}

TEST_F(RunSegmenterTest, PunctuationDevanagariCombining)
{
    CHECK_RUNS_HORIZONTAL({ { "क+े", USCRIPT_DEVANAGARI, OrientationIterator::OrientationKeep, FontFallbackPriority::Text }});
}

TEST_F(RunSegmenterTest, EmojiZWJSequences)
{
    CHECK_RUNS_HORIZONTAL({
        { "👩‍👩‍👧‍👦👩‍❤️‍💋‍👨", USCRIPT_LATIN, OrientationIterator::OrientationKeep, FontFallbackPriority::EmojiEmoji },
        { "abcd", USCRIPT_LATIN, OrientationIterator::OrientationKeep, FontFallbackPriority::Text },
        { "👩‍👩‍", USCRIPT_LATIN, OrientationIterator::OrientationKeep, FontFallbackPriority::EmojiEmoji },
        { "efg", USCRIPT_LATIN, OrientationIterator::OrientationKeep, FontFallbackPriority::Text }
    });
}

TEST_F(RunSegmenterTest, JapaneseLetterlikeEnd)
{
    CHECK_RUNS_MIXED({ { "いろは", USCRIPT_HIRAGANA, OrientationIterator::OrientationKeep, FontFallbackPriority::Text },
        { "ℐℒℐℒℐℒℐℒℐℒℐℒℐℒ", USCRIPT_HIRAGANA, OrientationIterator::OrientationRotateSideways, FontFallbackPriority::Text } });
}

TEST_F(RunSegmenterTest, JapaneseCase)
{
    CHECK_RUNS_MIXED({
        { "いろは", USCRIPT_HIRAGANA, OrientationIterator::OrientationKeep, FontFallbackPriority::Text },
        { "aaAA", USCRIPT_LATIN, OrientationIterator::OrientationRotateSideways, FontFallbackPriority::Text },
        { "いろは", USCRIPT_HIRAGANA, OrientationIterator::OrientationKeep, FontFallbackPriority::Text }
    });
}

TEST_F(RunSegmenterTest, DingbatsMiscSymbolsModifier)
{
    CHECK_RUNS_HORIZONTAL({ { "⛹🏻✍🏻✊🏼", USCRIPT_COMMON, OrientationIterator::OrientationKeep, FontFallbackPriority::EmojiEmoji } });
}

TEST_F(RunSegmenterTest, ArmenianCyrillicCase)
{
    CHECK_RUNS_HORIZONTAL({ { "աբգ", USCRIPT_ARMENIAN, OrientationIterator::OrientationKeep, FontFallbackPriority::Text },
        { "αβγ", USCRIPT_GREEK, OrientationIterator::OrientationKeep, FontFallbackPriority::Text },
        { "ԱԲԳ", USCRIPT_ARMENIAN, OrientationIterator::OrientationKeep, FontFallbackPriority::Text } });
}

} // namespace blink
