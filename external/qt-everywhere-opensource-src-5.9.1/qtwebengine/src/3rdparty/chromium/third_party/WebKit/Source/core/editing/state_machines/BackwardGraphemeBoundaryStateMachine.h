// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BackwardGraphemeBoundaryStateMachine_h
#define BackwardGraphemeBoundaryStateMachine_h

#include "core/CoreExport.h"
#include "core/editing/state_machines/TextSegmentationMachineState.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/Unicode.h"
#include <iosfwd>

namespace blink {

class CORE_EXPORT BackwardGraphemeBoundaryStateMachine {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(BackwardGraphemeBoundaryStateMachine);

 public:
  BackwardGraphemeBoundaryStateMachine();

  // Find boundary offset by feeding preceding text.
  // This method must not be called after feedFollowingCodeUnit().
  TextSegmentationMachineState feedPrecedingCodeUnit(UChar codeUnit);

  // Tells the end of preceding text to the state machine.
  TextSegmentationMachineState tellEndOfPrecedingText();

  // Find boundary offset by feeding following text.
  // This method must be called after feedPrecedingCodeUnit() returns
  // NeedsFollowingCodeUnit.
  TextSegmentationMachineState feedFollowingCodeUnit(UChar codeUnit);

  // Returns the next boundary offset. This method finalizes the state machine
  // if it is not finished.
  int finalizeAndGetBoundaryOffset();

  // Resets the internal state to the initial state.
  void reset();

 private:
  enum class InternalState;
  friend std::ostream& operator<<(std::ostream&, InternalState);

  TextSegmentationMachineState moveToNextState(InternalState nextState);

  TextSegmentationMachineState staySameState();

  // Updates the internal state to InternalState::Finished then returns
  // TextSegmentationMachineState::Finished.
  TextSegmentationMachineState finish();

  // Used for composing supplementary code point with surrogate pairs.
  UChar m_trailSurrogate = 0;

  // The code point immediately after the m_BoundaryOffset.
  UChar32 m_nextCodePoint;

  // The relative offset from the begging of this state machine.
  int m_boundaryOffset = 0;

  // The number of regional indicator symbols preceding to the begging offset.
  int m_precedingRISCount = 0;

  // The internal state.
  InternalState m_internalState;
};

}  // namespace blink

#endif
