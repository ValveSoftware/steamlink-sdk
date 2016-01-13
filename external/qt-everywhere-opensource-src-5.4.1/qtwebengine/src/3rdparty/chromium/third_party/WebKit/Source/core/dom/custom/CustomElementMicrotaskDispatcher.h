// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElementMicrotaskDispatcher_h
#define CustomElementMicrotaskDispatcher_h

#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

class CustomElementCallbackQueue;
class CustomElementMicrotaskImportStep;
class CustomElementMicrotaskStep;
class CustomElementMicrotaskStepDispatcher;
class HTMLImportLoader;

class CustomElementMicrotaskDispatcher FINAL : public NoBaseWillBeGarbageCollected<CustomElementMicrotaskDispatcher> {
    WTF_MAKE_NONCOPYABLE(CustomElementMicrotaskDispatcher);
    DECLARE_EMPTY_DESTRUCTOR_WILL_BE_REMOVED(CustomElementMicrotaskDispatcher);
public:
    static CustomElementMicrotaskDispatcher& instance();

    void enqueue(HTMLImportLoader* parentLoader, PassOwnPtrWillBeRawPtr<CustomElementMicrotaskStep>);
    void enqueue(HTMLImportLoader* parentLoader, PassOwnPtrWillBeRawPtr<CustomElementMicrotaskImportStep>, bool importIsSync);

    void enqueue(CustomElementCallbackQueue*);


    void importDidFinish(CustomElementMicrotaskImportStep*);

    bool elementQueueIsEmpty() { return m_elements.isEmpty(); }

    void trace(Visitor*);

#if !defined(NDEBUG)
    void show();
#endif

private:
    CustomElementMicrotaskDispatcher();

    void ensureMicrotaskScheduledForElementQueue();
    void ensureMicrotaskScheduledForMicrotaskSteps();
    void ensureMicrotaskScheduled();

    static void dispatch();
    void doDispatch();

    bool m_hasScheduledMicrotask;
    enum {
        Quiescent,
        Resolving,
        DispatchingCallbacks
    } m_phase;

    RefPtrWillBeMember<CustomElementMicrotaskStepDispatcher> m_steps;
    WillBeHeapVector<RawPtrWillBeMember<CustomElementCallbackQueue> > m_elements;
};

}

#if !defined(NDEBUG)
void showCEMD();
#endif

#endif // CustomElementMicrotaskDispatcher_h
