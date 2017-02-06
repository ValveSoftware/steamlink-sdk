// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_scheduler.h"

#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"

namespace content {

CacheStorageScheduler::CacheStorageScheduler() : operation_running_(false) {
}

CacheStorageScheduler::~CacheStorageScheduler() {
}

void CacheStorageScheduler::ScheduleOperation(const base::Closure& closure) {
  pending_operations_.push_back(closure);
  RunOperationIfIdle();
}

void CacheStorageScheduler::CompleteOperationAndRunNext() {
  DCHECK(operation_running_);
  operation_running_ = false;
  RunOperationIfIdle();
}

bool CacheStorageScheduler::ScheduledOperations() const {
  return operation_running_ || !pending_operations_.empty();
}

void CacheStorageScheduler::RunOperationIfIdle() {
  if (!operation_running_ && !pending_operations_.empty()) {
    operation_running_ = true;
    // TODO(jkarlin): Run multiple operations in parallel where allowed.
    base::Closure closure = pending_operations_.front();
    pending_operations_.pop_front();
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(closure));
  }
}

}  // namespace content
