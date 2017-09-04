// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/state_machines/ForwardGraphemeBoundaryStateMachine.h"

#include "core/editing/state_machines/StateMachineUtil.h"
#include "core/editing/state_machines/TextSegmentationMachineState.h"
#include "platform/text/Character.h"
#include "wtf/text/Unicode.h"
#include <ostream>  // NOLINT

namespace blink {
namespace {
const UChar32 kInvalidCodePoint = WTF::Unicode::kMaxCodepoint + 1;
}  // namespace

#define FOR_EACH_FORWARD_GRAPHEME_BOUNDARY_STATE(V)                    \
  /* Counting preceding regional indicators. This is initial state. */ \
  V(CountRIS)                                                          \
  /* Waiting lead surrogate during counting regional indicators. */    \
  V(CountRISWaitLeadSurrogate)                                         \
  /* Waiting first following code unit. */                             \
  V(StartForward)                                                      \
  /* Waiting trail surrogate for the first following code point. */    \
  V(StartForwardWaitTrailSurrgate)                                     \
  /* Searching grapheme boundary. */                                   \
  V(Search)                                                            \
  /* Waiting trail surrogate during searching grapheme boundary. */    \
  V(SearchWaitTrailSurrogate)                                          \
  /* The state machine has stopped. */                                 \
  V(Finished)

enum class ForwardGraphemeBoundaryStateMachine::InternalState {
#define V(name) name,
  FOR_EACH_FORWARD_GRAPHEME_BOUNDARY_STATE(V)
#undef V
};

std::ostream& operator<<(
    std::ostream& os,
    ForwardGraphemeBoundaryStateMachine::InternalState state) {
  static const char* const texts[] = {
#define V(name) #name,
      FOR_EACH_FORWARD_GRAPHEME_BOUNDARY_STATE(V)
#undef V
  };
  const auto& it = std::begin(texts) + static_cast<size_t>(state);
  DCHECK_GE(it, std::begin(texts)) << "Unknown state value";
  DCHECK_LT(it, std::end(texts)) << "Unknown state value";
  return os << *it;
}

ForwardGraphemeBoundaryStateMachine::ForwardGraphemeBoundaryStateMachine()
    : m_prevCodePoint(kInvalidCodePoint),
      m_internalState(InternalState::CountRIS) {}

TextSegmentationMachineState
ForwardGraphemeBoundaryStateMachine::feedPrecedingCodeUnit(UChar codeUnit) {
  DCHECK_EQ(m_prevCodePoint, kInvalidCodePoint);
  DCHECK_EQ(m_boundaryOffset, 0);
  switch (m_internalState) {
    case InternalState::CountRIS:
      DCHECK_EQ(m_pendingCodeUnit, 0);
      if (U16_IS_TRAIL(codeUnit)) {
        m_pendingCodeUnit = codeUnit;
        return moveToNextState(InternalState::CountRISWaitLeadSurrogate);
      }
      return moveToNextState(InternalState::StartForward);
    case InternalState::CountRISWaitLeadSurrogate:
      DCHECK_NE(m_pendingCodeUnit, 0);
      if (U16_IS_LEAD(codeUnit)) {
        const UChar32 codePoint =
            U16_GET_SUPPLEMENTARY(codeUnit, m_pendingCodeUnit);
        m_pendingCodeUnit = 0;
        if (Character::isRegionalIndicator(codePoint)) {
          ++m_precedingRISCount;
          return moveToNextState(InternalState::CountRIS);
        }
      }
      m_pendingCodeUnit = 0;
      return moveToNextState(InternalState::StartForward);
    case InternalState::StartForward:                   // Fallthrough
    case InternalState::StartForwardWaitTrailSurrgate:  // Fallthrough
    case InternalState::Search:                         // Fallthrough
    case InternalState::SearchWaitTrailSurrogate:       // Fallthrough
      NOTREACHED() << "Do not call feedPrecedingCodeUnit() once "
                   << TextSegmentationMachineState::NeedFollowingCodeUnit
                   << " is returned. InternalState: " << m_internalState;
      return finish();
    case InternalState::Finished:
      NOTREACHED() << "Do not call feedPrecedingCodeUnit() once it finishes.";
      return finish();
  }
  NOTREACHED() << "Unhandled state: " << m_internalState;
  return finish();
}

TextSegmentationMachineState
ForwardGraphemeBoundaryStateMachine::feedFollowingCodeUnit(UChar codeUnit) {
  switch (m_internalState) {
    case InternalState::CountRIS:  // Fallthrough
    case InternalState::CountRISWaitLeadSurrogate:
      NOTREACHED() << "Do not call feedFollowingCodeUnit() until "
                   << TextSegmentationMachineState::NeedFollowingCodeUnit
                   << " is returned. InternalState: " << m_internalState;
      return finish();
    case InternalState::StartForward:
      DCHECK_EQ(m_prevCodePoint, kInvalidCodePoint);
      DCHECK_EQ(m_boundaryOffset, 0);
      DCHECK_EQ(m_pendingCodeUnit, 0);
      if (U16_IS_TRAIL(codeUnit)) {
        // Lonely trail surrogate.
        m_boundaryOffset = 1;
        return finish();
      }
      if (U16_IS_LEAD(codeUnit)) {
        m_pendingCodeUnit = codeUnit;
        return moveToNextState(InternalState::StartForwardWaitTrailSurrgate);
      }
      m_prevCodePoint = codeUnit;
      m_boundaryOffset = 1;
      return moveToNextState(InternalState::Search);
    case InternalState::StartForwardWaitTrailSurrgate:
      DCHECK_EQ(m_prevCodePoint, kInvalidCodePoint);
      DCHECK_EQ(m_boundaryOffset, 0);
      DCHECK_NE(m_pendingCodeUnit, 0);
      if (U16_IS_TRAIL(codeUnit)) {
        m_prevCodePoint = U16_GET_SUPPLEMENTARY(m_pendingCodeUnit, codeUnit);
        m_boundaryOffset = 2;
        m_pendingCodeUnit = 0;
        return moveToNextState(InternalState::Search);
      }
      // Lonely lead surrogate.
      m_boundaryOffset = 1;
      return finish();
    case InternalState::Search:
      DCHECK_NE(m_prevCodePoint, kInvalidCodePoint);
      DCHECK_NE(m_boundaryOffset, 0);
      DCHECK_EQ(m_pendingCodeUnit, 0);
      if (U16_IS_LEAD(codeUnit)) {
        m_pendingCodeUnit = codeUnit;
        return moveToNextState(InternalState::SearchWaitTrailSurrogate);
      }
      if (U16_IS_TRAIL(codeUnit))
        return finish();  // Lonely trail surrogate.
      if (isGraphemeBreak(m_prevCodePoint, codeUnit))
        return finish();
      m_prevCodePoint = codeUnit;
      m_boundaryOffset += 1;
      return staySameState();
    case InternalState::SearchWaitTrailSurrogate:
      DCHECK_NE(m_prevCodePoint, kInvalidCodePoint);
      DCHECK_NE(m_boundaryOffset, 0);
      DCHECK_NE(m_pendingCodeUnit, 0);
      if (!U16_IS_TRAIL(codeUnit))
        return finish();  // Lonely lead surrogate.

      {
        const UChar32 codePoint =
            U16_GET_SUPPLEMENTARY(m_pendingCodeUnit, codeUnit);
        m_pendingCodeUnit = 0;
        if (Character::isRegionalIndicator(m_prevCodePoint) &&
            Character::isRegionalIndicator(codePoint)) {
          if (m_precedingRISCount % 2 == 0) {
            // Odd numbered RI case, note that m_prevCodePoint is also
            // RI.
            m_boundaryOffset += 2;
          }
          return finish();
        }
        if (isGraphemeBreak(m_prevCodePoint, codePoint))
          return finish();
        m_prevCodePoint = codePoint;
        m_boundaryOffset += 2;
        return moveToNextState(InternalState::Search);
      }
    case InternalState::Finished:
      NOTREACHED() << "Do not call feedFollowingCodeUnit() once it finishes.";
      return finish();
  }
  NOTREACHED() << "Unhandled staet: " << m_internalState;
  return finish();
}

TextSegmentationMachineState
ForwardGraphemeBoundaryStateMachine::tellEndOfPrecedingText() {
  DCHECK(m_internalState == InternalState::CountRIS ||
         m_internalState == InternalState::CountRISWaitLeadSurrogate)
      << "Do not call tellEndOfPrecedingText() once "
      << TextSegmentationMachineState::NeedFollowingCodeUnit
      << " is returned. InternalState: " << m_internalState;

  // Clear pending code unit since preceding buffer may end with lonely trail
  // surrogate. We can just ignore it since preceding buffer is only used for
  // counting preceding regional indicators.
  m_pendingCodeUnit = 0;
  return moveToNextState(InternalState::StartForward);
}

int ForwardGraphemeBoundaryStateMachine::finalizeAndGetBoundaryOffset() {
  if (m_internalState != InternalState::Finished)
    finishWithEndOfText();
  DCHECK_GE(m_boundaryOffset, 0);
  return m_boundaryOffset;
}

void ForwardGraphemeBoundaryStateMachine::reset() {
  m_pendingCodeUnit = 0;
  m_boundaryOffset = 0;
  m_precedingRISCount = 0;
  m_prevCodePoint = kInvalidCodePoint;
  m_internalState = InternalState::CountRIS;
}

TextSegmentationMachineState ForwardGraphemeBoundaryStateMachine::finish() {
  DCHECK_NE(m_internalState, InternalState::Finished);
  m_internalState = InternalState::Finished;
  return TextSegmentationMachineState::Finished;
}

TextSegmentationMachineState
ForwardGraphemeBoundaryStateMachine::moveToNextState(InternalState nextState) {
  DCHECK_NE(nextState, InternalState::Finished) << "Use finish() instead";
  DCHECK_NE(nextState, m_internalState) << "Use staySameSatate() instead";
  m_internalState = nextState;
  if (nextState == InternalState::StartForward)
    return TextSegmentationMachineState::NeedFollowingCodeUnit;
  return TextSegmentationMachineState::NeedMoreCodeUnit;
}

TextSegmentationMachineState
ForwardGraphemeBoundaryStateMachine::staySameState() {
  DCHECK_EQ(m_internalState, InternalState::Search)
      << "Only Search can stay the same state.";
  return TextSegmentationMachineState::NeedMoreCodeUnit;
}

void ForwardGraphemeBoundaryStateMachine::finishWithEndOfText() {
  switch (m_internalState) {
    case InternalState::CountRIS:                   // Fallthrough
    case InternalState::CountRISWaitLeadSurrogate:  // Fallthrough
    case InternalState::StartForward:               // Fallthrough
      return;  // Haven't search anything to forward. Just finish.
    case InternalState::StartForwardWaitTrailSurrgate:
      // Lonely lead surrogate.
      m_boundaryOffset = 1;
      return;
    case InternalState::Search:                    // Fallthrough
    case InternalState::SearchWaitTrailSurrogate:  // Fallthrough
      return;
    case InternalState::Finished:  // Fallthrough
      NOTREACHED() << "Do not call finishWithEndOfText() once it finishes.";
  }
  NOTREACHED() << "Unhandled state: " << m_internalState;
}
}  // namespace blink
