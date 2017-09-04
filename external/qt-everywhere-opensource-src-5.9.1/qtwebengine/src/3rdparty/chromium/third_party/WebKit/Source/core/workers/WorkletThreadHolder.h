// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkletThreadHolder_h
#define WorkletThreadHolder_h

#include "core/CoreExport.h"
#include "core/workers/WorkerBackingThread.h"
#include "platform/WaitableEvent.h"
#include "platform/WebThreadSupportingGC.h"
#include "wtf/PtrUtil.h"

namespace blink {

// WorkletThreadHolder is a template class which is designed for singleton
// instance of DerivedWorkletThread (i.e. AnimationWorkletThread,
// AudioWorkletThread).
template <class DerivedWorkletThread>
class WorkletThreadHolder {
 public:
  static WorkletThreadHolder<DerivedWorkletThread>* threadHolderInstance() {
    MutexLocker locker(holderInstanceMutex());
    return s_threadHolderInstance;
  }

  static void ensureInstance(const char* threadName,
                             BlinkGC::ThreadHeapMode threadHeapMode) {
    DCHECK(isMainThread());
    MutexLocker locker(holderInstanceMutex());
    if (s_threadHolderInstance)
      return;
    s_threadHolderInstance = new WorkletThreadHolder<DerivedWorkletThread>;
    s_threadHolderInstance->initialize(
        WorkerBackingThread::create(threadName, threadHeapMode));
  }

  static void ensureInstance(WebThread* thread) {
    DCHECK(isMainThread());
    MutexLocker locker(holderInstanceMutex());
    if (s_threadHolderInstance)
      return;
    s_threadHolderInstance = new WorkletThreadHolder<DerivedWorkletThread>;
    s_threadHolderInstance->initialize(WorkerBackingThread::create(thread));
  }

  static void createForTest(const char* threadName,
                            BlinkGC::ThreadHeapMode threadHeapMode) {
    MutexLocker locker(holderInstanceMutex());
    DCHECK(!s_threadHolderInstance);
    s_threadHolderInstance = new WorkletThreadHolder<DerivedWorkletThread>;
    s_threadHolderInstance->initialize(
        WorkerBackingThread::createForTest(threadName, threadHeapMode));
  }

  static void createForTest(WebThread* thread) {
    MutexLocker locker(holderInstanceMutex());
    DCHECK(!s_threadHolderInstance);
    s_threadHolderInstance = new WorkletThreadHolder<DerivedWorkletThread>;
    s_threadHolderInstance->initialize(
        WorkerBackingThread::createForTest(thread));
  }

  static void clearInstance() {
    DCHECK(isMainThread());
    MutexLocker locker(holderInstanceMutex());
    if (s_threadHolderInstance) {
      s_threadHolderInstance->shutdownAndWait();
      delete s_threadHolderInstance;
      s_threadHolderInstance = nullptr;
    }
  }

  WorkerBackingThread* thread() { return m_thread.get(); }

 private:
  WorkletThreadHolder() {}
  ~WorkletThreadHolder() {}

  static Mutex& holderInstanceMutex() {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(Mutex, holderMutex, new Mutex);
    return holderMutex;
  }

  void initialize(std::unique_ptr<WorkerBackingThread> backingThread) {
    m_thread = std::move(backingThread);
    m_thread->backingThread().postTask(
        BLINK_FROM_HERE,
        crossThreadBind(&WorkletThreadHolder::initializeOnWorkletThread,
                        crossThreadUnretained(this)));
  }

  void initializeOnWorkletThread() {
    MutexLocker locker(holderInstanceMutex());
    DCHECK(!m_initialized);
    m_thread->initialize();
    m_initialized = true;
  }

  void shutdownAndWait() {
    DCHECK(isMainThread());
    WaitableEvent waitableEvent;
    m_thread->backingThread().postTask(
        BLINK_FROM_HERE,
        crossThreadBind(&WorkletThreadHolder::shutdownOnWorlketThread,
                        crossThreadUnretained(this),
                        crossThreadUnretained(&waitableEvent)));
    waitableEvent.wait();
  }

  void shutdownOnWorlketThread(WaitableEvent* waitableEvent) {
    m_thread->shutdown();
    waitableEvent->signal();
  }

  std::unique_ptr<WorkerBackingThread> m_thread;
  bool m_initialized = false;

  static WorkletThreadHolder<DerivedWorkletThread>* s_threadHolderInstance;
};

template <class DerivedWorkletThread>
WorkletThreadHolder<DerivedWorkletThread>*
    WorkletThreadHolder<DerivedWorkletThread>::s_threadHolderInstance = nullptr;

}  // namespace blink

#endif  // WorkletBackingThreadHolder_h
