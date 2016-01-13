// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/latency_info_swap_promise.h"

#include "base/logging.h"

namespace {
  ui::LatencyComponentType DidNotSwapReasonToLatencyComponentType(
      cc::SwapPromise::DidNotSwapReason reason) {
    switch (reason) {
      case cc::SwapPromise::DID_NOT_SWAP_UNKNOWN:
      case cc::SwapPromise::SWAP_FAILS:
        return ui::INPUT_EVENT_LATENCY_TERMINATED_SWAP_FAILED_COMPONENT;
      case cc::SwapPromise::COMMIT_FAILS:
        return ui::INPUT_EVENT_LATENCY_TERMINATED_COMMIT_FAILED_COMPONENT;
      case cc::SwapPromise::SWAP_PROMISE_LIST_OVERFLOW:
        return ui::LATENCY_INFO_LIST_TERMINATED_OVERFLOW_COMPONENT;
    }
    NOTREACHED() << "Unhandled DidNotSwapReason.";
    return ui::INPUT_EVENT_LATENCY_TERMINATED_SWAP_FAILED_COMPONENT;
  }
}  // namespace

namespace cc {

LatencyInfoSwapPromise::LatencyInfoSwapPromise(const ui::LatencyInfo& latency)
    : latency_(latency) {
}

LatencyInfoSwapPromise::~LatencyInfoSwapPromise() {
}

void LatencyInfoSwapPromise::DidSwap(CompositorFrameMetadata* metadata) {
  DCHECK(!latency_.terminated);
  metadata->latency_info.push_back(latency_);
}

void LatencyInfoSwapPromise::DidNotSwap(DidNotSwapReason reason) {
  latency_.AddLatencyNumber(DidNotSwapReasonToLatencyComponentType(reason),
                            0, 0);
  // TODO(miletus): Turn this back on once per-event LatencyInfo tracking
  // is enabled in GPU side.
  // DCHECK(latency_.terminated);
}

}  // namespace cc
