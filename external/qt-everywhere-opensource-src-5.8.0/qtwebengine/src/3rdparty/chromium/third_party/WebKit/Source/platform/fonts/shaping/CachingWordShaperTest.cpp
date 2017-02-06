// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/shaping/CachingWordShaper.h"

#include "platform/fonts/CharacterRange.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/GlyphBuffer.h"
#include "platform/fonts/shaping/CachingWordShapeIterator.h"
#include "platform/fonts/shaping/ShapeResultTestInfo.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

class CachingWordShaperTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        fontDescription.setComputedSize(12.0);
        fontDescription.setLocale("en");
        ASSERT_EQ(USCRIPT_LATIN, fontDescription.script());
        fontDescription.setGenericFamily(FontDescription::StandardFamily);

        font = Font(fontDescription);
        font.update(nullptr);
        ASSERT_TRUE(font.canShapeWordByWord());
        fallbackFonts = nullptr;
        cache = wrapUnique(new ShapeCache());
    }

    FontCachePurgePreventer fontCachePurgePreventer;
    FontDescription fontDescription;
    Font font;
    std::unique_ptr<ShapeCache> cache;
    HashSet<const SimpleFontData*>* fallbackFonts;
    unsigned startIndex = 0;
    unsigned numGlyphs = 0;
    hb_script_t script = HB_SCRIPT_INVALID;
};

static inline const ShapeResultTestInfo* testInfo(
    RefPtr<const ShapeResult>& result)
{
    return static_cast<const ShapeResultTestInfo*>(result.get());
}

TEST_F(CachingWordShaperTest, LatinLeftToRightByWord)
{
    TextRun textRun(reinterpret_cast<const LChar*>("ABC DEF."), 8);

    RefPtr<const ShapeResult> result;
    CachingWordShapeIterator iterator(cache.get(), textRun, &font);
    ASSERT_TRUE(iterator.next(&result));
    ASSERT_TRUE(testInfo(result)->runInfoForTesting(0, startIndex, numGlyphs, script));
    EXPECT_EQ(0u, startIndex);
    EXPECT_EQ(3u, numGlyphs);
    EXPECT_EQ(HB_SCRIPT_LATIN, script);

    ASSERT_TRUE(iterator.next(&result));
    ASSERT_TRUE(testInfo(result)->runInfoForTesting(0, startIndex, numGlyphs, script));
    EXPECT_EQ(0u, startIndex);
    EXPECT_EQ(1u, numGlyphs);
    EXPECT_EQ(HB_SCRIPT_COMMON, script);

    ASSERT_TRUE(iterator.next(&result));
    ASSERT_TRUE(testInfo(result)->runInfoForTesting(0, startIndex, numGlyphs, script));
    EXPECT_EQ(0u, startIndex);
    EXPECT_EQ(4u, numGlyphs);
    EXPECT_EQ(HB_SCRIPT_LATIN, script);

    ASSERT_FALSE(iterator.next(&result));
}

TEST_F(CachingWordShaperTest, CommonAccentLeftToRightByWord)
{
    const UChar str[] = { 0x2F, 0x301, 0x2E, 0x20, 0x2E, 0x0 };
    TextRun textRun(str, 5);

    unsigned offset = 0;
    RefPtr<const ShapeResult> result;
    CachingWordShapeIterator iterator(cache.get(), textRun, &font);
    ASSERT_TRUE(iterator.next(&result));
    ASSERT_TRUE(testInfo(result)->runInfoForTesting(0, startIndex, numGlyphs, script));
    EXPECT_EQ(0u, offset + startIndex);
    EXPECT_EQ(3u, numGlyphs);
    EXPECT_EQ(HB_SCRIPT_COMMON, script);
    offset += result->numCharacters();

    ASSERT_TRUE(iterator.next(&result));
    ASSERT_TRUE(testInfo(result)->runInfoForTesting(0, startIndex, numGlyphs, script));
    EXPECT_EQ(3u, offset + startIndex);
    EXPECT_EQ(1u, numGlyphs);
    EXPECT_EQ(HB_SCRIPT_COMMON, script);
    offset += result->numCharacters();

    ASSERT_TRUE(iterator.next(&result));
    ASSERT_TRUE(testInfo(result)->runInfoForTesting(0, startIndex, numGlyphs, script));
    EXPECT_EQ(4u, offset + startIndex);
    EXPECT_EQ(1u, numGlyphs);
    EXPECT_EQ(HB_SCRIPT_COMMON, script);
    offset += result->numCharacters();

    ASSERT_EQ(5u, offset);
    ASSERT_FALSE(iterator.next(&result));
}

// Tests that filling a glyph buffer for a specific range returns the same
// results when shaping word by word as when shaping the full run in one go.
TEST_F(CachingWordShaperTest, CommonAccentLeftToRightFillGlyphBuffer)
{
    // "/. ." with an accent mark over the first dot.
    const UChar str[] = { 0x2F, 0x301, 0x2E, 0x20, 0x2E, 0x0 };
    TextRun textRun(str, 5);

    CachingWordShaper shaper(cache.get());
    GlyphBuffer glyphBuffer;
    shaper.fillGlyphBuffer(&font, textRun, fallbackFonts, &glyphBuffer, 0, 3);

    std::unique_ptr<ShapeCache> referenceCache = wrapUnique(new ShapeCache());
    CachingWordShaper referenceShaper(referenceCache.get());
    GlyphBuffer referenceGlyphBuffer;
    font.setCanShapeWordByWordForTesting(false);
    referenceShaper.fillGlyphBuffer(&font, textRun, fallbackFonts,
        &referenceGlyphBuffer, 0, 3);

    ASSERT_EQ(referenceGlyphBuffer.glyphAt(0), glyphBuffer.glyphAt(0));
    ASSERT_EQ(referenceGlyphBuffer.glyphAt(1), glyphBuffer.glyphAt(1));
    ASSERT_EQ(referenceGlyphBuffer.glyphAt(2), glyphBuffer.glyphAt(2));
}

// Tests that filling a glyph buffer for a specific range returns the same
// results when shaping word by word as when shaping the full run in one go.
TEST_F(CachingWordShaperTest, CommonAccentRightToLeftFillGlyphBuffer)
{
    // "[] []" with an accent mark over the last square bracket.
    const UChar str[] = { 0x5B, 0x5D, 0x20, 0x5B, 0x301, 0x5D, 0x0 };
    TextRun textRun(str, 6);
    textRun.setDirection(RTL);

    CachingWordShaper shaper(cache.get());
    GlyphBuffer glyphBuffer;
    shaper.fillGlyphBuffer(&font, textRun, fallbackFonts, &glyphBuffer, 1, 6);

    std::unique_ptr<ShapeCache> referenceCache = wrapUnique(new ShapeCache());
    CachingWordShaper referenceShaper(referenceCache.get());
    GlyphBuffer referenceGlyphBuffer;
    font.setCanShapeWordByWordForTesting(false);
    referenceShaper.fillGlyphBuffer(&font, textRun, fallbackFonts,
        &referenceGlyphBuffer, 1, 6);

    ASSERT_EQ(5u, referenceGlyphBuffer.size());
    ASSERT_EQ(referenceGlyphBuffer.size(), glyphBuffer.size());

    ASSERT_EQ(referenceGlyphBuffer.glyphAt(0), glyphBuffer.glyphAt(0));
    ASSERT_EQ(referenceGlyphBuffer.glyphAt(1), glyphBuffer.glyphAt(1));
    ASSERT_EQ(referenceGlyphBuffer.glyphAt(2), glyphBuffer.glyphAt(2));
    ASSERT_EQ(referenceGlyphBuffer.glyphAt(3), glyphBuffer.glyphAt(3));
    ASSERT_EQ(referenceGlyphBuffer.glyphAt(4), glyphBuffer.glyphAt(4));
}

// Tests that runs with zero glyphs (the ZWJ non-printable character in this
// case) are handled correctly. This test passes if it does not cause a crash.
TEST_F(CachingWordShaperTest, SubRunWithZeroGlyphs)
{
    // "Foo &zwnj; bar"
    const UChar str[] = {
        0x46, 0x6F, 0x6F, 0x20, 0x200C, 0x20, 0x62, 0x61, 0x71, 0x0
    };
    TextRun textRun(str, 9);

    CachingWordShaper shaper(cache.get());
    FloatRect glyphBounds;
    ASSERT_GT(shaper.width(&font, textRun, nullptr, &glyphBounds), 0);

    GlyphBuffer glyphBuffer;
    shaper.fillGlyphBuffer(&font, textRun, fallbackFonts, &glyphBuffer, 0, 8);


    shaper.getCharacterRange(&font, textRun, 0, 8);
}

TEST_F(CachingWordShaperTest, SegmentCJKByCharacter)
{
    const UChar str[] = {
        0x56FD, 0x56FD, // CJK Unified Ideograph
        'a', 'b',
        0x56FD, // CJK Unified Ideograph
        'x', 'y', 'z',
        0x3042, // HIRAGANA LETTER A
        0x56FD, // CJK Unified Ideograph
        0x0
    };
    TextRun textRun(str, 10);

    RefPtr<const ShapeResult> wordResult;
    CachingWordShapeIterator iterator(cache.get(), textRun, &font);

    ASSERT_TRUE(iterator.next(&wordResult));
    EXPECT_EQ(1u, wordResult->numCharacters());
    ASSERT_TRUE(iterator.next(&wordResult));
    EXPECT_EQ(1u, wordResult->numCharacters());

    ASSERT_TRUE(iterator.next(&wordResult));
    EXPECT_EQ(2u, wordResult->numCharacters());

    ASSERT_TRUE(iterator.next(&wordResult));
    EXPECT_EQ(1u, wordResult->numCharacters());

    ASSERT_TRUE(iterator.next(&wordResult));
    EXPECT_EQ(3u, wordResult->numCharacters());

    ASSERT_TRUE(iterator.next(&wordResult));
    EXPECT_EQ(1u, wordResult->numCharacters());
    ASSERT_TRUE(iterator.next(&wordResult));
    EXPECT_EQ(1u, wordResult->numCharacters());

    ASSERT_FALSE(iterator.next(&wordResult));
}

TEST_F(CachingWordShaperTest, SegmentCJKAndCommon)
{
    const UChar str[] = {
        'a', 'b',
        0xFF08, // FULLWIDTH LEFT PARENTHESIS (script=common)
        0x56FD, // CJK Unified Ideograph
        0x56FD, // CJK Unified Ideograph
        0x56FD, // CJK Unified Ideograph
        0x3002, // IDEOGRAPHIC FULL STOP (script=common)
        0x0
    };
    TextRun textRun(str, 7);

    RefPtr<const ShapeResult> wordResult;
    CachingWordShapeIterator iterator(cache.get(), textRun, &font);

    ASSERT_TRUE(iterator.next(&wordResult));
    EXPECT_EQ(2u, wordResult->numCharacters());

    ASSERT_TRUE(iterator.next(&wordResult));
    EXPECT_EQ(2u, wordResult->numCharacters());

    ASSERT_TRUE(iterator.next(&wordResult));
    EXPECT_EQ(1u, wordResult->numCharacters());

    ASSERT_TRUE(iterator.next(&wordResult));
    EXPECT_EQ(2u, wordResult->numCharacters());

    ASSERT_FALSE(iterator.next(&wordResult));
}

TEST_F(CachingWordShaperTest, SegmentCJKAndInherit)
{
    const UChar str[] = {
        0x304B, // HIRAGANA LETTER KA
        0x304B, // HIRAGANA LETTER KA
        0x3009, // COMBINING KATAKANA-HIRAGANA VOICED SOUND MARK
        0x304B, // HIRAGANA LETTER KA
        0x0
    };
    TextRun textRun(str, 4);

    RefPtr<const ShapeResult> wordResult;
    CachingWordShapeIterator iterator(cache.get(), textRun, &font);

    ASSERT_TRUE(iterator.next(&wordResult));
    EXPECT_EQ(1u, wordResult->numCharacters());

    ASSERT_TRUE(iterator.next(&wordResult));
    EXPECT_EQ(2u, wordResult->numCharacters());

    ASSERT_TRUE(iterator.next(&wordResult));
    EXPECT_EQ(1u, wordResult->numCharacters());

    ASSERT_FALSE(iterator.next(&wordResult));
}

TEST_F(CachingWordShaperTest, SegmentCJKAndNonCJKCommon)
{
    const UChar str[] = {
        0x56FD, // CJK Unified Ideograph
        ' ',
        0x0
    };
    TextRun textRun(str, 2);

    RefPtr<const ShapeResult> wordResult;
    CachingWordShapeIterator iterator(cache.get(), textRun, &font);

    ASSERT_TRUE(iterator.next(&wordResult));
    EXPECT_EQ(1u, wordResult->numCharacters());

    ASSERT_TRUE(iterator.next(&wordResult));
    EXPECT_EQ(1u, wordResult->numCharacters());

    ASSERT_FALSE(iterator.next(&wordResult));
}

TEST_F(CachingWordShaperTest, SegmentEmojiZWJCommon)
{
    // A family followed by a couple with heart emoji sequence,
    // the latter including a variation selector.
    const UChar str[] = {
        0xD83D, 0xDC68,
        0x200D,
        0xD83D, 0xDC69,
        0x200D,
        0xD83D, 0xDC67,
        0x200D,
        0xD83D, 0xDC66,
        0xD83D, 0xDC69,
        0x200D,
        0x2764, 0xFE0F,
        0x200D,
        0xD83D, 0xDC8B,
        0x200D,
        0xD83D, 0xDC68,
        0x0
    };
    TextRun textRun(str, 22);

    RefPtr<const ShapeResult> wordResult;
    CachingWordShapeIterator iterator(cache.get(), textRun, &font);

    ASSERT_TRUE(iterator.next(&wordResult));
    EXPECT_EQ(22u, wordResult->numCharacters());

    ASSERT_FALSE(iterator.next(&wordResult));
}

TEST_F(CachingWordShaperTest, SegmentEmojiHeartZWJSequence)
{
    // A ZWJ, followed by two family ZWJ Sequences.
    const UChar str[] = {
        0xD83D, 0xDC69,
        0x200D,
        0x2764, 0xFE0F,
        0x200D,
        0xD83D, 0xDC8B,
        0x200D,
        0xD83D, 0xDC68,
        0x0
    };
    TextRun textRun(str, 11);

    RefPtr<const ShapeResult> wordResult;
    CachingWordShapeIterator iterator(cache.get(), textRun, &font);

    ASSERT_TRUE(iterator.next(&wordResult));
    EXPECT_EQ(11u, wordResult->numCharacters());

    ASSERT_FALSE(iterator.next(&wordResult));
}

TEST_F(CachingWordShaperTest, SegmentEmojiSignsOfHornsModifier)
{
    // A Sign of the Horns emoji, followed by a fitzpatrick modifer
    const UChar str[] = {
        0xD83E, 0xDD18,
        0xD83C, 0xDFFB,
        0x0
    };
    TextRun textRun(str, 4);

    RefPtr<const ShapeResult> wordResult;
    CachingWordShapeIterator iterator(cache.get(), textRun, &font);

    ASSERT_TRUE(iterator.next(&wordResult));
    EXPECT_EQ(4u, wordResult->numCharacters());

    ASSERT_FALSE(iterator.next(&wordResult));
}

TEST_F(CachingWordShaperTest, SegmentEmojiExtraZWJPrefix)
{
    // A ZWJ, followed by a family and a heart-kiss sequence.
    const UChar str[] = {
        0x200D,
        0xD83D, 0xDC68,
        0x200D,
        0xD83D, 0xDC69,
        0x200D,
        0xD83D, 0xDC67,
        0x200D,
        0xD83D, 0xDC66,
        0xD83D, 0xDC69,
        0x200D,
        0x2764, 0xFE0F,
        0x200D,
        0xD83D, 0xDC8B,
        0x200D,
        0xD83D, 0xDC68,
        0x0
    };
    TextRun textRun(str, 23);

    RefPtr<const ShapeResult> wordResult;
    CachingWordShapeIterator iterator(cache.get(), textRun, &font);

    ASSERT_TRUE(iterator.next(&wordResult));
    EXPECT_EQ(1u, wordResult->numCharacters());

    ASSERT_TRUE(iterator.next(&wordResult));
    EXPECT_EQ(22u, wordResult->numCharacters());

    ASSERT_FALSE(iterator.next(&wordResult));
}

TEST_F(CachingWordShaperTest, SegmentCJKCommon)
{
    const UChar str[] = {
        0xFF08, // FULLWIDTH LEFT PARENTHESIS (script=common)
        0xFF08, // FULLWIDTH LEFT PARENTHESIS (script=common)
        0xFF08, // FULLWIDTH LEFT PARENTHESIS (script=common)
        0x0
    };
    TextRun textRun(str, 3);

    RefPtr<const ShapeResult> wordResult;
    CachingWordShapeIterator iterator(cache.get(), textRun, &font);

    ASSERT_TRUE(iterator.next(&wordResult));
    EXPECT_EQ(3u, wordResult->numCharacters());

    ASSERT_FALSE(iterator.next(&wordResult));
}

TEST_F(CachingWordShaperTest, SegmentCJKCommonAndNonCJK)
{
    const UChar str[] = {
        0xFF08, // FULLWIDTH LEFT PARENTHESIS (script=common)
        'a', 'b',
        0x0
    };
    TextRun textRun(str, 3);

    RefPtr<const ShapeResult> wordResult;
    CachingWordShapeIterator iterator(cache.get(), textRun, &font);

    ASSERT_TRUE(iterator.next(&wordResult));
    EXPECT_EQ(1u, wordResult->numCharacters());

    ASSERT_TRUE(iterator.next(&wordResult));
    EXPECT_EQ(2u, wordResult->numCharacters());

    ASSERT_FALSE(iterator.next(&wordResult));
}

TEST_F(CachingWordShaperTest, SegmentCJKSmallFormVariants)
{
    const UChar str[] = {
        0x5916, // CJK UNIFIED IDEOGRPAH
        0xFE50, // SMALL COMMA
        0x0
    };
    TextRun textRun(str, 2);

    RefPtr<const ShapeResult> wordResult;
    CachingWordShapeIterator iterator(cache.get(), textRun, &font);

    ASSERT_TRUE(iterator.next(&wordResult));
    EXPECT_EQ(2u, wordResult->numCharacters());

    ASSERT_FALSE(iterator.next(&wordResult));
}

TEST_F(CachingWordShaperTest, SegmentHangulToneMark)
{
    const UChar str[] = {
        0xC740, // HANGUL SYLLABLE EUN
        0x302E, // HANGUL SINGLE DOT TONE MARK
        0x0
    };
    TextRun textRun(str, 2);

    RefPtr<const ShapeResult> wordResult;
    CachingWordShapeIterator iterator(cache.get(), textRun, &font);

    ASSERT_TRUE(iterator.next(&wordResult));
    EXPECT_EQ(2u, wordResult->numCharacters());

    ASSERT_FALSE(iterator.next(&wordResult));
}

TEST_F(CachingWordShaperTest, TextOrientationFallbackShouldNotInFallbackList)
{
    const UChar str[] = {
        'A', // code point for verticalRightOrientationFontData()
        // Ideally we'd like to test uprightOrientationFontData() too
        // using code point such as U+3042, but it'd fallback to system
        // fonts as the glyph is missing.
        0x0
    };
    TextRun textRun(str, 1);

    fontDescription.setOrientation(FontOrientation::VerticalMixed);
    Font verticalMixedFont = Font(fontDescription);
    verticalMixedFont.update(nullptr);
    ASSERT_TRUE(verticalMixedFont.canShapeWordByWord());

    CachingWordShaper shaper(cache.get());
    FloatRect glyphBounds;
    HashSet<const SimpleFontData*> fallbackFonts;
    ASSERT_GT(shaper.width(&verticalMixedFont, textRun, &fallbackFonts, &glyphBounds), 0);
    EXPECT_EQ(0u, fallbackFonts.size());
}

TEST_F(CachingWordShaperTest, GlyphBoundsWithSpaces)
{
    CachingWordShaper shaper(cache.get());

    TextRun periods(reinterpret_cast<const LChar*>(".........."), 10);
    FloatRect periodsGlyphBounds;
    float periodsWidth = shaper.width(&font, periods, nullptr, &periodsGlyphBounds);

    TextRun periodsAndSpaces(reinterpret_cast<const LChar*>(". . . . . . . . . ."), 19);
    FloatRect periodsAndSpacesGlyphBounds;
    float periodsAndSpacesWidth = shaper.width(&font, periodsAndSpaces, nullptr, &periodsAndSpacesGlyphBounds);

    // The total width of periods and spaces should be longer than the width of periods alone.
    ASSERT_GT(periodsAndSpacesWidth, periodsWidth);

    // The glyph bounds of periods and spaces should be longer than the glyph bounds of periods alone.
    ASSERT_GT(periodsAndSpacesGlyphBounds.width(), periodsGlyphBounds.width());
}

} // namespace blink
