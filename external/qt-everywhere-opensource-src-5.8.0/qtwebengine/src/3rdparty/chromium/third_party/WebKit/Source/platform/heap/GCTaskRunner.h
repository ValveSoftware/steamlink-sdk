/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef GCTaskRunner_h
#define GCTaskRunner_h

#include "platform/CrossThreadFunctional.h"
#include "platform/heap/ThreadState.h"
#include "public/platform/WebTaskRunner.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebTraceLocation.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

class MessageLoopInterruptor final : public BlinkGCInterruptor {
public:
    explicit MessageLoopInterruptor(WebTaskRunner* taskRunner) : m_taskRunner(taskRunner) { }

    void requestInterrupt() override
    {
        // GCTask has an empty run() method. Its only purpose is to guarantee
        // that MessageLoop will have a task to process which will result
        // in GCTaskRunner::didProcessTask being executed.
        m_taskRunner->postTask(BLINK_FROM_HERE, crossThreadBind(&runGCTask));
    }

private:
    static void runGCTask()
    {
        // Don't do anything here because we don't know if this is
        // a nested event loop or not. GCTaskRunner::didProcessTask
        // will enter correct safepoint for us.
        // We are not calling onInterrupted() because that always
        // conservatively enters safepoint with pointers on stack.
    }

    WebTaskRunner* m_taskRunner;
};

class GCTaskObserver final : public WebThread::TaskObserver {
    USING_FAST_MALLOC(GCTaskObserver);
public:
    GCTaskObserver() : m_nesting(0) { }

    ~GCTaskObserver()
    {
        // m_nesting can be 1 if this was unregistered in a task and
        // didProcessTask was not called.
        ASSERT(!m_nesting || m_nesting == 1);
    }

    virtual void willProcessTask()
    {
        m_nesting++;
    }

    virtual void didProcessTask()
    {
        // In the production code WebKit::initialize is called from inside the
        // message loop so we can get didProcessTask() without corresponding
        // willProcessTask once. This is benign.
        if (m_nesting)
            m_nesting--;

        ThreadState::current()->safePoint(m_nesting ? BlinkGC::HeapPointersOnStack : BlinkGC::NoHeapPointersOnStack);
    }

private:
    int m_nesting;
};

class GCTaskRunner final {
    USING_FAST_MALLOC(GCTaskRunner);
public:
    explicit GCTaskRunner(WebThread* thread)
        : m_gcTaskObserver(wrapUnique(new GCTaskObserver))
        , m_thread(thread)
    {
        m_thread->addTaskObserver(m_gcTaskObserver.get());
        ThreadState::current()->addInterruptor(wrapUnique(new MessageLoopInterruptor(thread->getWebTaskRunner())));
    }

    ~GCTaskRunner()
    {
        m_thread->removeTaskObserver(m_gcTaskObserver.get());
    }

private:
    std::unique_ptr<GCTaskObserver> m_gcTaskObserver;
    WebThread* m_thread;
};

} // namespace blink

#endif
