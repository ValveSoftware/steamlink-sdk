// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebTaskRunner_h
#define WebTaskRunner_h

#include "base/callback_forward.h"
#include "public/platform/WebCommon.h"
#include "public/platform/WebTraceLocation.h"
#include "wtf/Compiler.h"
#include "wtf/Functional.h"
#include "wtf/RefCounted.h"
#include "wtf/WeakPtr.h"
#include <memory>

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {

using SingleThreadTaskRunner = base::SingleThreadTaskRunner;

// TaskHandle is associated to a task posted by
// WebTaskRunner::postCancellableTask or
// WebTaskRunner::postCancellableDelayedTask and cancels the associated task on
// TaskHandle::cancel() call or on TaskHandle destruction.
class BLINK_PLATFORM_EXPORT TaskHandle {
 public:
  // Returns true if the task will run later. Returns false if the task is
  // cancelled or the task is run already.
  // This function is not thread safe. Call this on the thread that has posted
  // the task.
  bool isActive() const;

  // Cancels the task invocation. Do nothing if the task is cancelled or run
  // already.
  // This function is not thread safe. Call this on the thread that has posted
  // the task.
  void cancel();

  TaskHandle();
  ~TaskHandle();

  TaskHandle(TaskHandle&&);
  TaskHandle& operator=(TaskHandle&&);

 private:
  class Runner;
  friend class WebTaskRunner;

  explicit TaskHandle(RefPtr<Runner>);
  RefPtr<Runner> m_runner;
};

// The blink representation of a chromium SingleThreadTaskRunner.
class BLINK_PLATFORM_EXPORT WebTaskRunner {
 public:
  virtual ~WebTaskRunner() {}

  class BLINK_PLATFORM_EXPORT Task {
   public:
    virtual ~Task() {}
    virtual void run() = 0;
  };

  // Schedule a task to be run on the the associated WebThread.
  // Takes ownership of |Task|. Can be called from any thread.
  virtual void postTask(const WebTraceLocation&, Task*) = 0;

  // Schedule a task to be run after |delayMs| on the the associated WebThread.
  // Takes ownership of |Task|. Can be called from any thread.
  virtual void postDelayedTask(const WebTraceLocation&,
                               Task*,
                               double delayMs) = 0;

  // Schedule a task to be run after |delayMs| on the the associated WebThread.
  // Can be called from any thread.
  virtual void postDelayedTask(const WebTraceLocation&,
                               const base::Closure&,
                               double delayMs) = 0;

  // Returns a clone of the WebTaskRunner.
  virtual std::unique_ptr<WebTaskRunner> clone() = 0;

  // Returns true if the current thread is a thread on which a task may be run.
  // Can be called from any thread.
  virtual bool runsTasksOnCurrentThread() = 0;

  // ---

  // Headless Chrome virtualises time for determinism and performance (fast
  // forwarding of timers). To make this work some parts of blink (e.g. Timers)
  // need to use virtual time, however by default new code should use the normal
  // non-virtual time APIs.

  // Returns a double which is the number of seconds since epoch (Jan 1, 1970).
  // This may represent either the real time, or a virtual time depending on
  // whether or not the WebTaskRunner is associated with a virtual time domain
  // or a real time domain.
  virtual double virtualTimeSeconds() const = 0;

  // Returns a microsecond resolution platform dependant time source.
  // This may represent either the real time, or a virtual time depending on
  // whether or not the WebTaskRunner is associated with a virtual time domain
  // or a real time domain.
  virtual double monotonicallyIncreasingVirtualTimeSeconds() const = 0;

  // Returns the underlying task runner object.
  virtual SingleThreadTaskRunner* toSingleThreadTaskRunner() = 0;

  // Helpers for posting bound functions as tasks.

  // For cross-thread posting. Can be called from any thread.
  void postTask(const WebTraceLocation&, std::unique_ptr<CrossThreadClosure>);
  void postDelayedTask(const WebTraceLocation&,
                       std::unique_ptr<CrossThreadClosure>,
                       long long delayMs);

  // For same-thread posting. Must be called from the associated WebThread.
  void postTask(const WebTraceLocation&, std::unique_ptr<WTF::Closure>);
  void postDelayedTask(const WebTraceLocation&,
                       std::unique_ptr<WTF::Closure>,
                       long long delayMs);

  // For same-thread cancellable task posting. Returns a TaskHandle object for
  // cancellation.
  TaskHandle postCancellableTask(const WebTraceLocation&,
                                 std::unique_ptr<WTF::Closure>)
      WARN_UNUSED_RETURN;
  TaskHandle postDelayedCancellableTask(const WebTraceLocation&,
                                        std::unique_ptr<WTF::Closure>,
                                        long long delayMs) WARN_UNUSED_RETURN;
};

}  // namespace blink

#endif  // WebTaskRunner_h
