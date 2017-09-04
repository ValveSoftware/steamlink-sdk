// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/renderer/web_frame_scheduler_impl.h"

#include "base/trace_event/blame_context.h"
#include "platform/scheduler/base/real_time_domain.h"
#include "platform/scheduler/base/virtual_time_domain.h"
#include "platform/scheduler/child/web_task_runner_impl.h"
#include "platform/scheduler/renderer/auto_advancing_virtual_time_domain.h"
#include "platform/scheduler/renderer/renderer_scheduler_impl.h"
#include "platform/scheduler/renderer/web_view_scheduler_impl.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "public/platform/BlameContext.h"
#include "public/platform/WebString.h"

namespace blink {
namespace scheduler {

WebFrameSchedulerImpl::WebFrameSchedulerImpl(
    RendererSchedulerImpl* renderer_scheduler,
    WebViewSchedulerImpl* parent_web_view_scheduler,
    base::trace_event::BlameContext* blame_context)
    : renderer_scheduler_(renderer_scheduler),
      parent_web_view_scheduler_(parent_web_view_scheduler),
      blame_context_(blame_context),
      frame_visible_(true),
      page_visible_(true),
      cross_origin_(false) {}

WebFrameSchedulerImpl::~WebFrameSchedulerImpl() {
  if (loading_task_queue_) {
    loading_task_queue_->UnregisterTaskQueue();
    loading_task_queue_->SetBlameContext(nullptr);
  }

  if (timer_task_queue_) {
    RemoveTimerQueueFromBackgroundTimeBudgetPool();
    timer_task_queue_->UnregisterTaskQueue();
    timer_task_queue_->SetBlameContext(nullptr);
  }

  if (unthrottled_task_queue_) {
    unthrottled_task_queue_->UnregisterTaskQueue();
    unthrottled_task_queue_->SetBlameContext(nullptr);
  }

  if (parent_web_view_scheduler_)
    parent_web_view_scheduler_->Unregister(this);
}

void WebFrameSchedulerImpl::DetachFromWebViewScheduler() {
  RemoveTimerQueueFromBackgroundTimeBudgetPool();

  parent_web_view_scheduler_ = nullptr;
}

void WebFrameSchedulerImpl::RemoveTimerQueueFromBackgroundTimeBudgetPool() {
  if (!timer_task_queue_)
    return;

  if (!parent_web_view_scheduler_)
    return;

  TaskQueueThrottler::TimeBudgetPool* time_budget_pool =
      parent_web_view_scheduler_->BackgroundTimeBudgetPool();

  if (!time_budget_pool)
    return;

  time_budget_pool->RemoveQueue(renderer_scheduler_->tick_clock()->NowTicks(),
                                timer_task_queue_.get());
}

void WebFrameSchedulerImpl::setFrameVisible(bool frame_visible) {
  DCHECK(parent_web_view_scheduler_);
  if (frame_visible_ == frame_visible)
    return;
  bool was_throttled = ShouldThrottleTimers();
  frame_visible_ = frame_visible;
  UpdateTimerThrottling(was_throttled);
}

void WebFrameSchedulerImpl::setCrossOrigin(bool cross_origin) {
  DCHECK(parent_web_view_scheduler_);
  if (cross_origin_ == cross_origin)
    return;
  bool was_throttled = ShouldThrottleTimers();
  cross_origin_ = cross_origin;
  UpdateTimerThrottling(was_throttled);
}

blink::WebTaskRunner* WebFrameSchedulerImpl::loadingTaskRunner() {
  DCHECK(parent_web_view_scheduler_);
  if (!loading_web_task_runner_) {
    loading_task_queue_ = renderer_scheduler_->NewLoadingTaskRunner(
        TaskQueue::QueueType::FRAME_LOADING);
    loading_task_queue_->SetBlameContext(blame_context_);
    loading_web_task_runner_.reset(new WebTaskRunnerImpl(loading_task_queue_));
  }
  return loading_web_task_runner_.get();
}

blink::WebTaskRunner* WebFrameSchedulerImpl::timerTaskRunner() {
  DCHECK(parent_web_view_scheduler_);
  if (!timer_web_task_runner_) {
    timer_task_queue_ = renderer_scheduler_->NewTimerTaskRunner(
        TaskQueue::QueueType::FRAME_TIMER);
    timer_task_queue_->SetBlameContext(blame_context_);

    TaskQueueThrottler::TimeBudgetPool* time_budget_pool =
        parent_web_view_scheduler_->BackgroundTimeBudgetPool();
    if (time_budget_pool) {
      time_budget_pool->AddQueue(renderer_scheduler_->tick_clock()->NowTicks(),
                                 timer_task_queue_.get());
    }

    if (ShouldThrottleTimers()) {
      renderer_scheduler_->task_queue_throttler()->IncreaseThrottleRefCount(
          timer_task_queue_.get());
    }
    timer_web_task_runner_.reset(new WebTaskRunnerImpl(timer_task_queue_));
  }
  return timer_web_task_runner_.get();
}

blink::WebTaskRunner* WebFrameSchedulerImpl::unthrottledTaskRunner() {
  DCHECK(parent_web_view_scheduler_);
  if (!unthrottled_web_task_runner_) {
    unthrottled_task_queue_ = renderer_scheduler_->NewUnthrottledTaskRunner(
        TaskQueue::QueueType::FRAME_UNTHROTTLED);
    unthrottled_task_queue_->SetBlameContext(blame_context_);
    unthrottled_web_task_runner_.reset(
        new WebTaskRunnerImpl(unthrottled_task_queue_));
  }
  return unthrottled_web_task_runner_.get();
}

blink::WebViewScheduler* WebFrameSchedulerImpl::webViewScheduler() {
  return parent_web_view_scheduler_;
}

void WebFrameSchedulerImpl::didStartLoading(unsigned long identifier) {
  if (parent_web_view_scheduler_)
    parent_web_view_scheduler_->DidStartLoading(identifier);
}

void WebFrameSchedulerImpl::didStopLoading(unsigned long identifier) {
  if (parent_web_view_scheduler_)
    parent_web_view_scheduler_->DidStopLoading(identifier);
}

void WebFrameSchedulerImpl::setDocumentParsingInBackground(
    bool background_parser_active) {
  if (background_parser_active)
    parent_web_view_scheduler_->IncrementBackgroundParserCount();
  else
    parent_web_view_scheduler_->DecrementBackgroundParserCount();
}

void WebFrameSchedulerImpl::setPageVisible(bool page_visible) {
  DCHECK(parent_web_view_scheduler_);
  if (page_visible_ == page_visible)
    return;
  bool was_throttled = ShouldThrottleTimers();
  page_visible_ = page_visible;
  UpdateTimerThrottling(was_throttled);
}

bool WebFrameSchedulerImpl::ShouldThrottleTimers() const {
  if (!page_visible_)
    return true;
  return RuntimeEnabledFeatures::timerThrottlingForHiddenFramesEnabled() &&
         !frame_visible_ && cross_origin_;
}

void WebFrameSchedulerImpl::UpdateTimerThrottling(bool was_throttled) {
  bool should_throttle = ShouldThrottleTimers();
  if (was_throttled == should_throttle || !timer_web_task_runner_)
    return;
  if (should_throttle) {
    renderer_scheduler_->task_queue_throttler()->IncreaseThrottleRefCount(
        timer_task_queue_.get());
  } else {
    renderer_scheduler_->task_queue_throttler()->DecreaseThrottleRefCount(
        timer_task_queue_.get());
  }
}

}  // namespace scheduler
}  // namespace blink
