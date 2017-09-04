// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/AudioWorkletThread.h"

#include "core/workers/WorkerBackingThread.h"
#include "core/workers/WorkerThreadStartupData.h"
#include "modules/webaudio/AudioWorkletGlobalScope.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WaitableEvent.h"
#include "platform/WebThreadSupportingGC.h"
#include "platform/tracing/TraceEvent.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "wtf/Assertions.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

template class WorkletThreadHolder<AudioWorkletThread>;

std::unique_ptr<AudioWorkletThread> AudioWorkletThread::create(
    PassRefPtr<WorkerLoaderProxy> workerLoaderProxy,
    WorkerReportingProxy& workerReportingProxy) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("audio-worklet"),
               "AudioWorkletThread::create");
  DCHECK(isMainThread());
  return wrapUnique(new AudioWorkletThread(std::move(workerLoaderProxy),
                                           workerReportingProxy));
}

AudioWorkletThread::AudioWorkletThread(
    PassRefPtr<WorkerLoaderProxy> workerLoaderProxy,
    WorkerReportingProxy& workerReportingProxy)
    : WorkerThread(std::move(workerLoaderProxy), workerReportingProxy) {}

AudioWorkletThread::~AudioWorkletThread() {}

WorkerBackingThread& AudioWorkletThread::workerBackingThread() {
  return *WorkletThreadHolder<AudioWorkletThread>::threadHolderInstance()
              ->thread();
}

void collectAllGarbageOnAudioWorkletThread(WaitableEvent* doneEvent) {
  blink::ThreadState::current()->collectAllGarbage();
  doneEvent->signal();
}

void AudioWorkletThread::collectAllGarbage() {
  DCHECK(isMainThread());
  WaitableEvent doneEvent;
  WorkletThreadHolder<AudioWorkletThread>* threadHolderInstance =
      WorkletThreadHolder<AudioWorkletThread>::threadHolderInstance();
  if (!threadHolderInstance)
    return;
  threadHolderInstance->thread()->backingThread().postTask(
      BLINK_FROM_HERE, crossThreadBind(&collectAllGarbageOnAudioWorkletThread,
                                       crossThreadUnretained(&doneEvent)));
  doneEvent.wait();
}

void AudioWorkletThread::ensureSharedBackingThread() {
  DCHECK(isMainThread());
  WorkletThreadHolder<AudioWorkletThread>::ensureInstance(
      "AudioWorkletThread", BlinkGC::PerThreadHeapMode);
}

void AudioWorkletThread::clearSharedBackingThread() {
  DCHECK(isMainThread());
  WorkletThreadHolder<AudioWorkletThread>::clearInstance();
}

void AudioWorkletThread::createSharedBackingThreadForTest() {
  WorkletThreadHolder<AudioWorkletThread>::createForTest(
      "AudioWorkletThread", BlinkGC::PerThreadHeapMode);
}

WorkerOrWorkletGlobalScope* AudioWorkletThread::createWorkerGlobalScope(
    std::unique_ptr<WorkerThreadStartupData> startupData) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("audio-worklet"),
               "AudioWorkletThread::createWorkerGlobalScope");

  RefPtr<SecurityOrigin> securityOrigin =
      SecurityOrigin::create(startupData->m_scriptURL);
  if (startupData->m_starterOriginPrivilegeData) {
    securityOrigin->transferPrivilegesFrom(
        std::move(startupData->m_starterOriginPrivilegeData));
  }

  return AudioWorkletGlobalScope::create(
      startupData->m_scriptURL, startupData->m_userAgent,
      securityOrigin.release(), this->isolate(), this);
}

}  // namespace blink
