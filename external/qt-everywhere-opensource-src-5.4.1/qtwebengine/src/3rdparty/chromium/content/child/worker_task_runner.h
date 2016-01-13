// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WORKER_TASK_RUNNER_H_
#define CONTENT_CHILD_WORKER_TASK_RUNNER_H_

#include <map>

#include "base/atomic_sequence_num.h"
#include "base/callback_forward.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_local.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebWorkerRunLoop.h"

namespace content {

class CONTENT_EXPORT WorkerTaskRunner {
 public:
  WorkerTaskRunner();

  bool PostTask(int id, const base::Closure& task);
  int PostTaskToAllThreads(const base::Closure& task);
  int CurrentWorkerId();
  static WorkerTaskRunner* Instance();

  class CONTENT_EXPORT Observer {
   public:
    virtual ~Observer() {}
    virtual void OnWorkerRunLoopStopped() = 0;
  };
  // Add/Remove an observer that will get notified when the current worker run
  // loop is stopping. This observer will not get notified when other threads
  // are stopping.  It's only valid to call these on a worker thread.
  void AddStopObserver(Observer* observer);
  void RemoveStopObserver(Observer* observer);

  void OnWorkerRunLoopStarted(const blink::WebWorkerRunLoop& loop);
  void OnWorkerRunLoopStopped(const blink::WebWorkerRunLoop& loop);

 private:
  friend class WorkerTaskRunnerTest;

  typedef std::map<int, blink::WebWorkerRunLoop> IDToLoopMap;

  ~WorkerTaskRunner();

  struct ThreadLocalState;
  base::ThreadLocalPointer<ThreadLocalState> current_tls_;

  base::AtomicSequenceNumber id_sequence_;
  IDToLoopMap loop_map_;
  base::Lock loop_map_lock_;
};

}  // namespace content

#endif // CONTENT_CHILD_WORKER_TASK_RUNNER_H_
