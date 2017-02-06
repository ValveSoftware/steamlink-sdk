// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkerThreadLifecycleObserver_h
#define WorkerThreadLifecycleObserver_h

#include "core/CoreExport.h"
#include "platform/LifecycleObserver.h"

namespace blink {

class WorkerThreadLifecycleContext;

// An interface for observing worker thread termination from the main thread.
// This may be useful, for example, when an object living on the main thread
// needs to release references to objects on the worker thread before it gets
// terminated.
//
// A class that inherits this interface should override
// LifecycleObserver::contextDestroyed() that is called on the main thread when
// the worker thread is about to terminate. While contextDestroyed() is called,
// it is guaranteed that the worker thread is still alive.
//
// A newly created observer should firstly check whether the worker thread is
// alive by wasContextDestroyedBeforeObserverCreation(). If this return true,
// the worker thread has already been terminated before the observer is created,
// and contextDestroyed() is never notified.
class CORE_EXPORT WorkerThreadLifecycleObserver : public LifecycleObserver<WorkerThreadLifecycleContext, WorkerThreadLifecycleObserver> {
protected:
    explicit WorkerThreadLifecycleObserver(WorkerThreadLifecycleContext*);

    bool wasContextDestroyedBeforeObserverCreation() const
    {
        return m_wasContextDestroyedBeforeObserverCreation;
    }

private:
    const bool m_wasContextDestroyedBeforeObserverCreation;
};

} // namespace blink

#endif // WorkerThreadLifecycleObserver_h
