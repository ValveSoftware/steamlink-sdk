// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/state_machines/ForwardGraphemeBoundaryStateMachine.h"

#include "core/editing/state_machines/StateMachineTestUtil.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/CharacterNames.h"

namespace blink {

// Notations:
// | indicates inidicates initial offset position.
// SOT indicates start of text.
// EOT indicates end of text.
// [Lead] indicates broken lonely lead surrogate.
// [Trail] indicates broken lonely trail surrogate.
// [U] indicates regional indicator symbol U.
// [S] indicates regional indicator symbol S.

namespace {
// kWatch kVS16, kEye kVS16 are valid standardized variants.
const UChar32 kWatch = 0x231A;
const UChar32 kEye = WTF::Unicode::eyeCharacter;
const UChar32 kVS16 = 0xFE0F;

// kHanBMP KVS17, kHanSIP kVS17 are valie IVD sequences.
const UChar32 kHanBMP = 0x845B;
const UChar32 kHanSIP = 0x20000;
const UChar32 kVS17 = 0xE0100;

// Following lead/trail values are used for invalid surrogate pairs.
const UChar kLead = 0xD83D;
const UChar kTrail = 0xDC66;

// U+1F1FA is REGIONAL INDICATOR SYMBOL LETTER U
const UChar32 kRisU = 0x1F1FA;
// U+1F1F8 is REGIONAL INDICATOR SYMBOL LETTER S
const UChar32 kRisS = 0x1F1F8;
} // namespace

class ForwardGraphemeBoundaryStatemachineTest : public GraphemeStateMachineTestBase {
protected:
    ForwardGraphemeBoundaryStatemachineTest() = default;
    ~ForwardGraphemeBoundaryStatemachineTest() override = default;

private:
    DISALLOW_COPY_AND_ASSIGN(ForwardGraphemeBoundaryStatemachineTest);
};


TEST_F(ForwardGraphemeBoundaryStatemachineTest, DoNothingCase)
{
    ForwardGraphemeBoundaryStateMachine machine;

    EXPECT_EQ(0, machine.finalizeAndGetBoundaryOffset());
    EXPECT_EQ(0, machine.finalizeAndGetBoundaryOffset());
}

TEST_F(ForwardGraphemeBoundaryStatemachineTest, PrecedingText)
{
    ForwardGraphemeBoundaryStateMachine machine;
    // Preceding text should not affect the result except for flags.
    // SOT + | + 'a' + 'a'
    EXPECT_EQ("SRF", processSequenceForward(&machine, asCodePoints(), asCodePoints('a', 'a')));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());
    // SOT + [U] + | + 'a' + 'a'
    EXPECT_EQ("RRSRF", processSequenceForward(&machine,
        asCodePoints(kRisU), asCodePoints('a', 'a')));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());
    // SOT + [U] + [S] + | + 'a' + 'a'
    EXPECT_EQ("RRRRSRF", processSequenceForward(&machine,
        asCodePoints(kRisU, kRisS), asCodePoints('a', 'a')));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());

    // U+0000 + | + 'a' + 'a'
    EXPECT_EQ("SRF", processSequenceForward(&machine, asCodePoints(0), asCodePoints('a', 'a')));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());
    // U+0000 + [U] + | + 'a' + 'a'
    EXPECT_EQ("RRSRF", processSequenceForward(&machine,
        asCodePoints(0, kRisU), asCodePoints('a', 'a')));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());
    // U+0000 + [U] + [S] + | + 'a' + 'a'
    EXPECT_EQ("RRRRSRF", processSequenceForward(&machine,
        asCodePoints(0, kRisU, kRisS), asCodePoints('a', 'a')));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());

    // 'a' + | + 'a' + 'a'
    EXPECT_EQ("SRF", processSequenceForward(&machine, asCodePoints('a'), asCodePoints('a', 'a')));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());
    // 'a' + [U] + | + 'a' + 'a'
    EXPECT_EQ("RRSRF", processSequenceForward(&machine,
        asCodePoints('a', kRisU), asCodePoints('a', 'a')));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());
    // 'a' + [U] + [S] + | + 'a' + 'a'
    EXPECT_EQ("RRRRSRF", processSequenceForward(&machine,
        asCodePoints('a', kRisU, kRisS), asCodePoints('a', 'a')));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());

    // U+1F441 + | + 'a' + 'a'
    EXPECT_EQ("RSRF", processSequenceForward(&machine,
        asCodePoints(kEye), asCodePoints('a', 'a')));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());
    // U+1F441 + [U] + | + 'a' + 'a'
    EXPECT_EQ("RRRSRF", processSequenceForward(&machine,
        asCodePoints(kEye, kRisU), asCodePoints('a', 'a')));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());
    // U+1F441 + [U] + [S] + | + 'a' + 'a'
    EXPECT_EQ("RRRRRSRF", processSequenceForward(&machine,
        asCodePoints(kEye, kRisU, kRisS), asCodePoints('a', 'a')));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());

    // Broken surrogates in preceding text.

    // [Lead] + | + 'a' + 'a'
    EXPECT_EQ("SRF", processSequenceForward(&machine,
        asCodePoints(kLead), asCodePoints('a', 'a')));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());
    // [Lead] + [U] + | + 'a' + 'a'
    EXPECT_EQ("RRSRF", processSequenceForward(&machine,
        asCodePoints(kLead, kRisU), asCodePoints('a', 'a')));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());
    // [Lead] + [U] + [S] + | + 'a' + 'a'
    EXPECT_EQ("RRRRSRF", processSequenceForward(&machine,
        asCodePoints(kLead, kRisU, kRisS), asCodePoints('a', 'a')));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());

    // 'a' + [Trail] + | + 'a' + 'a'
    EXPECT_EQ("RSRF", processSequenceForward(&machine,
        asCodePoints('a', kTrail), asCodePoints('a', 'a')));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());
    // 'a' + [Trail] + [U] + | + 'a' + 'a'
    EXPECT_EQ("RRRSRF", processSequenceForward(&machine,
        asCodePoints('a', kTrail, kRisU), asCodePoints('a', 'a')));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());
    // 'a' + [Trail] + [U] + [S] + | + 'a' + 'a'
    EXPECT_EQ("RRRRRSRF", processSequenceForward(&machine,
        asCodePoints('a', kTrail, kRisU, kRisS), asCodePoints('a', 'a')));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());

    // [Trail] + [Trail] + | + 'a' + 'a'
    EXPECT_EQ("RSRF", processSequenceForward(&machine,
        asCodePoints(kTrail, kTrail), asCodePoints('a', 'a')));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());
    // [Trail] + [Trail] + [U] + | + 'a' + 'a'
    EXPECT_EQ("RRRSRF", processSequenceForward(&machine,
        asCodePoints(kTrail, kTrail, kRisU), asCodePoints('a', 'a')));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());
    // [Trail] + [Trail] + [U] + [S] + | + 'a' + 'a'
    EXPECT_EQ("RRRRRSRF", processSequenceForward(&machine,
        asCodePoints(kTrail, kTrail, kRisU, kRisS), asCodePoints('a', 'a')));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());

    // SOT + [Trail] + | + 'a' + 'a'
    EXPECT_EQ("RSRF", processSequenceForward(&machine,
        asCodePoints(kTrail), asCodePoints('a', 'a')));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());
    // SOT + [Trail] + [U] + | + 'a' + 'a'
    EXPECT_EQ("RRRSRF", processSequenceForward(&machine,
        asCodePoints(kTrail, kRisU), asCodePoints('a', 'a')));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());
    // SOT + [Trail] + [U] + [S] + | + 'a' + 'a'
    EXPECT_EQ("RRRRRSRF", processSequenceForward(&machine,
        asCodePoints(kTrail, kRisU, kRisS), asCodePoints('a', 'a')));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());
}

TEST_F(ForwardGraphemeBoundaryStatemachineTest, BrokenSurrogatePair)
{
    ForwardGraphemeBoundaryStateMachine machine;
    // SOT + | + [Trail]
    EXPECT_EQ("SF", processSequenceForward(&machine, asCodePoints(), asCodePoints(kTrail)));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());
    // SOT + | + [Lead] + 'a'
    EXPECT_EQ("SRF", processSequenceForward(&machine, asCodePoints(), asCodePoints(kLead, 'a')));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());
    // SOT + | + [Lead] + [Lead]
    EXPECT_EQ("SRF", processSequenceForward(&machine, asCodePoints(),
        asCodePoints(kLead, kLead)));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());
    // SOT + | + [Lead] + EOT
    EXPECT_EQ("SR", processSequenceForward(&machine, asCodePoints(), asCodePoints(kLead)));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());
}

TEST_F(ForwardGraphemeBoundaryStatemachineTest, BreakImmediately_BMP)
{
    ForwardGraphemeBoundaryStateMachine machine;

    // SOT + | + U+0000 + U+0000
    EXPECT_EQ("SRF", processSequenceForward(&machine, asCodePoints(), asCodePoints(0, 0)));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + 'a' + 'a'
    EXPECT_EQ("SRF", processSequenceForward(&machine, asCodePoints(), asCodePoints('a', 'a')));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + 'a' + U+1F441
    EXPECT_EQ("SRRF", processSequenceForward(&machine, asCodePoints(), asCodePoints('a', kEye)));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + 'a' + EOT
    EXPECT_EQ("SR", processSequenceForward(&machine, asCodePoints(), asCodePoints('a')));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + 'a' + [Trail]
    EXPECT_EQ("SRF", processSequenceForward(&machine, asCodePoints(), asCodePoints('a', kTrail)));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + 'a' + [Lead] + 'a'
    EXPECT_EQ("SRRF", processSequenceForward(&machine, asCodePoints(),
        asCodePoints('a', kLead, 'a')));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + 'a' + [Lead] + [Lead]
    EXPECT_EQ("SRRF", processSequenceForward(&machine, asCodePoints(),
        asCodePoints('a', kLead, kLead)));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + 'a' + [Lead] + EOT
    EXPECT_EQ("SRR", processSequenceForward(&machine, asCodePoints(), asCodePoints('a', kLead)));
    EXPECT_EQ(1, machine.finalizeAndGetBoundaryOffset());
}

TEST_F(ForwardGraphemeBoundaryStatemachineTest, BreakImmediately_Supplementary)
{
    ForwardGraphemeBoundaryStateMachine machine;

    // SOT + | + U+1F441 + 'a'
    EXPECT_EQ("SRRF", processSequenceForward(&machine, asCodePoints(), asCodePoints(kEye, 'a')));
    EXPECT_EQ(2, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+1F441 + U+1F441
    EXPECT_EQ("SRRRF", processSequenceForward(&machine, asCodePoints(),
        asCodePoints(kEye, kEye)));
    EXPECT_EQ(2, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+1F441 + EOT
    EXPECT_EQ("SRR", processSequenceForward(&machine, asCodePoints(), asCodePoints(kEye)));
    EXPECT_EQ(2, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+1F441 + [Trail]
    EXPECT_EQ("SRRF", processSequenceForward(&machine, asCodePoints(),
        asCodePoints(kEye, kTrail)));
    EXPECT_EQ(2, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+1F441 + [Lead] + 'a'
    EXPECT_EQ("SRRRF", processSequenceForward(&machine, asCodePoints(),
        asCodePoints(kEye, kLead, 'a')));
    EXPECT_EQ(2, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+1F441 + [Lead] + [Lead]
    EXPECT_EQ("SRRRF", processSequenceForward(&machine, asCodePoints(),
        asCodePoints(kEye, kLead, kLead)));
    EXPECT_EQ(2, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+1F441 + [Lead] + EOT
    EXPECT_EQ("SRRR", processSequenceForward(&machine, asCodePoints(),
        asCodePoints(kEye, kLead)));
    EXPECT_EQ(2, machine.finalizeAndGetBoundaryOffset());
}

TEST_F(ForwardGraphemeBoundaryStatemachineTest, NotBreakImmediatelyAfter_BMP_BMP)
{
    ForwardGraphemeBoundaryStateMachine machine;

    // SOT + | + U+231A + U+FE0F + 'a'
    EXPECT_EQ("SRRF", processSequenceForward(&machine, asCodePoints(),
        asCodePoints(kWatch, kVS16, 'a')));
    EXPECT_EQ(2, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+231A + U+FE0F + U+1F441
    EXPECT_EQ("SRRRF", processSequenceForward(&machine, asCodePoints(),
        asCodePoints(kWatch, kVS16, kEye)));
    EXPECT_EQ(2, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+231A + U+FE0F + EOT
    EXPECT_EQ("SRR", processSequenceForward(&machine, asCodePoints(),
        asCodePoints(kWatch, kVS16)));
    EXPECT_EQ(2, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+231A + U+FE0F + [Trail]
    EXPECT_EQ("SRRF", processSequenceForward(&machine, asCodePoints(),
        asCodePoints(kWatch, kVS16, kTrail)));
    EXPECT_EQ(2, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+231A + U+FE0F + [Lead] + 'a'
    EXPECT_EQ("SRRRF", processSequenceForward(&machine, asCodePoints(),
        asCodePoints(kWatch, kVS16, kLead, 'a')));
    EXPECT_EQ(2, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+231A + U+FE0F + [Lead] + [Lead]
    EXPECT_EQ("SRRRF", processSequenceForward(&machine, asCodePoints(),
        asCodePoints(kWatch, kVS16, kLead, kLead)));
    EXPECT_EQ(2, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+231A + U+FE0F + [Lead] + EOT
    EXPECT_EQ("SRRR", processSequenceForward(&machine, asCodePoints(),
        asCodePoints(kWatch, kVS16, kLead)));
    EXPECT_EQ(2, machine.finalizeAndGetBoundaryOffset());
}

TEST_F(ForwardGraphemeBoundaryStatemachineTest,
    NotBreakImmediatelyAfter_Supplementary_BMP)
{
    ForwardGraphemeBoundaryStateMachine machine;

    // SOT + | + U+1F441 + U+FE0F + 'a'
    EXPECT_EQ("SRRRF", processSequenceForward(&machine, asCodePoints(),
        asCodePoints(kEye, kVS16, 'a')));
    EXPECT_EQ(3, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+1F441 + U+FE0F + U+1F441
    EXPECT_EQ("SRRRRF", processSequenceForward(&machine, asCodePoints(),
        asCodePoints(kEye, kVS16, kEye)));
    EXPECT_EQ(3, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+1F441 + U+FE0F + EOT
    EXPECT_EQ("SRRR", processSequenceForward(&machine, asCodePoints(),
        asCodePoints(kEye, kVS16)));
    EXPECT_EQ(3, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+1F441 + U+FE0F + [Trail]
    EXPECT_EQ("SRRRF", processSequenceForward(&machine, asCodePoints(),
        asCodePoints(kEye, kVS16, kTrail)));
    EXPECT_EQ(3, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+1F441 + U+FE0F + [Lead] + 'a'
    EXPECT_EQ("SRRRRF", processSequenceForward(&machine, asCodePoints(),
        asCodePoints(kEye, kVS16, kLead, 'a')));
    EXPECT_EQ(3, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+1F441 + U+FE0F + [Lead] + [Lead]
    EXPECT_EQ("SRRRRF", processSequenceForward(&machine, asCodePoints(),
        asCodePoints(kEye, kVS16, kLead, kLead)));
    EXPECT_EQ(3, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+1F441 + U+FE0F + [Lead] + EOT
    EXPECT_EQ("SRRRR", processSequenceForward(&machine, asCodePoints(),
        asCodePoints(kEye, kVS16, kLead)));
    EXPECT_EQ(3, machine.finalizeAndGetBoundaryOffset());
}

TEST_F(ForwardGraphemeBoundaryStatemachineTest,
    NotBreakImmediatelyAfter_BMP_Supplementary)
{
    ForwardGraphemeBoundaryStateMachine machine;

    // SOT + | + U+845B + U+E0100 + 'a'
    EXPECT_EQ("SRRRF", processSequenceForward(&machine, asCodePoints(),
        asCodePoints(kHanBMP, kVS17, 'a')));
    EXPECT_EQ(3, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+845B + U+E0100 + U+1F441
    EXPECT_EQ("SRRRRF", processSequenceForward(&machine, asCodePoints(),
        asCodePoints(kHanBMP, kVS17, kEye)));
    EXPECT_EQ(3, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+845B + U+E0100 + EOT
    EXPECT_EQ("SRRR", processSequenceForward(&machine, asCodePoints(),
        asCodePoints(kHanBMP, kVS17)));
    EXPECT_EQ(3, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+845B + U+E0100 + [Trail]
    EXPECT_EQ("SRRRF", processSequenceForward(&machine, asCodePoints(),
        asCodePoints(kHanBMP, kVS17, kTrail)));
    EXPECT_EQ(3, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+845B + U+E0100 + [Lead] + 'a'
    EXPECT_EQ("SRRRRF", processSequenceForward(&machine, asCodePoints(),
        asCodePoints(kHanBMP, kVS17, kLead, 'a')));
    EXPECT_EQ(3, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+845B + U+E0100 + [Lead] + [Lead]
    EXPECT_EQ("SRRRRF", processSequenceForward(&machine, asCodePoints(),
        asCodePoints(kHanBMP, kVS17, kLead, kLead)));
    EXPECT_EQ(3, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+845B + U+E0100 + [Lead] + EOT
    EXPECT_EQ("SRRRR", processSequenceForward(&machine, asCodePoints(),
        asCodePoints(kHanBMP, kVS17, kLead)));
    EXPECT_EQ(3, machine.finalizeAndGetBoundaryOffset());
}

TEST_F(ForwardGraphemeBoundaryStatemachineTest,
    NotBreakImmediatelyAfter_Supplementary_Supplementary)
{
    ForwardGraphemeBoundaryStateMachine machine;

    // SOT + | + U+20000 + U+E0100 + 'a'
    EXPECT_EQ("SRRRRF", processSequenceForward(&machine, asCodePoints(),
        asCodePoints(kHanSIP, kVS17, 'a')));
    EXPECT_EQ(4, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+20000 + U+E0100 + U+1F441
    EXPECT_EQ("SRRRRRF", processSequenceForward(&machine, asCodePoints(),
        asCodePoints(kHanSIP, kVS17, kEye)));
    EXPECT_EQ(4, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+20000 + U+E0100 + EOT
    EXPECT_EQ("SRRRR", processSequenceForward(&machine, asCodePoints(),
        asCodePoints(kHanSIP, kVS17)));
    EXPECT_EQ(4, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+20000 + U+E0100 + [Trail]
    EXPECT_EQ("SRRRRF", processSequenceForward(&machine, asCodePoints(),
        asCodePoints(kHanSIP, kVS17, kTrail)));
    EXPECT_EQ(4, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+20000 + U+E0100 + [Lead] + 'a'
    EXPECT_EQ("SRRRRRF", processSequenceForward(&machine, asCodePoints(),
        asCodePoints(kHanSIP, kVS17, kLead, 'a')));
    EXPECT_EQ(4, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+20000 + U+E0100 + [Lead] + [Lead]
    EXPECT_EQ("SRRRRRF", processSequenceForward(&machine, asCodePoints(),
        asCodePoints(kHanSIP, kVS17, kLead, kLead)));
    EXPECT_EQ(4, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + U+20000 + U+E0100 + [Lead] + EOT
    EXPECT_EQ("SRRRRR", processSequenceForward(&machine, asCodePoints(),
        asCodePoints(kHanSIP, kVS17, kLead)));
    EXPECT_EQ(4, machine.finalizeAndGetBoundaryOffset());
}

TEST_F(ForwardGraphemeBoundaryStatemachineTest, MuchLongerCase)
{
    ForwardGraphemeBoundaryStateMachine machine;

    const UChar32 kMan = WTF::Unicode::manCharacter;
    const UChar32 kZwj = WTF::Unicode::zeroWidthJoinerCharacter;
    const UChar32 kHeart = WTF::Unicode::heavyBlackHeartCharacter;
    const UChar32 kKiss = WTF::Unicode::kissMarkCharacter;

    // U+1F468 U+200D U+2764 U+FE0F U+200D U+1F48B U+200D U+1F468 is a valid ZWJ
    // emoji sequence.
    // SOT + | + ZWJ Emoji Sequence + 'a'
    EXPECT_EQ("SRRRRRRRRRRRF", processSequenceForward(&machine, asCodePoints(),
        asCodePoints(kMan, kZwj, kHeart, kVS16, kZwj, kKiss, kZwj, kMan, 'a')));
    EXPECT_EQ(11, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + ZWJ Emoji Sequence + U+1F441
    EXPECT_EQ("SRRRRRRRRRRRRF", processSequenceForward(&machine, asCodePoints(),
        asCodePoints(kMan, kZwj, kHeart, kVS16, kZwj, kKiss, kZwj, kMan, kEye)));
    EXPECT_EQ(11, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + ZWJ Emoji Sequence + EOT
    EXPECT_EQ("SRRRRRRRRRRR", processSequenceForward(&machine, asCodePoints(),
        asCodePoints(kMan, kZwj, kHeart, kVS16, kZwj, kKiss, kZwj, kMan)));
    EXPECT_EQ(11, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + ZWJ Emoji Sequence + [Trail]
    EXPECT_EQ("SRRRRRRRRRRRF", processSequenceForward(&machine, asCodePoints(),
        asCodePoints(kMan, kZwj, kHeart, kVS16, kZwj, kKiss, kZwj, kMan, kTrail)));
    EXPECT_EQ(11, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + ZWJ Emoji Sequence + [Lead] + 'a'
    EXPECT_EQ("SRRRRRRRRRRRRF", processSequenceForward(&machine, asCodePoints(),
        asCodePoints(kMan, kZwj, kHeart, kVS16, kZwj, kKiss, kZwj, kMan, kLead, 'a')));
    EXPECT_EQ(11, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + ZWJ Emoji Sequence + [Lead] + [Lead]
    EXPECT_EQ("SRRRRRRRRRRRRF", processSequenceForward(&machine, asCodePoints(),
        asCodePoints(kMan, kZwj, kHeart, kVS16, kZwj, kKiss, kZwj, kMan, kLead, kLead)));
    EXPECT_EQ(11, machine.finalizeAndGetBoundaryOffset());

    // SOT + | + ZWJ Emoji Sequence + [Lead] + EOT
    EXPECT_EQ("SRRRRRRRRRRRR", processSequenceForward(&machine, asCodePoints(),
        asCodePoints(kMan, kZwj, kHeart, kVS16, kZwj, kKiss, kZwj, kMan, kLead)));
    EXPECT_EQ(11, machine.finalizeAndGetBoundaryOffset());

    // Preceding text should not affect the result except for flags.
    // 'a' + | + ZWJ Emoji Sequence + [Lead] + EOT
    EXPECT_EQ("SRRRRRRRRRRRF", processSequenceForward(&machine,
        asCodePoints('a'),
        asCodePoints(kMan, kZwj, kHeart, kVS16, kZwj, kKiss, kZwj, kMan, 'a')));
    EXPECT_EQ(11, machine.finalizeAndGetBoundaryOffset());

    // U+1F441 + | + ZWJ Emoji Sequence + [Lead] + EOT
    EXPECT_EQ("RSRRRRRRRRRRRF", processSequenceForward(&machine,
        asCodePoints(kEye),
        asCodePoints(kMan, kZwj, kHeart, kVS16, kZwj, kKiss, kZwj, kMan, 'a')));
    EXPECT_EQ(11, machine.finalizeAndGetBoundaryOffset());

    // [Lead] + | + ZWJ Emoji Sequence + [Lead] + EOT
    EXPECT_EQ("SRRRRRRRRRRRF", processSequenceForward(&machine,
        asCodePoints(kLead),
        asCodePoints(kMan, kZwj, kHeart, kVS16, kZwj, kKiss, kZwj, kMan, 'a')));
    EXPECT_EQ(11, machine.finalizeAndGetBoundaryOffset());

    // 'a' + [Trail] + | + ZWJ Emoji Sequence + [Lead] + EOT
    EXPECT_EQ("RSRRRRRRRRRRRF", processSequenceForward(&machine,
        asCodePoints('a', kTrail),
        asCodePoints(kMan, kZwj, kHeart, kVS16, kZwj, kKiss, kZwj, kMan, 'a')));
    EXPECT_EQ(11, machine.finalizeAndGetBoundaryOffset());

    // [Trail] + [Trail] + | + ZWJ Emoji Sequence + [Lead] + EOT
    EXPECT_EQ("RSRRRRRRRRRRRF", processSequenceForward(&machine,
        asCodePoints(kTrail, kTrail),
        asCodePoints(kMan, kZwj, kHeart, kVS16, kZwj, kKiss, kZwj, kMan, 'a')));
    EXPECT_EQ(11, machine.finalizeAndGetBoundaryOffset());

    // SOT + [Trail] + | + ZWJ Emoji Sequence + [Lead] + EOT
    EXPECT_EQ("RSRRRRRRRRRRRF", processSequenceForward(&machine,
        asCodePoints(kTrail),
        asCodePoints(kMan, kZwj, kHeart, kVS16, kZwj, kKiss, kZwj, kMan, 'a')));
    EXPECT_EQ(11, machine.finalizeAndGetBoundaryOffset());

    // 'a' + [U] + | + ZWJ Emoji Sequence + [Lead] + EOT
    EXPECT_EQ("RRSRRRRRRRRRRRF", processSequenceForward(&machine,
        asCodePoints('a', kRisU),
        asCodePoints(kMan, kZwj, kHeart, kVS16, kZwj, kKiss, kZwj, kMan, 'a')));
    EXPECT_EQ(11, machine.finalizeAndGetBoundaryOffset());

    // 'a' + [U] + [S] + | + ZWJ Emoji Sequence + [Lead] + EOT
    EXPECT_EQ("RRRRSRRRRRRRRRRRF", processSequenceForward(&machine,
        asCodePoints('a', kRisU, kRisS),
        asCodePoints(kMan, kZwj, kHeart, kVS16, kZwj, kKiss, kZwj, kMan, 'a')));
    EXPECT_EQ(11, machine.finalizeAndGetBoundaryOffset());

}

TEST_F(ForwardGraphemeBoundaryStatemachineTest, singleFlags)
{
    ForwardGraphemeBoundaryStateMachine machine;

    // SOT + | + [U] + [S]
    EXPECT_EQ("SRRRF", processSequenceForward(&machine,
        asCodePoints(), asCodePoints(kRisU, kRisS)));
    EXPECT_EQ(4, machine.finalizeAndGetBoundaryOffset());

    // 'a' + | + [U] + [S]
    EXPECT_EQ("SRRRF", processSequenceForward(&machine,
        asCodePoints('a'), asCodePoints(kRisU, kRisS)));
    EXPECT_EQ(4, machine.finalizeAndGetBoundaryOffset());

    // U+1F441 + | + [U] + [S]
    EXPECT_EQ("RSRRRF", processSequenceForward(&machine,
        asCodePoints(kEye), asCodePoints(kRisU, kRisS)));
    EXPECT_EQ(4, machine.finalizeAndGetBoundaryOffset());

    // [Lead] + | + [U] + [S]
    EXPECT_EQ("SRRRF", processSequenceForward(&machine,
        asCodePoints(kLead), asCodePoints(kRisU, kRisS)));
    EXPECT_EQ(4, machine.finalizeAndGetBoundaryOffset());

    // 'a' + [Trail] + | + [U] + [S]
    EXPECT_EQ("RSRRRF", processSequenceForward(&machine,
        asCodePoints('a', kTrail), asCodePoints(kRisU, kRisS)));
    EXPECT_EQ(4, machine.finalizeAndGetBoundaryOffset());

    // [Trail] + [Trail] + | + [U] + [S]
    EXPECT_EQ("RSRRRF", processSequenceForward(&machine,
        asCodePoints(kTrail, kTrail), asCodePoints(kRisU, kRisS)));
    EXPECT_EQ(4, machine.finalizeAndGetBoundaryOffset());

    // SOT + [Trail] + | + [U] + [S]
    EXPECT_EQ("RSRRRF", processSequenceForward(&machine,
        asCodePoints(kTrail), asCodePoints(kRisU, kRisS)));
    EXPECT_EQ(4, machine.finalizeAndGetBoundaryOffset());
}

TEST_F(ForwardGraphemeBoundaryStatemachineTest, twoFlags)
{
    ForwardGraphemeBoundaryStateMachine machine;

    // SOT + [U] + [S] + | + [U] + [S]
    EXPECT_EQ("RRRRSRRRF", processSequenceForward(&machine,
        asCodePoints(kRisU, kRisS), asCodePoints(kRisU, kRisS)));
    EXPECT_EQ(4, machine.finalizeAndGetBoundaryOffset());

    // 'a' + [U] + [S] + | + [U] + [S]
    EXPECT_EQ("RRRRSRRRF", processSequenceForward(&machine,
        asCodePoints('a', kRisU, kRisS), asCodePoints(kRisU, kRisS)));
    EXPECT_EQ(4, machine.finalizeAndGetBoundaryOffset());

    // U+1F441 + [U] + [S] + | + [U] + [S]
    EXPECT_EQ("RRRRRSRRRF", processSequenceForward(&machine,
        asCodePoints(kEye, kRisU, kRisS), asCodePoints(kRisU, kRisS)));
    EXPECT_EQ(4, machine.finalizeAndGetBoundaryOffset());

    // [Lead] + [U] + [S] + | + [U] + [S]
    EXPECT_EQ("RRRRSRRRF", processSequenceForward(&machine,
        asCodePoints(kLead, kRisU, kRisS), asCodePoints(kRisU, kRisS)));
    EXPECT_EQ(4, machine.finalizeAndGetBoundaryOffset());

    // 'a' + [Trail] + [U] + [S] + | + [U] + [S]
    EXPECT_EQ("RRRRRSRRRF", processSequenceForward(&machine,
        asCodePoints('a', kTrail, kRisU, kRisS), asCodePoints(kRisU, kRisS)));
    EXPECT_EQ(4, machine.finalizeAndGetBoundaryOffset());

    // [Trail] + [Trail] + [U] + [S] + | + [U] + [S]
    EXPECT_EQ("RRRRRSRRRF", processSequenceForward(&machine,
        asCodePoints(kTrail, kTrail, kRisU, kRisS), asCodePoints(kRisU, kRisS)));
    EXPECT_EQ(4, machine.finalizeAndGetBoundaryOffset());

    // SOT + [Trail] + [U] + [S] + | + [U] + [S]
    EXPECT_EQ("RRRRRSRRRF", processSequenceForward(&machine,
        asCodePoints(kTrail, kRisU, kRisS), asCodePoints(kRisU, kRisS)));
    EXPECT_EQ(4, machine.finalizeAndGetBoundaryOffset());
}

TEST_F(ForwardGraphemeBoundaryStatemachineTest, oddNumberedFlags)
{
    ForwardGraphemeBoundaryStateMachine machine;

    // SOT + [U] + | + [S] + [S]
    EXPECT_EQ("RRSRRRF", processSequenceForward(&machine,
        asCodePoints(kRisU), asCodePoints(kRisS, kRisU)));
    EXPECT_EQ(2, machine.finalizeAndGetBoundaryOffset());

    // 'a' + [U] + | + [S] + [S]
    EXPECT_EQ("RRSRRRF", processSequenceForward(&machine,
        asCodePoints('a', kRisU), asCodePoints(kRisS, kRisU)));
    EXPECT_EQ(2, machine.finalizeAndGetBoundaryOffset());

    // U+1F441 + [U] + | + [S] + [S]
    EXPECT_EQ("RRRSRRRF", processSequenceForward(&machine,
        asCodePoints(kEye, kRisU), asCodePoints(kRisS, kRisU)));
    EXPECT_EQ(2, machine.finalizeAndGetBoundaryOffset());

    // [Lead] + [U] + | + [S] + [S]
    EXPECT_EQ("RRSRRRF", processSequenceForward(&machine,
        asCodePoints(kLead, kRisU), asCodePoints(kRisS, kRisU)));
    EXPECT_EQ(2, machine.finalizeAndGetBoundaryOffset());

    // 'a' + [Trail] + [U] + | + [S] + [S]
    EXPECT_EQ("RRRSRRRF", processSequenceForward(&machine,
        asCodePoints('a', kTrail, kRisU), asCodePoints(kRisS, kRisU)));
    EXPECT_EQ(2, machine.finalizeAndGetBoundaryOffset());

    // [Trail] + [Trail] + [U] + | + [S] + [S]
    EXPECT_EQ("RRRSRRRF", processSequenceForward(&machine,
        asCodePoints(kTrail, kTrail, kRisU), asCodePoints(kRisS, kRisU)));
    EXPECT_EQ(2, machine.finalizeAndGetBoundaryOffset());

    // SOT + [Trail] + [U] + | + [S] + [S]
    EXPECT_EQ("RRRSRRRF", processSequenceForward(&machine,
        asCodePoints(kTrail, kRisU), asCodePoints(kRisS, kRisU)));
    EXPECT_EQ(2, machine.finalizeAndGetBoundaryOffset());
}

} // namespace blink
