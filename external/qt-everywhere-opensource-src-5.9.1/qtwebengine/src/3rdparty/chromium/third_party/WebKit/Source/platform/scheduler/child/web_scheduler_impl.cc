// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/child/web_scheduler_impl.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "platform/scheduler/child/web_task_runner_impl.h"
#include "public/platform/WebTraceLocation.h"
#include "public/platform/WebViewScheduler.h"
#include "public/platform/scheduler/child/worker_scheduler.h"

namespace blink {
namespace scheduler {

WebSchedulerImpl::WebSchedulerImpl(
    ChildScheduler* child_scheduler,
    scoped_refptr<SingleThreadIdleTaskRunner> idle_task_runner,
    scoped_refptr<TaskQueue> loading_task_runner,
    scoped_refptr<TaskQueue> timer_task_runner)
    : child_scheduler_(child_scheduler),
      idle_task_runner_(idle_task_runner),
      timer_task_runner_(timer_task_runner),
      loading_web_task_runner_(new WebTaskRunnerImpl(loading_task_runner)),
      timer_web_task_runner_(new WebTaskRunnerImpl(timer_task_runner)) {}

WebSchedulerImpl::~WebSchedulerImpl() {}

void WebSchedulerImpl::shutdown() {
  child_scheduler_->Shutdown();
}

bool WebSchedulerImpl::shouldYieldForHighPriorityWork() {
  return child_scheduler_->ShouldYieldForHighPriorityWork();
}

bool WebSchedulerImpl::canExceedIdleDeadlineIfRequired() {
  return child_scheduler_->CanExceedIdleDeadlineIfRequired();
}

void WebSchedulerImpl::runIdleTask(
    std::unique_ptr<blink::WebThread::IdleTask> task,
    base::TimeTicks deadline) {
  task->run((deadline - base::TimeTicks()).InSecondsF());
}

void WebSchedulerImpl::postIdleTask(const blink::WebTraceLocation& location,
                                    blink::WebThread::IdleTask* task) {
  DCHECK(idle_task_runner_);
  idle_task_runner_->PostIdleTask(
      location, base::Bind(&WebSchedulerImpl::runIdleTask,
                           base::Passed(base::WrapUnique(task))));
}

void WebSchedulerImpl::postNonNestableIdleTask(
    const blink::WebTraceLocation& location,
    blink::WebThread::IdleTask* task) {
  DCHECK(idle_task_runner_);
  idle_task_runner_->PostNonNestableIdleTask(
      location, base::Bind(&WebSchedulerImpl::runIdleTask,
                           base::Passed(base::WrapUnique(task))));
}

blink::WebTaskRunner* WebSchedulerImpl::loadingTaskRunner() {
  return loading_web_task_runner_.get();
}

blink::WebTaskRunner* WebSchedulerImpl::timerTaskRunner() {
  return timer_web_task_runner_.get();
}

std::unique_ptr<blink::WebViewScheduler>
WebSchedulerImpl::createWebViewScheduler(
    InterventionReporter*,
    WebViewScheduler::WebViewSchedulerSettings*) {
  NOTREACHED();
  return nullptr;
}

}  // namespace scheduler
}  // namespace blink
