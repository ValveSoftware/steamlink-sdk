/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/dom/DocumentLifecycle.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "wtf/Assertions.h"

namespace blink {

static DocumentLifecycle::DeprecatedTransition* s_deprecatedTransitionStack = 0;

// TODO(skyostil): Come up with a better way to store cross-frame lifecycle
// related data to avoid this being a global setting.
static unsigned s_allowThrottlingCount = 0;

DocumentLifecycle::Scope::Scope(DocumentLifecycle& lifecycle, LifecycleState finalState)
    : m_lifecycle(lifecycle)
    , m_finalState(finalState)
{
}

DocumentLifecycle::Scope::~Scope()
{
    m_lifecycle.advanceTo(m_finalState);
}

DocumentLifecycle::DeprecatedTransition::DeprecatedTransition(LifecycleState from, LifecycleState to)
    : m_previous(s_deprecatedTransitionStack)
    , m_from(from)
    , m_to(to)
{
    s_deprecatedTransitionStack = this;
}

DocumentLifecycle::DeprecatedTransition::~DeprecatedTransition()
{
    s_deprecatedTransitionStack = m_previous;
}

DocumentLifecycle::AllowThrottlingScope::AllowThrottlingScope(DocumentLifecycle& lifecycle)
{
    s_allowThrottlingCount++;
}

DocumentLifecycle::AllowThrottlingScope::~AllowThrottlingScope()
{
    DCHECK_GT(s_allowThrottlingCount, 0u);
    s_allowThrottlingCount--;
}

DocumentLifecycle::DisallowThrottlingScope::DisallowThrottlingScope(DocumentLifecycle& lifecycle)
{
    m_savedCount = s_allowThrottlingCount;
    s_allowThrottlingCount = 0;
}

DocumentLifecycle::DisallowThrottlingScope::~DisallowThrottlingScope()
{
    s_allowThrottlingCount = m_savedCount;
}

DocumentLifecycle::DocumentLifecycle()
    : m_state(Uninitialized)
    , m_detachCount(0)
{
}

DocumentLifecycle::~DocumentLifecycle()
{
}

#if DCHECK_IS_ON()

bool DocumentLifecycle::canAdvanceTo(LifecycleState nextState) const
{
    // We can stop from anywhere.
    if (nextState == Stopping)
        return true;

    switch (m_state) {
    case Uninitialized:
        return nextState == Inactive;
    case Inactive:
        if (nextState == StyleClean)
            return true;
        break;
    case VisualUpdatePending:
        if (nextState == InPreLayout)
            return true;
        if (nextState == InStyleRecalc)
            return true;
        if (nextState == InPerformLayout)
            return true;
        break;
    case InStyleRecalc:
        return nextState == StyleClean;
    case StyleClean:
        // We can synchronously recalc style.
        if (nextState == InStyleRecalc)
            return true;
        // We can notify layout objects that subtrees changed.
        if (nextState == InLayoutSubtreeChange)
            return true;
        // We can synchronously perform layout.
        if (nextState == InPreLayout)
            return true;
        if (nextState == InPerformLayout)
            return true;
        // We can redundant arrive in the style clean state.
        if (nextState == StyleClean)
            return true;
        if (nextState == LayoutClean)
            return true;
        if (nextState == InCompositingUpdate)
            return true;
        break;
    case InLayoutSubtreeChange:
        return nextState == LayoutSubtreeChangeClean;
    case LayoutSubtreeChangeClean:
        // We can synchronously recalc style.
        if (nextState == InStyleRecalc)
            return true;
        // We can synchronously perform layout.
        if (nextState == InPreLayout)
            return true;
        if (nextState == InPerformLayout)
            return true;
        // Can move back to style clean.
        if (nextState == StyleClean)
            return true;
        if (nextState == LayoutClean)
            return true;
        if (nextState == InCompositingUpdate)
            return true;
        break;
    case InPreLayout:
        if (nextState == InStyleRecalc)
            return true;
        if (nextState == StyleClean)
            return true;
        if (nextState == InPreLayout)
            return true;
        break;
    case InPerformLayout:
        return nextState == AfterPerformLayout;
    case AfterPerformLayout:
        // We can synchronously recompute layout in AfterPerformLayout.
        // FIXME: Ideally, we would unnest this recursion into a loop.
        if (nextState == InPreLayout)
            return true;
        if (nextState == LayoutClean)
            return true;
        break;
    case LayoutClean:
        // We can synchronously recalc style.
        if (nextState == InStyleRecalc)
            return true;
        // We can synchronously perform layout.
        if (nextState == InPreLayout)
            return true;
        if (nextState == InPerformLayout)
            return true;
        // We can redundantly arrive in the layout clean state. This situation
        // can happen when we call layout recursively and we unwind the stack.
        if (nextState == LayoutClean)
            return true;
        if (nextState == StyleClean)
            return true;
        if (nextState == InCompositingUpdate)
            return true;
        break;
    case InCompositingUpdate:
        return nextState == CompositingClean;
    case CompositingClean:
        if (nextState == InStyleRecalc)
            return true;
        if (nextState == InPreLayout)
            return true;
        if (nextState == InCompositingUpdate)
            return true;
        if (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled()) {
            if (nextState == InPrePaint)
                return true;
        } else if (nextState == InPaintInvalidation) {
            return true;
        }
        break;
    case InPaintInvalidation:
        DCHECK(!RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled());
        return nextState == PaintInvalidationClean;
    case PaintInvalidationClean:
        DCHECK(!RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled());
        if (nextState == InStyleRecalc)
            return true;
        if (nextState == InPreLayout)
            return true;
        if (nextState == InCompositingUpdate)
            return true;
        if (nextState == InPaint)
            return true;
        break;
    case InPrePaint:
        if (nextState == PrePaintClean && RuntimeEnabledFeatures::slimmingPaintV2Enabled())
            return true;
        break;
    case PrePaintClean:
        if (!RuntimeEnabledFeatures::slimmingPaintV2Enabled())
            break;
        if (nextState == InPaint)
            return true;
        if (nextState == InStyleRecalc)
            return true;
        if (nextState == InPreLayout)
            return true;
        if (nextState == InCompositingUpdate)
            return true;
        break;
    case InPaint:
        if (nextState == PaintClean)
            return true;
        break;
    case PaintClean:
        if (nextState == InStyleRecalc)
            return true;
        if (nextState == InPreLayout)
            return true;
        if (nextState == InCompositingUpdate)
            return true;
        break;
    case Stopping:
        return nextState == Stopped;
    case Stopped:
        return false;
    }
    return false;
}

bool DocumentLifecycle::canRewindTo(LifecycleState nextState) const
{
    // This transition is bogus, but we've whitelisted it anyway.
    if (s_deprecatedTransitionStack && m_state == s_deprecatedTransitionStack->from() && nextState == s_deprecatedTransitionStack->to())
        return true;
    return m_state == StyleClean
        || m_state == LayoutSubtreeChangeClean
        || m_state == AfterPerformLayout
        || m_state == LayoutClean
        || m_state == CompositingClean
        || m_state == PaintInvalidationClean
        || m_state == PaintClean;
}

#endif

void DocumentLifecycle::advanceTo(LifecycleState nextState)
{
#if DCHECK_IS_ON()
    DCHECK(canAdvanceTo(nextState))
        << "Cannot advance document lifecycle from " << stateAsDebugString(m_state)
        << " to " << stateAsDebugString(nextState) << ".";
#endif
    m_state = nextState;
}

void DocumentLifecycle::ensureStateAtMost(LifecycleState state)
{
    DCHECK(state == VisualUpdatePending || state == StyleClean || state == LayoutClean);
    if (m_state <= state)
        return;
#if DCHECK_IS_ON()
    DCHECK(canRewindTo(state))
        << "Cannot rewind document lifecycle from " << stateAsDebugString(m_state)
        << " to " <<stateAsDebugString(state) << ".";
#endif
    m_state = state;
}

bool DocumentLifecycle::throttlingAllowed() const
{
    return s_allowThrottlingCount;
}

#if DCHECK_IS_ON()
#define DEBUG_STRING_CASE(StateName) \
    case StateName: return #StateName

const char* DocumentLifecycle::stateAsDebugString(const LifecycleState state)
{
    switch (state) {
        DEBUG_STRING_CASE(Uninitialized);
        DEBUG_STRING_CASE(Inactive);
        DEBUG_STRING_CASE(VisualUpdatePending);
        DEBUG_STRING_CASE(InStyleRecalc);
        DEBUG_STRING_CASE(StyleClean);
        DEBUG_STRING_CASE(InLayoutSubtreeChange);
        DEBUG_STRING_CASE(LayoutSubtreeChangeClean);
        DEBUG_STRING_CASE(InPreLayout);
        DEBUG_STRING_CASE(InPerformLayout);
        DEBUG_STRING_CASE(AfterPerformLayout);
        DEBUG_STRING_CASE(LayoutClean);
        DEBUG_STRING_CASE(InCompositingUpdate);
        DEBUG_STRING_CASE(CompositingClean);
        DEBUG_STRING_CASE(InPaintInvalidation);
        DEBUG_STRING_CASE(PaintInvalidationClean);
        DEBUG_STRING_CASE(InPrePaint);
        DEBUG_STRING_CASE(PrePaintClean);
        DEBUG_STRING_CASE(InPaint);
        DEBUG_STRING_CASE(PaintClean);
        DEBUG_STRING_CASE(Stopping);
        DEBUG_STRING_CASE(Stopped);
    }

    ASSERT_NOT_REACHED();
    return "Unknown";
}
#endif

} // namespace blink
