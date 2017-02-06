// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/state_machines/StateMachineTestUtil.h"

#include "core/editing/state_machines/BackwardGraphemeBoundaryStateMachine.h"
#include "core/editing/state_machines/ForwardGraphemeBoundaryStateMachine.h"
#include "core/editing/state_machines/TextSegmentationMachineState.h"
#include "wtf/Assertions.h"
#include "wtf/text/StringBuilder.h"
#include <algorithm>

namespace blink {

namespace {
char MachineStateToChar(TextSegmentationMachineState state)
{
    static const char indicators[] = {
        'I', // Invalid
        'R', // NeedMoreCodeUnit (Repeat)
        'S', // NeedFollowingCodeUnit (Switch)
        'F', // Finished
    };
    const auto& it = std::begin(indicators) + static_cast<size_t>(state);
    DCHECK_GE(it, std::begin(indicators)) << "Unknown backspace value";
    DCHECK_LT(it, std::end(indicators)) << "Unknown backspace value";
    return *it;
}

Vector<UChar> codePointsToCodeUnits(const Vector<UChar32>& codePoints)
{
    Vector<UChar> out;
    for (const auto& codePoint : codePoints) {
        if (U16_LENGTH(codePoint) == 2) {
            out.append(U16_LEAD(codePoint));
            out.append(U16_TRAIL(codePoint));
        } else {
            out.append(static_cast<UChar>(codePoint));
        }
    }
    return out;
}

template<typename StateMachine>
String processSequence(StateMachine* machine,
    const Vector<UChar32>& preceding,
    const Vector<UChar32>& following)
{
    machine->reset();
    StringBuilder out;
    TextSegmentationMachineState state = TextSegmentationMachineState::Invalid;
    Vector<UChar> precedingCodeUnits = codePointsToCodeUnits(preceding);
    std::reverse(precedingCodeUnits.begin(), precedingCodeUnits.end());
    for (const auto& codeUnit : precedingCodeUnits) {
        state = machine->feedPrecedingCodeUnit(codeUnit);
        out.append(MachineStateToChar(state));
        switch (state) {
        case TextSegmentationMachineState::Invalid:
        case TextSegmentationMachineState::Finished:
            return out.toString();
        case TextSegmentationMachineState::NeedMoreCodeUnit:
            continue;
        case TextSegmentationMachineState::NeedFollowingCodeUnit:
            break;
        }
    }
    if (preceding.isEmpty()
        || state == TextSegmentationMachineState::NeedMoreCodeUnit) {
        state = machine->tellEndOfPrecedingText();
        out.append(MachineStateToChar(state));
    }
    if (state == TextSegmentationMachineState::Finished)
        return out.toString();

    Vector<UChar> followingCodeUnits = codePointsToCodeUnits(following);
    for (const auto& codeUnit : followingCodeUnits) {
        state = machine->feedFollowingCodeUnit(codeUnit);
        out.append(MachineStateToChar(state));
        switch (state) {
        case TextSegmentationMachineState::Invalid:
        case TextSegmentationMachineState::Finished:
            return out.toString();
        case TextSegmentationMachineState::NeedMoreCodeUnit:
            continue;
        case TextSegmentationMachineState::NeedFollowingCodeUnit:
            break;
        }
    }
    return out.toString();
}
} // namespace

String GraphemeStateMachineTestBase::processSequenceBackward(
    BackwardGraphemeBoundaryStateMachine* machine,
    const Vector<UChar32>& preceding)
{
    const String& out =
        processSequence(machine, preceding, Vector<UChar32>());
    if (machine->finalizeAndGetBoundaryOffset()
        != machine->finalizeAndGetBoundaryOffset())
        return "State machine changes final offset after finished.";
    return out;
}

String GraphemeStateMachineTestBase::processSequenceForward(
    ForwardGraphemeBoundaryStateMachine* machine,
    const Vector<UChar32>& preceding,
    const Vector<UChar32>& following)
{
    const String& out = processSequence(machine, preceding, following);
    if (machine->finalizeAndGetBoundaryOffset()
        != machine->finalizeAndGetBoundaryOffset())
        return "State machine changes final offset after finished.";
    return out;
}

} // namespace blink
