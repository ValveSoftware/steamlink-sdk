// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/state_machines/BackspaceStateMachine.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/Unicode.h"

namespace blink {

namespace {
const TextSegmentationMachineState kNeedMoreCodeUnit =
    TextSegmentationMachineState::NeedMoreCodeUnit;
const TextSegmentationMachineState kFinished =
    TextSegmentationMachineState::Finished;
}  // namespace

TEST(BackspaceStateMachineTest, DoNothingCase) {
  BackspaceStateMachine machine;
  EXPECT_EQ(0, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(0, machine.finalizeAndGetBoundaryOffset());
}

TEST(BackspaceStateMachineTest, SingleCharacter) {
  BackspaceStateMachine machine;
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit('a'));
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

  machine.reset();
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit('-'));
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

  machine.reset();
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit('\t'));
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

  machine.reset();
  // U+3042 HIRAGANA LETTER A.
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(0x3042));
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
}

TEST(BackspaceStateMachineTest, SurrogatePair) {
  BackspaceStateMachine machine;

  // U+20BB7 is \uD83D\uDDFA in UTF-16.
  const UChar leadSurrogate = 0xD842;
  const UChar trailSurrogate = 0xDFB7;

  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(trailSurrogate));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(leadSurrogate));
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

  // Edge cases
  // Unpaired trailing surrogate. Delete only broken trail surrogate.
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(trailSurrogate));
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(trailSurrogate));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit('a'));
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(trailSurrogate));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(trailSurrogate));
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

  // Unpaired leading surrogate. Delete only broken lead surrogate.
  machine.reset();
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(leadSurrogate));
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
}

TEST(BackspaceStateMachineTest, CRLF) {
  BackspaceStateMachine machine;

  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit('\r'));
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit('\n'));
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit('\n'));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(' '));
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

  // CR LF should be deleted at the same time.
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit('\n'));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit('\r'));
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());
}

TEST(BackspaceStateMachineTest, KeyCap) {
  BackspaceStateMachine machine;

  const UChar keycap = 0x20E3;
  const UChar vs16 = 0xFE0F;
  const UChar notKeycapBaseLead = 0xD83C;
  const UChar notKeycapBaseTrail = 0xDCCF;

  // keycapBase + keycap
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(keycap));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit('0'));
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

  // keycapBase + VS + keycap
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(keycap));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(vs16));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit('0'));
  EXPECT_EQ(-3, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-3, machine.finalizeAndGetBoundaryOffset());

  // Followings are edge cases. Remove only keycap character.
  // Not keycapBase + keycap
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(keycap));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit('a'));
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

  // Not keycapBase + VS + keycap
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(keycap));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(vs16));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit('a'));
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

  // Not keycapBase(surrogate pair) + keycap
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(keycap));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(notKeycapBaseTrail));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(notKeycapBaseLead));
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

  // Not keycapBase(surrogate pair) + VS + keycap
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(keycap));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(vs16));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(notKeycapBaseTrail));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(notKeycapBaseLead));
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

  // Sot + keycap
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(keycap));
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

  // Sot + VS + keycap
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(keycap));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(vs16));
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
}

TEST(BackspaceStateMachineTest, EmojiModifier) {
  BackspaceStateMachine machine;

  const UChar emojiModifierLead = 0xD83C;
  const UChar emojiModifierTrail = 0xDFFB;
  const UChar emojiModifierBase = 0x261D;
  const UChar emojiModifierBaseLead = 0xD83D;
  const UChar emojiModifierBaseTrail = 0xDC66;
  const UChar notEmojiModifierBaseLead = 0xD83C;
  const UChar notEmojiModifierBaseTrail = 0xDCCF;
  const UChar vs16 = 0xFE0F;

  // EMOJI_MODIFIER_BASE + EMOJI_MODIFIER
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(emojiModifierTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(emojiModifierLead));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(emojiModifierBase));
  EXPECT_EQ(-3, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-3, machine.finalizeAndGetBoundaryOffset());

  // EMOJI_MODIFIER_BASE(surrogate pairs) + EMOJI_MODIFIER
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(emojiModifierTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(emojiModifierLead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(emojiModifierBaseTrail));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(emojiModifierBaseLead));
  EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

  // EMOJI_MODIFIER_BASE + VS + EMOJI_MODIFIER
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(emojiModifierTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(emojiModifierLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(vs16));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(emojiModifierBase));
  EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

  // EMOJI_MODIFIER_BASE(surrogate pairs) + VS + EMOJI_MODIFIER
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(emojiModifierTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(emojiModifierLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(vs16));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(emojiModifierBaseTrail));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(emojiModifierBaseLead));
  EXPECT_EQ(-5, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-5, machine.finalizeAndGetBoundaryOffset());

  // Followings are edge cases. Remove only emoji modifier.
  // Not EMOJI_MODIFIER_BASE + EMOJI_MODIFIER
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(emojiModifierTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(emojiModifierLead));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit('a'));
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

  // Not EMOJI_MODIFIER_BASE(surrogate pairs) + EMOJI_MODIFIER
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(emojiModifierTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(emojiModifierLead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(notEmojiModifierBaseTrail));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(notEmojiModifierBaseLead));
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

  // Not EMOJI_MODIFIER_BASE + VS + EMOJI_MODIFIER
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(emojiModifierTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(emojiModifierLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(vs16));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit('a'));
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

  // Not EMOJI_MODIFIER_BASE(surrogate pairs) + VS + EMOJI_MODIFIER
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(emojiModifierTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(emojiModifierLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(vs16));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(notEmojiModifierBaseTrail));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(notEmojiModifierBaseLead));
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

  // Sot + EMOJI_MODIFIER
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(emojiModifierTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(emojiModifierLead));
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

  // Sot + VS + EMOJI_MODIFIER
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(emojiModifierTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(emojiModifierLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(vs16));
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());
}

TEST(BackspaceStateMachineTest, RegionalIndicator) {
  BackspaceStateMachine machine;

  const UChar regionalIndicatorULead = 0xD83C;
  const UChar regionalIndicatorUTrail = 0xDDFA;
  const UChar regionalIndicatorSLead = 0xD83C;
  const UChar regionalIndicatorSTrail = 0xDDF8;
  const UChar notRegionalIndicatorLead = 0xD83C;
  const UChar notRegionalIndicatorTrail = 0xDCCF;

  // Not RI + RI + RI
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorUTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorULead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorSTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorSLead));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit('a'));
  EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

  // Not RI(surrogate pairs) + RI + RI
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorUTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorULead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorSTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorSLead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(notRegionalIndicatorTrail));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(notRegionalIndicatorLead));
  EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

  // Sot + RI + RI
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorUTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorULead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorSTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorSLead));
  EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

  // Not RI + RI + RI + RI + RI
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorUTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorULead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorSTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorSLead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorUTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorULead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorSTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorSLead));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit('a'));
  EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

  // Not RI(surrogate pairs) + RI + RI + RI + RI
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorUTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorULead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorSTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorSLead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorUTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorULead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorSTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorSLead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(notRegionalIndicatorTrail));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(notRegionalIndicatorLead));
  EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

  // Sot + RI + RI + RI + RI
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorUTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorULead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorSTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorSLead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorUTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorULead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorSTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorSLead));
  EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

  // Followings are edge cases. Delete last regional indicator only.
  // Not RI + RI
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorUTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorULead));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit('a'));
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

  // Not RI(surrogate pairs) + RI
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorUTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorULead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(notRegionalIndicatorTrail));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(notRegionalIndicatorLead));
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

  // Sot + RI
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorUTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorULead));
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

  // Not RI + RI + RI + RI
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorUTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorULead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorSTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorSLead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorUTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorULead));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit('a'));
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

  // Not RI(surrogate pairs) + RI + RI + RI
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorUTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorULead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorSTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorSLead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorUTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorULead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(notRegionalIndicatorTrail));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(notRegionalIndicatorLead));
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

  // Sot + RI + RI + RI
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorUTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorULead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorSTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorSLead));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorUTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(regionalIndicatorULead));
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());
}

TEST(BackspaceStateMachineTest, VariationSequencec) {
  BackspaceStateMachine machine;

  UChar vs01 = 0xFE00;
  UChar vs01Base = 0xA85E;
  UChar vs01BaseLead = 0xD802;
  UChar vs01BaseTrail = 0xDEC6;

  UChar vs17Lead = 0xDB40;
  UChar vs17Trail = 0xDD00;
  UChar vs17Base = 0x3402;
  UChar vs17BaseLead = 0xD841;
  UChar vs17BaseTrail = 0xDC8C;

  UChar mongolianVs = 0x180B;
  UChar mongolianVsBase = 0x1820;
  // Variation selectors can't be a base of variation sequence.
  UChar notvsBase = 0xFE00;
  UChar notvsBaseLead = 0xDB40;
  UChar notvsBaseTrail = 0xDD01;

  // VS_BASE + VS
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(vs01));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(vs01Base));
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

  // VS_BASE + VS(surrogate pairs)
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(vs17Trail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(vs17Lead));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(vs17Base));
  EXPECT_EQ(-3, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-3, machine.finalizeAndGetBoundaryOffset());

  // VS_BASE(surrogate pairs) + VS
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(vs01));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(vs01BaseTrail));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(vs01BaseLead));
  EXPECT_EQ(-3, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-3, machine.finalizeAndGetBoundaryOffset());

  // VS_BASE(surrogate pairs) + VS(surrogate pairs)
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(vs17Trail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(vs17Lead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(vs17BaseTrail));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(vs17BaseLead));
  EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-4, machine.finalizeAndGetBoundaryOffset());

  // mongolianVsBase + mongolianVs
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(mongolianVs));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(mongolianVsBase));
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

  // Followings are edge case. Delete only variation selector.
  // Not VS_BASE + VS
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(vs01));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(notvsBase));
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

  // Not VS_BASE + VS(surrogate pairs)
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(vs17Trail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(vs17Lead));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(notvsBase));
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

  // Not VS_BASE(surrogate pairs) + VS
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(vs01));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(notvsBaseTrail));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(notvsBaseLead));
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

  // Not VS_BASE(surrogate pairs) + VS(surrogate pairs)
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(vs17Trail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(vs17Lead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(notvsBaseTrail));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(notvsBaseLead));
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

  // Not VS_BASE + MONGOLIAN_VS
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(mongolianVs));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(notvsBase));
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

  // Not VS_BASE(surrogate pairs) + MONGOLIAN_VS
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(mongolianVs));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(notvsBaseTrail));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(notvsBaseLead));
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

  // Sot + VS
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(vs01));
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

  // Sot + VS(surrogate pair)
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(vs17Trail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(vs17Lead));
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

  // Sot + MONGOLIAN_VS
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(mongolianVs));
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
}

TEST(BackspaceStateMachineTest, ZWJSequence) {
  BackspaceStateMachine machine;

  const UChar zwj = 0x200D;
  const UChar eyeLead = 0xD83D;
  const UChar eyeTrail = 0xDC41;
  const UChar leftSpeachBubbleLead = 0xD83D;
  const UChar leftSpeachBubbleTrail = 0xDDE8;
  const UChar manLead = 0xD83D;
  const UChar manTrail = 0xDC68;
  const UChar boyLead = 0xD83D;
  const UChar boyTrail = 0xDC66;
  const UChar heart = 0x2764;
  const UChar kissLead = 0xD83D;
  const UChar killTrail = 0xDC8B;
  const UChar vs16 = 0xFE0F;
  const UChar other = 'a';
  const UChar otherLead = 0xD83C;
  const UChar otherTrail = 0xDCCF;

  // Followings are chosen from valid zwj sequcne.
  // See http://www.unicode.org/Public/emoji/2.0//emoji-zwj-sequences.txt

  // others + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI
  // As an example, use EYE + ZWJ + LEFT_SPEACH_BUBBLE
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(leftSpeachBubbleTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(leftSpeachBubbleLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(zwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(eyeTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(eyeLead));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(other));
  EXPECT_EQ(-5, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-5, machine.finalizeAndGetBoundaryOffset());

  // others(surrogate pairs) + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI
  // As an example, use EYE + ZWJ + LEFT_SPEACH_BUBBLE
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(leftSpeachBubbleTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(leftSpeachBubbleLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(zwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(eyeTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(eyeLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(otherTrail));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(otherLead));
  EXPECT_EQ(-5, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-5, machine.finalizeAndGetBoundaryOffset());

  // Sot + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI
  // As an example, use EYE + ZWJ + LEFT_SPEACH_BUBBLE
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(leftSpeachBubbleTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(leftSpeachBubbleLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(zwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(eyeTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(eyeLead));
  EXPECT_EQ(-5, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-5, machine.finalizeAndGetBoundaryOffset());

  // others + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI
  // As an example, use MAN + ZWJ + heart + ZWJ + MAN
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(zwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(heart));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(zwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manLead));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(other));
  EXPECT_EQ(-7, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-7, machine.finalizeAndGetBoundaryOffset());

  // others(surrogate pairs) + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI
  // As an example, use MAN + ZWJ + heart + ZWJ + MAN
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(zwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(heart));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(zwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(otherTrail));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(otherLead));
  EXPECT_EQ(-7, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-7, machine.finalizeAndGetBoundaryOffset());

  // Sot + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI
  // As an example, use MAN + ZWJ + heart + ZWJ + MAN
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(zwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(heart));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(zwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manLead));
  EXPECT_EQ(-7, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-7, machine.finalizeAndGetBoundaryOffset());

  // others + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI + VS + ZWJ + ZWJ_EMOJI
  // As an example, use MAN + ZWJ + heart + vs16 + ZWJ + MAN
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(zwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(vs16));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(heart));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(zwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manLead));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(other));
  EXPECT_EQ(-8, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-8, machine.finalizeAndGetBoundaryOffset());

  // others(surrogate pairs) + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI + VS + ZWJ +
  // ZWJ_EMOJI
  // As an example, use MAN + ZWJ + heart + vs16 + ZWJ + MAN
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(zwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(vs16));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(heart));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(zwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(otherTrail));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(otherLead));
  EXPECT_EQ(-8, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-8, machine.finalizeAndGetBoundaryOffset());

  // Sot + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI + VS + ZWJ + ZWJ_EMOJI
  // As an example, use MAN + ZWJ + heart + vs16 + ZWJ + MAN
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(zwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(vs16));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(heart));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(zwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manLead));
  EXPECT_EQ(-8, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-8, machine.finalizeAndGetBoundaryOffset());

  // others + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI
  // As an example, use MAN + ZWJ + MAN + ZWJ + boy + ZWJ + BOY
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(boyTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(boyLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(zwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(boyTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(boyLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(zwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(zwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manLead));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(other));
  EXPECT_EQ(-11, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-11, machine.finalizeAndGetBoundaryOffset());

  // others(surrogate pairs) + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI +
  // ZWJ + ZWJ_EMOJI
  // As an example, use MAN + ZWJ + MAN + ZWJ + boy + ZWJ + BOY
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(boyTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(boyLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(zwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(boyTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(boyLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(zwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(zwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(otherTrail));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(otherLead));
  EXPECT_EQ(-11, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-11, machine.finalizeAndGetBoundaryOffset());

  // Sot + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI
  // As an example, use MAN + ZWJ + MAN + ZWJ + boy + ZWJ + BOY
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(boyTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(boyLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(zwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(boyTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(boyLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(zwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(zwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manLead));
  EXPECT_EQ(-11, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-11, machine.finalizeAndGetBoundaryOffset());

  // others + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI + VS + ZWJ + ZWJ_EMOJI + ZWJ +
  // ZWJ_EMOJI
  // As an example, use MAN + ZWJ + heart + VS + ZWJ + KISS + ZWJ + MAN
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(zwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(killTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(kissLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(zwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(vs16));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(heart));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(zwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manLead));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(other));
  EXPECT_EQ(-11, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-11, machine.finalizeAndGetBoundaryOffset());

  // others(surrogate pairs) + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI + VS + ZWJ +
  // ZWJ_EMOJI + ZWJ + ZWJ_EMOJI
  // As an example, use MAN + ZWJ + heart + VS + ZWJ + KISS + ZWJ + MAN
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(zwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(killTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(kissLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(zwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(vs16));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(heart));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(zwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(otherTrail));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(otherLead));
  EXPECT_EQ(-11, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-11, machine.finalizeAndGetBoundaryOffset());

  // Sot + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI + VS + ZWJ + ZWJ_EMOJI + ZWJ + ZWJ_EMOJI
  // As an example, use MAN + ZWJ + heart + VS + ZWJ + KISS + ZWJ + MAN
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(zwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(killTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(kissLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(zwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(vs16));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(heart));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(zwj));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manLead));
  EXPECT_EQ(-11, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-11, machine.finalizeAndGetBoundaryOffset());

  // Sot + EMOJI_MODIFIER_BASE + EMOJI_MODIFIER + ZWJ + ZWJ_EMOJI
  // As an example, use WOMAN + MODIFIER + ZWJ + BRIEFCASE
  const UChar womanLead = 0xD83D;
  const UChar womanTrail = 0xDC69;
  const UChar emojiModifierLead = 0xD83C;
  const UChar emojiModifierTrail = 0xDFFB;
  const UChar briefcaseLead = 0xD83D;
  const UChar briefcaseTrail = 0xDCBC;
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(briefcaseTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(briefcaseLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(zwj));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(emojiModifierTrail));
  EXPECT_EQ(kNeedMoreCodeUnit,
            machine.feedPrecedingCodeUnit(emojiModifierLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(womanTrail));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(womanLead));
  EXPECT_EQ(-7, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-7, machine.finalizeAndGetBoundaryOffset());

  // Followings are not edge cases but good to check.
  // If leading character is not zwj, delete only ZWJ_EMOJI.
  // other + ZWJ_EMOJI
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(heart));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(other));
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

  // other(surrogate pairs) + ZWJ_EMOJI
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(heart));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(otherTrail));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(otherLead));
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

  // Sot + ZWJ_EMOJI
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(heart));
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());

  // other + ZWJ_EMOJI(surrogate pairs)
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manLead));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(other));
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

  // other(surrogate pairs) + ZWJ_EMOJI(surrogate pairs)
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manLead));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(otherTrail));
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(otherLead));
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

  // Sot + ZWJ_EMOJI(surrogate pairs)
  machine.reset();
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manTrail));
  EXPECT_EQ(kNeedMoreCodeUnit, machine.feedPrecedingCodeUnit(manLead));
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());
  EXPECT_EQ(-2, machine.finalizeAndGetBoundaryOffset());

  // Followings are edge case.
  // It is hard to list all edge case patterns. Check only over deleting by ZWJ.
  // any + ZWJ: should delete only last ZWJ.
  machine.reset();
  EXPECT_EQ(kFinished, machine.feedPrecedingCodeUnit(zwj));
  EXPECT_EQ(-1, machine.finalizeAndGetBoundaryOffset());
}
}  // namespace blink
