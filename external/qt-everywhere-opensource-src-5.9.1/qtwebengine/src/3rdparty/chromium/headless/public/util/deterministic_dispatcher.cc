// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/public/util/deterministic_dispatcher.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "headless/public/util/managed_dispatch_url_request_job.h"

namespace headless {

DeterministicDispatcher::DeterministicDispatcher(
    scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner)
    : io_thread_task_runner_(std::move(io_thread_task_runner)),
      dispatch_pending_(false),
      weak_ptr_factory_(this) {}

DeterministicDispatcher::~DeterministicDispatcher() {}

void DeterministicDispatcher::JobCreated(ManagedDispatchURLRequestJob* job) {
  base::AutoLock lock(lock_);
  pending_requests_.push_back(job);
}

void DeterministicDispatcher::JobKilled(ManagedDispatchURLRequestJob* job) {
  base::AutoLock lock(lock_);
  for (auto it = pending_requests_.begin(); it != pending_requests_.end();
       it++) {
    if (*it == job) {
      pending_requests_.erase(it);
      break;
    }
  }
  ready_status_map_.erase(job);
  // We rely on JobDeleted getting called to call MaybeDispatchJobLocked.
}

void DeterministicDispatcher::JobFailed(ManagedDispatchURLRequestJob* job,
                                        net::Error error) {
  base::AutoLock lock(lock_);
  ready_status_map_[job] = error;
  MaybeDispatchJobLocked();
}

void DeterministicDispatcher::DataReady(ManagedDispatchURLRequestJob* job) {
  base::AutoLock lock(lock_);
  ready_status_map_[job] = net::OK;
  MaybeDispatchJobLocked();
}

void DeterministicDispatcher::JobDeleted(ManagedDispatchURLRequestJob* job) {
  base::AutoLock lock(lock_);
  MaybeDispatchJobLocked();
}

void DeterministicDispatcher::MaybeDispatchJobLocked() {
  if (dispatch_pending_ || ready_status_map_.empty())
    return;

  dispatch_pending_ = true;
  io_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DeterministicDispatcher::MaybeDispatchJobOnIOThreadTask,
                 weak_ptr_factory_.GetWeakPtr()));
}

void DeterministicDispatcher::MaybeDispatchJobOnIOThreadTask() {
  ManagedDispatchURLRequestJob* job;
  net::Error job_status;

  {
    base::AutoLock lock(lock_);
    dispatch_pending_ = false;
    // If the job got deleted, |pending_requests_| may be empty.
    if (pending_requests_.empty())
      return;
    job = pending_requests_.front();
    StatusMap::const_iterator it = ready_status_map_.find(job);
    // Bail out if the oldest job is not be ready for dispatch yet.
    if (it == ready_status_map_.end())
      return;

    job_status = it->second;
    ready_status_map_.erase(it);
    pending_requests_.pop_front();
  }

  if (job_status == net::OK) {
    job->OnHeadersComplete();
  } else {
    job->OnStartError(job_status);
  }
}

}  // namespace headless
