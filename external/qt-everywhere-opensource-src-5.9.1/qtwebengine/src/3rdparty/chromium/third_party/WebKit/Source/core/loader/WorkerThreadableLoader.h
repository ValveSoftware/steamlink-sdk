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

#include "core/dom/ExecutionContextTask.h"
#include "core/loader/ThreadableLoader.h"
#include "core/loader/ThreadableLoaderClient.h"
#include "core/workers/WorkerThread.h"
#include "core/workers/WorkerThreadLifecycleObserver.h"
#include "platform/WaitableEvent.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebTraceLocation.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/Threading.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"
#include <memory>

namespace blink {

class ResourceError;
class ResourceRequest;
class ResourceResponse;
class WorkerGlobalScope;
class WorkerLoaderProxy;
struct CrossThreadResourceRequestData;
struct CrossThreadResourceTimingInfoData;

// A WorkerThreadableLoader is a ThreadableLoader implementation intended to
// be used in a WebWorker thread. Because Blink's ResourceFetcher and
// ResourceLoader work only in the main thread, a WorkerThreadableLoader holds
// a ThreadableLoader in the main thread and delegates tasks asynchronously
// to the loader.
//
// CTP: CrossThreadPersistent
// CTWP: CrossThreadWeakPersistent
//
// ----------------------------------------------------------------
//                 +------------------------+
//       raw ptr   | ThreadableLoaderClient |
//      +--------> | worker thread          |
//      |          +------------------------+
//      |
// +----+------------------+    CTP  +------------------------+
// + WorkerThreadableLoader|<--------+ MainThreadLoaderHolder |
// | worker thread         +-------->| main thread            |
// +-----------------------+   CTWP  +----------------------+-+
//                                                          |
//                                 +------------------+     | Member
//                                 | ThreadableLoader | <---+
//                                 |      main thread |
//                                 +------------------+
//
class WorkerThreadableLoader final : public ThreadableLoader {
 public:
  static void loadResourceSynchronously(WorkerGlobalScope&,
                                        const ResourceRequest&,
                                        ThreadableLoaderClient&,
                                        const ThreadableLoaderOptions&,
                                        const ResourceLoaderOptions&);
  static WorkerThreadableLoader* create(
      WorkerGlobalScope& workerGlobalScope,
      ThreadableLoaderClient* client,
      const ThreadableLoaderOptions& options,
      const ResourceLoaderOptions& resourceLoaderOptions) {
    return new WorkerThreadableLoader(workerGlobalScope, client, options,
                                      resourceLoaderOptions,
                                      LoadAsynchronously);
  }

  ~WorkerThreadableLoader() override;

  // ThreadableLoader functions
  void start(const ResourceRequest&) override;
  void overrideTimeout(unsigned long timeout) override;
  void cancel() override;

  DECLARE_TRACE();

 private:
  enum BlockingBehavior { LoadSynchronously, LoadAsynchronously };

  // A TaskForwarder forwards an ExecutionContextTask to the worker thread.
  class TaskForwarder : public GarbageCollectedFinalized<TaskForwarder> {
   public:
    virtual ~TaskForwarder() {}
    virtual void forwardTask(const WebTraceLocation&,
                             std::unique_ptr<ExecutionContextTask>) = 0;
    virtual void forwardTaskWithDoneSignal(
        const WebTraceLocation&,
        std::unique_ptr<ExecutionContextTask>) = 0;
    virtual void abort() = 0;

    DEFINE_INLINE_VIRTUAL_TRACE() {}
  };
  class AsyncTaskForwarder;
  struct TaskWithLocation;
  class WaitableEventWithTasks;
  class SyncTaskForwarder;

  // An instance of this class lives in the main thread. It is a
  // ThreadableLoaderClient for a DocumentThreadableLoader and forward
  // notifications to the associated WorkerThreadableLoader living in the
  // worker thread.
  class MainThreadLoaderHolder final
      : public GarbageCollectedFinalized<MainThreadLoaderHolder>,
        public ThreadableLoaderClient,
        public WorkerThreadLifecycleObserver {
    USING_GARBAGE_COLLECTED_MIXIN(MainThreadLoaderHolder);

   public:
    static void createAndStart(WorkerThreadableLoader*,
                               PassRefPtr<WorkerLoaderProxy>,
                               WorkerThreadLifecycleContext*,
                               std::unique_ptr<CrossThreadResourceRequestData>,
                               const ThreadableLoaderOptions&,
                               const ResourceLoaderOptions&,
                               PassRefPtr<WaitableEventWithTasks>,
                               ExecutionContext*);
    ~MainThreadLoaderHolder() override;

    void overrideTimeout(unsigned long timeoutMillisecond);
    void cancel();

    void didSendData(unsigned long long bytesSent,
                     unsigned long long totalBytesToBeSent) override;
    void didReceiveResponse(unsigned long identifier,
                            const ResourceResponse&,
                            std::unique_ptr<WebDataConsumerHandle>) override;
    void didReceiveData(const char*, unsigned dataLength) override;
    void didDownloadData(int dataLength) override;
    void didReceiveCachedMetadata(const char*, int dataLength) override;
    void didFinishLoading(unsigned long identifier, double finishTime) override;
    void didFail(const ResourceError&) override;
    void didFailAccessControlCheck(const ResourceError&) override;
    void didFailRedirectCheck() override;
    void didReceiveResourceTiming(const ResourceTimingInfo&) override;

    void contextDestroyed() override;

    DECLARE_TRACE();

   private:
    MainThreadLoaderHolder(TaskForwarder*, WorkerThreadLifecycleContext*);
    void start(Document&,
               std::unique_ptr<CrossThreadResourceRequestData>,
               const ThreadableLoaderOptions&,
               const ResourceLoaderOptions&);

    Member<TaskForwarder> m_forwarder;
    Member<ThreadableLoader> m_mainThreadLoader;

    // |*m_workerLoader| lives in the worker thread.
    CrossThreadWeakPersistent<WorkerThreadableLoader> m_workerLoader;
  };

  WorkerThreadableLoader(WorkerGlobalScope&,
                         ThreadableLoaderClient*,
                         const ThreadableLoaderOptions&,
                         const ResourceLoaderOptions&,
                         BlockingBehavior);
  void didStart(MainThreadLoaderHolder*);

  void didSendData(unsigned long long bytesSent,
                   unsigned long long totalBytesToBeSent);
  void didReceiveResponse(unsigned long identifier,
                          std::unique_ptr<CrossThreadResourceResponseData>,
                          std::unique_ptr<WebDataConsumerHandle>);
  void didReceiveData(std::unique_ptr<Vector<char>> data);
  void didReceiveCachedMetadata(std::unique_ptr<Vector<char>> data);
  void didFinishLoading(unsigned long identifier, double finishTime);
  void didFail(const ResourceError&);
  void didFailAccessControlCheck(const ResourceError&);
  void didFailRedirectCheck();
  void didDownloadData(int dataLength);
  void didReceiveResourceTiming(
      std::unique_ptr<CrossThreadResourceTimingInfoData>);

  Member<WorkerGlobalScope> m_workerGlobalScope;
  RefPtr<WorkerLoaderProxy> m_workerLoaderProxy;
  ThreadableLoaderClient* m_client;

  ThreadableLoaderOptions m_threadableLoaderOptions;
  ResourceLoaderOptions m_resourceLoaderOptions;
  BlockingBehavior m_blockingBehavior;

  // |*m_mainThreadLoaderHolder| lives in the main thread.
  CrossThreadPersistent<MainThreadLoaderHolder> m_mainThreadLoaderHolder;
};

}  // namespace blink

#endif  // WorkerThreadableLoader_h
