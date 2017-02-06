// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorTaskRunner_h
#define InspectorTaskRunner_h

#include "core/CoreExport.h"
#include "wtf/Allocator.h"
#include "wtf/Deque.h"
#include "wtf/Forward.h"
#include "wtf/Functional.h"
#include "wtf/Noncopyable.h"
#include "wtf/ThreadingPrimitives.h"
#include <v8.h>

namespace blink {

class CORE_EXPORT InspectorTaskRunner final {
    WTF_MAKE_NONCOPYABLE(InspectorTaskRunner);
    USING_FAST_MALLOC(InspectorTaskRunner);
public:
    InspectorTaskRunner();
    ~InspectorTaskRunner();

    using Task = WTF::CrossThreadClosure;
    void appendTask(std::unique_ptr<Task>);

    enum WaitMode { WaitForTask, DontWaitForTask };
    std::unique_ptr<Task> takeNextTask(WaitMode);

    void interruptAndRunAllTasksDontWait(v8::Isolate*);
    void runAllTasksDontWait();

    void kill();

    class CORE_EXPORT IgnoreInterruptsScope final {
        USING_FAST_MALLOC(IgnoreInterruptsScope);
    public:
        explicit IgnoreInterruptsScope(InspectorTaskRunner*);
        ~IgnoreInterruptsScope();

    private:
        bool m_wasIgnoring;
        InspectorTaskRunner* m_taskRunner;
    };

private:
    static void v8InterruptCallback(v8::Isolate*, void* data);

    bool m_ignoreInterrupts;
    Mutex m_mutex;
    ThreadCondition m_condition;
    Deque<std::unique_ptr<Task>> m_queue;
    bool m_killed;
};

} // namespace blink


#endif // !defined(InspectorTaskRunner_h)
