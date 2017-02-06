// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/renderer/renderer_web_scheduler_impl.h"

#include <memory>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "components/scheduler/base/task_queue.h"
#include "components/scheduler/common/scheduler_switches.h"
#include "components/scheduler/renderer/renderer_scheduler_impl.h"
#include "components/scheduler/renderer/web_view_scheduler_impl.h"

namespace scheduler {

RendererWebSchedulerImpl::RendererWebSchedulerImpl(
    RendererSchedulerImpl* renderer_scheduler)
    : WebSchedulerImpl(renderer_scheduler,
                       renderer_scheduler->IdleTaskRunner(),
                       renderer_scheduler->LoadingTaskRunner(),
                       renderer_scheduler->TimerTaskRunner()),
      renderer_scheduler_(renderer_scheduler) {}

RendererWebSchedulerImpl::~RendererWebSchedulerImpl() {
}

void RendererWebSchedulerImpl::suspendTimerQueue() {
  renderer_scheduler_->SuspendTimerQueue();
}

void RendererWebSchedulerImpl::resumeTimerQueue() {
  renderer_scheduler_->ResumeTimerQueue();
}

std::unique_ptr<blink::WebViewScheduler>
RendererWebSchedulerImpl::createWebViewScheduler(blink::WebView* web_view) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return base::WrapUnique(new WebViewSchedulerImpl(
      web_view, renderer_scheduler_,
      command_line->HasSwitch(switches::kDisableBackgroundTimerThrottling)));
}

void RendererWebSchedulerImpl::onNavigationStarted() {
  renderer_scheduler_->OnNavigationStarted();
}

}  // namespace scheduler
