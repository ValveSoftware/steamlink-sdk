// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_RENDERER_PAGE_LOAD_METRICS_RENDER_FRAME_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_RENDERER_PAGE_LOAD_METRICS_RENDER_FRAME_OBSERVER_H_

#include <memory>

#include "base/macros.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "content/public/renderer/render_frame_observer.h"
#include "third_party/WebKit/public/platform/WebLoadingBehaviorFlag.h"

namespace base {
class Timer;
}  // namespace base

namespace page_load_metrics {

class PageTimingMetricsSender;

// MetricsRenderFrameObserver observes RenderFrame notifications, and
// sends page load timing information to the browser process over IPC.
class MetricsRenderFrameObserver : public content::RenderFrameObserver {
 public:
  explicit MetricsRenderFrameObserver(content::RenderFrame* render_frame);
  ~MetricsRenderFrameObserver() override;

  // RenderFrameObserver implementation
  void DidChangePerformanceTiming() override;
  void DidObserveLoadingBehavior(
      blink::WebLoadingBehaviorFlag behavior) override;
  void DidCommitProvisionalLoad(bool is_new_navigation,
                                bool is_same_page_navigation) override;
  void OnDestruct() override;

 private:
  // Will be null when we're not actively sending metrics.
  std::unique_ptr<PageTimingMetricsSender> page_timing_metrics_sender_;

  void SendMetrics();
  virtual bool ShouldSendMetrics() const;
  virtual PageLoadTiming GetTiming() const;
  virtual std::unique_ptr<base::Timer> CreateTimer() const;
  virtual bool HasNoRenderFrame() const;

  DISALLOW_COPY_AND_ASSIGN(MetricsRenderFrameObserver);
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_RENDERER_PAGE_LOAD_METRICS_RENDER_FRAME_OBSERVER_H_
