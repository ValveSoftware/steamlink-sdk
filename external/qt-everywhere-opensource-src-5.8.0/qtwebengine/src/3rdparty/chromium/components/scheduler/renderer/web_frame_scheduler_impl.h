// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_RENDERER_WEB_FRAME_SCHEDULER_IMPL_H_
#define COMPONENTS_SCHEDULER_RENDERER_WEB_FRAME_SCHEDULER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/trace_event/trace_event.h"
#include "components/scheduler/base/task_queue.h"
#include "components/scheduler/scheduler_export.h"
#include "third_party/WebKit/public/platform/WebFrameScheduler.h"

namespace base {
namespace trace_event {
class BlameContext;
}  // namespace trace_event
class SingleThreadTaskRunner;
}  // namespace base

namespace scheduler {

class AutoAdvancingVirtualTimeDomain;
class RendererSchedulerImpl;
class TaskQueue;
class WebTaskRunnerImpl;
class WebViewSchedulerImpl;

class SCHEDULER_EXPORT WebFrameSchedulerImpl : public blink::WebFrameScheduler {
 public:
  WebFrameSchedulerImpl(RendererSchedulerImpl* renderer_scheduler,
                        WebViewSchedulerImpl* parent_web_view_scheduler,
                        base::trace_event::BlameContext* blame_context);

  ~WebFrameSchedulerImpl() override;

  // blink::WebFrameScheduler implementation:
  void setFrameVisible(bool frame_visible) override;
  void setPageVisible(bool page_visible) override;
  blink::WebTaskRunner* loadingTaskRunner() override;
  blink::WebTaskRunner* timerTaskRunner() override;

  void OnVirtualTimeDomainChanged();

 private:
  friend class WebViewSchedulerImpl;

  void DetachFromWebViewScheduler();
  void ApplyPolicyToTimerQueue();

  scoped_refptr<TaskQueue> loading_task_queue_;
  scoped_refptr<TaskQueue> timer_task_queue_;
  std::unique_ptr<WebTaskRunnerImpl> loading_web_task_runner_;
  std::unique_ptr<WebTaskRunnerImpl> timer_web_task_runner_;
  RendererSchedulerImpl* renderer_scheduler_;            // NOT OWNED
  WebViewSchedulerImpl* parent_web_view_scheduler_;      // NOT OWNED
  base::trace_event::BlameContext* blame_context_;       // NOT OWNED
  TaskQueue::PumpPolicy virtual_time_pump_policy_;
  bool frame_visible_;
  bool page_visible_;

  DISALLOW_COPY_AND_ASSIGN(WebFrameSchedulerImpl);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_RENDERER_WEB_FRAME_SCHEDULER_IMPL_H_
