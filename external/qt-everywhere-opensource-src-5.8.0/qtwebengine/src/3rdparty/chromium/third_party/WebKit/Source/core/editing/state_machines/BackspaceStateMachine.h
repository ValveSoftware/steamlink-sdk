// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BackspaceStateMachine_h
#define BackspaceStateMachine_h

#include "core/CoreExport.h"
#include "core/editing/state_machines/TextSegmentationMachineState.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/Unicode.h"
#include <iosfwd>

namespace blink {

class CORE_EXPORT BackspaceStateMachine {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(BackspaceStateMachine);
public:
    BackspaceStateMachine();

    // Prepares by feeding preceding text.
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
    enum class BackspaceState;
    friend std::ostream& operator<<(std::ostream&, BackspaceState);

    // Updates the internal state to the |newState| then return
    // InternalState::NeedMoreCodeUnit.
    TextSegmentationMachineState moveToNextState(BackspaceState newState);

    // Update the internal state to BackspaceState::Finished, then return
    // MachineState::Finished.
    TextSegmentationMachineState finish();

    // Used for composing supplementary code point with surrogate pairs.
    UChar m_trailSurrogate = 0;

    // The number of code units to be deleted.
    int m_codeUnitsToBeDeleted = 0;

    // The length of the previously seen variation selector.
    int m_lastSeenVSCodeUnits = 0;

    // The internal state.
    BackspaceState m_state;
};

} // namespace blink

#endif
