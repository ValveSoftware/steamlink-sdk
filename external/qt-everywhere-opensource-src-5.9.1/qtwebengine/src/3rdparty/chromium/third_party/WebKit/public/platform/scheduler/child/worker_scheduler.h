// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_PUBLIC_PLATFORM_SCHEDULER_CHILD_WORKER_SCHEDULER_H_
#define THIRD_PARTY_WEBKIT_PUBLIC_PLATFORM_SCHEDULER_CHILD_WORKER_SCHEDULER_H_

#include <memory>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "public/platform/scheduler/child/child_scheduler.h"
#include "public/platform/scheduler/child/single_thread_idle_task_runner.h"
#include "public/platform/WebCommon.h"

namespace base {
class MessageLoop;
}

namespace blink {
namespace scheduler {
class SchedulerTqmDelegate;

class BLINK_PLATFORM_EXPORT WorkerScheduler : public ChildScheduler {
 public:
  ~WorkerScheduler() override;
  static std::unique_ptr<WorkerScheduler> Create(
      scoped_refptr<SchedulerTqmDelegate> main_task_runner);

  // Must be called before the scheduler can be used. Does any post construction
  // initialization needed such as initializing idle period detection.
  virtual void Init() = 0;

 protected:
  WorkerScheduler();
  DISALLOW_COPY_AND_ASSIGN(WorkerScheduler);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_PUBLIC_PLATFORM_SCHEDULER_CHILD_WORKER_SCHEDULER_H_
