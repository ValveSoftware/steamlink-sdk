// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_CHILD_WEB_SCHEDULER_IMPL_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_CHILD_WEB_SCHEDULER_IMPL_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "public/platform/WebCommon.h"
#include "public/platform/WebScheduler.h"
#include "public/platform/WebThread.h"

namespace blink {
namespace scheduler {

class ChildScheduler;
class SingleThreadIdleTaskRunner;
class TaskQueue;
class WebTaskRunnerImpl;

class BLINK_PLATFORM_EXPORT WebSchedulerImpl : public WebScheduler {
 public:
  WebSchedulerImpl(ChildScheduler* child_scheduler,
                   scoped_refptr<SingleThreadIdleTaskRunner> idle_task_runner,
                   scoped_refptr<TaskQueue> loading_task_runner,
                   scoped_refptr<TaskQueue> timer_task_runner);
  ~WebSchedulerImpl() override;

  // WebScheduler implementation:
  void shutdown() override;
  bool shouldYieldForHighPriorityWork() override;
  bool canExceedIdleDeadlineIfRequired() override;
  void postIdleTask(const WebTraceLocation& location,
                    WebThread::IdleTask* task) override;
  void postNonNestableIdleTask(const WebTraceLocation& location,
                               WebThread::IdleTask* task) override;
  WebTaskRunner* loadingTaskRunner() override;
  WebTaskRunner* timerTaskRunner() override;
  std::unique_ptr<WebViewScheduler> createWebViewScheduler(
      InterventionReporter*,
      WebViewScheduler::WebViewSchedulerSettings*) override;
  void suspendTimerQueue() override {}
  void resumeTimerQueue() override {}
  void addPendingNavigation(WebScheduler::NavigatingFrameType type) override {}
  void removePendingNavigation(
      WebScheduler::NavigatingFrameType type) override {}
  void onNavigationStarted() override {}

 private:
  static void runIdleTask(std::unique_ptr<WebThread::IdleTask> task,
                          base::TimeTicks deadline);

  ChildScheduler* child_scheduler_;  // NOT OWNED
  scoped_refptr<SingleThreadIdleTaskRunner> idle_task_runner_;
  scoped_refptr<TaskQueue> timer_task_runner_;
  std::unique_ptr<WebTaskRunnerImpl> loading_web_task_runner_;
  std::unique_ptr<WebTaskRunnerImpl> timer_web_task_runner_;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_CHILD_WEB_SCHEDULER_IMPL_H_
