// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_WEB_FRAME_SCHEDULER_IMPL_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_WEB_FRAME_SCHEDULER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/trace_event/trace_event.h"
#include "public/platform/scheduler/base/task_queue.h"
#include "public/platform/WebCommon.h"
#include "public/platform/WebFrameScheduler.h"

namespace base {
namespace trace_event {
class BlameContext;
}  // namespace trace_event
}  // namespace base

namespace blink {
namespace scheduler {

class RendererSchedulerImpl;
class TaskQueue;
class WebTaskRunnerImpl;
class WebViewSchedulerImpl;

class BLINK_PLATFORM_EXPORT WebFrameSchedulerImpl : public WebFrameScheduler {
 public:
  WebFrameSchedulerImpl(RendererSchedulerImpl* renderer_scheduler,
                        WebViewSchedulerImpl* parent_web_view_scheduler,
                        base::trace_event::BlameContext* blame_context);

  ~WebFrameSchedulerImpl() override;

  // WebFrameScheduler implementation:
  void setFrameVisible(bool frame_visible) override;
  void setPageVisible(bool page_visible) override;
  void setCrossOrigin(bool cross_origin) override;
  WebTaskRunner* loadingTaskRunner() override;
  WebTaskRunner* timerTaskRunner() override;
  WebTaskRunner* unthrottledTaskRunner() override;
  WebViewScheduler* webViewScheduler() override;
  void didStartLoading(unsigned long identifier) override;
  void didStopLoading(unsigned long identifier) override;
  void setDocumentParsingInBackground(bool background_parser_active) override;

 private:
  friend class WebViewSchedulerImpl;

  void DetachFromWebViewScheduler();
  void RemoveTimerQueueFromBackgroundTimeBudgetPool();
  void ApplyPolicyToTimerQueue();
  bool ShouldThrottleTimers() const;
  void UpdateTimerThrottling(bool was_throttled);

  scoped_refptr<TaskQueue> loading_task_queue_;
  scoped_refptr<TaskQueue> timer_task_queue_;
  scoped_refptr<TaskQueue> unthrottled_task_queue_;
  std::unique_ptr<WebTaskRunnerImpl> loading_web_task_runner_;
  std::unique_ptr<WebTaskRunnerImpl> timer_web_task_runner_;
  std::unique_ptr<WebTaskRunnerImpl> unthrottled_web_task_runner_;
  RendererSchedulerImpl* renderer_scheduler_;        // NOT OWNED
  WebViewSchedulerImpl* parent_web_view_scheduler_;  // NOT OWNED
  base::trace_event::BlameContext* blame_context_;   // NOT OWNED
  bool frame_visible_;
  bool page_visible_;
  bool cross_origin_;

  DISALLOW_COPY_AND_ASSIGN(WebFrameSchedulerImpl);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_WEB_FRAME_SCHEDULER_IMPL_H_
