// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SCHEDULER_BASE_WEB_SCHEDULER_IMPL_H_
#define CONTENT_CHILD_SCHEDULER_BASE_WEB_SCHEDULER_IMPL_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "components/scheduler/scheduler_export.h"
#include "third_party/WebKit/public/platform/WebScheduler.h"
#include "third_party/WebKit/public/platform/WebThread.h"

namespace scheduler {

class ChildScheduler;
class SingleThreadIdleTaskRunner;
class TaskQueue;
class WebTaskRunnerImpl;

class SCHEDULER_EXPORT WebSchedulerImpl : public blink::WebScheduler {
 public:
  WebSchedulerImpl(ChildScheduler* child_scheduler,
                   scoped_refptr<SingleThreadIdleTaskRunner> idle_task_runner,
                   scoped_refptr<TaskQueue> loading_task_runner,
                   scoped_refptr<TaskQueue> timer_task_runner);
  ~WebSchedulerImpl() override;

  // blink::WebScheduler implementation:
  void shutdown() override;
  bool shouldYieldForHighPriorityWork() override;
  bool canExceedIdleDeadlineIfRequired() override;
  void postIdleTask(const blink::WebTraceLocation& location,
                    blink::WebThread::IdleTask* task) override;
  void postNonNestableIdleTask(const blink::WebTraceLocation& location,
                               blink::WebThread::IdleTask* task) override;
  void postIdleTaskAfterWakeup(const blink::WebTraceLocation& location,
                               blink::WebThread::IdleTask* task) override;
  blink::WebTaskRunner* loadingTaskRunner() override;
  blink::WebTaskRunner* timerTaskRunner() override;
  std::unique_ptr<blink::WebViewScheduler> createWebViewScheduler(
      blink::WebView*) override;
  void suspendTimerQueue() override {}
  void resumeTimerQueue() override {}
  void addPendingNavigation(
      blink::WebScheduler::NavigatingFrameType type) override {}
  void removePendingNavigation(
      blink::WebScheduler::NavigatingFrameType type) override {}
  void onNavigationStarted() override {}

 private:
  static void runIdleTask(std::unique_ptr<blink::WebThread::IdleTask> task,
                          base::TimeTicks deadline);

  ChildScheduler* child_scheduler_;  // NOT OWNED
  scoped_refptr<SingleThreadIdleTaskRunner> idle_task_runner_;
  scoped_refptr<TaskQueue> timer_task_runner_;
  std::unique_ptr<WebTaskRunnerImpl> loading_web_task_runner_;
  std::unique_ptr<WebTaskRunnerImpl> timer_web_task_runner_;
};

}  // namespace scheduler

#endif  // CONTENT_CHILD_SCHEDULER_BASE_WEB_SCHEDULER_IMPL_H_
