// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_PUBLIC_PLATFORM_SCHEDULER_CHILD_WEBTHREAD_BASE_H_
#define THIRD_PARTY_WEBKIT_PUBLIC_PLATFORM_SCHEDULER_CHILD_WEBTHREAD_BASE_H_

#include <map>
#include <memory>

#include "base/threading/thread.h"
#include "public/platform/WebCommon.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebTraceLocation.h"

namespace blink {
namespace scheduler {
class SingleThreadIdleTaskRunner;
class TaskTimeObserver;

class BLINK_PLATFORM_EXPORT WebThreadBase : public WebThread {
 public:
  ~WebThreadBase() override;

  // WebThread implementation.
  bool isCurrentThread() const override;
  PlatformThreadId threadId() const override = 0;

  virtual void postIdleTask(const WebTraceLocation& location,
                            IdleTask* idle_task);

  void addTaskObserver(TaskObserver* observer) override;
  void removeTaskObserver(TaskObserver* observer) override;

  void addTaskTimeObserver(TaskTimeObserver* task_time_observer) override;
  void removeTaskTimeObserver(TaskTimeObserver* task_time_observer) override;

  // Returns the base::Bind-compatible task runner for posting tasks to this
  // thread. Can be called from any thread.
  virtual base::SingleThreadTaskRunner* GetTaskRunner() const = 0;

  // Returns the base::Bind-compatible task runner for posting idle tasks to
  // this thread. Can be called from any thread.
  virtual scheduler::SingleThreadIdleTaskRunner* GetIdleTaskRunner() const = 0;

 protected:
  class TaskObserverAdapter;

  WebThreadBase();

  virtual void AddTaskObserverInternal(
      base::MessageLoop::TaskObserver* observer);
  virtual void RemoveTaskObserverInternal(
      base::MessageLoop::TaskObserver* observer);

  virtual void AddTaskTimeObserverInternal(TaskTimeObserver*) {}
  virtual void RemoveTaskTimeObserverInternal(TaskTimeObserver*) {}

  static void RunWebThreadIdleTask(
      std::unique_ptr<WebThread::IdleTask> idle_task,
      base::TimeTicks deadline);

 private:
  typedef std::map<TaskObserver*, TaskObserverAdapter*> TaskObserverMap;
  TaskObserverMap task_observer_map_;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_PUBLIC_PLATFORM_SCHEDULER_CHILD_WEBTHREAD_BASE_H_
