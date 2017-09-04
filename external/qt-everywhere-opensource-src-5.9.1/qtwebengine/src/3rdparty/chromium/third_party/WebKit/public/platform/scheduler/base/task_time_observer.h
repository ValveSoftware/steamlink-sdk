// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_PUBLIC_PLATFORM_SCHEDULER_BASE_TASK_TIME_OBSERVER_H_
#define THIRD_PARTY_WEBKIT_PUBLIC_PLATFORM_SCHEDULER_BASE_TASK_TIME_OBSERVER_H_

#include "base/time/time.h"
#include "public/platform/WebCommon.h"

namespace blink {
namespace scheduler {

class TaskQueue;

// TaskTimeObserver provides an API for observing completion of renderer tasks.
class BLINK_PLATFORM_EXPORT TaskTimeObserver {
 public:
  TaskTimeObserver() {}
  virtual ~TaskTimeObserver() {}

  // Callback to be called when task is completed.
  // |task_queue| - TaskQueue on which this task was run,
  // |start_time| - time in seconds when task started to run,
  // |end_time| - time in seconds when task was completed.
  virtual void ReportTaskTime(TaskQueue* task_queue,
                              double start_time,
                              double end_time) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(TaskTimeObserver);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_PUBLIC_PLATFORM_SCHEDULER_BASE_TASK_TIME_OBSERVER_H_
