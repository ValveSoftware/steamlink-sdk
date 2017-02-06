// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_CHILD_WEB_TASK_RUNNER_H_
#define COMPONENTS_SCHEDULER_CHILD_WEB_TASK_RUNNER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "components/scheduler/scheduler_export.h"
#include "third_party/WebKit/public/platform/WebTaskRunner.h"

namespace scheduler {
class TaskQueue;

class SCHEDULER_EXPORT WebTaskRunnerImpl : public blink::WebTaskRunner {
 public:
  explicit WebTaskRunnerImpl(scoped_refptr<TaskQueue> task_queue);

  ~WebTaskRunnerImpl() override;

  // blink::WebTaskRunner implementation:
  void postTask(const blink::WebTraceLocation& web_location,
                blink::WebTaskRunner::Task* task) override;
  void postDelayedTask(const blink::WebTraceLocation& web_location,
                       blink::WebTaskRunner::Task* task,
                       double delayMs) override;
  double virtualTimeSeconds() const override;
  double monotonicallyIncreasingVirtualTimeSeconds() const override;
  blink::WebTaskRunner* clone() override;

  // blink::WebTaskRunner::Task should be wrapped by base::Passed() when
  // used with base::Bind(). See https://crbug.com/551356.
  // runTask() is a helper to call blink::WebTaskRunner::Task::run from
  // std::unique_ptr<blink::WebTaskRunner::Task>.
  // runTask() is placed here because std::unique_ptr<> cannot be used from
  // Blink.
  static void runTask(std::unique_ptr<blink::WebTaskRunner::Task>);

 private:
  base::TimeTicks Now() const;

  scoped_refptr<TaskQueue> task_queue_;

  DISALLOW_COPY_AND_ASSIGN(WebTaskRunnerImpl);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_CHILD_WEB_TASK_RUNNER_H_
