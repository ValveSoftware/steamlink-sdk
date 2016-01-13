// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BASE_LATENCY_INFO_SWAP_PROMISE_H_
#define CC_BASE_LATENCY_INFO_SWAP_PROMISE_H_

#include "base/compiler_specific.h"
#include "cc/base/swap_promise.h"
#include "ui/events/latency_info.h"

namespace cc {

class CC_EXPORT LatencyInfoSwapPromise : public SwapPromise {
 public:
  explicit LatencyInfoSwapPromise(const ui::LatencyInfo& latency_info);
  virtual ~LatencyInfoSwapPromise();

  virtual void DidSwap(CompositorFrameMetadata* metadata) OVERRIDE;
  virtual void DidNotSwap(DidNotSwapReason reason) OVERRIDE;

 private:
  ui::LatencyInfo latency_;
};

}  // namespace cc

#endif  // CC_BASE_LATENCY_INFO_SWAP_PROMISE_H_
