// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebThreadSupportingGC_h
#define WebThreadSupportingGC_h

#include "platform/heap/GCTaskRunner.h"
#include "public/platform/Platform.h"
#include "public/platform/WebTaskRunner.h"
#include "public/platform/WebThread.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"
#include <memory>

namespace blink {

// WebThreadSupportingGC wraps a WebThread and adds support for attaching
// to and detaching from the Blink GC infrastructure. The initialize method
// must be called during initialization on the WebThread and before the
// thread allocates any objects managed by the Blink GC. The shutdown
// method must be called on the WebThread during shutdown when the thread
// no longer needs to access objects managed by the Blink GC.
//
// WebThreadSupportingGC usually internally creates and owns WebThread unless
// an existing WebThread is given via createForThread.
class PLATFORM_EXPORT WebThreadSupportingGC final {
    USING_FAST_MALLOC(WebThreadSupportingGC);
    WTF_MAKE_NONCOPYABLE(WebThreadSupportingGC);
public:
    static std::unique_ptr<WebThreadSupportingGC> create(const char* name, bool perThreadHeapEnabled = false);
    static std::unique_ptr<WebThreadSupportingGC> createForThread(WebThread*, bool perThreadHeapEnabled = false);
    ~WebThreadSupportingGC();

    void postTask(const WebTraceLocation& location, std::unique_ptr<WTF::Closure> task)
    {
        m_thread->getWebTaskRunner()->postTask(location, std::move(task));
    }

    void postDelayedTask(const WebTraceLocation& location, std::unique_ptr<WTF::Closure> task, long long delayMs)
    {
        m_thread->getWebTaskRunner()->postDelayedTask(location, std::move(task), delayMs);
    }

    void postTask(const WebTraceLocation& location, std::unique_ptr<CrossThreadClosure> task)
    {
        m_thread->getWebTaskRunner()->postTask(location, std::move(task));
    }

    void postDelayedTask(const WebTraceLocation& location, std::unique_ptr<CrossThreadClosure> task, long long delayMs)
    {
        m_thread->getWebTaskRunner()->postDelayedTask(location, std::move(task), delayMs);
    }

    bool isCurrentThread() const
    {
        return m_thread->isCurrentThread();
    }

    void addTaskObserver(WebThread::TaskObserver* observer)
    {
        m_thread->addTaskObserver(observer);
    }

    void removeTaskObserver(WebThread::TaskObserver* observer)
    {
        m_thread->removeTaskObserver(observer);
    }

    void initialize();
    void shutdown();

    WebThread& platformThread() const
    {
        ASSERT(m_thread);
        return *m_thread;
    }

private:
    WebThreadSupportingGC(const char* name, WebThread*, bool perThreadHeapEnabled);

    std::unique_ptr<GCTaskRunner> m_gcTaskRunner;

    // m_thread is guaranteed to be non-null after this instance is constructed.
    // m_owningThread is non-null unless this instance is constructed for an
    // existing thread via createForThread().
    WebThread* m_thread = nullptr;
    std::unique_ptr<WebThread> m_owningThread;
    bool m_perThreadHeapEnabled;
};

} // namespace blink

#endif
