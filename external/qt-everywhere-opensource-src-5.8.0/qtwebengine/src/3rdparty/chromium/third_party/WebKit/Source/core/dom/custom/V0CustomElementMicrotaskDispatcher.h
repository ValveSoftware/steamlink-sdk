// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V0CustomElementMicrotaskDispatcher_h
#define V0CustomElementMicrotaskDispatcher_h

#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"
#include "wtf/Vector.h"

namespace blink {

class V0CustomElementCallbackQueue;

class V0CustomElementMicrotaskDispatcher final : public GarbageCollected<V0CustomElementMicrotaskDispatcher> {
    WTF_MAKE_NONCOPYABLE(V0CustomElementMicrotaskDispatcher);
public:
    static V0CustomElementMicrotaskDispatcher& instance();

    void enqueue(V0CustomElementCallbackQueue*);

    bool elementQueueIsEmpty() { return m_elements.isEmpty(); }

    DECLARE_TRACE();

private:
    V0CustomElementMicrotaskDispatcher();

    void ensureMicrotaskScheduledForElementQueue();
    void ensureMicrotaskScheduled();

    static void dispatch();
    void doDispatch();

    bool m_hasScheduledMicrotask;
    enum {
        Quiescent,
        Resolving,
        DispatchingCallbacks
    } m_phase;

    HeapVector<Member<V0CustomElementCallbackQueue>> m_elements;
};

} // namespace blink

#endif // V0CustomElementMicrotaskDispatcher_h
