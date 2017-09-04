// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/state_machines/BackspaceStateMachine.h"

#include "platform/text/Character.h"
#include "wtf/text/CharacterNames.h"
#include "wtf/text/Unicode.h"
#include <ostream>  // NOLINT

namespace blink {

#define FOR_EACH_BACKSPACE_STATE_MACHINE_STATE(V)                        \
  /* Initial state */                                                    \
  V(Start)                                                               \
  /* The current offset is just before line feed. */                     \
  V(BeforeLF)                                                            \
  /* The current offset is just before keycap. */                        \
  V(BeforeKeycap)                                                        \
  /* The current offset is just before variation selector and keycap. */ \
  V(BeforeVSAndKeycap)                                                   \
  /* The current offset is just before emoji modifier. */                \
  V(BeforeEmojiModifier)                                                 \
  /* The current offset is just before variation selector and emoji*/    \
  /* modifier. */                                                        \
  V(BeforeVSAndEmojiModifier)                                            \
  /* The current offset is just before variation sequence. */            \
  V(BeforeVS)                                                            \
  /* The current offset is just before ZWJ emoji. */                     \
  V(BeforeZWJEmoji)                                                      \
  /* The current offset is just before ZWJ. */                           \
  V(BeforeZWJ)                                                           \
  /* The current offset is just before variation selector and ZWJ. */    \
  V(BeforeVSAndZWJ)                                                      \
  /* That there are odd numbered RIS from the beggining. */              \
  V(OddNumberedRIS)                                                      \
  /* That there are even numbered RIS from the begging. */               \
  V(EvenNumberedRIS)                                                     \
  /* This state machine has finished. */                                 \
  V(Finished)

enum class BackspaceStateMachine::BackspaceState {
#define V(name) name,
  FOR_EACH_BACKSPACE_STATE_MACHINE_STATE(V)
#undef V
};

std::ostream& operator<<(std::ostream& os,
                         BackspaceStateMachine::BackspaceState state) {
  static const char* const texts[] = {
#define V(name) #name,
      FOR_EACH_BACKSPACE_STATE_MACHINE_STATE(V)
#undef V
  };
  const auto& it = std::begin(texts) + static_cast<size_t>(state);
  DCHECK_GE(it, std::begin(texts)) << "Unknown backspace value";
  DCHECK_LT(it, std::end(texts)) << "Unknown backspace value";
  return os << *it;
}

BackspaceStateMachine::BackspaceStateMachine()
    : m_state(BackspaceState::Start) {}

TextSegmentationMachineState BackspaceStateMachine::feedPrecedingCodeUnit(
    UChar codeUnit) {
  DCHECK_NE(BackspaceState::Finished, m_state);
  uint32_t codePoint = codeUnit;
  if (U16_IS_LEAD(codeUnit)) {
    if (m_trailSurrogate == 0) {
      // Unpaired lead surrogate. Aborting with deleting broken surrogate.
      ++m_codeUnitsToBeDeleted;
      return TextSegmentationMachineState::Finished;
    }
    codePoint = U16_GET_SUPPLEMENTARY(codeUnit, m_trailSurrogate);
    m_trailSurrogate = 0;
  } else if (U16_IS_TRAIL(codeUnit)) {
    if (m_trailSurrogate != 0) {
      // Unpaired trail surrogate. Aborting with deleting broken
      // surrogate.
      return TextSegmentationMachineState::Finished;
    }
    m_trailSurrogate = codeUnit;
    return TextSegmentationMachineState::NeedMoreCodeUnit;
  } else {
    if (m_trailSurrogate != 0) {
      // Unpaired trail surrogate. Aborting with deleting broken
      // surrogate.
      return TextSegmentationMachineState::Finished;
    }
  }

  switch (m_state) {
    case BackspaceState::Start:
      m_codeUnitsToBeDeleted = U16_LENGTH(codePoint);
      if (codePoint == newlineCharacter)
        return moveToNextState(BackspaceState::BeforeLF);
      if (u_hasBinaryProperty(codePoint, UCHAR_VARIATION_SELECTOR))
        return moveToNextState(BackspaceState::BeforeVS);
      if (Character::isRegionalIndicator(codePoint))
        return moveToNextState(BackspaceState::OddNumberedRIS);
      if (Character::isModifier(codePoint))
        return moveToNextState(BackspaceState::BeforeEmojiModifier);
      if (Character::isEmoji(codePoint))
        return moveToNextState(BackspaceState::BeforeZWJEmoji);
      if (codePoint == combiningEnclosingKeycapCharacter)
        return moveToNextState(BackspaceState::BeforeKeycap);
      return finish();
    case BackspaceState::BeforeLF:
      if (codePoint == carriageReturnCharacter)
        ++m_codeUnitsToBeDeleted;
      return finish();
    case BackspaceState::BeforeKeycap:
      if (u_hasBinaryProperty(codePoint, UCHAR_VARIATION_SELECTOR)) {
        DCHECK_EQ(m_lastSeenVSCodeUnits, 0);
        m_lastSeenVSCodeUnits = U16_LENGTH(codePoint);
        return moveToNextState(BackspaceState::BeforeVSAndKeycap);
      }
      if (Character::isEmojiKeycapBase(codePoint))
        m_codeUnitsToBeDeleted += U16_LENGTH(codePoint);
      return finish();
    case BackspaceState::BeforeVSAndKeycap:
      if (Character::isEmojiKeycapBase(codePoint)) {
        DCHECK_GT(m_lastSeenVSCodeUnits, 0);
        DCHECK_LE(m_lastSeenVSCodeUnits, 2);
        m_codeUnitsToBeDeleted += m_lastSeenVSCodeUnits + U16_LENGTH(codePoint);
      }
      return finish();
    case BackspaceState::BeforeEmojiModifier:
      if (u_hasBinaryProperty(codePoint, UCHAR_VARIATION_SELECTOR)) {
        DCHECK_EQ(m_lastSeenVSCodeUnits, 0);
        m_lastSeenVSCodeUnits = U16_LENGTH(codePoint);
        return moveToNextState(BackspaceState::BeforeVSAndEmojiModifier);
      }
      if (Character::isEmojiModifierBase(codePoint))
        m_codeUnitsToBeDeleted += U16_LENGTH(codePoint);
      return finish();
    case BackspaceState::BeforeVSAndEmojiModifier:
      if (Character::isEmojiModifierBase(codePoint)) {
        DCHECK_GT(m_lastSeenVSCodeUnits, 0);
        DCHECK_LE(m_lastSeenVSCodeUnits, 2);
        m_codeUnitsToBeDeleted += m_lastSeenVSCodeUnits + U16_LENGTH(codePoint);
      }
      return finish();
    case BackspaceState::BeforeVS:
      if (Character::isEmoji(codePoint)) {
        m_codeUnitsToBeDeleted += U16_LENGTH(codePoint);
        return moveToNextState(BackspaceState::BeforeZWJEmoji);
      }
      if (!u_hasBinaryProperty(codePoint, UCHAR_VARIATION_SELECTOR) &&
          u_getCombiningClass(codePoint) == 0)
        m_codeUnitsToBeDeleted += U16_LENGTH(codePoint);
      return finish();
    case BackspaceState::BeforeZWJEmoji:
      return codePoint == zeroWidthJoinerCharacter
                 ? moveToNextState(BackspaceState::BeforeZWJ)
                 : finish();
    case BackspaceState::BeforeZWJ:
      if (Character::isEmoji(codePoint)) {
        m_codeUnitsToBeDeleted += U16_LENGTH(codePoint) + 1;  // +1 for ZWJ
        return Character::isModifier(codePoint)
                   ? moveToNextState(BackspaceState::BeforeEmojiModifier)
                   : moveToNextState(BackspaceState::BeforeZWJEmoji);
      }
      if (u_hasBinaryProperty(codePoint, UCHAR_VARIATION_SELECTOR)) {
        DCHECK_EQ(m_lastSeenVSCodeUnits, 0);
        m_lastSeenVSCodeUnits = U16_LENGTH(codePoint);
        return moveToNextState(BackspaceState::BeforeVSAndZWJ);
      }
      return finish();
    case BackspaceState::BeforeVSAndZWJ:
      if (!Character::isEmoji(codePoint))
        return finish();

      DCHECK_GT(m_lastSeenVSCodeUnits, 0);
      DCHECK_LE(m_lastSeenVSCodeUnits, 2);
      // +1 for ZWJ
      m_codeUnitsToBeDeleted +=
          U16_LENGTH(codePoint) + 1 + m_lastSeenVSCodeUnits;
      m_lastSeenVSCodeUnits = 0;
      return moveToNextState(BackspaceState::BeforeZWJEmoji);
    case BackspaceState::OddNumberedRIS:
      if (!Character::isRegionalIndicator(codePoint))
        return finish();
      m_codeUnitsToBeDeleted += 2;  // Code units of RIS
      return moveToNextState(BackspaceState::EvenNumberedRIS);
    case BackspaceState::EvenNumberedRIS:
      if (!Character::isRegionalIndicator(codePoint))
        return finish();
      m_codeUnitsToBeDeleted -= 2;  // Code units of RIS
      return moveToNextState(BackspaceState::OddNumberedRIS);
    case BackspaceState::Finished:
      NOTREACHED() << "Do not call feedPrecedingCodeUnit() once it finishes.";
    default:
      NOTREACHED() << "Unhandled state: " << m_state;
  }
  NOTREACHED() << "Unhandled state: " << m_state;
  return TextSegmentationMachineState::Invalid;
}

TextSegmentationMachineState BackspaceStateMachine::tellEndOfPrecedingText() {
  if (m_trailSurrogate != 0) {
    // Unpaired trail surrogate. Removing broken surrogate.
    ++m_codeUnitsToBeDeleted;
    m_trailSurrogate = 0;
  }
  return TextSegmentationMachineState::Finished;
}

TextSegmentationMachineState BackspaceStateMachine::feedFollowingCodeUnit(
    UChar codeUnit) {
  NOTREACHED();
  return TextSegmentationMachineState::Invalid;
}

int BackspaceStateMachine::finalizeAndGetBoundaryOffset() {
  if (m_trailSurrogate != 0) {
    // Unpaired trail surrogate. Removing broken surrogate.
    ++m_codeUnitsToBeDeleted;
    m_trailSurrogate = 0;
  }
  if (m_state != BackspaceState::Finished) {
    m_lastSeenVSCodeUnits = 0;
    m_state = BackspaceState::Finished;
  }
  return -m_codeUnitsToBeDeleted;
}

void BackspaceStateMachine::reset() {
  m_codeUnitsToBeDeleted = 0;
  m_trailSurrogate = 0;
  m_state = BackspaceState::Start;
  m_lastSeenVSCodeUnits = 0;
}

TextSegmentationMachineState BackspaceStateMachine::moveToNextState(
    BackspaceState newState) {
  DCHECK_NE(BackspaceState::Finished, newState) << "Use finish() instead.";
  DCHECK_NE(BackspaceState::Start, newState) << "Don't move to Start.";
  // Below |DCHECK_NE()| prevent us to infinite loop in state machine.
  DCHECK_NE(m_state, newState) << "State should be changed.";
  m_state = newState;
  return TextSegmentationMachineState::NeedMoreCodeUnit;
}

TextSegmentationMachineState BackspaceStateMachine::finish() {
  DCHECK_NE(BackspaceState::Finished, m_state);
  m_state = BackspaceState::Finished;
  return TextSegmentationMachineState::Finished;
}

}  // namespace blink
