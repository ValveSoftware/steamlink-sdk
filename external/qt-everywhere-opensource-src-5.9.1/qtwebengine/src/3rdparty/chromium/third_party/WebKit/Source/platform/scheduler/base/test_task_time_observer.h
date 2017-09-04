// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TEST_TASK_TIME_OBSERVER_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TEST_TASK_TIME_OBSERVER_H_

#include "base/time/time.h"
#include "public/platform/scheduler/base/task_time_observer.h"

namespace blink {
namespace scheduler {

class TestTaskTimeObserver : public TaskTimeObserver {
 public:
  void ReportTaskTime(TaskQueue* task_queue,
                      double start_time,
                      double end_time) override {}
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TEST_TASK_TIME_OBSERVER_H_
