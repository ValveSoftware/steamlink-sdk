// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_TASK_RUNNER_
#define CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_TASK_RUNNER_

#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/time.h"
#include "content/common/content_export.h"

namespace base {
class MessageLoopProxy;
}

namespace content {

// DOMStorage uses two task sequences (primary vs commit) to avoid
// primary access from queuing up behind commits to disk.
// * Initialization, shutdown, and administrative tasks are performed as
//   shutdown-blocking primary sequence tasks.
// * Tasks directly related to the javascript'able interface are performed
//   as shutdown-blocking primary sequence tasks.
//   TODO(michaeln): Skip tasks for reading during shutdown.
// * Internal tasks related to committing changes to disk are performed as
//   shutdown-blocking commit sequence tasks.
class CONTENT_EXPORT DOMStorageTaskRunner
    : public base::TaskRunner {
 public:
  enum SequenceID {
    PRIMARY_SEQUENCE,
    COMMIT_SEQUENCE
  };

  // The PostTask() and PostDelayedTask() methods defined by TaskRunner
  // post shutdown-blocking tasks on the primary sequence.
  virtual bool PostDelayedTask(
      const tracked_objects::Location& from_here,
      const base::Closure& task,
      base::TimeDelta delay) = 0;

  // Posts a shutdown blocking task to |sequence_id|.
  virtual bool PostShutdownBlockingTask(
      const tracked_objects::Location& from_here,
      SequenceID sequence_id,
      const base::Closure& task) = 0;

  // The TaskRunner override returns true if the current thread is running
  // on the primary sequence.
  virtual bool RunsTasksOnCurrentThread() const OVERRIDE;

  // Returns true if the current thread is running on the given |sequence_id|.
  virtual bool IsRunningOnSequence(SequenceID sequence_id) const = 0;
  bool IsRunningOnPrimarySequence() const {
    return IsRunningOnSequence(PRIMARY_SEQUENCE);
  }
  bool IsRunningOnCommitSequence() const {
    return IsRunningOnSequence(COMMIT_SEQUENCE);
  }

 protected:
  virtual ~DOMStorageTaskRunner() {}
};

// A derived class used in chromium that utilizes a SequenceWorkerPool
// under dom_storage specific SequenceTokens. The |delayed_task_loop|
// is used to delay scheduling on the worker pool.
class CONTENT_EXPORT DOMStorageWorkerPoolTaskRunner :
      public DOMStorageTaskRunner {
 public:
  DOMStorageWorkerPoolTaskRunner(
      base::SequencedWorkerPool* sequenced_worker_pool,
      base::SequencedWorkerPool::SequenceToken primary_sequence_token,
      base::SequencedWorkerPool::SequenceToken commit_sequence_token,
      base::MessageLoopProxy* delayed_task_loop);

  virtual bool PostDelayedTask(
      const tracked_objects::Location& from_here,
      const base::Closure& task,
      base::TimeDelta delay) OVERRIDE;

  virtual bool PostShutdownBlockingTask(
      const tracked_objects::Location& from_here,
      SequenceID sequence_id,
      const base::Closure& task) OVERRIDE;

  virtual bool IsRunningOnSequence(SequenceID sequence_id) const OVERRIDE;

 protected:
  virtual ~DOMStorageWorkerPoolTaskRunner();

 private:

  base::SequencedWorkerPool::SequenceToken IDtoToken(SequenceID id) const;

  const scoped_refptr<base::MessageLoopProxy> message_loop_;
  const scoped_refptr<base::SequencedWorkerPool> sequenced_worker_pool_;
  base::SequencedWorkerPool::SequenceToken primary_sequence_token_;
  base::SequencedWorkerPool::SequenceToken commit_sequence_token_;
};

// A derived class used in unit tests that ignores all delays so
// we don't block in unit tests waiting for timeouts to expire.
// There is no distinction between [non]-shutdown-blocking or
// the primary sequence vs the commit sequence in the mock,
// all tasks are scheduled on |message_loop| with zero delay.
class CONTENT_EXPORT MockDOMStorageTaskRunner :
      public DOMStorageTaskRunner {
 public:
  explicit MockDOMStorageTaskRunner(base::MessageLoopProxy* message_loop);

  virtual bool PostDelayedTask(
      const tracked_objects::Location& from_here,
      const base::Closure& task,
      base::TimeDelta delay) OVERRIDE;

  virtual bool PostShutdownBlockingTask(
      const tracked_objects::Location& from_here,
      SequenceID sequence_id,
      const base::Closure& task) OVERRIDE;

  virtual bool IsRunningOnSequence(SequenceID sequence_id) const OVERRIDE;

 protected:
  virtual ~MockDOMStorageTaskRunner();

 private:
  const scoped_refptr<base::MessageLoopProxy> message_loop_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_TASK_RUNNER_
