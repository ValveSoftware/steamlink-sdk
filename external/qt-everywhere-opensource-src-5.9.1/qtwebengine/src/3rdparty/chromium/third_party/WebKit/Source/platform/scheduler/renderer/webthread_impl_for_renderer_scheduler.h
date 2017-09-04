// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_WEBTHREAD_IMPL_FOR_RENDERER_SCHEDULER_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_WEBTHREAD_IMPL_FOR_RENDERER_SCHEDULER_H_

#include "base/containers/scoped_ptr_hash_map.h"
#include "public/platform/scheduler/child/webthread_base.h"

namespace blink {
class WebScheduler;
};

namespace blink {
namespace scheduler {
class RendererSchedulerImpl;
class WebSchedulerImpl;
class WebTaskRunnerImpl;

class BLINK_PLATFORM_EXPORT WebThreadImplForRendererScheduler
    : public WebThreadBase {
 public:
  explicit WebThreadImplForRendererScheduler(RendererSchedulerImpl* scheduler);
  ~WebThreadImplForRendererScheduler() override;

  // WebThread implementation.
  WebScheduler* scheduler() const override;
  PlatformThreadId threadId() const override;
  WebTaskRunner* getWebTaskRunner() override;

  // WebThreadBase implementation.
  base::SingleThreadTaskRunner* GetTaskRunner() const override;
  SingleThreadIdleTaskRunner* GetIdleTaskRunner() const override;

 private:
  void AddTaskObserverInternal(
      base::MessageLoop::TaskObserver* observer) override;
  void RemoveTaskObserverInternal(
      base::MessageLoop::TaskObserver* observer) override;

  void AddTaskTimeObserverInternal(TaskTimeObserver*) override;
  void RemoveTaskTimeObserverInternal(TaskTimeObserver*) override;

  std::unique_ptr<WebSchedulerImpl> web_scheduler_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<SingleThreadIdleTaskRunner> idle_task_runner_;
  RendererSchedulerImpl* scheduler_;  // Not owned.
  PlatformThreadId thread_id_;
  std::unique_ptr<WebTaskRunnerImpl> web_task_runner_;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_WEBTHREAD_IMPL_FOR_RENDERER_SCHEDULER_H_
