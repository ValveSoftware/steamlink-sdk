// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/state_machines/BackwardGraphemeBoundaryStateMachine.h"

#include "core/editing/state_machines/StateMachineTestUtil.h"
#include "wtf/text/CharacterNames.h"

namespace blink {

// Notations:
// SOT indicates start of text.
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
// U+1F1F8 is REGIONAL INDICATOR SYMBOL LETTER S
const UChar32 kRisU = 0x1F1FA;
const UChar32 kRisS = 0x1F1F8;
}  // namespace

class BackwardGraphemeBoundaryStatemachineTest
    : public GraphemeStateMachineTestBase {
 protected:
  BackwardGraphemeBoundaryStatemachineTest() = default;
  ~BackwardGraphemeBoundaryStatemachineTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(BackwardGraphemeBoundaryStatemachineTest);
};

TEST_F(BackwardGraphemeBoundaryStatemachineTest, DoNothingCase) {
  BackwardGraphemeBoundaryStateMachine machine;

  EXPECT_EQ(0, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(0, machine.finalizeAndGetBoundaryOffset());
}

TEST_F(BackwardGraphemeBoundaryStatemachineTest, BrokenSurrogatePair) {
  BackwardGraphemeBoundaryStateMachine machine;

  // [Lead]
  EXPECT_EQ("F", processSequenceBackward(&machine, asCodePoints(kLead)));
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

  // 'a' + [Trail]
  EXPECT_EQ("RF", processSequenceBackward(&machine, asCodePoints('a', kTrail)));
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

  // [Trail] + [Trail]
  EXPECT_EQ("RF",
            processSequenceBackward(&machine, asCodePoints(kTrail, kTrail)));
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

  // SOT + [Trail]
  EXPECT_EQ("RF", processSequenceBackward(&machine, asCodePoints(kTrail)));
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
}

TEST_F(BackwardGraphemeBoundaryStatemachineTest, BreakImmediately_BMP) {
  BackwardGraphemeBoundaryStateMachine machine;

  // U+0000 + U+0000
  EXPECT_EQ("RF", processSequenceBackward(&machine, asCodePoints(0, 0)));
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

  // 'a' + 'a'
  EXPECT_EQ("RF", processSequenceBackward(&machine, asCodePoints('a', 'a')));
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

  // U+1F441 + 'a'
  EXPECT_EQ("RRF", processSequenceBackward(&machine, asCodePoints(kEye, 'a')));
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

  // SOT + 'a'
  EXPECT_EQ("RF", processSequenceBackward(&machine, asCodePoints('a')));
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

  // Broken surrogates.
  // [Lead] + 'a'
  EXPECT_EQ("RF", processSequenceBackward(&machine, asCodePoints(kLead, 'a')));
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

  // 'a' + [Trail] + 'a'
  EXPECT_EQ("RRF",
            processSequenceBackward(&machine, asCodePoints('a', kTrail, 'a')));
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

  // [Trail] + [Trail] + 'a'
  EXPECT_EQ("RRF", processSequenceBackward(&machine,
                                           asCodePoints(kTrail, kTrail, 'a')));
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

  // SOT + [Trail] + 'a'
  EXPECT_EQ("RRF",
            processSequenceBackward(&machine, asCodePoints(kTrail, 'a')));
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
}

TEST_F(BackwardGraphemeBoundaryStatemachineTest,
       BreakImmediately_SupplementaryPlane) {
  BackwardGraphemeBoundaryStateMachine machine;

  // 'a' + U+1F441
  EXPECT_EQ("RRF", processSequenceBackward(&machine, asCodePoints('a', kEye)));
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

  // U+1F441 + U+1F441
  EXPECT_EQ("RRRF",
            processSequenceBackward(&machine, asCodePoints(kEye, kEye)));
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

  // SOT + U+1F441
  EXPECT_EQ("RRF", processSequenceBackward(&machine, asCodePoints(kEye)));
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

  // Broken surrogates.
  // [Lead] + U+1F441
  EXPECT_EQ("RRF",
            processSequenceBackward(&machine, asCodePoints(kLead, kEye)));
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

  // 'a' + [Trail] + U+1F441
  EXPECT_EQ("RRRF",
            processSequenceBackward(&machine, asCodePoints('a', kTrail, kEye)));
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

  // [Trail] + [Trail] + U+1F441
  EXPECT_EQ("RRRF", processSequenceBackward(
                        &machine, asCodePoints(kTrail, kTrail, kEye)));
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

  // SOT + [Trail] + U+1F441
  EXPECT_EQ("RRRF",
            processSequenceBackward(&machine, asCodePoints(kTrail, kEye)));
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());
}

TEST_F(BackwardGraphemeBoundaryStatemachineTest,
       NotBreakImmediatelyBefore_BMP_BMP) {
  BackwardGraphemeBoundaryStateMachine machine;

  // 'a' + U+231A + U+FE0F
  EXPECT_EQ("RRF", processSequenceBackward(&machine,
                                           asCodePoints('a', kWatch, kVS16)));
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

  // U+1F441 + U+231A + U+FE0F
  EXPECT_EQ("RRRF", processSequenceBackward(&machine,
                                            asCodePoints(kEye, kWatch, kVS16)));
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

  // SOT + U+231A + U+FE0F
  EXPECT_EQ("RRF",
            processSequenceBackward(&machine, asCodePoints(kWatch, kVS16)));
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

  // [Lead] + U+231A + U+FE0F
  EXPECT_EQ("RRF", processSequenceBackward(&machine,
                                           asCodePoints(kLead, kWatch, kVS16)));
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

  // 'a' + [Trail] + U+231A + U+FE0F
  EXPECT_EQ("RRRF", processSequenceBackward(
                        &machine, asCodePoints('a', kTrail, kWatch, kVS16)));
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

  // [Trail] + [Trail] + U+231A + U+FE0F
  EXPECT_EQ("RRRF", processSequenceBackward(
                        &machine, asCodePoints(kTrail, kTrail, kWatch, kVS16)));
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

  // SOT + [Trail] + U+231A + U+FE0F
  EXPECT_EQ("RRRF", processSequenceBackward(
                        &machine, asCodePoints(kTrail, kWatch, kVS16)));
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());
}

TEST_F(BackwardGraphemeBoundaryStatemachineTest,
       NotBreakImmediatelyBefore_Supplementary_BMP) {
  BackwardGraphemeBoundaryStateMachine machine;

  // 'a' + U+1F441 + U+FE0F
  EXPECT_EQ("RRRF",
            processSequenceBackward(&machine, asCodePoints('a', kEye, kVS16)));
  EXPECT_EQ(-3, machine.finalizeAndGetBoundaryOffset());

  // U+1F441 + U+1F441 + U+FE0F
  EXPECT_EQ("RRRRF",
            processSequenceBackward(&machine, asCodePoints(kEye, kEye, kVS16)));
  EXPECT_EQ(-3, machine.finalizeAndGetBoundaryOffset());

  // SOT + U+1F441 + U+FE0F
  EXPECT_EQ("RRRF",
            processSequenceBackward(&machine, asCodePoints(kEye, kVS16)));
  EXPECT_EQ(-3, machine.finalizeAndGetBoundaryOffset());

  // [Lead] + U+1F441 + U+FE0F
  EXPECT_EQ("RRRF", processSequenceBackward(&machine,
                                            asCodePoints(kLead, kEye, kVS16)));
  EXPECT_EQ(-3, machine.finalizeAndGetBoundaryOffset());

  // 'a' + [Trail] + U+1F441 + U+FE0F
  EXPECT_EQ("RRRRF", processSequenceBackward(
                         &machine, asCodePoints('a', kTrail, kEye, kVS16)));
  EXPECT_EQ(-3, machine.finalizeAndGetBoundaryOffset());

  // [Trail] + [Trail] + U+1F441 + U+FE0F
  EXPECT_EQ("RRRRF", processSequenceBackward(
                         &machine, asCodePoints(kTrail, kTrail, kEye, kVS16)));
  EXPECT_EQ(-3, machine.finalizeAndGetBoundaryOffset());

  // SOT + [Trail] + U+1F441 + U+FE0F
  EXPECT_EQ("RRRRF", processSequenceBackward(
                         &machine, asCodePoints(kTrail, kEye, kVS16)));
  EXPECT_EQ(-3, machine.finalizeAndGetBoundaryOffset());
}

TEST_F(BackwardGraphemeBoundaryStatemachineTest,
       NotBreakImmediatelyBefore_BMP_Supplementary) {
  BackwardGraphemeBoundaryStateMachine machine;

  // 'a' + U+845B + U+E0100
  EXPECT_EQ("RRRF", processSequenceBackward(&machine,
                                            asCodePoints('a', kHanBMP, kVS17)));
  EXPECT_EQ(-3, machine.finalizeAndGetBoundaryOffset());

  // U+1F441 + U+845B + U+E0100
  EXPECT_EQ("RRRRF", processSequenceBackward(
                         &machine, asCodePoints(kEye, kHanBMP, kVS17)));
  EXPECT_EQ(-3, machine.finalizeAndGetBoundaryOffset());

  // SOT + U+845B + U+E0100
  EXPECT_EQ("RRRF",
            processSequenceBackward(&machine, asCodePoints(kHanBMP, kVS17)));
  EXPECT_EQ(-3, machine.finalizeAndGetBoundaryOffset());

  // [Lead] + U+845B + U+E0100
  EXPECT_EQ("RRRF", processSequenceBackward(
                        &machine, asCodePoints(kLead, kHanBMP, kVS17)));
  EXPECT_EQ(-3, machine.finalizeAndGetBoundaryOffset());

  // 'a' + [Trail] + U+845B + U+E0100
  EXPECT_EQ("RRRRF", processSequenceBackward(
                         &machine, asCodePoints('a', kTrail, kHanBMP, kVS17)));
  EXPECT_EQ(-3, machine.finalizeAndGetBoundaryOffset());

  // [Trail] + [Trail] + U+845B + U+E0100
  EXPECT_EQ("RRRRF",
            processSequenceBackward(
                &machine, asCodePoints(kTrail, kTrail, kHanBMP, kVS17)));
  EXPECT_EQ(-3, machine.finalizeAndGetBoundaryOffset());

  // SOT + [Trail] + U+845B + U+E0100
  EXPECT_EQ("RRRRF", processSequenceBackward(
                         &machine, asCodePoints(kTrail, kHanBMP, kVS17)));
  EXPECT_EQ(-3, machine.finalizeAndGetBoundaryOffset());
}

TEST_F(BackwardGraphemeBoundaryStatemachineTest,
       NotBreakImmediatelyBefore_Supplementary_Supplementary) {
  BackwardGraphemeBoundaryStateMachine machine;

  // 'a' + U+20000 + U+E0100
  EXPECT_EQ("RRRRF", processSequenceBackward(
                         &machine, asCodePoints('a', kHanSIP, kVS17)));
  EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

  // U+1F441 + U+20000 + U+E0100
  EXPECT_EQ("RRRRRF", processSequenceBackward(
                          &machine, asCodePoints(kEye, kHanSIP, kVS17)));
  EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

  // SOT + U+20000 + U+E0100
  EXPECT_EQ("RRRRF",
            processSequenceBackward(&machine, asCodePoints(kHanSIP, kVS17)));
  EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

  // [Lead] + U+20000 + U+E0100
  EXPECT_EQ("RRRRF", processSequenceBackward(
                         &machine, asCodePoints(kLead, kHanSIP, kVS17)));
  EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

  // 'a' + [Trail] + U+20000 + U+E0100
  EXPECT_EQ("RRRRRF", processSequenceBackward(
                          &machine, asCodePoints('a', kTrail, kHanSIP, kVS17)));
  EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

  // [Trail] + [Trail] + U+20000 + U+E0100
  EXPECT_EQ("RRRRRF",
            processSequenceBackward(
                &machine, asCodePoints(kTrail, kTrail, kHanSIP, kVS17)));
  EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

  // SOT + [Trail] + U+20000 + U+E0100
  EXPECT_EQ("RRRRRF", processSequenceBackward(
                          &machine, asCodePoints(kTrail, kHanSIP, kVS17)));
  EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());
}

TEST_F(BackwardGraphemeBoundaryStatemachineTest, MuchLongerCase) {
  const UChar32 kMan = WTF::Unicode::manCharacter;
  const UChar32 kZwj = WTF::Unicode::zeroWidthJoinerCharacter;
  const UChar32 kHeart = WTF::Unicode::heavyBlackHeartCharacter;
  const UChar32 kKiss = WTF::Unicode::kissMarkCharacter;

  BackwardGraphemeBoundaryStateMachine machine;

  // U+1F468 U+200D U+2764 U+FE0F U+200D U+1F48B U+200D U+1F468 is a valid ZWJ
  // emoji sequence.
  // 'a' + ZWJ Emoji Sequence
  EXPECT_EQ("RRRRRRRRRRRF",
            processSequenceBackward(
                &machine, asCodePoints('a', kMan, kZwj, kHeart, kVS16, kZwj,
                                       kKiss, kZwj, kMan)));
  EXPECT_EQ(-11, machine.finalizeAndGetBoundaryOffset());

  // U+1F441 + ZWJ Emoji Sequence
  EXPECT_EQ("RRRRRRRRRRRRF",
            processSequenceBackward(
                &machine, asCodePoints(kEye, kMan, kZwj, kHeart, kVS16, kZwj,
                                       kKiss, kZwj, kMan)));
  EXPECT_EQ(-11, machine.finalizeAndGetBoundaryOffset());

  // SOT + ZWJ Emoji Sequence
  EXPECT_EQ(
      "RRRRRRRRRRRF",
      processSequenceBackward(&machine, asCodePoints(kMan, kZwj, kHeart, kVS16,
                                                     kZwj, kKiss, kZwj, kMan)));
  EXPECT_EQ(-11, machine.finalizeAndGetBoundaryOffset());

  // [Lead] + ZWJ Emoji Sequence
  EXPECT_EQ("RRRRRRRRRRRF",
            processSequenceBackward(
                &machine, asCodePoints(kLead, kMan, kZwj, kHeart, kVS16, kZwj,
                                       kKiss, kZwj, kMan)));
  EXPECT_EQ(-11, machine.finalizeAndGetBoundaryOffset());

  // 'a' + [Trail] + ZWJ Emoji Sequence
  EXPECT_EQ("RRRRRRRRRRRRF",
            processSequenceBackward(
                &machine, asCodePoints('a', kTrail, kMan, kZwj, kHeart, kVS16,
                                       kZwj, kKiss, kZwj, kMan)));
  EXPECT_EQ(-11, machine.finalizeAndGetBoundaryOffset());

  // [Trail] + [Trail] + ZWJ Emoji Sequence
  EXPECT_EQ("RRRRRRRRRRRRF",
            processSequenceBackward(
                &machine, asCodePoints(kTrail, kTrail, kMan, kZwj, kHeart,
                                       kVS16, kZwj, kKiss, kZwj, kMan)));
  EXPECT_EQ(-11, machine.finalizeAndGetBoundaryOffset());

  // SOT + [Trail] + ZWJ Emoji Sequence
  EXPECT_EQ("RRRRRRRRRRRRF",
            processSequenceBackward(
                &machine, asCodePoints(kTrail, kMan, kZwj, kHeart, kVS16, kZwj,
                                       kKiss, kZwj, kMan)));
  EXPECT_EQ(-11, machine.finalizeAndGetBoundaryOffset());
}

TEST_F(BackwardGraphemeBoundaryStatemachineTest, Flags_singleFlag) {
  BackwardGraphemeBoundaryStateMachine machine;

  // 'a' + [U] + [S]
  EXPECT_EQ("RRRRF",
            processSequenceBackward(&machine, asCodePoints('a', kRisU, kRisS)));
  EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

  // U+1F441 + [U] + [S]
  EXPECT_EQ("RRRRRF", processSequenceBackward(
                          &machine, asCodePoints(kEye, kRisU, kRisS)));
  EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

  // SOT + [U] + [S]
  EXPECT_EQ("RRRRF",
            processSequenceBackward(&machine, asCodePoints(kRisU, kRisS)));
  EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

  // [Lead] + [U] + [S]
  EXPECT_EQ("RRRRF", processSequenceBackward(
                         &machine, asCodePoints(kLead, kRisU, kRisS)));
  EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

  // 'a' + [Trail] + [U] + [S]
  EXPECT_EQ("RRRRRF", processSequenceBackward(
                          &machine, asCodePoints('a', kTrail, kRisU, kRisS)));
  EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

  // [Trail] + [Trail] + [U] + [S]
  EXPECT_EQ("RRRRRF",
            processSequenceBackward(
                &machine, asCodePoints(kTrail, kTrail, kRisU, kRisS)));
  EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

  // SOT + [Trail] + [U] + [S]
  EXPECT_EQ("RRRRRF", processSequenceBackward(
                          &machine, asCodePoints(kTrail, kRisU, kRisS)));
  EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());
}

TEST_F(BackwardGraphemeBoundaryStatemachineTest, Flags_twoFlags) {
  BackwardGraphemeBoundaryStateMachine machine;

  // 'a' + [U] + [S] + [U] + [S]
  EXPECT_EQ("RRRRRRRRF",
            processSequenceBackward(
                &machine, asCodePoints('a', kRisU, kRisS, kRisU, kRisS)));
  EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

  // U+1F441 + [U] + [S] + [U] + [S]
  EXPECT_EQ("RRRRRRRRRF",
            processSequenceBackward(
                &machine, asCodePoints(kEye, kRisU, kRisS, kRisU, kRisS)));
  EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

  // SOT + [U] + [S] + [U] + [S]
  EXPECT_EQ("RRRRRRRRF",
            processSequenceBackward(&machine,
                                    asCodePoints(kRisU, kRisS, kRisU, kRisS)));
  EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

  // [Lead] + [U] + [S] + [U] + [S]
  EXPECT_EQ("RRRRRRRRF",
            processSequenceBackward(
                &machine, asCodePoints(kLead, kRisU, kRisS, kRisU, kRisS)));
  EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

  // 'a' + [Trail] + [U] + [S] + [U] + [S]
  EXPECT_EQ("RRRRRRRRRF", processSequenceBackward(
                              &machine, asCodePoints('a', kTrail, kRisU, kRisS,
                                                     kRisU, kRisS)));
  EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

  // [Trail] + [Trail] + [U] + [S] + [U] + [S]
  EXPECT_EQ("RRRRRRRRRF", processSequenceBackward(
                              &machine, asCodePoints(kTrail, kTrail, kRisU,
                                                     kRisS, kRisU, kRisS)));
  EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

  // SOT + [Trail] + [U] + [S] + [U] + [S]
  EXPECT_EQ("RRRRRRRRRF",
            processSequenceBackward(
                &machine, asCodePoints(kTrail, kRisU, kRisS, kRisU, kRisS)));
  EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());
}

TEST_F(BackwardGraphemeBoundaryStatemachineTest, Flags_oddNumberedRIS) {
  BackwardGraphemeBoundaryStateMachine machine;

  // 'a' + [U] + [S] + [U]
  EXPECT_EQ("RRRRRRF", processSequenceBackward(
                           &machine, asCodePoints('a', kRisU, kRisS, kRisU)));
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

  // U+1F441 + [U] + [S] + [U]
  EXPECT_EQ("RRRRRRRF", processSequenceBackward(
                            &machine, asCodePoints(kEye, kRisU, kRisS, kRisU)));
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

  // SOT + [U] + [S] + [U]
  EXPECT_EQ("RRRRRRF", processSequenceBackward(
                           &machine, asCodePoints(kRisU, kRisS, kRisU)));
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

  // [Lead] + [U] + [S] + [U]
  EXPECT_EQ("RRRRRRF", processSequenceBackward(
                           &machine, asCodePoints(kLead, kRisU, kRisS, kRisU)));
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

  // 'a' + [Trail] + [U] + [S] + [U]
  EXPECT_EQ("RRRRRRRF",
            processSequenceBackward(
                &machine, asCodePoints('a', kTrail, kRisU, kRisS, kRisU)));
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

  // [Trail] + [Trail] + [U] + [S] + [U]
  EXPECT_EQ("RRRRRRRF",
            processSequenceBackward(
                &machine, asCodePoints(kTrail, kTrail, kRisU, kRisS, kRisU)));
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

  // SOT + [Trail] + [U] + [S] + [U]
  EXPECT_EQ("RRRRRRRF",
            processSequenceBackward(&machine,
                                    asCodePoints(kTrail, kRisU, kRisS, kRisU)));
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());
}
}  // namespace blink
