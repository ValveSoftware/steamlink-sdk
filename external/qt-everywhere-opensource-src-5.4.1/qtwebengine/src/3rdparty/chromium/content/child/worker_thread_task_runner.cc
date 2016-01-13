// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/worker_thread_task_runner.h"

#include "base/logging.h"
#include "content/child/worker_task_runner.h"

namespace content {

WorkerThreadTaskRunner::WorkerThreadTaskRunner(int worker_thread_id)
    : worker_thread_id_(worker_thread_id) {
}

scoped_refptr<WorkerThreadTaskRunner> WorkerThreadTaskRunner::current() {
  int worker_thread_id = WorkerTaskRunner::Instance()->CurrentWorkerId();
  if (!worker_thread_id)
    return scoped_refptr<WorkerThreadTaskRunner>();
  return make_scoped_refptr(new WorkerThreadTaskRunner(worker_thread_id));
}

bool WorkerThreadTaskRunner::PostDelayedTask(
    const tracked_objects::Location& /* from_here */,
    const base::Closure& task,
    base::TimeDelta delay) {
  // Currently non-zero delay is not supported.
  DCHECK(!delay.ToInternalValue());
  return WorkerTaskRunner::Instance()->PostTask(worker_thread_id_, task);
}

bool WorkerThreadTaskRunner::RunsTasksOnCurrentThread() const {
  return worker_thread_id_ == WorkerTaskRunner::Instance()->CurrentWorkerId();
}

WorkerThreadTaskRunner::~WorkerThreadTaskRunner() {}

}  // namespace content
