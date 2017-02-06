// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V0CustomElementMicrotaskRunQueue_h
#define V0CustomElementMicrotaskRunQueue_h

#include "platform/heap/Handle.h"

namespace blink {

class V0CustomElementSyncMicrotaskQueue;
class V0CustomElementAsyncImportMicrotaskQueue;
class V0CustomElementMicrotaskStep;
class HTMLImportLoader;

class V0CustomElementMicrotaskRunQueue : public GarbageCollected<V0CustomElementMicrotaskRunQueue> {
public:
    static V0CustomElementMicrotaskRunQueue* create()
    {
        return new V0CustomElementMicrotaskRunQueue;
    }

    void enqueue(HTMLImportLoader* parentLoader, V0CustomElementMicrotaskStep*, bool importIsSync);
    void requestDispatchIfNeeded();
    bool isEmpty() const;

    DECLARE_TRACE();

private:
    V0CustomElementMicrotaskRunQueue();

    void dispatch();

    Member<V0CustomElementSyncMicrotaskQueue> m_syncQueue;
    Member<V0CustomElementAsyncImportMicrotaskQueue> m_asyncQueue;
    bool m_dispatchIsPending;
};

} // namespace blink

#endif // V0CustomElementMicrotaskRunQueue_h
