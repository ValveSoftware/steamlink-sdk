// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/CompositorWorkerThread.h"

#include "core/workers/InProcessWorkerObjectProxy.h"
#include "core/workers/WorkerThreadStartupData.h"
#include "modules/compositorworker/CompositorWorkerGlobalScope.h"
#include "platform/tracing/TraceEvent.h"
#include "wtf/Assertions.h"
#include <memory>

namespace blink {

std::unique_ptr<CompositorWorkerThread> CompositorWorkerThread::create(
    PassRefPtr<WorkerLoaderProxy> workerLoaderProxy,
    InProcessWorkerObjectProxy& workerObjectProxy,
    double timeOrigin) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("compositor-worker"),
               "CompositorWorkerThread::create");
  ASSERT(isMainThread());
  return wrapUnique(new CompositorWorkerThread(std::move(workerLoaderProxy),
                                               workerObjectProxy, timeOrigin));
}

CompositorWorkerThread::CompositorWorkerThread(
    PassRefPtr<WorkerLoaderProxy> workerLoaderProxy,
    InProcessWorkerObjectProxy& workerObjectProxy,
    double timeOrigin)
    : AbstractAnimationWorkletThread(std::move(workerLoaderProxy),
                                     workerObjectProxy),
      m_workerObjectProxy(workerObjectProxy),
      m_timeOrigin(timeOrigin) {}

CompositorWorkerThread::~CompositorWorkerThread() {}

WorkerOrWorkletGlobalScope* CompositorWorkerThread::createWorkerGlobalScope(
    std::unique_ptr<WorkerThreadStartupData> startupData) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("compositor-worker"),
               "CompositorWorkerThread::createWorkerGlobalScope");
  return CompositorWorkerGlobalScope::create(this, std::move(startupData),
                                             m_timeOrigin);
}

}  // namespace blink
