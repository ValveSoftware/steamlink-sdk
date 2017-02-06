// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebTaskRunner_h
#define WebTaskRunner_h

#include "WebCommon.h"

#ifdef INSIDE_BLINK
#include "wtf/Functional.h"
#include "wtf/PtrUtil.h"
#include <memory>
#endif

namespace blink {

class WebTraceLocation;

// The blink representation of a chromium SingleThreadTaskRunner.
class BLINK_PLATFORM_EXPORT WebTaskRunner {
public:
    virtual ~WebTaskRunner() {}

    class BLINK_PLATFORM_EXPORT Task {
    public:
        virtual ~Task() { }
        virtual void run() = 0;
    };

    // Schedule a task to be run on the the associated WebThread.
    // Takes ownership of |Task|. Can be called from any thread.
    virtual void postTask(const WebTraceLocation&, Task*) = 0;

    // Schedule a task to be run after |delayMs| on the the associated WebThread.
    // Takes ownership of |Task|. Can be called from any thread.
    virtual void postDelayedTask(const WebTraceLocation&, Task*, double delayMs) = 0;

    // Returns a clone of the WebTaskRunner.
    virtual WebTaskRunner* clone() = 0;

    // ---

    // Headless Chrome virtualises time for determinism and performance (fast forwarding
    // of timers). To make this work some parts of blink (e.g. Timers) need to use virtual
    // time, however by default new code should use the normal non-virtual time APIs.

    // Returns a double which is the number of seconds since epoch (Jan 1, 1970).
    // This may represent either the real time, or a virtual time depending on
    // whether or not the WebTaskRunner is associated with a virtual time domain or a
    // real time domain.
    virtual double virtualTimeSeconds() const = 0;

    // Returns a microsecond resolution platform dependant time source.
    // This may represent either the real time, or a virtual time depending on
    // whether or not the WebTaskRunner is associated with a virtual time domain or a
    // real time domain.
    virtual double monotonicallyIncreasingVirtualTimeSeconds() const = 0;

#ifdef INSIDE_BLINK
    // Helpers for posting bound functions as tasks.

    // For cross-thread posting. Can be called from any thread.
    void postTask(const WebTraceLocation&, std::unique_ptr<CrossThreadClosure>);
    void postDelayedTask(const WebTraceLocation&, std::unique_ptr<CrossThreadClosure>, long long delayMs);

    // For same-thread posting. Must be called from the associated WebThread.
    void postTask(const WebTraceLocation&, std::unique_ptr<WTF::Closure>);
    void postDelayedTask(const WebTraceLocation&, std::unique_ptr<WTF::Closure>, long long delayMs);

    std::unique_ptr<WebTaskRunner> adoptClone()
    {
        return wrapUnique(clone());
    }
#endif
};

} // namespace blink

#endif // WebTaskRunner_h
