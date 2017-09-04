// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/scheduler/child/worker_scheduler.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "platform/scheduler/child/scheduler_tqm_delegate.h"
#include "platform/scheduler/child/worker_scheduler_impl.h"

namespace blink {
namespace scheduler {

WorkerScheduler::WorkerScheduler() {}

WorkerScheduler::~WorkerScheduler() {}

// static
std::unique_ptr<WorkerScheduler> WorkerScheduler::Create(
    scoped_refptr<SchedulerTqmDelegate> main_task_runner) {
  return base::WrapUnique(new WorkerSchedulerImpl(std::move(main_task_runner)));
}

}  // namespace scheduler
}  // namespace blink
