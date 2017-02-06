// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IntersectionObserverCallback_h
#define IntersectionObserverCallback_h

#include "platform/heap/Handle.h"

namespace blink {

class ExecutionContext;
class IntersectionObserver;
class IntersectionObserverEntry;

class IntersectionObserverCallback : public GarbageCollectedFinalized<IntersectionObserverCallback> {
public:
    virtual ~IntersectionObserverCallback() {}
    virtual void handleEvent(const HeapVector<Member<IntersectionObserverEntry>>&, IntersectionObserver&) = 0;
    virtual ExecutionContext* getExecutionContext() const = 0;
    DEFINE_INLINE_VIRTUAL_TRACE() { }
};

} // namespace blink

#endif // IntersectionObserverCallback_h
