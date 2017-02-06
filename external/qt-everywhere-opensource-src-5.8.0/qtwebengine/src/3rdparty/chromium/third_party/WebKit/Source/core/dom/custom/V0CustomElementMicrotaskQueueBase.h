// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V0CustomElementMicrotaskQueueBase_h
#define V0CustomElementMicrotaskQueueBase_h

#include "core/dom/custom/V0CustomElementMicrotaskStep.h"
#include "platform/heap/Handle.h"
#include "wtf/Vector.h"

namespace blink {

class V0CustomElementMicrotaskQueueBase : public GarbageCollectedFinalized<V0CustomElementMicrotaskQueueBase> {
    WTF_MAKE_NONCOPYABLE(V0CustomElementMicrotaskQueueBase);
public:
    virtual ~V0CustomElementMicrotaskQueueBase() { }

    bool isEmpty() const { return m_queue.isEmpty(); }
    void dispatch();

    DECLARE_TRACE();

#if !defined(NDEBUG)
    void show(unsigned indent);
#endif

protected:
    V0CustomElementMicrotaskQueueBase() : m_inDispatch(false) { }
    virtual void doDispatch() = 0;

    HeapVector<Member<V0CustomElementMicrotaskStep>> m_queue;
    bool m_inDispatch;
};

} // namespace blink

#endif // V0CustomElementMicrotaskQueueBase_h
