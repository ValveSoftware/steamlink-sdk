// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// An implementation of WebThread in terms of base::MessageLoop and
// base::Thread

#include "components/scheduler/child/webthread_base.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/pending_task.h"
#include "base/threading/platform_thread.h"
#include "components/scheduler/child/single_thread_idle_task_runner.h"
#include "third_party/WebKit/public/platform/WebTraceLocation.h"

namespace scheduler {

class WebThreadBase::TaskObserverAdapter
    : public base::MessageLoop::TaskObserver {
 public:
  explicit TaskObserverAdapter(WebThread::TaskObserver* observer)
      : observer_(observer) {}

  void WillProcessTask(const base::PendingTask& pending_task) override {
    observer_->willProcessTask();
  }

  void DidProcessTask(const base::PendingTask& pending_task) override {
    observer_->didProcessTask();
  }

 private:
  WebThread::TaskObserver* observer_;
};

WebThreadBase::WebThreadBase() {
}

WebThreadBase::~WebThreadBase() {
  for (auto& observer_entry : task_observer_map_) {
    delete observer_entry.second;
  }
}

void WebThreadBase::addTaskObserver(TaskObserver* observer) {
  CHECK(isCurrentThread());
  std::pair<TaskObserverMap::iterator, bool> result = task_observer_map_.insert(
      std::make_pair(observer, nullptr));
  if (result.second)
    result.first->second = new TaskObserverAdapter(observer);
  AddTaskObserverInternal(result.first->second);
}

void WebThreadBase::removeTaskObserver(TaskObserver* observer) {
  CHECK(isCurrentThread());
  TaskObserverMap::iterator iter = task_observer_map_.find(observer);
  if (iter == task_observer_map_.end())
    return;
  RemoveTaskObserverInternal(iter->second);
  delete iter->second;
  task_observer_map_.erase(iter);
}

void WebThreadBase::AddTaskObserverInternal(
    base::MessageLoop::TaskObserver* observer) {
  base::MessageLoop::current()->AddTaskObserver(observer);
}

void WebThreadBase::RemoveTaskObserverInternal(
    base::MessageLoop::TaskObserver* observer) {
  base::MessageLoop::current()->RemoveTaskObserver(observer);
}

// static
void WebThreadBase::RunWebThreadIdleTask(
    std::unique_ptr<blink::WebThread::IdleTask> idle_task,
    base::TimeTicks deadline) {
  idle_task->run((deadline - base::TimeTicks()).InSecondsF());
}

void WebThreadBase::postIdleTask(const blink::WebTraceLocation& web_location,
                                 IdleTask* idle_task) {
  tracked_objects::Location location(web_location.functionName(),
                                     web_location.fileName(), -1, nullptr);
  GetIdleTaskRunner()->PostIdleTask(
      location, base::Bind(&WebThreadBase::RunWebThreadIdleTask,
                           base::Passed(base::WrapUnique(idle_task))));
}

void WebThreadBase::postIdleTaskAfterWakeup(
    const blink::WebTraceLocation& web_location,
    IdleTask* idle_task) {
  tracked_objects::Location location(web_location.functionName(),
                                     web_location.fileName(), -1, nullptr);
  GetIdleTaskRunner()->PostIdleTaskAfterWakeup(
      location, base::Bind(&WebThreadBase::RunWebThreadIdleTask,
                           base::Passed(base::WrapUnique(idle_task))));
}

bool WebThreadBase::isCurrentThread() const {
  return GetTaskRunner()->BelongsToCurrentThread();
}

}  // namespace scheduler
