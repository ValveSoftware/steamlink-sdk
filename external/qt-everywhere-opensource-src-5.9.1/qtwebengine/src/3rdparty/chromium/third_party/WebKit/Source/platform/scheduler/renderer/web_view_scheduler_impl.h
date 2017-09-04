// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_WEB_VIEW_SCHEDULER_IMPL_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_WEB_VIEW_SCHEDULER_IMPL_H_

#include <memory>
#include <set>
#include <string>

#include "base/macros.h"
#include "public/platform/scheduler/base/task_queue.h"
#include "public/platform/WebCommon.h"
#include "public/platform/WebScheduler.h"
#include "public/platform/WebViewScheduler.h"
#include "platform/scheduler/renderer/task_queue_throttler.h"

namespace base {
namespace trace_event {
class BlameContext;
}  // namespace trace_event
}  // namespace base

namespace blink {
namespace scheduler {

class RendererSchedulerImpl;
class WebFrameSchedulerImpl;

class BLINK_PLATFORM_EXPORT WebViewSchedulerImpl : public WebViewScheduler {
 public:
  WebViewSchedulerImpl(
      WebScheduler::InterventionReporter* intervention_reporter,
      WebViewScheduler::WebViewSchedulerSettings* settings,
      RendererSchedulerImpl* renderer_scheduler,
      bool disable_background_timer_throttling);

  ~WebViewSchedulerImpl() override;

  // WebViewScheduler implementation:
  void setPageVisible(bool page_visible) override;
  std::unique_ptr<WebFrameScheduler> createFrameScheduler(
      BlameContext* blame_context) override;
  void enableVirtualTime() override;
  bool virtualTimeAllowedToAdvance() const override;
  void setVirtualTimePolicy(VirtualTimePolicy virtual_time_policy) override;
  void audioStateChanged(bool is_audio_playing) override;

  // Virtual for testing.
  virtual void ReportIntervention(const std::string& message);

  std::unique_ptr<WebFrameSchedulerImpl> createWebFrameSchedulerImpl(
      base::trace_event::BlameContext* blame_context);

  void DidStartLoading(unsigned long identifier);
  void DidStopLoading(unsigned long identifier);
  void IncrementBackgroundParserCount();
  void DecrementBackgroundParserCount();
  void Unregister(WebFrameSchedulerImpl* frame_scheduler);
  void OnNavigation();

  bool IsAudioPlaying() const;

 private:
  friend class WebFrameSchedulerImpl;

  TaskQueueThrottler::TimeBudgetPool* BackgroundTimeBudgetPool();
  void MaybeInitializeBackgroundTimeBudgetPool();

  void setAllowVirtualTimeToAdvance(bool allow_virtual_time_to_advance);
  void ApplyVirtualTimePolicy();

  void OnThrottlingReported(base::TimeDelta throttling_duration);

  // Depending on page visibility, either turns budget throttling off, or
  // schedules a call to enable it after a grace period.
  void UpdateBackgroundBudgetThrottlingState();

  void EnableBackgroundBudgetThrottling();

  std::set<WebFrameSchedulerImpl*> frame_schedulers_;
  std::set<unsigned long> pending_loads_;
  WebScheduler::InterventionReporter* intervention_reporter_;  // Not owned.
  RendererSchedulerImpl* renderer_scheduler_;
  VirtualTimePolicy virtual_time_policy_;
  int background_parser_count_;
  bool page_visible_;
  bool disable_background_timer_throttling_;
  bool allow_virtual_time_to_advance_;
  bool have_seen_loading_task_;
  bool virtual_time_;
  bool is_audio_playing_;
  bool reported_background_throttling_since_navigation_;
  TaskQueueThrottler::TimeBudgetPool*
      background_time_budget_pool_;  // Not owned.
  CancelableClosureHolder delayed_background_budget_throttling_enabler_;
  WebViewScheduler::WebViewSchedulerSettings* settings_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(WebViewSchedulerImpl);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_RENDERER_WEB_VIEW_SCHEDULER_IMPL_H_
