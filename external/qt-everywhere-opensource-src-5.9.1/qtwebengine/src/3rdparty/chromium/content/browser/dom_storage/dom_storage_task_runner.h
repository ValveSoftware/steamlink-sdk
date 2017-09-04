// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_TASK_RUNNER_H_
#define CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_TASK_RUNNER_H_

#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/time.h"
#include "content/common/content_export.h"

namespace base {
class SingleThreadTaskRunner;
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
  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       const base::Closure& task,
                       base::TimeDelta delay) override = 0;

  // Posts a shutdown blocking task to |sequence_id|.
  virtual bool PostShutdownBlockingTask(
      const tracked_objects::Location& from_here,
      SequenceID sequence_id,
      const base::Closure& task) = 0;

  virtual void AssertIsRunningOnPrimarySequence() const = 0;
  virtual void AssertIsRunningOnCommitSequence() const = 0;

  virtual scoped_refptr<base::SequencedTaskRunner> GetSequencedTaskRunner(
      SequenceID sequence_id) = 0;

 protected:
  ~DOMStorageTaskRunner() override {}
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
      base::SingleThreadTaskRunner* delayed_task_task_runner);

  bool RunsTasksOnCurrentThread() const override;

  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       const base::Closure& task,
                       base::TimeDelta delay) override;

  bool PostShutdownBlockingTask(const tracked_objects::Location& from_here,
                                SequenceID sequence_id,
                                const base::Closure& task) override;

  void AssertIsRunningOnPrimarySequence() const override;
  void AssertIsRunningOnCommitSequence() const override;

  scoped_refptr<base::SequencedTaskRunner> GetSequencedTaskRunner(
      SequenceID sequence_id) override;

 protected:
  ~DOMStorageWorkerPoolTaskRunner() override;

 private:

  base::SequencedWorkerPool::SequenceToken IDtoToken(SequenceID id) const;

  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  const scoped_refptr<base::SequencedWorkerPool> sequenced_worker_pool_;
  base::SequencedWorkerPool::SequenceToken primary_sequence_token_;
  base::SequenceChecker primary_sequence_checker_;
  base::SequencedWorkerPool::SequenceToken commit_sequence_token_;
  base::SequenceChecker commit_sequence_checker_;
};

// A derived class used in unit tests that ignores all delays so
// we don't block in unit tests waiting for timeouts to expire.
// There is no distinction between [non]-shutdown-blocking or
// the primary sequence vs the commit sequence in the mock,
// all tasks are scheduled on |message_loop| with zero delay.
class CONTENT_EXPORT MockDOMStorageTaskRunner :
      public DOMStorageTaskRunner {
 public:
  explicit MockDOMStorageTaskRunner(base::SingleThreadTaskRunner* task_runner);

  bool RunsTasksOnCurrentThread() const override;

  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       const base::Closure& task,
                       base::TimeDelta delay) override;

  bool PostShutdownBlockingTask(const tracked_objects::Location& from_here,
                                SequenceID sequence_id,
                                const base::Closure& task) override;

  void AssertIsRunningOnPrimarySequence() const override;
  void AssertIsRunningOnCommitSequence() const override;

  scoped_refptr<base::SequencedTaskRunner> GetSequencedTaskRunner(
      SequenceID sequence_id) override;

 protected:
  ~MockDOMStorageTaskRunner() override;

 private:
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_TASK_RUNNER_H_
