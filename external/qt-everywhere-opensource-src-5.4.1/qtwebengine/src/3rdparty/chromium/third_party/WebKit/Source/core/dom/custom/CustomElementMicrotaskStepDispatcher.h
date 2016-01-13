// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElementMicrotaskStepDispatcher_h
#define CustomElementMicrotaskStepDispatcher_h

#include "core/dom/custom/CustomElementAsyncImportMicrotaskQueue.h"
#include "core/dom/custom/CustomElementSyncMicrotaskQueue.h"
#include "platform/heap/Handle.h"

namespace WebCore {

class HTMLImportLoader;

class CustomElementMicrotaskStepDispatcher : public RefCountedWillBeGarbageCollectedFinalized<CustomElementMicrotaskStepDispatcher> {
    WTF_MAKE_NONCOPYABLE(CustomElementMicrotaskStepDispatcher);
public:
    static PassRefPtrWillBeRawPtr<CustomElementMicrotaskStepDispatcher> create() { return adoptRefWillBeNoop(new CustomElementMicrotaskStepDispatcher()); }

    void enqueue(HTMLImportLoader* parentLoader, PassOwnPtrWillBeRawPtr<CustomElementMicrotaskStep>);
    void enqueue(HTMLImportLoader* parentLoader, PassOwnPtrWillBeRawPtr<CustomElementMicrotaskImportStep>, bool importIsSync);

    void dispatch();
    void trace(Visitor*);

#if !defined(NDEBUG)
    void show(unsigned indent);
#endif

private:
    CustomElementMicrotaskStepDispatcher();

    RefPtrWillBeMember<CustomElementSyncMicrotaskQueue> m_syncQueue;
    RefPtrWillBeMember<CustomElementAsyncImportMicrotaskQueue> m_asyncQueue;
};

}

#endif // CustomElementMicrotaskStepDispatcher_h
