// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CharacterPropertyDataGenerator_h
#define CharacterPropertyDataGenerator_h

#include <unicode/uobject.h>

namespace blink {

static const UChar32 isCJKIdeographOrSymbolArray[] = {
    // 0x2C7 Caron, Mandarin Chinese 3rd Tone
    0x2C7,
    // 0x2CA Modifier Letter Acute Accent, Mandarin Chinese 2nd Tone
    0x2CA,
    // 0x2CB Modifier Letter Grave Access, Mandarin Chinese 4th Tone
    0x2CB,
    // 0x2D9 Dot Above, Mandarin Chinese 5th Tone
    0x2D9,
    0x2020, 0x2021, 0x2030, 0x203B, 0x203C, 0x2042, 0x2047, 0x2048, 0x2049, 0x2051,
    0x20DD, 0x20DE, 0x2100, 0x2103, 0x2105, 0x2109, 0x210A, 0x2113, 0x2116, 0x2121,
    0x212B, 0x213B, 0x2150, 0x2151, 0x2152, 0x217F, 0x2189, 0x2307, 0x2312, 0x23CE,
    0x2423, 0x25A0, 0x25A1, 0x25A2, 0x25AA, 0x25AB, 0x25B1, 0x25B2, 0x25B3, 0x25B6,
    0x25B7, 0x25BC, 0x25BD, 0x25C0, 0x25C1, 0x25C6, 0x25C7, 0x25C9, 0x25CB, 0x25CC,
    0x25EF, 0x2605, 0x2606, 0x260E, 0x2616, 0x2617, 0x261D, 0x2640, 0x2642, 0x26A0,
    0x26BD, 0x26BE, 0x26F9, 0x2713, 0x271A, 0x273F, 0x2740, 0x2756, 0x2B1A, 0xFE10,
    0xFE11, 0xFE12, 0xFE19, 0xFF1D,
    // Emoji.
    0x1F100
};

static const UChar32 isCJKIdeographOrSymbolRanges[] = {
    // cjkIdeographRanges
    // CJK Radicals Supplement and Kangxi Radicals.
    0x2E80, 0x2FDF,
    // CJK Strokes.
    0x31C0, 0x31EF,
    // CJK Unified Ideographs Extension A.
    0x3400, 0x4DBF,
    // The basic CJK Unified Ideographs block.
    0x4E00, 0x9FFF,
    // CJK Compatibility Ideographs.
    0xF900, 0xFAFF,
    // CJK Unified Ideographs Extension B.
    0x20000, 0x2A6DF,
    // CJK Unified Ideographs Extension C.
    // CJK Unified Ideographs Extension D.
    0x2A700, 0x2B81F,
    // CJK Compatibility Ideographs Supplement.
    0x2F800, 0x2FA1F,

    // cjkSymbolRanges
    0x2156, 0x215A,
    0x2160, 0x216B,
    0x2170, 0x217B,
    0x23BE, 0x23CC,
    0x2460, 0x2492,
    0x249C, 0x24FF,
    0x25CE, 0x25D3,
    0x25E2, 0x25E6,
    0x2600, 0x2603,
    0x2660, 0x266F,
    // Emoji HEAVY HEART EXCLAMATION MARK ORNAMENT..HEAVY BLACK HEART
    // Needed in order not to break Emoji heart-kiss sequences in
    // CachingWordShapeIterator.
    // cmp. http://www.unicode.org/emoji/charts/emoji-zwj-sequences.html
    0x2763, 0x2764,
    0x2672, 0x267D,
    0x2776, 0x277F,
    // Hand signs needed in order
    // not to break Emoji modifier sequences.
    0x270A, 0x270D,
    // Ideographic Description Characters, with CJK Symbols and Punctuation,
    // excluding 0x3030.
    // Exclude Hangul Tone Marks (0x302E .. 0x302F) because Hangul is not Han
    // and no other Hangul are included.
    // Then Hiragana 0x3040 .. 0x309F, Katakana 0x30A0 .. 0x30FF, Bopomofo
    // 0x3100 .. 0x312F
    0x2FF0, 0x302D,
    0x3031, 0x312F,
    // More Bopomofo and Bopomofo Extended 0x31A0 .. 0x31BF
    0x3190, 0x31BF,
    // Enclosed CJK Letters and Months (0x3200 .. 0x32FF).
    // CJK Compatibility (0x3300 .. 0x33FF).
    0x3200, 0x33FF,
    0xF860, 0xF862,
    // CJK Compatibility Forms.
    // Small Form Variants (for CNS 11643).
    0xFE30, 0xFE6F,
    // Halfwidth and Fullwidth Forms
    // Usually only used in CJK
    0xFF00, 0xFF0C,
    0xFF0E, 0xFF1A,
    0xFF1F, 0xFFEF,
    // Emoji.
    0x1F110, 0x1F129,
    0x1F130, 0x1F149,
    0x1F150, 0x1F169,
    0x1F170, 0x1F189,
    0x1F200, 0x1F6FF,
    // Modifiers
    0x1F3FB, 0x1F3FF,
    // ZIPPER-MOUTH FACE...SIGN OF THE HORNS
    0x1F910, 0x1F918
};

// Individual codepoints needed for Unicode vertical text layout according to
// http://www.unicode.org/reports/tr50/
// Taken from the corresponding data file:
// http://www.unicode.org/Public/vertical/revision-13/VerticalOrientation-13.txt
static const UChar32 isUprightInMixedVerticalArray[] = {
    0x000A7,
    0x000A9,
    0x000AE,
    0x000B1,
    0x000D7,
    0x000F7
};

static const UChar32 isUprightInMixedVerticalRanges[] = {
    0x000BC, 0x000BE,
    // Spacing Modifier Letters (Part of)
    0x002EA, 0x002EB,
    // Hangul Jamo
    0x01100, 0x011FF,
    // Unified Canadian Aboriginal Syllabics
    0x01401, 0x0167F,
    // Unified Canadian Aboriginal Syllabics Extended
    0x018B0, 0x018FF,
    // General Punctuation (Part of)
    0x02016, 0x02016,
    0x02020, 0x02021,
    0x02030, 0x02031,
    0x0203B, 0x0203C,
    0x02042, 0x02042,
    0x02047, 0x02049,
    0x02051, 0x02051,
    0x02065, 0x02069,
    // Combining Diacritical Marks for Symbols (Part of)
    0x020DD, 0x020E0,
    0x020E2, 0x020E4,
    // Letterlike Symbols (Part of)/Number Forms
    0x02100, 0x02101,
    0x02103, 0x02109,
    0x0210F, 0x0210F,
    0x02113, 0x02114,
    0x02116, 0x02117,
    0x0211E, 0x02123,
    0x02125, 0x02125,
    0x02127, 0x02127,
    0x02129, 0x02129,
    0x0212E, 0x0212E,
    0x02135, 0x0213F,
    0x02145, 0x0214A,
    0x0214C, 0x0214D,
    0x0214F, 0x0218F,
    // Mathematical Operators (Part of)
    0x0221E, 0x0221E,
    0x02234, 0x02235,
    // Miscellaneous Technical (Part of)
    0x02300, 0x02307,
    0x0230C, 0x0231F,
    0x02324, 0x0232B,
    0x0237D, 0x0239A,
    0x023BE, 0x023CD,
    0x023CF, 0x023CF,
    0x023D1, 0x023DB,
    0x023E2, 0x02422,
    // Control Pictures (Part of)/Optical Character Recognition/Enclosed
    // Alphanumerics
    0x02424, 0x024FF,
    // Geometric Shapes/Miscellaneous Symbols (Part of)
    0x025A0, 0x02619,
    0x02620, 0x02767,
    0x02776, 0x02793,
    // Miscellaneous Symbols and Arrows (Part of)
    0x02B12, 0x02B2F,
    0x02B50, 0x02B59,
    0x02BB8, 0x02BFF,
    // Common CJK
    0x02E80, 0x0A4CF,
    // Hangul Jamo Extended-A
    0x0A960, 0x0A97F,
    // Hangul Syllables/Hangul Jamo Extended-B
    0x0AC00, 0x0D7FF,
    // Private Use Area/CJK Compatibility Ideographs
    0x0E000, 0x0FAFF,
    // Vertical Forms
    0x0FE10, 0x0FE1F,
    // CJK Compatibility Forms (Part of)
    0x0FE30, 0x0FE48,
    // Small Form Variants (Part of)
    0x0FE50, 0x0FE57,
    0x0FE59, 0x0FE62,
    0x0FE67, 0x0FE6F,
    // Halfwidth and Fullwidth Forms
    0x0FF01, 0x0FF0C,
    0x0FF0E, 0x0FF1B,
    0x0FF1F, 0x0FF60,
    0x0FFE0, 0x0FFE7,
    // Specials (Part of)
    0x0FFF0, 0x0FFF8,
    0x0FFFC, 0x0FFFD,
    // Meroitic Hieroglyphs
    0x10980, 0x1099F,
    // Siddham
    0x11580, 0x115FF,
    // Egyptian Hieroglyphs
    0x13000, 0x1342F,
    // Kana Supplement
    0x1B000, 0x1B0FF,
    // Byzantine Musical Symbols/Musical Symbols
    0x1D000, 0x1D1FF,
    // Tai Xuan Jing Symbols/Counting Rod Numerals
    0x1D300, 0x1D37F,
    // Mahjong Tiles/Domino Tiles/Playing Cards/Enclosed Alphanumeric Supplement
    // Enclosed Ideographic Supplement/Enclosed Ideographic Supplement
    // Emoticons/Ornamental Dingbats/Transport and Map Symbols/Alchemical
    // Symbols Alchemical Symbols
    0x1F000, 0x1F7FF,
    // CJK Unified Ideographs Extension B/C/D
    // CJK Compatibility Ideographs Supplement
    0x20000, 0x2FFFD,
    0x30000, 0x3FFFD,
    // Supplementary Private Use Area-A
    0xF0000, 0xFFFFD,
    // Supplementary Private Use Area-B
    0x100000, 0x10FFFD,
};

// https://html.spec.whatwg.org/multipage/scripting.html#prod-potentialcustomelementname
static const UChar32 isPotentialCustomElementNameCharArray[] = {
    '-',
    '.',
    '_',
    0xB7,
};

static const UChar32 isPotentialCustomElementNameCharRanges[] = {
    '0', '9',
    'a', 'z',
    0xC0, 0xD6,
    0xD8, 0xF6,
    0xF8, 0x2FF,
    0x300, 0x37D,
    0x37F, 0x1FFF,
    0x200C, 0x200D,
    0x203F, 0x2040,
    0x2070, 0x218F,
    0x2C00, 0x2FEF,
    0x3001, 0xD7FF,
    0xF900, 0xFDCF,
    0xFDF0, 0xFFFD,
    0x10000, 0xEFFFF,
};

} // namespace blink

#endif
