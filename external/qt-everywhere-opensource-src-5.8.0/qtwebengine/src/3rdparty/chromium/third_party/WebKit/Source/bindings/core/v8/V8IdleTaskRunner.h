/*
 * Copyright (C) 2015 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef V8IdleTaskRunner_h
#define V8IdleTaskRunner_h

#include "core/CoreExport.h"
#include "gin/public/v8_idle_task_runner.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "public/platform/Platform.h"
#include "public/platform/WebScheduler.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebTraceLocation.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

class V8IdleTaskAdapter : public WebThread::IdleTask {
    USING_FAST_MALLOC(V8IdleTaskAdapter);
    WTF_MAKE_NONCOPYABLE(V8IdleTaskAdapter);
public:
    V8IdleTaskAdapter(v8::IdleTask* task) : m_task(wrapUnique(task)) { }
    ~V8IdleTaskAdapter() override { }
    void run(double delaySeconds) override
    {
        m_task->Run(delaySeconds);
    }
private:
    std::unique_ptr<v8::IdleTask> m_task;
};

class V8IdleTaskRunner : public gin::V8IdleTaskRunner {
    USING_FAST_MALLOC(V8IdleTaskRunner);
    WTF_MAKE_NONCOPYABLE(V8IdleTaskRunner);
public:
    V8IdleTaskRunner(WebScheduler* scheduler) : m_scheduler(scheduler) { }
    ~V8IdleTaskRunner() override { }
    void PostIdleTask(v8::IdleTask* task) override
    {
        ASSERT(RuntimeEnabledFeatures::v8IdleTasksEnabled());
        m_scheduler->postIdleTask(BLINK_FROM_HERE, new V8IdleTaskAdapter(task));
    }
private:
    WebScheduler* m_scheduler;
};


} // namespace blink

#endif // V8Initializer_h
