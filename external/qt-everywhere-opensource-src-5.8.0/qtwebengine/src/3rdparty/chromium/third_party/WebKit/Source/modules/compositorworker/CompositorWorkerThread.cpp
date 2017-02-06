// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/CompositorWorkerThread.h"

#include "bindings/core/v8/V8GCController.h"
#include "bindings/core/v8/V8Initializer.h"
#include "core/workers/InProcessWorkerObjectProxy.h"
#include "core/workers/WorkerBackingThread.h"
#include "core/workers/WorkerThreadStartupData.h"
#include "modules/compositorworker/CompositorWorkerGlobalScope.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/TraceEvent.h"
#include "platform/WaitableEvent.h"
#include "platform/WebThreadSupportingGC.h"
#include "public/platform/Platform.h"
#include "wtf/Assertions.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

namespace {

// This is a singleton class holding the compositor worker thread in this
// renderer process. BackingThreadHolder::m_thread is cleared by
// ModulesInitializer::shutdown.
// See WorkerThread::terminateAndWaitForAllWorkers for the process shutdown
// case.
class BackingThreadHolder {
public:
    static BackingThreadHolder& instance()
    {
        MutexLocker locker(holderInstanceMutex());
        return *s_instance;
    }

    static void ensureInstance()
    {
        if (!s_instance)
            s_instance = new BackingThreadHolder;
    }

    static void clear()
    {
        MutexLocker locker(holderInstanceMutex());
        if (s_instance) {
            s_instance->shutdownAndWait();
            delete s_instance;
            s_instance = nullptr;
        }
    }

    static void createForTest()
    {
        MutexLocker locker(holderInstanceMutex());
        DCHECK_EQ(nullptr, s_instance);
        s_instance = new BackingThreadHolder(WorkerBackingThread::createForTest(Platform::current()->compositorThread()));
    }

    WorkerBackingThread* thread() { return m_thread.get(); }

private:
    BackingThreadHolder(std::unique_ptr<WorkerBackingThread> useBackingThread = nullptr)
        : m_thread(useBackingThread ? std::move(useBackingThread) : WorkerBackingThread::create(Platform::current()->compositorThread()))
    {
        DCHECK(isMainThread());
        m_thread->backingThread().postTask(BLINK_FROM_HERE, crossThreadBind(&BackingThreadHolder::initializeOnThread, crossThreadUnretained(this)));
    }

    static Mutex& holderInstanceMutex()
    {
        DEFINE_THREAD_SAFE_STATIC_LOCAL(Mutex, holderMutex, new Mutex);
        return holderMutex;
    }

    void initializeOnThread()
    {
        MutexLocker locker(holderInstanceMutex());
        DCHECK(!m_initialized);
        m_thread->initialize();
        m_initialized = true;
    }

    void shutdownAndWait()
    {
        DCHECK(isMainThread());
        WaitableEvent doneEvent;
        m_thread->backingThread().postTask(BLINK_FROM_HERE, crossThreadBind(&BackingThreadHolder::shutdownOnThread, crossThreadUnretained(this), crossThreadUnretained(&doneEvent)));
        doneEvent.wait();
    }

    void shutdownOnThread(WaitableEvent* doneEvent)
    {
        m_thread->shutdown();
        doneEvent->signal();
    }

    std::unique_ptr<WorkerBackingThread> m_thread;
    bool m_initialized = false;

    static BackingThreadHolder* s_instance;
};

BackingThreadHolder* BackingThreadHolder::s_instance = nullptr;

} // namespace

std::unique_ptr<CompositorWorkerThread> CompositorWorkerThread::create(PassRefPtr<WorkerLoaderProxy> workerLoaderProxy, InProcessWorkerObjectProxy& workerObjectProxy, double timeOrigin)
{
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("compositor-worker"), "CompositorWorkerThread::create");
    ASSERT(isMainThread());
    return wrapUnique(new CompositorWorkerThread(workerLoaderProxy, workerObjectProxy, timeOrigin));
}

CompositorWorkerThread::CompositorWorkerThread(PassRefPtr<WorkerLoaderProxy> workerLoaderProxy, InProcessWorkerObjectProxy& workerObjectProxy, double timeOrigin)
    : WorkerThread(workerLoaderProxy, workerObjectProxy)
    , m_workerObjectProxy(workerObjectProxy)
    , m_timeOrigin(timeOrigin)
{
}

CompositorWorkerThread::~CompositorWorkerThread()
{
}

WorkerBackingThread& CompositorWorkerThread::workerBackingThread()
{
    return *BackingThreadHolder::instance().thread();
}

WorkerGlobalScope*CompositorWorkerThread::createWorkerGlobalScope(std::unique_ptr<WorkerThreadStartupData> startupData)
{
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("compositor-worker"), "CompositorWorkerThread::createWorkerGlobalScope");
    return CompositorWorkerGlobalScope::create(this, std::move(startupData), m_timeOrigin);
}

void CompositorWorkerThread::ensureSharedBackingThread()
{
    DCHECK(isMainThread());
    BackingThreadHolder::ensureInstance();
}

void CompositorWorkerThread::clearSharedBackingThread()
{
    DCHECK(isMainThread());
    BackingThreadHolder::clear();
}

void CompositorWorkerThread::createSharedBackingThreadForTest()
{
    BackingThreadHolder::createForTest();
}

} // namespace blink
