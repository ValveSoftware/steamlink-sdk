// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScopedWindowFocusAllowedIndicator_h
#define ScopedWindowFocusAllowedIndicator_h

#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/ExecutionContext.h"
#include "wtf/Noncopyable.h"

namespace blink {

class ScopedWindowFocusAllowedIndicator final {
    USING_FAST_MALLOC(ScopedWindowFocusAllowedIndicator);
    WTF_MAKE_NONCOPYABLE(ScopedWindowFocusAllowedIndicator);
public:
    explicit ScopedWindowFocusAllowedIndicator(ExecutionContext* executionContext)
        : m_observer(new Observer(executionContext))
    {
    }
    ~ScopedWindowFocusAllowedIndicator()
    {
        m_observer->dispose();
    }

private:
    class Observer final : public GarbageCollected<Observer>, public ContextLifecycleObserver {
        USING_GARBAGE_COLLECTED_MIXIN(Observer);
    public:
        explicit Observer(ExecutionContext* executionContext)
            : ContextLifecycleObserver(executionContext)
        {
            if (executionContext)
                executionContext->allowWindowInteraction();
        }

        void dispose()
        {
            if (getExecutionContext())
                getExecutionContext()->consumeWindowInteraction();
        }
    };

    // In Oilpan, destructors are not allowed to touch other on-heap objects.
    // The Observer indirection is needed to keep
    // ScopedWindowFocusAllowedIndicator off-heap and thus allows its destructor
    // to call getExecutionContext()->consumeWindowInteraction().
    Persistent<Observer> m_observer;
};

} // namespace blink

#endif // ScopedWindowFocusAllowedIndicator_h
