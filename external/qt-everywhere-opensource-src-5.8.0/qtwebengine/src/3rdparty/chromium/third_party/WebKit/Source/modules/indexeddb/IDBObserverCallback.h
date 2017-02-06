// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IDBObserverCallback_h
#define IDBObserverCallback_h

#include "platform/heap/Handle.h"

namespace blink {

class ExecutionContext;
class IDBObserver;

class IDBObserverCallback : public GarbageCollectedFinalized<IDBObserverCallback> {
public:
    virtual ~IDBObserverCallback() {}
    virtual ExecutionContext* getExecutionContext() const = 0;
    DEFINE_INLINE_VIRTUAL_TRACE() {}
};

} // namespace blink

#endif // IDBObserverCallback_h
