// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/dom_storage_task_runner.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/tracked_objects.h"

namespace content {

// DOMStorageTaskRunner

bool DOMStorageTaskRunner::RunsTasksOnCurrentThread() const {
  return IsRunningOnSequence(PRIMARY_SEQUENCE);
}

// DOMStorageWorkerPoolTaskRunner

DOMStorageWorkerPoolTaskRunner::DOMStorageWorkerPoolTaskRunner(
    base::SequencedWorkerPool* sequenced_worker_pool,
    base::SequencedWorkerPool::SequenceToken primary_sequence_token,
    base::SequencedWorkerPool::SequenceToken commit_sequence_token,
    base::SingleThreadTaskRunner* delayed_task_task_runner)
    : task_runner_(delayed_task_task_runner),
      sequenced_worker_pool_(sequenced_worker_pool),
      primary_sequence_token_(primary_sequence_token),
      commit_sequence_token_(commit_sequence_token) {
}

DOMStorageWorkerPoolTaskRunner::~DOMStorageWorkerPoolTaskRunner() {
}

bool DOMStorageWorkerPoolTaskRunner::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  // Note base::TaskRunner implements PostTask in terms of PostDelayedTask
  // with a delay of zero, we detect that usage and avoid the unecessary
  // trip thru the message loop.
  if (delay.is_zero()) {
    return sequenced_worker_pool_->PostSequencedWorkerTaskWithShutdownBehavior(
        primary_sequence_token_, from_here, task,
        base::SequencedWorkerPool::BLOCK_SHUTDOWN);
  }
  // Post a task to call this->PostTask() after the delay.
  return task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&DOMStorageWorkerPoolTaskRunner::PostTask),
                 this, from_here, task),
      delay);
}

bool DOMStorageWorkerPoolTaskRunner::PostShutdownBlockingTask(
    const tracked_objects::Location& from_here,
    SequenceID sequence_id,
    const base::Closure& task) {
  return sequenced_worker_pool_->PostSequencedWorkerTaskWithShutdownBehavior(
      IDtoToken(sequence_id), from_here, task,
      base::SequencedWorkerPool::BLOCK_SHUTDOWN);
}

bool DOMStorageWorkerPoolTaskRunner::IsRunningOnSequence(
    SequenceID sequence_id) const {
  return sequenced_worker_pool_->IsRunningSequenceOnCurrentThread(
      IDtoToken(sequence_id));
}

scoped_refptr<base::SequencedTaskRunner>
DOMStorageWorkerPoolTaskRunner::GetSequencedTaskRunner(SequenceID sequence_id) {
  return sequenced_worker_pool_->GetSequencedTaskRunner(IDtoToken(sequence_id));
}

base::SequencedWorkerPool::SequenceToken
DOMStorageWorkerPoolTaskRunner::IDtoToken(SequenceID id) const {
  if (id == PRIMARY_SEQUENCE)
    return primary_sequence_token_;
  DCHECK_EQ(COMMIT_SEQUENCE, id);
  return commit_sequence_token_;
}

// MockDOMStorageTaskRunner

MockDOMStorageTaskRunner::MockDOMStorageTaskRunner(
    base::SingleThreadTaskRunner* task_runner)
    : task_runner_(task_runner) {
}

MockDOMStorageTaskRunner::~MockDOMStorageTaskRunner() {
}

bool MockDOMStorageTaskRunner::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  return task_runner_->PostTask(from_here, task);
}

bool MockDOMStorageTaskRunner::PostShutdownBlockingTask(
    const tracked_objects::Location& from_here,
    SequenceID sequence_id,
    const base::Closure& task) {
  return task_runner_->PostTask(from_here, task);
}

bool MockDOMStorageTaskRunner::IsRunningOnSequence(SequenceID) const {
  return task_runner_->RunsTasksOnCurrentThread();
}

scoped_refptr<base::SequencedTaskRunner>
MockDOMStorageTaskRunner::GetSequencedTaskRunner(SequenceID sequence_id) {
  return task_runner_;
}

}  // namespace content
