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

#ifndef DocumentLifecycle_h
#define DocumentLifecycle_h

#include "core/CoreExport.h"
#include "wtf/Allocator.h"
#include "wtf/Assertions.h"
#include "wtf/Noncopyable.h"

namespace blink {

class CORE_EXPORT DocumentLifecycle {
    DISALLOW_NEW();
    WTF_MAKE_NONCOPYABLE(DocumentLifecycle);
public:
    enum LifecycleState {
        Uninitialized,
        Inactive,

        // When the document is active, it traverses these states.

        VisualUpdatePending,

        InStyleRecalc,
        StyleClean,

        InLayoutSubtreeChange,
        LayoutSubtreeChangeClean,

        InPreLayout,
        InPerformLayout,
        AfterPerformLayout,
        LayoutClean,

        InCompositingUpdate,
        CompositingClean,

        InPaintInvalidation,
        PaintInvalidationClean,

        // In InPrePaint step, any data needed by painting are prepared.
        // When RuntimeEnabledFeatures::slimmingPaintV2Enabled, paint property trees are built.
        // Otherwise these steps are not applicable.
        InPrePaint,
        PrePaintClean,

        InPaint,
        PaintClean,

        // Once the document starts shutting down, we cannot return
        // to the style/layout/compositing states.
        Stopping,
        Stopped,
    };

    class Scope {
        STACK_ALLOCATED();
        WTF_MAKE_NONCOPYABLE(Scope);
    public:
        Scope(DocumentLifecycle&, LifecycleState finalState);
        ~Scope();

    private:
        DocumentLifecycle& m_lifecycle;
        LifecycleState m_finalState;
    };

    class DeprecatedTransition {
        DISALLOW_NEW();
        WTF_MAKE_NONCOPYABLE(DeprecatedTransition);
    public:
        DeprecatedTransition(LifecycleState from, LifecycleState to);
        ~DeprecatedTransition();

        LifecycleState from() const { return m_from; }
        LifecycleState to() const { return m_to; }

    private:
        DeprecatedTransition* m_previous;
        LifecycleState m_from;
        LifecycleState m_to;
    };

    class DetachScope {
        STACK_ALLOCATED();
        WTF_MAKE_NONCOPYABLE(DetachScope);
    public:
        explicit DetachScope(DocumentLifecycle& documentLifecycle)
            : m_documentLifecycle(documentLifecycle)
        {
            m_documentLifecycle.incrementDetachCount();
        }

        ~DetachScope()
        {
            m_documentLifecycle.decrementDetachCount();
        }

    private:
        DocumentLifecycle& m_documentLifecycle;
    };

    // Throttling is disabled by default. Instantiating this class allows
    // throttling (e.g., during BeginMainFrame). If a script needs to run inside
    // this scope, DisallowThrottlingScope should be used to let the script
    // perform a synchronous layout if necessary.
    class CORE_EXPORT AllowThrottlingScope {
        STACK_ALLOCATED();
        WTF_MAKE_NONCOPYABLE(AllowThrottlingScope);
    public:
        AllowThrottlingScope(DocumentLifecycle&);
        ~AllowThrottlingScope();
    };

    class CORE_EXPORT DisallowThrottlingScope {
        STACK_ALLOCATED();
        WTF_MAKE_NONCOPYABLE(DisallowThrottlingScope);

    public:
        DisallowThrottlingScope(DocumentLifecycle&);
        ~DisallowThrottlingScope();

    private:
        int m_savedCount;
    };

    DocumentLifecycle();
    ~DocumentLifecycle();

    bool isActive() const { return m_state > Inactive && m_state < Stopping; }
    LifecycleState state() const { return m_state; }

    bool stateAllowsTreeMutations() const;
    bool stateAllowsLayoutTreeMutations() const;
    bool stateAllowsDetach() const;
    bool stateAllowsLayoutInvalidation() const;
    bool stateAllowsLayoutTreeNotifications() const;

    void advanceTo(LifecycleState);
    void ensureStateAtMost(LifecycleState);

    bool inDetach() const { return m_detachCount; }
    void incrementDetachCount() { m_detachCount++; }
    void decrementDetachCount()
    {
        DCHECK_GT(m_detachCount, 0);
        m_detachCount--;
    }

    bool throttlingAllowed() const;


private:
#if DCHECK_IS_ON()
    static const char* stateAsDebugString(const LifecycleState);
    bool canAdvanceTo(LifecycleState) const;
    bool canRewindTo(LifecycleState) const;
#endif

    LifecycleState m_state;
    int m_detachCount;
};

inline bool DocumentLifecycle::stateAllowsTreeMutations() const
{
    // FIXME: We should not allow mutations in InPreLayout or AfterPerformLayout either,
    // but we need to fix MediaList listeners and plugins first.
    return m_state != InStyleRecalc
        && m_state != InPerformLayout
        && m_state != InCompositingUpdate
        && m_state != InPrePaint
        && m_state != InPaint;
}

inline bool DocumentLifecycle::stateAllowsLayoutTreeMutations() const
{
    return m_detachCount || m_state == InStyleRecalc || m_state == InLayoutSubtreeChange;
}

inline bool DocumentLifecycle::stateAllowsLayoutTreeNotifications() const
{
    return m_state == InLayoutSubtreeChange;
}

inline bool DocumentLifecycle::stateAllowsDetach() const
{
    return m_state == VisualUpdatePending
        || m_state == InStyleRecalc
        || m_state == StyleClean
        || m_state == LayoutSubtreeChangeClean
        || m_state == InPreLayout
        || m_state == LayoutClean
        || m_state == CompositingClean
        || m_state == PaintInvalidationClean
        || m_state == PrePaintClean
        || m_state == PaintClean
        || m_state == Stopping;
}

inline bool DocumentLifecycle::stateAllowsLayoutInvalidation() const
{
    return m_state != InPerformLayout
        && m_state != InCompositingUpdate
        && m_state != InPaintInvalidation
        && m_state != InPrePaint
        && m_state != InPaint;
}

} // namespace blink

#endif
