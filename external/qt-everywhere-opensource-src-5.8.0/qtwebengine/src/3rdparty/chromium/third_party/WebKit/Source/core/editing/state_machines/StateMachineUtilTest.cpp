// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/state_machines/StateMachineUtil.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/CharacterNames.h"
#include "wtf/text/Unicode.h"

namespace blink {

TEST(StateMachineUtilTest, IsGraphmeBreak_LineBreak)
{
    // U+000AD (SOFT HYPHEN) has Control grapheme property.
    const UChar32 kControl = WTF::Unicode::softHyphenCharacter;

    // Grapheme Cluster Boundary Rule GB3: CR x LF
    EXPECT_FALSE(isGraphemeBreak('\r', '\n'));
    EXPECT_TRUE(isGraphemeBreak('\n', '\r'));

    // Grapheme Cluster Boundary Rule GB4: (Control | CR | LF) ÷
    EXPECT_TRUE(isGraphemeBreak('\r', 'a'));
    EXPECT_TRUE(isGraphemeBreak('\n', 'a'));
    EXPECT_TRUE(isGraphemeBreak(kControl, 'a'));

    // Grapheme Cluster Boundary Rule GB5: ÷ (Control | CR | LF)
    EXPECT_TRUE(isGraphemeBreak('a', '\r'));
    EXPECT_TRUE(isGraphemeBreak('a', '\n'));
    EXPECT_TRUE(isGraphemeBreak('a', kControl));
}

TEST(StateMachineUtilTest, IsGraphmeBreak_Hangul)
{
    // U+1100 (HANGUL CHOSEONG KIYEOK) has L grapheme property.
    const UChar32 kL = 0x1100;
    // U+1160 (HANGUL JUNGSEONG FILLER) has V grapheme property.
    const UChar32 kV = 0x1160;
    // U+AC00 (HANGUL SYLLABLE GA) has LV grapheme property.
    const UChar32 kLV = 0xAC00;
    // U+AC01 (HANGUL SYLLABLE GAG) has LVT grapheme property.
    const UChar32 kLVT = 0xAC01;
    // U+11A8 (HANGUL JONGSEONG KIYEOK) has T grapheme property.
    const UChar32 kT = 0x11A8;

    // Grapheme Cluster Boundary Rule GB6: L x (L | V | LV | LVT)
    EXPECT_FALSE(isGraphemeBreak(kL, kL));
    EXPECT_FALSE(isGraphemeBreak(kL, kV));
    EXPECT_FALSE(isGraphemeBreak(kL, kLV));
    EXPECT_FALSE(isGraphemeBreak(kL, kLVT));
    EXPECT_TRUE(isGraphemeBreak(kL, kT));

    // Grapheme Cluster Boundary Rule GB7: (LV | V) x (V | T)
    EXPECT_TRUE(isGraphemeBreak(kV, kL));
    EXPECT_FALSE(isGraphemeBreak(kV, kV));
    EXPECT_TRUE(isGraphemeBreak(kV, kLV));
    EXPECT_TRUE(isGraphemeBreak(kV, kLVT));
    EXPECT_FALSE(isGraphemeBreak(kV, kT));

    // Grapheme Cluster Boundary Rule GB7: (LV | V) x (V | T)
    EXPECT_TRUE(isGraphemeBreak(kLV, kL));
    EXPECT_FALSE(isGraphemeBreak(kLV, kV));
    EXPECT_TRUE(isGraphemeBreak(kLV, kLV));
    EXPECT_TRUE(isGraphemeBreak(kLV, kLVT));
    EXPECT_FALSE(isGraphemeBreak(kLV, kT));

    // Grapheme Cluster Boundary Rule GB8: (LVT | T) x T
    EXPECT_TRUE(isGraphemeBreak(kLVT, kL));
    EXPECT_TRUE(isGraphemeBreak(kLVT, kV));
    EXPECT_TRUE(isGraphemeBreak(kLVT, kLV));
    EXPECT_TRUE(isGraphemeBreak(kLVT, kLVT));
    EXPECT_FALSE(isGraphemeBreak(kLVT, kT));

    // Grapheme Cluster Boundary Rule GB8: (LVT | T) x T
    EXPECT_TRUE(isGraphemeBreak(kT, kL));
    EXPECT_TRUE(isGraphemeBreak(kT, kV));
    EXPECT_TRUE(isGraphemeBreak(kT, kLV));
    EXPECT_TRUE(isGraphemeBreak(kT, kLVT));
    EXPECT_FALSE(isGraphemeBreak(kT, kT));
}

TEST(StateMachineUtilTest, IsGraphmeBreak_Extend_or_ZWJ)
{
    // U+0300 (COMBINING GRAVE ACCENT) has Extend grapheme property.
    const UChar32 kExtend = 0x0300;
    // Grapheme Cluster Boundary Rule GB9: x (Extend | ZWJ)
    EXPECT_FALSE(isGraphemeBreak('a', kExtend));
    EXPECT_FALSE(isGraphemeBreak('a', WTF::Unicode::zeroWidthJoinerCharacter));
    EXPECT_FALSE(isGraphemeBreak(kExtend, kExtend));
    EXPECT_FALSE(isGraphemeBreak(WTF::Unicode::zeroWidthJoinerCharacter,
        WTF::Unicode::zeroWidthJoinerCharacter));
    EXPECT_FALSE(isGraphemeBreak(kExtend,
        WTF::Unicode::zeroWidthJoinerCharacter));
    EXPECT_FALSE(isGraphemeBreak(WTF::Unicode::zeroWidthJoinerCharacter,
        kExtend));
}

TEST(StateMachineUtilTest, IsGraphmeBreak_SpacingMark)
{
    // U+0903 (DEVANAGARI SIGN VISARGA) has SpacingMark grapheme property.
    const UChar32 kSpacingMark = 0x0903;

    // Grapheme Cluster Boundary Rule GB9a: x SpacingMark.
    EXPECT_FALSE(isGraphemeBreak('a', kSpacingMark));
}

// TODO(nona): Introduce tests for GB9b rule once ICU grabs Unicod 9.0.
// There is no character having Prepend grapheme property in Unicode 8.0.

TEST(StateMachineUtilTest, IsGraphmeBreak_EmojiModifier)
{
    // U+261D (WHITE UP POINTING INDEX) has E_Base grapheme property.
    const UChar32 kEBase = 0x261D;
    // U+1F466 (BOY) has E_Base_GAZ grapheme property.
    const UChar32 kEBaseGAZ = 0x1F466;
    // U+1F3FB (EMOJI MODIFIER FITZPATRICK TYPE-1-2) has E_Modifier grapheme
    // property.
    const UChar32 kEModifier = 0x1F3FB;

    // Grapheme Cluster Boundary Rule GB10: (E_Base, E_Base_GAZ) x E_Modifier
    EXPECT_FALSE(isGraphemeBreak(kEBase, kEModifier));
    EXPECT_FALSE(isGraphemeBreak(kEBaseGAZ, kEModifier));
    EXPECT_FALSE(isGraphemeBreak(kEBase, kEModifier));

    EXPECT_TRUE(isGraphemeBreak(kEBase, kEBase));
    EXPECT_TRUE(isGraphemeBreak(kEBaseGAZ, kEBase));
    EXPECT_TRUE(isGraphemeBreak(kEBase, kEBaseGAZ));
    EXPECT_TRUE(isGraphemeBreak(kEBaseGAZ, kEBaseGAZ));
    EXPECT_TRUE(isGraphemeBreak(kEModifier, kEModifier));
}

TEST(StateMachineUtilTest, IsGraphmeBreak_ZWJSequecne)
{
    // U+2764 (HEAVY BLACK HEART) has Glue_After_Zwj grapheme property.
    const UChar32 kGlueAfterZwj = 0x2764;
    // U+1F466 (BOY) has E_Base_GAZ grapheme property.
    const UChar32 kEBaseGAZ = 0x1F466;
    // U+1F5FA (WORLD MAP) doesn'T have Glue_After_Zwj or E_Base_GAZ property
    // but has Emoji property.
    const UChar32 kEmoji = 0x1F5FA;

    // Grapheme Cluster Boundary Rule GB11: ZWJ x (Glue_After_Zwj | EBG)
    EXPECT_FALSE(isGraphemeBreak(WTF::Unicode::zeroWidthJoinerCharacter,
        kGlueAfterZwj));
    EXPECT_FALSE(isGraphemeBreak(WTF::Unicode::zeroWidthJoinerCharacter,
        kEBaseGAZ));
    EXPECT_FALSE(isGraphemeBreak(WTF::Unicode::zeroWidthJoinerCharacter,
        kEmoji));

    EXPECT_TRUE(isGraphemeBreak(kGlueAfterZwj, kEBaseGAZ));
    EXPECT_TRUE(isGraphemeBreak(kGlueAfterZwj, kGlueAfterZwj));
    EXPECT_TRUE(isGraphemeBreak(kEBaseGAZ, kGlueAfterZwj));

    EXPECT_TRUE(isGraphemeBreak(WTF::Unicode::zeroWidthJoinerCharacter, 'a'));
}

TEST(StateMachineUtilTest, IsGraphmeBreak_IndicSyllabicCategoryVirama)
{
    // U+094D (DEVANAGARI SIGN VIRAMA) has Indic_Syllabic_Category=Virama
    // property.
    const UChar32 kVirama = 0x094D;

    // U+0915 (DEVANAGARI LETTER KA). Should not break after kVirama and before
    // this character.
    const UChar32 kDevangariKa = 0x0915;

    // Do not break after character having Indic_Syllabic_Category=Virama
    // property if following character has General_Category=C(Other) property.
    EXPECT_FALSE(isGraphemeBreak(kVirama, kDevangariKa));
}

} // namespace blink
