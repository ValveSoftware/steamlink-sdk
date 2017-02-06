// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_RENDERER_PAGE_TIMING_METRICS_SENDER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_RENDERER_PAGE_TIMING_METRICS_SENDER_H_

#include <memory>

#include "base/macros.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "third_party/WebKit/public/platform/WebLoadingBehaviorFlag.h"

namespace base {
class Timer;
}  // namespace base

namespace IPC {
class Sender;
}  // namespace IPC

namespace page_load_metrics {

// PageTimingMetricsSender is responsible for sending page load timing metrics
// over IPC. PageTimingMetricsSender may coalesce sent IPCs in order to
// minimize IPC contention.
class PageTimingMetricsSender {
 public:
  PageTimingMetricsSender(IPC::Sender* ipc_sender,
                          int routing_id,
                          std::unique_ptr<base::Timer> timer,
                          const PageLoadTiming& initial_timing);
  ~PageTimingMetricsSender();

  void DidObserveLoadingBehavior(blink::WebLoadingBehaviorFlag behavior);
  void Send(const PageLoadTiming& timing);

 protected:
  base::Timer* timer() const { return timer_.get(); }

 private:
  void EnsureSendTimer(int delay);
  void SendNow();

  IPC::Sender* const ipc_sender_;
  const int routing_id_;
  std::unique_ptr<base::Timer> timer_;
  PageLoadTiming last_timing_;

  // The the sender keep track of metadata as it comes in, because the sender is
  // scoped to a single committed load.
  PageLoadMetadata metadata_;

  DISALLOW_COPY_AND_ASSIGN(PageTimingMetricsSender);
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_RENDERER_PAGE_TIMING_METRICS_SENDER_H_
