// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/state_machines/BackwardGraphemeBoundaryStateMachine.h"

#include "core/editing/state_machines/StateMachineUtil.h"
#include "core/editing/state_machines/TextSegmentationMachineState.h"
#include "platform/text/Character.h"
#include "wtf/text/CharacterNames.h"
#include "wtf/text/Unicode.h"
#include <ostream> // NOLINT

namespace blink {

namespace {
const UChar32 kInvalidCodePoint = WTF::Unicode::kMaxCodepoint + 1;
} // namespace

#define FOR_EACH_BACKWARD_GRAPHEME_BOUNDARY_STATE(V)                           \
    /* Initial state */                                                        \
    V(Start)                                                                   \
    /* Wating lead surrogate during initial state. */                          \
    V(StartWaitLeadSurrogate)                                                  \
    /* Searching grapheme boundary. */                                         \
    V(Search)                                                                  \
    /* Waiting lead surrogate during searching grapheme boundary. */           \
    V(SearchWaitLeadSurrogate)                                                 \
    /* Counting preceding regional indicators. */                              \
    V(CountRIS)                                                                \
    /* Wating lead surrogate during counting preceding regional indicators. */ \
    V(CountRISWaitLeadSurrogate)                                               \
    /* The state machine has stopped. */                                       \
    V(Finished)

enum class BackwardGraphemeBoundaryStateMachine::InternalState {
#define V(name) name,
        FOR_EACH_BACKWARD_GRAPHEME_BOUNDARY_STATE(V)
#undef V
};

std::ostream& operator<<(std::ostream& os,
    BackwardGraphemeBoundaryStateMachine::InternalState state)
{
    static const char* const texts[] = {
#define V(name) #name,
        FOR_EACH_BACKWARD_GRAPHEME_BOUNDARY_STATE(V)
#undef V
    };
    const auto& it = std::begin(texts) + static_cast<size_t>(state);
    DCHECK_GE(it, std::begin(texts)) << "Unknown state value";
    DCHECK_LT(it, std::end(texts)) << "Unknown state value";
    return os << *it;
}

BackwardGraphemeBoundaryStateMachine::BackwardGraphemeBoundaryStateMachine()
    : m_nextCodePoint(kInvalidCodePoint),
    m_internalState(InternalState::Start)
{
}

TextSegmentationMachineState
BackwardGraphemeBoundaryStateMachine::feedPrecedingCodeUnit(UChar codeUnit)
{
    switch (m_internalState) {
    case InternalState::Start:
        DCHECK_EQ(m_trailSurrogate, 0);
        DCHECK_EQ(m_nextCodePoint, kInvalidCodePoint);
        DCHECK_EQ(m_boundaryOffset, 0);
        DCHECK_EQ(m_precedingRISCount, 0);
        if (U16_IS_TRAIL(codeUnit)) {
            m_trailSurrogate = codeUnit;
            return moveToNextState(InternalState::StartWaitLeadSurrogate);
        }
        if (U16_IS_LEAD(codeUnit)) {
            // Lonely lead surrogate. Move to previous offset.
            m_boundaryOffset = -1;
            return finish();
        }
        m_nextCodePoint = codeUnit;
        m_boundaryOffset -= 1;
        return moveToNextState(InternalState::Search);
    case InternalState::StartWaitLeadSurrogate:
        DCHECK_NE(m_trailSurrogate, 0);
        DCHECK_EQ(m_nextCodePoint, kInvalidCodePoint);
        DCHECK_EQ(m_boundaryOffset, 0);
        DCHECK_EQ(m_precedingRISCount, 0);
        if (!U16_IS_LEAD(codeUnit)) {
            // Lonely trail surrogate. Move to previous offset.
            m_boundaryOffset = -1;
            return finish();
        }
        m_nextCodePoint = U16_GET_SUPPLEMENTARY(codeUnit, m_trailSurrogate);
        m_boundaryOffset = -2;
        m_trailSurrogate = 0;
        return moveToNextState(InternalState::Search);
    case InternalState::Search:
        DCHECK_EQ(m_trailSurrogate, 0);
        DCHECK_NE(m_nextCodePoint, kInvalidCodePoint);
        DCHECK_LT(m_boundaryOffset, 0);
        DCHECK_EQ(m_precedingRISCount, 0);
        if (U16_IS_TRAIL(codeUnit)) {
            DCHECK_EQ(m_trailSurrogate, 0);
            m_trailSurrogate = codeUnit;
            return moveToNextState(InternalState::SearchWaitLeadSurrogate);
        }
        if (U16_IS_LEAD(codeUnit))
            return finish(); // Lonely lead surrogate.
        if (isGraphemeBreak(codeUnit, m_nextCodePoint))
            return finish();
        m_nextCodePoint = codeUnit;
        m_boundaryOffset -= 1;
        return staySameState();
    case InternalState::SearchWaitLeadSurrogate:
        DCHECK_NE(m_trailSurrogate, 0);
        DCHECK_NE(m_nextCodePoint, kInvalidCodePoint);
        DCHECK_LT(m_boundaryOffset, 0);
        DCHECK_EQ(m_precedingRISCount, 0);
        if (!U16_IS_LEAD(codeUnit))
            return finish(); // Lonely trail surrogate.
        {
            const UChar32 codePoint = U16_GET_SUPPLEMENTARY(codeUnit, m_trailSurrogate);
            m_trailSurrogate = 0;
            if (Character::isRegionalIndicator(m_nextCodePoint)
                && Character::isRegionalIndicator(codePoint)) {
                m_precedingRISCount = 1;
                return moveToNextState(InternalState::CountRIS);
            }
            if (isGraphemeBreak(codePoint, m_nextCodePoint))
                return finish();
            m_nextCodePoint = codePoint;
            m_boundaryOffset -= 2;
            return moveToNextState(InternalState::Search);
        }
    case InternalState::CountRIS:
        DCHECK_EQ(m_trailSurrogate, 0);
        DCHECK(Character::isRegionalIndicator(m_nextCodePoint));
        DCHECK_LT(m_boundaryOffset, 0);
        DCHECK_GT(m_precedingRISCount, 0);
        if (U16_IS_TRAIL(codeUnit)) {
            DCHECK_EQ(m_trailSurrogate, 0);
            m_trailSurrogate = codeUnit;
            return moveToNextState(InternalState::CountRISWaitLeadSurrogate);
        }
        if (m_precedingRISCount % 2 != 0)
            m_boundaryOffset -= 2;
        return finish();
    case InternalState::CountRISWaitLeadSurrogate:
        DCHECK_NE(m_trailSurrogate, 0);
        DCHECK(Character::isRegionalIndicator(m_nextCodePoint));
        DCHECK_LT(m_boundaryOffset, 0);
        DCHECK_GT(m_precedingRISCount, 0);
        if (U16_IS_LEAD(codeUnit)) {
            DCHECK_NE(m_trailSurrogate, 0);
            const UChar32 codePoint = U16_GET_SUPPLEMENTARY(codeUnit, m_trailSurrogate);
            m_trailSurrogate = 0;
            if (Character::isRegionalIndicator(codePoint)) {
                ++m_precedingRISCount;
                return moveToNextState(InternalState::CountRIS);
            }
        }
        if (m_precedingRISCount % 2 != 0)
            m_boundaryOffset -= 2;
        return finish();
    case InternalState::Finished:
        NOTREACHED() << "Do not call feedPrecedingCodeUnit() once it finishes.";
    }
    NOTREACHED() << "Unhandled state: " << m_internalState;
    return finish();
}

TextSegmentationMachineState
BackwardGraphemeBoundaryStateMachine::tellEndOfPrecedingText()
{
    switch (m_internalState) {
    case InternalState::Start:
        // Did nothing.
        DCHECK_EQ(m_boundaryOffset, 0);
        return finish();
    case InternalState::StartWaitLeadSurrogate:
        // Lonely trail surrogate. Move to before of it.
        DCHECK_EQ(m_boundaryOffset, 0);
        m_boundaryOffset = -1;
        return finish();
    case InternalState::Search: // fallthrough
    case InternalState::SearchWaitLeadSurrogate:
        return finish();
    case InternalState::CountRIS: // fallthrough
    case InternalState::CountRISWaitLeadSurrogate:
        DCHECK_GT(m_precedingRISCount, 0);
        if (m_precedingRISCount % 2 != 0)
            m_boundaryOffset -= 2;
        return finish();
    case InternalState::Finished:
        NOTREACHED() << "Do not call tellEndOfPrecedingText() once it finishes.";
    }
    NOTREACHED() << "Unhandled state: " << m_internalState;
    return finish();
}

TextSegmentationMachineState
BackwardGraphemeBoundaryStateMachine::feedFollowingCodeUnit(UChar codeUnit)
{
    NOTREACHED();
    return TextSegmentationMachineState::Invalid;
}

int BackwardGraphemeBoundaryStateMachine::finalizeAndGetBoundaryOffset()
{
    if (m_internalState != InternalState::Finished)
        tellEndOfPrecedingText();
    DCHECK_LE(m_boundaryOffset, 0);
    return m_boundaryOffset;
}

TextSegmentationMachineState
BackwardGraphemeBoundaryStateMachine::moveToNextState(InternalState nextState)
{
    DCHECK_NE(nextState, InternalState::Finished) << "Use finish() instead";
    DCHECK_NE(nextState, InternalState::Start) << "Unable to move to Start";
    DCHECK_NE(m_internalState, nextState) << "Use staySameState() instead.";
    m_internalState = nextState;
    return TextSegmentationMachineState::NeedMoreCodeUnit;
}

TextSegmentationMachineState
BackwardGraphemeBoundaryStateMachine::staySameState()
{
    DCHECK_EQ(m_internalState, InternalState::Search)
        << "Only Search can stay.";
    return TextSegmentationMachineState::NeedMoreCodeUnit;
}

TextSegmentationMachineState BackwardGraphemeBoundaryStateMachine::finish()
{
    DCHECK_NE(m_internalState, InternalState::Finished);
    m_internalState = InternalState::Finished;
    return TextSegmentationMachineState::Finished;
}

void BackwardGraphemeBoundaryStateMachine::reset()
{
    m_trailSurrogate = 0;
    m_nextCodePoint = kInvalidCodePoint;
    m_boundaryOffset = 0;
    m_precedingRISCount = 0;
    m_internalState = InternalState::Start;
}

} // namespace blink
