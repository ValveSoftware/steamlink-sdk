// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/ThreadedMessagingProxyBase.h"

#include "bindings/core/v8/SourceLocation.h"
#include "core/dom/Document.h"
#include "core/loader/DocumentLoader.h"
#include "core/workers/ParentFrameTaskRunners.h"
#include "core/workers/WorkerInspectorProxy.h"
#include "core/workers/WorkerThreadStartupData.h"
#include "wtf/CurrentTime.h"

namespace blink {

namespace {

static int s_liveMessagingProxyCount = 0;

}  // namespace

ThreadedMessagingProxyBase::ThreadedMessagingProxyBase(
    ExecutionContext* executionContext)
    : m_executionContext(executionContext),
      m_workerInspectorProxy(WorkerInspectorProxy::create()),
      m_parentFrameTaskRunners(ParentFrameTaskRunners::create(
          toDocument(m_executionContext.get())->frame())),
      m_mayBeDestroyed(false),
      m_askedToTerminate(false) {
  DCHECK(isParentContextThread());
  s_liveMessagingProxyCount++;
}

ThreadedMessagingProxyBase::~ThreadedMessagingProxyBase() {
  DCHECK(isParentContextThread());
  if (m_loaderProxy)
    m_loaderProxy->detachProvider(this);
  s_liveMessagingProxyCount--;
}

int ThreadedMessagingProxyBase::proxyCount() {
  DCHECK(isMainThread());
  return s_liveMessagingProxyCount;
}

void ThreadedMessagingProxyBase::initializeWorkerThread(
    std::unique_ptr<WorkerThreadStartupData> startupData) {
  DCHECK(isParentContextThread());

  Document* document = toDocument(getExecutionContext());
  double originTime =
      document->loader() ? document->loader()->timing().referenceMonotonicTime()
                         : monotonicallyIncreasingTime();

  m_loaderProxy = WorkerLoaderProxy::create(this);
  m_workerThread = createWorkerThread(originTime);
  m_workerThread->start(std::move(startupData));
  workerThreadCreated();
}

void ThreadedMessagingProxyBase::postTaskToWorkerGlobalScope(
    const WebTraceLocation& location,
    std::unique_ptr<ExecutionContextTask> task) {
  if (m_askedToTerminate)
    return;

  DCHECK(m_workerThread);
  m_workerThread->postTask(location, std::move(task));
}

void ThreadedMessagingProxyBase::postTaskToLoader(
    const WebTraceLocation& location,
    std::unique_ptr<ExecutionContextTask> task) {
  DCHECK(getExecutionContext()->isDocument());
  // TODO(hiroshige,yuryu): Make this not use ExecutionContextTask and use
  // m_parentFrameTaskRunners->get(TaskType::Networking) instead.
  getExecutionContext()->postTask(location, std::move(task));
}

void ThreadedMessagingProxyBase::reportConsoleMessage(
    MessageSource source,
    MessageLevel level,
    const String& message,
    std::unique_ptr<SourceLocation> location) {
  DCHECK(isParentContextThread());
  if (m_askedToTerminate)
    return;
  if (m_workerInspectorProxy)
    m_workerInspectorProxy->addConsoleMessageFromWorker(level, message,
                                                        std::move(location));
}

void ThreadedMessagingProxyBase::workerThreadCreated() {
  DCHECK(isParentContextThread());
  DCHECK(!m_askedToTerminate);
  DCHECK(m_workerThread);
}

void ThreadedMessagingProxyBase::parentObjectDestroyed() {
  DCHECK(isParentContextThread());

  m_parentFrameTaskRunners->get(TaskType::Internal)
      ->postTask(
          BLINK_FROM_HERE,
          WTF::bind(&ThreadedMessagingProxyBase::parentObjectDestroyedInternal,
                    unretained(this)));
}

void ThreadedMessagingProxyBase::parentObjectDestroyedInternal() {
  DCHECK(isParentContextThread());
  m_mayBeDestroyed = true;
  if (m_workerThread)
    terminateGlobalScope();
  else
    workerThreadTerminated();
}

void ThreadedMessagingProxyBase::workerThreadTerminated() {
  DCHECK(isParentContextThread());

  // This method is always the last to be performed, so the proxy is not
  // needed for communication in either side any more. However, the Worker
  // object may still exist, and it assumes that the proxy exists, too.
  m_askedToTerminate = true;
  m_workerThread = nullptr;
  m_workerInspectorProxy->workerThreadTerminated();
  if (m_mayBeDestroyed)
    delete this;
}

void ThreadedMessagingProxyBase::terminateGlobalScope() {
  DCHECK(isParentContextThread());

  if (m_askedToTerminate)
    return;
  m_askedToTerminate = true;

  if (m_workerThread)
    m_workerThread->terminate();

  m_workerInspectorProxy->workerThreadTerminated();
}

void ThreadedMessagingProxyBase::postMessageToPageInspector(
    const String& message) {
  DCHECK(isParentContextThread());
  if (m_workerInspectorProxy)
    m_workerInspectorProxy->dispatchMessageFromWorker(message);
}

bool ThreadedMessagingProxyBase::isParentContextThread() const {
  // TODO(nhiroki): Nested worker is not supported yet, so the parent context
  // thread should be equal to the main thread (http://crbug.com/31666).
  DCHECK(getExecutionContext()->isDocument());
  return isMainThread();
}

}  // namespace blink
