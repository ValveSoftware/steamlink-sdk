// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_BLOCKING_TASK_RUNNER_H_
#define CC_TREES_BLOCKING_TASK_RUNNER_H_

#include <vector>

#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"
#include "cc/base/cc_export.h"

namespace cc {

// This class wraps a SingleThreadTaskRunner but allows posted tasks to be
// run without a round trip through the message loop. This shortcutting
// removes guarantees about ordering. Tasks posted while the
// BlockingTaskRunner is in a capturing state will run in order, and tasks
// posted while the BlockingTaskRunner is /not/ in a capturing state will
// run in order, but the two sets of tasks will *not* run in order relative
// to when they were posted.
//
// To use this class, post tasks to the task runner returned by
// BlockingTaskRunner::current() on the thread you want the tasks to run.
// Hold a reference to the BlockingTaskRunner as long as you intend to
// post tasks to it.
//
// Then, on the thread from which the BlockingTaskRunner was created, you
// may instantiate a BlockingTaskRunner::CapturePostTasks. While this object
// exists, the task runner will collect any PostTasks called on it, posting
// tasks to that thread from anywhere. This CapturePostTasks object provides
// a window in time where tasks can shortcut past the MessageLoop. As soon
// as the CapturePostTasks object is destroyed (goes out of scope), all
// tasks that had been posted to the thread during the window will be exectuted
// immediately.
//
// Beware of re-entrancy, make sure the CapturePostTasks object is destroyed at
// a time when it makes sense for the embedder to call arbitrary things.
class CC_EXPORT BlockingTaskRunner
    : public base::RefCountedThreadSafe<BlockingTaskRunner> {
 public:
  // Returns the BlockingTaskRunner for the current thread, creating one if
  // necessary.
  static scoped_refptr<BlockingTaskRunner> current();

  // While an object of this type is held alive on a thread, any tasks
  // posted to the thread will be captured and run as soon as the object
  // is destroyed, shortcutting past the MessageLoop.
  class CC_EXPORT CapturePostTasks {
   public:
    CapturePostTasks();
    ~CapturePostTasks();

   private:
    scoped_refptr<BlockingTaskRunner> blocking_runner_;

    DISALLOW_COPY_AND_ASSIGN(CapturePostTasks);
  };

  // True if tasks posted to the BlockingTaskRunner will run on the current
  // thread.
  bool BelongsToCurrentThread();

  // Posts a task using the contained SingleThreadTaskRunner unless |capture_|
  // is true. When |capture_| is true, tasks posted will be caught and stored
  // until the capturing stops. At that time the tasks will be run directly
  // instead of being posted to the SingleThreadTaskRunner.
  bool PostTask(const tracked_objects::Location& from_here,
                const base::Closure& task);

 private:
  friend class base::RefCountedThreadSafe<BlockingTaskRunner>;

  explicit BlockingTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  virtual ~BlockingTaskRunner();

  void SetCapture(bool capture);

  base::PlatformThreadId thread_id_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::Lock lock_;
  int capture_;
  std::vector<base::Closure> captured_tasks_;
};

}  // namespace cc

#endif  // CC_TREES_BLOCKING_TASK_RUNNER_H_
