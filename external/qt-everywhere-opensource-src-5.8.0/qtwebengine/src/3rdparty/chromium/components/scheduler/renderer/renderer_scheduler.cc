// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/renderer/renderer_scheduler.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_impl.h"
#include "components/scheduler/child/scheduler_tqm_delegate_impl.h"
#include "components/scheduler/common/scheduler_switches.h"
#include "components/scheduler/renderer/renderer_scheduler_impl.h"

namespace scheduler {
namespace {
const base::Feature kExpensiveTaskBlockingPolicyFeature{
    "SchedulerExpensiveTaskBlocking", base::FEATURE_ENABLED_BY_DEFAULT};
}

RendererScheduler::RendererScheduler() {
}

RendererScheduler::~RendererScheduler() {
}

RendererScheduler::RAILModeObserver::~RAILModeObserver() = default;

// static
std::unique_ptr<RendererScheduler> RendererScheduler::Create() {
  // Ensure worker.scheduler, worker.scheduler.debug and
  // renderer.scheduler.debug appear as an option in about://tracing
  base::trace_event::TraceLog::GetCategoryGroupEnabled(
      TRACE_DISABLED_BY_DEFAULT("worker.scheduler"));
  base::trace_event::TraceLog::GetCategoryGroupEnabled(
      TRACE_DISABLED_BY_DEFAULT("worker.scheduler.debug"));
  base::trace_event::TraceLog::GetCategoryGroupEnabled(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler.debug"));

  base::MessageLoop* message_loop = base::MessageLoop::current();
  std::unique_ptr<RendererSchedulerImpl> scheduler(
      new RendererSchedulerImpl(SchedulerTqmDelegateImpl::Create(
          message_loop, base::WrapUnique(new base::DefaultTickClock()))));

  // Runtime features are not currently available in html_viewer.
  if (base::FeatureList::GetInstance()) {
    bool blocking_allowed =
        base::FeatureList::IsEnabled(kExpensiveTaskBlockingPolicyFeature);
    // Also check the old style FieldTrial API for perf waterfall compatibility.
    const std::string group_name = base::FieldTrialList::FindFullName(
        kExpensiveTaskBlockingPolicyFeature.name);
    blocking_allowed |= base::StartsWith(group_name, "Enabled",
                                         base::CompareCase::INSENSITIVE_ASCII);
    scheduler->SetExpensiveTaskBlockingAllowed(blocking_allowed);
  }
  return base::WrapUnique<RendererScheduler>(scheduler.release());
}

// static
const char* RendererScheduler::InputEventStateToString(
    InputEventState input_event_state) {
  switch (input_event_state) {
    case InputEventState::EVENT_CONSUMED_BY_COMPOSITOR:
      return "event_consumed_by_compositor";
    case InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD:
      return "event_forwarded_to_main_thread";
    default:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace scheduler
