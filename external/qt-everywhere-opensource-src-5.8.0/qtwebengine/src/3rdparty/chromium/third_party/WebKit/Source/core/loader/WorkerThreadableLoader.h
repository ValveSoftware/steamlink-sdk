/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef WorkerThreadableLoader_h
#define WorkerThreadableLoader_h

#include "core/loader/ThreadableLoader.h"
#include "core/loader/ThreadableLoaderClient.h"
#include "core/loader/ThreadableLoaderClientWrapper.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/Referrer.h"
#include "wtf/Functional.h"
#include "wtf/PassRefPtr.h"
#include "wtf/PtrUtil.h"
#include "wtf/RefPtr.h"
#include "wtf/Threading.h"
#include "wtf/ThreadingPrimitives.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"
#include <memory>

namespace blink {

class ExecutionContextTask;
class ResourceError;
class ResourceRequest;
class ResourceResponse;
class WaitableEvent;
class WorkerGlobalScope;
class WorkerLoaderProxy;
struct CrossThreadResourceRequestData;

class WorkerThreadableLoader final : public ThreadableLoader, private ThreadableLoaderClientWrapper::ResourceTimingClient {
    USING_FAST_MALLOC(WorkerThreadableLoader);
public:
    static void loadResourceSynchronously(WorkerGlobalScope&, const ResourceRequest&, ThreadableLoaderClient&, const ThreadableLoaderOptions&, const ResourceLoaderOptions&);
    static std::unique_ptr<WorkerThreadableLoader> create(WorkerGlobalScope& workerGlobalScope, ThreadableLoaderClient* client, const ThreadableLoaderOptions& options, const ResourceLoaderOptions& resourceLoaderOptions)
    {
        return wrapUnique(new WorkerThreadableLoader(workerGlobalScope, client, options, resourceLoaderOptions, LoadAsynchronously));
    }

    ~WorkerThreadableLoader() override;

    void start(const ResourceRequest&) override;

    void overrideTimeout(unsigned long timeout) override;

    void cancel() override;

private:
    enum BlockingBehavior {
        LoadSynchronously,
        LoadAsynchronously
    };

    // Creates a loader on the main thread and bridges communication between
    // the main thread and the worker context's thread where WorkerThreadableLoader runs.
    //
    // Regarding the bridge and lifetimes of items used in callbacks, there are a few cases:
    //
    // all cases. All tasks posted from the worker context's thread are ok because
    //    the last task posted always is "mainThreadDestroy", so MainThreadBridge is
    //    around for all tasks that use it on the main thread.
    //
    // case 1. worker.terminate is called.
    //    In this case, no more tasks are posted from the worker object's thread to the worker
    //    context's thread -- WorkerGlobalScopeProxy implementation enforces this.
    //
    // case 2. xhr gets aborted and the worker context continues running.
    //    The ThreadableLoaderClientWrapper has the underlying client cleared, so no more calls
    //    go through it.  All tasks posted from the worker object's thread to the worker context's
    //    thread do "ThreadableLoaderClientWrapper::ref" (automatically inside of the cross thread copy
    //    done in createCrossThreadTask), so the ThreadableLoaderClientWrapper instance is there until all
    //    tasks are executed.
    class MainThreadBridgeBase : public ThreadableLoaderClient {
    public:
        // All executed on the worker context's thread.
        MainThreadBridgeBase(PassRefPtr<ThreadableLoaderClientWrapper>, PassRefPtr<WorkerLoaderProxy>);
        virtual void start(const ResourceRequest&, const WorkerGlobalScope&) = 0;
        void overrideTimeout(unsigned long timeoutMilliseconds);
        void cancel();
        void destroy();

        // All executed on the main thread.
        void didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent) final;
        void didReceiveResponse(unsigned long identifier, const ResourceResponse&, std::unique_ptr<WebDataConsumerHandle>) final;
        void didReceiveData(const char*, unsigned dataLength) final;
        void didDownloadData(int dataLength) final;
        void didReceiveCachedMetadata(const char*, int dataLength) final;
        void didFinishLoading(unsigned long identifier, double finishTime) final;
        void didFail(const ResourceError&) final;
        void didFailAccessControlCheck(const ResourceError&) final;
        void didFailRedirectCheck() final;
        void didReceiveResourceTiming(const ResourceTimingInfo&) final;

    protected:
        ~MainThreadBridgeBase() override;

        // Posts a task to the main thread to run mainThreadCreateLoader().
        void createLoaderInMainThread(const ThreadableLoaderOptions&, const ResourceLoaderOptions&);
        // Posts a task to the main thread to run mainThreadStart();
        void startInMainThread(const ResourceRequest&, const WorkerGlobalScope&);

        WorkerLoaderProxy* loaderProxy()
        {
            return m_loaderProxy.get();
        }

    private:
        // The following methods are overridden by the subclasses to implement
        // code to forward did.* method invocations to the worker context's
        // thread which is specialized for sync and async case respectively.
        virtual void forwardTaskToWorker(std::unique_ptr<ExecutionContextTask>) = 0;
        virtual void forwardTaskToWorkerOnLoaderDone(std::unique_ptr<ExecutionContextTask>) = 0;

        // All executed on the main thread.
        void mainThreadCreateLoader(ThreadableLoaderOptions, ResourceLoaderOptions, ExecutionContext*);
        void mainThreadStart(std::unique_ptr<CrossThreadResourceRequestData>);
        void mainThreadDestroy(ExecutionContext*);
        void mainThreadOverrideTimeout(unsigned long timeoutMilliseconds, ExecutionContext*);
        void mainThreadCancel(ExecutionContext*);

        // Only to be used on the main thread.
        std::unique_ptr<ThreadableLoader> m_mainThreadLoader;

        // ThreadableLoaderClientWrapper is to be used on the worker context thread.
        // The ref counting is done on either thread:
        // - worker context's thread: held by the tasks
        // - main thread: held by MainThreadBridgeBase
        // Therefore, this must be a ThreadSafeRefCounted.
        RefPtr<ThreadableLoaderClientWrapper> m_workerClientWrapper;

        // Used on the worker context thread.
        RefPtr<WorkerLoaderProxy> m_loaderProxy;
    };

    class MainThreadAsyncBridge final : public MainThreadBridgeBase {
    public:
        MainThreadAsyncBridge(WorkerGlobalScope&, PassRefPtr<ThreadableLoaderClientWrapper>, const ThreadableLoaderOptions&, const ResourceLoaderOptions&);
        void start(const ResourceRequest&, const WorkerGlobalScope&) override;

    private:
        ~MainThreadAsyncBridge() override;

        void forwardTaskToWorker(std::unique_ptr<ExecutionContextTask>) override;
        void forwardTaskToWorkerOnLoaderDone(std::unique_ptr<ExecutionContextTask>) override;
    };

    class MainThreadSyncBridge final : public MainThreadBridgeBase {
    public:
        MainThreadSyncBridge(WorkerGlobalScope&, PassRefPtr<ThreadableLoaderClientWrapper>, const ThreadableLoaderOptions&, const ResourceLoaderOptions&);
        void start(const ResourceRequest&, const WorkerGlobalScope&) override;

    private:
        ~MainThreadSyncBridge() override;

        void forwardTaskToWorker(std::unique_ptr<ExecutionContextTask>) override;
        void forwardTaskToWorkerOnLoaderDone(std::unique_ptr<ExecutionContextTask>) override;

        bool m_done;
        std::unique_ptr<WaitableEvent> m_loaderDoneEvent;
        // Thread-safety: |m_clientTasks| can be written (i.e. Closures are added)
        // on the main thread only before |m_loaderDoneEvent| is signaled and can be read
        // on the worker context thread only after |m_loaderDoneEvent| is signaled.
        Vector<std::unique_ptr<ExecutionContextTask>> m_clientTasks;
        Mutex m_lock;
    };

    WorkerThreadableLoader(WorkerGlobalScope&, ThreadableLoaderClient*, const ThreadableLoaderOptions&, const ResourceLoaderOptions&, BlockingBehavior);

    void didReceiveResourceTiming(const ResourceTimingInfo&) override;

    Persistent<WorkerGlobalScope> m_workerGlobalScope;
    RefPtr<ThreadableLoaderClientWrapper> m_workerClientWrapper;

    MainThreadBridgeBase* m_bridge;
};

} // namespace blink

#endif // WorkerThreadableLoader_h
