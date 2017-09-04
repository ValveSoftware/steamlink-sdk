// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/swap_promise_manager.h"
#include "cc/trees/swap_promise_monitor.h"

namespace cc {

SwapPromiseMonitor::SwapPromiseMonitor(SwapPromiseManager* swap_promise_manager,
                                       LayerTreeHostImpl* layer_tree_host_impl)
    : swap_promise_manager_(swap_promise_manager),
      layer_tree_host_impl_(layer_tree_host_impl) {
  DCHECK((swap_promise_manager && !layer_tree_host_impl) ||
         (!swap_promise_manager && layer_tree_host_impl));
  if (swap_promise_manager)
    swap_promise_manager->InsertSwapPromiseMonitor(this);
  if (layer_tree_host_impl_)
    layer_tree_host_impl_->InsertSwapPromiseMonitor(this);
}

SwapPromiseMonitor::~SwapPromiseMonitor() {
  if (swap_promise_manager_)
    swap_promise_manager_->RemoveSwapPromiseMonitor(this);
  if (layer_tree_host_impl_)
    layer_tree_host_impl_->RemoveSwapPromiseMonitor(this);
}

}  // namespace cc
