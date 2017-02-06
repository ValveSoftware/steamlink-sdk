// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_SCHEDULER_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_SCHEDULER_H_

#include <list>

#include "base/callback.h"
#include "base/macros.h"
#include "content/common/content_export.h"

namespace content {

// TODO(jkarlin): Support readers and writers so operations can run in parallel.
// TODO(jkarlin): Support operation identification so that ops can be checked in
// DCHECKs.

// CacheStorageScheduler runs the scheduled callbacks sequentially. Add an
// operation by calling ScheduleOperation() with your callback. Once your
// operation is done be sure to call CompleteOperationAndRunNext() to schedule
// the next operation.
class CONTENT_EXPORT CacheStorageScheduler {
 public:
  CacheStorageScheduler();
  virtual ~CacheStorageScheduler();

  // Adds the operation to the tail of the queue and starts it if the scheduler
  // is idle.
  void ScheduleOperation(const base::Closure& closure);

  // Call this after each operation completes. It cleans up the current
  // operation and starts the next.
  void CompleteOperationAndRunNext();

  // Returns true if there are any running or pending operations.
  bool ScheduledOperations() const;

 private:
  void RunOperationIfIdle();

  // The list of operations waiting on initialization.
  std::list<base::Closure> pending_operations_;
  bool operation_running_;

  DISALLOW_COPY_AND_ASSIGN(CacheStorageScheduler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_SCHEDULER_H_
