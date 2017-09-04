// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_CHILD_WEB_TASK_RUNNER_IMPL_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_CHILD_WEB_TASK_RUNNER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "platform/WebTaskRunner.h"
#include "public/platform/WebCommon.h"

namespace blink {
namespace scheduler {
class TaskQueue;

class BLINK_PLATFORM_EXPORT WebTaskRunnerImpl : public WebTaskRunner {
 public:
  explicit WebTaskRunnerImpl(scoped_refptr<TaskQueue> task_queue);

  ~WebTaskRunnerImpl() override;

  // WebTaskRunner implementation:
  void postTask(const WebTraceLocation& web_location,
                WebTaskRunner::Task* task) override;
  void postDelayedTask(const WebTraceLocation& web_location,
                       WebTaskRunner::Task* task,
                       double delayMs) override;
  void postDelayedTask(const WebTraceLocation&,
                       const base::Closure&,
                       double delayMs) override;
  bool runsTasksOnCurrentThread() override;
  double virtualTimeSeconds() const override;
  double monotonicallyIncreasingVirtualTimeSeconds() const override;
  std::unique_ptr<WebTaskRunner> clone() override;
  base::SingleThreadTaskRunner* toSingleThreadTaskRunner() override;

  // WebTaskRunner::Task should be wrapped by base::Passed() when
  // used with base::Bind(). See https://crbug.com/551356.
  // runTask() is a helper to call WebTaskRunner::Task::run from
  // std::unique_ptr<WebTaskRunner::Task>.
  // runTask() is placed here because std::unique_ptr<> cannot be used from
  // Blink.
  static void runTask(std::unique_ptr<WebTaskRunner::Task>);

 private:
  base::TimeTicks Now() const;

  scoped_refptr<TaskQueue> task_queue_;

  DISALLOW_COPY_AND_ASSIGN(WebTaskRunnerImpl);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_CHILD_WEB_TASK_RUNNER_IMPL_H_
