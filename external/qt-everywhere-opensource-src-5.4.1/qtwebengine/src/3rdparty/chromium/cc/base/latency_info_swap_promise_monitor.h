// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "cc/base/swap_promise_monitor.h"

#ifndef CC_BASE_LATENCY_INFO_SWAP_PROMISE_MONITOR_H_
#define CC_BASE_LATENCY_INFO_SWAP_PROMISE_MONITOR_H_

namespace ui {
struct LatencyInfo;
}  // namespace ui

namespace cc {

// A LatencyInfoSwapPromiseMonitor queues a LatencyInfoSwapPromise into
// LayerTreeHost or LayerTreeHostImpl if there is compositor state change
// while it is being mointored.
class CC_EXPORT LatencyInfoSwapPromiseMonitor : public SwapPromiseMonitor {
 public:
  LatencyInfoSwapPromiseMonitor(ui::LatencyInfo* latency,
                                LayerTreeHost* layer_tree_host,
                                LayerTreeHostImpl* layer_tree_host_impl);
  virtual ~LatencyInfoSwapPromiseMonitor();

  virtual void OnSetNeedsCommitOnMain() OVERRIDE;
  virtual void OnSetNeedsRedrawOnImpl() OVERRIDE;

 private:
  ui::LatencyInfo* latency_;
};

}  // namespace cc

#endif  // CC_BASE_LATENCY_INFO_SWAP_PROMISE_MONITOR_H_
