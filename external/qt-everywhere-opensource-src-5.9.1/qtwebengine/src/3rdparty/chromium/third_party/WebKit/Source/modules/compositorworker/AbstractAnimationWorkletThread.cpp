// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/AbstractAnimationWorkletThread.h"

#include "core/workers/WorkerBackingThread.h"
#include "core/workers/WorkletThreadHolder.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WebThreadSupportingGC.h"
#include "public/platform/Platform.h"
#include "wtf/Assertions.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

template class WorkletThreadHolder<AbstractAnimationWorkletThread>;

AbstractAnimationWorkletThread::AbstractAnimationWorkletThread(
    PassRefPtr<WorkerLoaderProxy> workerLoaderProxy,
    WorkerReportingProxy& workerReportingProxy)
    : WorkerThread(std::move(workerLoaderProxy), workerReportingProxy) {}

AbstractAnimationWorkletThread::~AbstractAnimationWorkletThread() {}

WorkerBackingThread& AbstractAnimationWorkletThread::workerBackingThread() {
  return *WorkletThreadHolder<
              AbstractAnimationWorkletThread>::threadHolderInstance()
              ->thread();
}

void collectAllGarbageOnThread(WaitableEvent* doneEvent) {
  blink::ThreadState::current()->collectAllGarbage();
  doneEvent->signal();
}

void AbstractAnimationWorkletThread::collectAllGarbage() {
  DCHECK(isMainThread());
  WaitableEvent doneEvent;
  WorkletThreadHolder<AbstractAnimationWorkletThread>* threadHolderInstance =
      WorkletThreadHolder<
          AbstractAnimationWorkletThread>::threadHolderInstance();
  if (!threadHolderInstance)
    return;
  threadHolderInstance->thread()->backingThread().postTask(
      BLINK_FROM_HERE, crossThreadBind(&collectAllGarbageOnThread,
                                       crossThreadUnretained(&doneEvent)));
  doneEvent.wait();
}

void AbstractAnimationWorkletThread::ensureSharedBackingThread() {
  DCHECK(isMainThread());
  WorkletThreadHolder<AbstractAnimationWorkletThread>::ensureInstance(
      Platform::current()->compositorThread());
}

void AbstractAnimationWorkletThread::clearSharedBackingThread() {
  DCHECK(isMainThread());
  WorkletThreadHolder<AbstractAnimationWorkletThread>::clearInstance();
}

void AbstractAnimationWorkletThread::createSharedBackingThreadForTest() {
  WorkletThreadHolder<AbstractAnimationWorkletThread>::createForTest(
      Platform::current()->compositorThread());
}

}  // namespace blink
