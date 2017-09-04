/*
 * Copyright (C) 2009, 2010 Google Inc. All rights reserved.
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

#include "core/loader/WorkerThreadableLoader.h"

#include "core/dom/Document.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/loader/DocumentThreadableLoader.h"
#include "core/timing/WorkerGlobalScopePerformance.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerLoaderProxy.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/heap/SafePoint.h"
#include "platform/network/ResourceError.h"
#include "platform/network/ResourceRequest.h"
#include "platform/network/ResourceResponse.h"
#include "platform/network/ResourceTimingInfo.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "wtf/debug/Alias.h"
#include <memory>

namespace blink {

namespace {

std::unique_ptr<Vector<char>> createVectorFromMemoryRegion(
    const char* data,
    unsigned dataLength) {
  std::unique_ptr<Vector<char>> buffer = makeUnique<Vector<char>>(dataLength);
  memcpy(buffer->data(), data, dataLength);
  return buffer;
}

}  // namespace

class WorkerThreadableLoader::AsyncTaskForwarder final
    : public WorkerThreadableLoader::TaskForwarder {
 public:
  explicit AsyncTaskForwarder(PassRefPtr<WorkerLoaderProxy> loaderProxy)
      : m_loaderProxy(loaderProxy) {
    DCHECK(isMainThread());
  }
  ~AsyncTaskForwarder() override { DCHECK(isMainThread()); }

  void forwardTask(const WebTraceLocation& location,
                   std::unique_ptr<ExecutionContextTask> task) override {
    DCHECK(isMainThread());
    m_loaderProxy->postTaskToWorkerGlobalScope(location, std::move(task));
  }
  void forwardTaskWithDoneSignal(
      const WebTraceLocation& location,
      std::unique_ptr<ExecutionContextTask> task) override {
    DCHECK(isMainThread());
    m_loaderProxy->postTaskToWorkerGlobalScope(location, std::move(task));
  }
  void abort() override { DCHECK(isMainThread()); }

 private:
  RefPtr<WorkerLoaderProxy> m_loaderProxy;
};

struct WorkerThreadableLoader::TaskWithLocation final {
  TaskWithLocation(const WebTraceLocation& location,
                   std::unique_ptr<ExecutionContextTask> task)
      : m_location(location), m_task(std::move(task)) {}
  TaskWithLocation(TaskWithLocation&& task)
      : TaskWithLocation(task.m_location, std::move(task.m_task)) {}
  ~TaskWithLocation() = default;

  WebTraceLocation m_location;
  std::unique_ptr<ExecutionContextTask> m_task;
};

// Observing functions and wait() need to be called on the worker thread.
// Setting functions and signal() need to be called on the main thread.
// All observing functions must be called after wait() returns, and all
// setting functions must be called before signal() is called.
class WorkerThreadableLoader::WaitableEventWithTasks final
    : public ThreadSafeRefCounted<WaitableEventWithTasks> {
 public:
  static PassRefPtr<WaitableEventWithTasks> create() {
    return adoptRef(new WaitableEventWithTasks);
  }

  void signal() {
    DCHECK(isMainThread());
    CHECK(!m_isSignalCalled);
    m_isSignalCalled = true;
    m_event.signal();
  }
  void wait() {
    DCHECK(!isMainThread());
    CHECK(!m_isWaitDone);
    m_event.wait();
    m_isWaitDone = true;
  }

  // Observing functions
  bool isAborted() const {
    DCHECK(!isMainThread());
    CHECK(m_isWaitDone);
    return m_isAborted;
  }
  Vector<TaskWithLocation> take() {
    DCHECK(!isMainThread());
    CHECK(m_isWaitDone);
    return std::move(m_tasks);
  }

  // Setting functions
  void append(TaskWithLocation task) {
    DCHECK(isMainThread());
    CHECK(!m_isSignalCalled);
    m_tasks.append(std::move(task));
  }
  void setIsAborted() {
    DCHECK(isMainThread());
    CHECK(!m_isSignalCalled);
    m_isAborted = true;
  }

 private:
  WaitableEventWithTasks() {}

  WaitableEvent m_event;
  Vector<TaskWithLocation> m_tasks;
  bool m_isAborted = false;
  bool m_isSignalCalled = false;
  bool m_isWaitDone = false;
};

class WorkerThreadableLoader::SyncTaskForwarder final
    : public WorkerThreadableLoader::TaskForwarder {
 public:
  explicit SyncTaskForwarder(PassRefPtr<WaitableEventWithTasks> eventWithTasks)
      : m_eventWithTasks(eventWithTasks) {
    DCHECK(isMainThread());
  }
  ~SyncTaskForwarder() override { DCHECK(isMainThread()); }

  void forwardTask(const WebTraceLocation& location,
                   std::unique_ptr<ExecutionContextTask> task) override {
    DCHECK(isMainThread());
    m_eventWithTasks->append(TaskWithLocation(location, std::move(task)));
  }
  void forwardTaskWithDoneSignal(
      const WebTraceLocation& location,
      std::unique_ptr<ExecutionContextTask> task) override {
    DCHECK(isMainThread());
    m_eventWithTasks->append(TaskWithLocation(location, std::move(task)));
    m_eventWithTasks->signal();
  }
  void abort() override {
    DCHECK(isMainThread());
    m_eventWithTasks->setIsAborted();
    m_eventWithTasks->signal();
  }

 private:
  RefPtr<WaitableEventWithTasks> m_eventWithTasks;
};

WorkerThreadableLoader::WorkerThreadableLoader(
    WorkerGlobalScope& workerGlobalScope,
    ThreadableLoaderClient* client,
    const ThreadableLoaderOptions& options,
    const ResourceLoaderOptions& resourceLoaderOptions,
    BlockingBehavior blockingBehavior)
    : m_workerGlobalScope(&workerGlobalScope),
      m_workerLoaderProxy(workerGlobalScope.thread()->workerLoaderProxy()),
      m_client(client),
      m_threadableLoaderOptions(options),
      m_resourceLoaderOptions(resourceLoaderOptions),
      m_blockingBehavior(blockingBehavior) {
  DCHECK(client);
}

void WorkerThreadableLoader::loadResourceSynchronously(
    WorkerGlobalScope& workerGlobalScope,
    const ResourceRequest& request,
    ThreadableLoaderClient& client,
    const ThreadableLoaderOptions& options,
    const ResourceLoaderOptions& resourceLoaderOptions) {
  (new WorkerThreadableLoader(workerGlobalScope, &client, options,
                              resourceLoaderOptions, LoadSynchronously))
      ->start(request);
}

WorkerThreadableLoader::~WorkerThreadableLoader() {
  DCHECK(!m_mainThreadLoaderHolder);
  DCHECK(!m_client);
}

void WorkerThreadableLoader::start(const ResourceRequest& originalRequest) {
  ResourceRequest request(originalRequest);
  if (!request.didSetHTTPReferrer()) {
    request.setHTTPReferrer(SecurityPolicy::generateReferrer(
        m_workerGlobalScope->getReferrerPolicy(), request.url(),
        m_workerGlobalScope->outgoingReferrer()));
  }

  DCHECK(!isMainThread());
  RefPtr<WaitableEventWithTasks> eventWithTasks;
  if (m_blockingBehavior == LoadSynchronously)
    eventWithTasks = WaitableEventWithTasks::create();

  m_workerLoaderProxy->postTaskToLoader(
      BLINK_FROM_HERE,
      createCrossThreadTask(
          &MainThreadLoaderHolder::createAndStart,
          wrapCrossThreadPersistent(this), m_workerLoaderProxy,
          wrapCrossThreadPersistent(
              m_workerGlobalScope->thread()->getWorkerThreadLifecycleContext()),
          request, m_threadableLoaderOptions, m_resourceLoaderOptions,
          eventWithTasks));

  if (m_blockingBehavior == LoadAsynchronously)
    return;

  eventWithTasks->wait();

  if (eventWithTasks->isAborted()) {
    // This thread is going to terminate.
    cancel();
    return;
  }

  for (const auto& task : eventWithTasks->take()) {
    // Store the program counter where the task is posted from, and alias
    // it to ensure it is stored in the crash dump.
    const void* programCounter = task.m_location.program_counter();
    WTF::debug::alias(&programCounter);

    // m_clientTask contains only CallClosureTasks. So, it's ok to pass
    // the nullptr.
    task.m_task->performTask(nullptr);
  }
}

void WorkerThreadableLoader::overrideTimeout(
    unsigned long timeoutMilliseconds) {
  DCHECK(!isMainThread());
  if (!m_mainThreadLoaderHolder)
    return;
  m_workerLoaderProxy->postTaskToLoader(
      BLINK_FROM_HERE,
      createCrossThreadTask(&MainThreadLoaderHolder::overrideTimeout,
                            m_mainThreadLoaderHolder, timeoutMilliseconds));
}

void WorkerThreadableLoader::cancel() {
  DCHECK(!isMainThread());
  if (m_mainThreadLoaderHolder) {
    m_workerLoaderProxy->postTaskToLoader(
        BLINK_FROM_HERE, createCrossThreadTask(&MainThreadLoaderHolder::cancel,
                                               m_mainThreadLoaderHolder));
    m_mainThreadLoaderHolder = nullptr;
  }

  if (!m_client)
    return;

  // If the client hasn't reached a termination state, then transition it
  // by sending a cancellation error.
  // Note: no more client callbacks will be done after this method -- the
  // clearClient() call ensures that.
  ResourceError error(String(), 0, String(), String());
  error.setIsCancellation(true);
  didFail(error);
  DCHECK(!m_client);
}

void WorkerThreadableLoader::didStart(
    MainThreadLoaderHolder* mainThreadLoaderHolder) {
  DCHECK(!isMainThread());
  DCHECK(!m_mainThreadLoaderHolder);
  DCHECK(mainThreadLoaderHolder);
  if (!m_client) {
    // The thread is terminating.
    m_workerLoaderProxy->postTaskToLoader(
        BLINK_FROM_HERE, createCrossThreadTask(&MainThreadLoaderHolder::cancel,
                                               wrapCrossThreadPersistent(
                                                   mainThreadLoaderHolder)));
    return;
  }

  m_mainThreadLoaderHolder = mainThreadLoaderHolder;
}

void WorkerThreadableLoader::didSendData(
    unsigned long long bytesSent,
    unsigned long long totalBytesToBeSent) {
  DCHECK(!isMainThread());
  if (!m_client)
    return;
  m_client->didSendData(bytesSent, totalBytesToBeSent);
}

void WorkerThreadableLoader::didReceiveResponse(
    unsigned long identifier,
    std::unique_ptr<CrossThreadResourceResponseData> responseData,
    std::unique_ptr<WebDataConsumerHandle> handle) {
  DCHECK(!isMainThread());
  if (!m_client)
    return;
  ResourceResponse response(responseData.get());
  m_client->didReceiveResponse(identifier, response, std::move(handle));
}

void WorkerThreadableLoader::didReceiveData(
    std::unique_ptr<Vector<char>> data) {
  DCHECK(!isMainThread());
  CHECK_LE(data->size(), std::numeric_limits<unsigned>::max());
  if (!m_client)
    return;
  m_client->didReceiveData(data->data(), data->size());
}

void WorkerThreadableLoader::didReceiveCachedMetadata(
    std::unique_ptr<Vector<char>> data) {
  DCHECK(!isMainThread());
  if (!m_client)
    return;
  m_client->didReceiveCachedMetadata(data->data(), data->size());
}

void WorkerThreadableLoader::didFinishLoading(unsigned long identifier,
                                              double finishTime) {
  DCHECK(!isMainThread());
  if (!m_client)
    return;
  auto* client = m_client;
  m_client = nullptr;
  m_mainThreadLoaderHolder = nullptr;
  client->didFinishLoading(identifier, finishTime);
}

void WorkerThreadableLoader::didFail(const ResourceError& error) {
  DCHECK(!isMainThread());
  if (!m_client)
    return;
  auto* client = m_client;
  m_client = nullptr;
  m_mainThreadLoaderHolder = nullptr;
  client->didFail(error);
}

void WorkerThreadableLoader::didFailAccessControlCheck(
    const ResourceError& error) {
  DCHECK(!isMainThread());
  if (!m_client)
    return;
  auto* client = m_client;
  m_client = nullptr;
  m_mainThreadLoaderHolder = nullptr;
  client->didFailAccessControlCheck(error);
}

void WorkerThreadableLoader::didFailRedirectCheck() {
  DCHECK(!isMainThread());
  if (!m_client)
    return;
  auto* client = m_client;
  m_client = nullptr;
  m_mainThreadLoaderHolder = nullptr;
  client->didFailRedirectCheck();
}

void WorkerThreadableLoader::didDownloadData(int dataLength) {
  DCHECK(!isMainThread());
  if (!m_client)
    return;
  m_client->didDownloadData(dataLength);
}

void WorkerThreadableLoader::didReceiveResourceTiming(
    std::unique_ptr<CrossThreadResourceTimingInfoData> timingData) {
  DCHECK(!isMainThread());
  if (!m_client)
    return;
  std::unique_ptr<ResourceTimingInfo> info(
      ResourceTimingInfo::adopt(std::move(timingData)));
  WorkerGlobalScopePerformance::performance(*m_workerGlobalScope)
      ->addResourceTiming(*info);
  m_client->didReceiveResourceTiming(*info);
}

DEFINE_TRACE(WorkerThreadableLoader) {
  visitor->trace(m_workerGlobalScope);
  ThreadableLoader::trace(visitor);
}

void WorkerThreadableLoader::MainThreadLoaderHolder::createAndStart(
    WorkerThreadableLoader* workerLoader,
    PassRefPtr<WorkerLoaderProxy> passLoaderProxy,
    WorkerThreadLifecycleContext* workerThreadLifecycleContext,
    std::unique_ptr<CrossThreadResourceRequestData> request,
    const ThreadableLoaderOptions& options,
    const ResourceLoaderOptions& resourceLoaderOptions,
    PassRefPtr<WaitableEventWithTasks> eventWithTasks,
    ExecutionContext* executionContext) {
  DCHECK(isMainThread());
  TaskForwarder* forwarder;
  RefPtr<WorkerLoaderProxy> loaderProxy = passLoaderProxy;
  if (eventWithTasks)
    forwarder = new SyncTaskForwarder(std::move(eventWithTasks));
  else
    forwarder = new AsyncTaskForwarder(loaderProxy);

  MainThreadLoaderHolder* mainThreadLoaderHolder =
      new MainThreadLoaderHolder(forwarder, workerThreadLifecycleContext);
  if (mainThreadLoaderHolder->wasContextDestroyedBeforeObserverCreation()) {
    // The thread is already terminating.
    forwarder->abort();
    mainThreadLoaderHolder->m_forwarder = nullptr;
    return;
  }
  mainThreadLoaderHolder->m_workerLoader = workerLoader;
  forwarder->forwardTask(
      BLINK_FROM_HERE,
      createCrossThreadTask(&WorkerThreadableLoader::didStart,
                            wrapCrossThreadPersistent(workerLoader),
                            wrapCrossThreadPersistent(mainThreadLoaderHolder)));
  mainThreadLoaderHolder->start(*toDocument(executionContext),
                                std::move(request), options,
                                resourceLoaderOptions);
}

WorkerThreadableLoader::MainThreadLoaderHolder::~MainThreadLoaderHolder() {
  DCHECK(isMainThread());
  DCHECK(!m_workerLoader);
}

void WorkerThreadableLoader::MainThreadLoaderHolder::overrideTimeout(
    unsigned long timeoutMilliseconds) {
  DCHECK(isMainThread());
  if (!m_mainThreadLoader)
    return;
  m_mainThreadLoader->overrideTimeout(timeoutMilliseconds);
}

void WorkerThreadableLoader::MainThreadLoaderHolder::cancel() {
  DCHECK(isMainThread());
  m_workerLoader = nullptr;
  if (!m_mainThreadLoader)
    return;
  m_mainThreadLoader->cancel();
  m_mainThreadLoader = nullptr;
}

void WorkerThreadableLoader::MainThreadLoaderHolder::didSendData(
    unsigned long long bytesSent,
    unsigned long long totalBytesToBeSent) {
  DCHECK(isMainThread());
  CrossThreadPersistent<WorkerThreadableLoader> workerLoader =
      m_workerLoader.get();
  if (!workerLoader || !m_forwarder)
    return;
  m_forwarder->forwardTask(
      BLINK_FROM_HERE,
      createCrossThreadTask(&WorkerThreadableLoader::didSendData, workerLoader,
                            bytesSent, totalBytesToBeSent));
}

void WorkerThreadableLoader::MainThreadLoaderHolder::didReceiveResponse(
    unsigned long identifier,
    const ResourceResponse& response,
    std::unique_ptr<WebDataConsumerHandle> handle) {
  DCHECK(isMainThread());
  CrossThreadPersistent<WorkerThreadableLoader> workerLoader =
      m_workerLoader.get();
  if (!workerLoader || !m_forwarder)
    return;
  m_forwarder->forwardTask(
      BLINK_FROM_HERE,
      createCrossThreadTask(&WorkerThreadableLoader::didReceiveResponse,
                            workerLoader, identifier, response,
                            passed(std::move(handle))));
}

void WorkerThreadableLoader::MainThreadLoaderHolder::didReceiveData(
    const char* data,
    unsigned dataLength) {
  DCHECK(isMainThread());
  CrossThreadPersistent<WorkerThreadableLoader> workerLoader =
      m_workerLoader.get();
  if (!workerLoader || !m_forwarder)
    return;
  m_forwarder->forwardTask(
      BLINK_FROM_HERE,
      createCrossThreadTask(
          &WorkerThreadableLoader::didReceiveData, workerLoader,
          passed(createVectorFromMemoryRegion(data, dataLength))));
}

void WorkerThreadableLoader::MainThreadLoaderHolder::didDownloadData(
    int dataLength) {
  DCHECK(isMainThread());
  CrossThreadPersistent<WorkerThreadableLoader> workerLoader =
      m_workerLoader.get();
  if (!workerLoader || !m_forwarder)
    return;
  m_forwarder->forwardTask(
      BLINK_FROM_HERE,
      createCrossThreadTask(&WorkerThreadableLoader::didDownloadData,
                            workerLoader, dataLength));
}

void WorkerThreadableLoader::MainThreadLoaderHolder::didReceiveCachedMetadata(
    const char* data,
    int dataLength) {
  DCHECK(isMainThread());
  CrossThreadPersistent<WorkerThreadableLoader> workerLoader =
      m_workerLoader.get();
  if (!workerLoader || !m_forwarder)
    return;
  m_forwarder->forwardTask(
      BLINK_FROM_HERE,
      createCrossThreadTask(
          &WorkerThreadableLoader::didReceiveCachedMetadata, workerLoader,
          passed(createVectorFromMemoryRegion(data, dataLength))));
}

void WorkerThreadableLoader::MainThreadLoaderHolder::didFinishLoading(
    unsigned long identifier,
    double finishTime) {
  DCHECK(isMainThread());
  CrossThreadPersistent<WorkerThreadableLoader> workerLoader =
      m_workerLoader.release();
  if (!workerLoader || !m_forwarder)
    return;
  m_forwarder->forwardTaskWithDoneSignal(
      BLINK_FROM_HERE,
      createCrossThreadTask(&WorkerThreadableLoader::didFinishLoading,
                            workerLoader, identifier, finishTime));
  m_forwarder = nullptr;
}

void WorkerThreadableLoader::MainThreadLoaderHolder::didFail(
    const ResourceError& error) {
  DCHECK(isMainThread());
  CrossThreadPersistent<WorkerThreadableLoader> workerLoader =
      m_workerLoader.release();
  if (!workerLoader || !m_forwarder)
    return;
  m_forwarder->forwardTaskWithDoneSignal(
      BLINK_FROM_HERE, createCrossThreadTask(&WorkerThreadableLoader::didFail,
                                             workerLoader, error));
  m_forwarder = nullptr;
}

void WorkerThreadableLoader::MainThreadLoaderHolder::didFailAccessControlCheck(
    const ResourceError& error) {
  DCHECK(isMainThread());
  CrossThreadPersistent<WorkerThreadableLoader> workerLoader =
      m_workerLoader.release();
  if (!workerLoader || !m_forwarder)
    return;
  m_forwarder->forwardTaskWithDoneSignal(
      BLINK_FROM_HERE,
      createCrossThreadTask(&WorkerThreadableLoader::didFailAccessControlCheck,
                            workerLoader, error));
  m_forwarder = nullptr;
}

void WorkerThreadableLoader::MainThreadLoaderHolder::didFailRedirectCheck() {
  DCHECK(isMainThread());
  CrossThreadPersistent<WorkerThreadableLoader> workerLoader =
      m_workerLoader.release();
  if (!workerLoader || !m_forwarder)
    return;
  m_forwarder->forwardTaskWithDoneSignal(
      BLINK_FROM_HERE,
      createCrossThreadTask(&WorkerThreadableLoader::didFailRedirectCheck,
                            workerLoader));
  m_forwarder = nullptr;
}

void WorkerThreadableLoader::MainThreadLoaderHolder::didReceiveResourceTiming(
    const ResourceTimingInfo& info) {
  DCHECK(isMainThread());
  CrossThreadPersistent<WorkerThreadableLoader> workerLoader =
      m_workerLoader.get();
  if (!workerLoader || !m_forwarder)
    return;
  m_forwarder->forwardTask(
      BLINK_FROM_HERE,
      createCrossThreadTask(&WorkerThreadableLoader::didReceiveResourceTiming,
                            workerLoader, info));
}

void WorkerThreadableLoader::MainThreadLoaderHolder::contextDestroyed() {
  DCHECK(isMainThread());
  if (m_forwarder) {
    m_forwarder->abort();
    m_forwarder = nullptr;
  }
  cancel();
}

DEFINE_TRACE(WorkerThreadableLoader::MainThreadLoaderHolder) {
  visitor->trace(m_forwarder);
  visitor->trace(m_mainThreadLoader);
  WorkerThreadLifecycleObserver::trace(visitor);
}

WorkerThreadableLoader::MainThreadLoaderHolder::MainThreadLoaderHolder(
    TaskForwarder* forwarder,
    WorkerThreadLifecycleContext* context)
    : WorkerThreadLifecycleObserver(context), m_forwarder(forwarder) {
  DCHECK(isMainThread());
}

void WorkerThreadableLoader::MainThreadLoaderHolder::start(
    Document& document,
    std::unique_ptr<CrossThreadResourceRequestData> request,
    const ThreadableLoaderOptions& options,
    const ResourceLoaderOptions& originalResourceLoaderOptions) {
  DCHECK(isMainThread());
  ResourceLoaderOptions resourceLoaderOptions = originalResourceLoaderOptions;
  resourceLoaderOptions.requestInitiatorContext = WorkerContext;
  m_mainThreadLoader = DocumentThreadableLoader::create(document, this, options,
                                                        resourceLoaderOptions);
  m_mainThreadLoader->start(ResourceRequest(request.get()));
}

}  // namespace blink
