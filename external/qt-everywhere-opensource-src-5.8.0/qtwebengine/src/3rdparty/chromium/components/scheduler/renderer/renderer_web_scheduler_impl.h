// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_RENDERER_RENDERER_WEB_SCHEDULER_IMPL_H_
#define COMPONENTS_SCHEDULER_RENDERER_RENDERER_WEB_SCHEDULER_IMPL_H_

#include "components/scheduler/child/web_scheduler_impl.h"

namespace scheduler {

class RendererSchedulerImpl;

class SCHEDULER_EXPORT RendererWebSchedulerImpl : public WebSchedulerImpl {
 public:
  explicit RendererWebSchedulerImpl(RendererSchedulerImpl* renderer_scheduler);

  ~RendererWebSchedulerImpl() override;

  // blink::WebScheduler implementation:
  void suspendTimerQueue() override;
  void resumeTimerQueue() override;
  std::unique_ptr<blink::WebViewScheduler> createWebViewScheduler(
      blink::WebView* web_view) override;
  void onNavigationStarted() override;

 private:
  RendererSchedulerImpl* renderer_scheduler_;  // NOT OWNED
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_RENDERER_RENDERER_WEB_SCHEDULER_IMPL_H_
