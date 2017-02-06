// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebTaskRunner.h"

namespace blink {

class SameThreadTask : public WebTaskRunner::Task {
    USING_FAST_MALLOC(SameThreadTask);
    WTF_MAKE_NONCOPYABLE(SameThreadTask);
public:
    explicit SameThreadTask(std::unique_ptr<WTF::Closure> closure)
        : m_closure(std::move(closure))
    {
    }

    void run() override
    {
        (*m_closure)();
    }

private:
    std::unique_ptr<WTF::Closure> m_closure;
};

class CrossThreadTask : public WebTaskRunner::Task {
    USING_FAST_MALLOC(CrossThreadTask);
    WTF_MAKE_NONCOPYABLE(CrossThreadTask);
public:
    explicit CrossThreadTask(std::unique_ptr<CrossThreadClosure> closure)
        : m_closure(std::move(closure))
    {
    }

    void run() override
    {
        (*m_closure)();
    }

private:
    std::unique_ptr<CrossThreadClosure> m_closure;
};

void WebTaskRunner::postTask(const WebTraceLocation& location, std::unique_ptr<CrossThreadClosure> task)
{
    postTask(location, new CrossThreadTask(std::move(task)));
}

void WebTaskRunner::postDelayedTask(const WebTraceLocation& location, std::unique_ptr<CrossThreadClosure> task, long long delayMs)
{
    postDelayedTask(location, new CrossThreadTask(std::move(task)), delayMs);
}

void WebTaskRunner::postTask(const WebTraceLocation& location, std::unique_ptr<WTF::Closure> task)
{
    postTask(location, new SameThreadTask(std::move(task)));
}

void WebTaskRunner::postDelayedTask(const WebTraceLocation& location, std::unique_ptr<WTF::Closure> task, long long delayMs)
{
    postDelayedTask(location, new SameThreadTask(std::move(task)), delayMs);
}

} // namespace blink
