// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PerformanceObserverCallback_h
#define PerformanceObserverCallback_h

#include "platform/heap/Handle.h"
#include "wtf/RefPtr.h"

namespace blink {

class ExecutionContext;
class PerformanceObserverEntryList;
class PerformanceObserver;

class PerformanceObserverCallback : public GarbageCollectedFinalized<PerformanceObserverCallback> {
public:
    virtual ~PerformanceObserverCallback() { }

    virtual void handleEvent(PerformanceObserverEntryList*, PerformanceObserver*) = 0;
    virtual ExecutionContext* getExecutionContext() const = 0;

    DEFINE_INLINE_VIRTUAL_TRACE() { }
};

} // namespace blink

#endif // PerformanceObserverCallback_h
