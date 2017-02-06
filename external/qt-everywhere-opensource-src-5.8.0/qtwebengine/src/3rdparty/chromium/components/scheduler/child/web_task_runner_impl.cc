// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/child/web_task_runner_impl.h"

#include "base/bind.h"
#include "base/location.h"
#include "components/scheduler/base/task_queue.h"
#include "components/scheduler/base/time_domain.h"
#include "third_party/WebKit/public/platform/WebTraceLocation.h"

namespace scheduler {

WebTaskRunnerImpl::WebTaskRunnerImpl(scoped_refptr<TaskQueue> task_queue)
    : task_queue_(task_queue) {}

WebTaskRunnerImpl::~WebTaskRunnerImpl() {}

void WebTaskRunnerImpl::postTask(const blink::WebTraceLocation& web_location,
                                 blink::WebTaskRunner::Task* task) {
  tracked_objects::Location location(web_location.functionName(),
                                     web_location.fileName(), -1, nullptr);
  task_queue_->PostTask(
      location,
      base::Bind(
          &WebTaskRunnerImpl::runTask,
          base::Passed(std::unique_ptr<blink::WebTaskRunner::Task>(task))));
}

void WebTaskRunnerImpl::postDelayedTask(
    const blink::WebTraceLocation& web_location,
    blink::WebTaskRunner::Task* task,
    double delayMs) {
  DCHECK_GE(delayMs, 0.0);
  tracked_objects::Location location(web_location.functionName(),
                                     web_location.fileName(), -1, nullptr);
  task_queue_->PostDelayedTask(
      location,
      base::Bind(
          &WebTaskRunnerImpl::runTask,
          base::Passed(std::unique_ptr<blink::WebTaskRunner::Task>(task))),
      base::TimeDelta::FromMillisecondsD(delayMs));
}

double WebTaskRunnerImpl::virtualTimeSeconds() const {
  return (Now() - base::TimeTicks::UnixEpoch()).InSecondsF();
}

double WebTaskRunnerImpl::monotonicallyIncreasingVirtualTimeSeconds() const {
  return Now().ToInternalValue() /
         static_cast<double>(base::Time::kMicrosecondsPerSecond);
}

base::TimeTicks WebTaskRunnerImpl::Now() const {
  TimeDomain* time_domain = task_queue_->GetTimeDomain();
  // It's possible task_queue_ has been Unregistered which can lead to a null
  // TimeDomain.  If that happens just return the current real time.
  if (!time_domain)
    return base::TimeTicks::Now();
  return time_domain->Now();
}

blink::WebTaskRunner* WebTaskRunnerImpl::clone() {
  return new WebTaskRunnerImpl(task_queue_);
}

void WebTaskRunnerImpl::runTask(
    std::unique_ptr<blink::WebTaskRunner::Task> task) {
  task->run();
}

}  // namespace scheduler
