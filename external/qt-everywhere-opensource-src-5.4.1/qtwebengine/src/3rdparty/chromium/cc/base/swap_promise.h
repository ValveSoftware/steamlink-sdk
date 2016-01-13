// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BASE_SWAP_PROMISE_H_
#define CC_BASE_SWAP_PROMISE_H_

#include "cc/output/compositor_frame_metadata.h"

namespace cc {

const unsigned int kMaxQueuedSwapPromiseNumber = 100;

// When a change to the compositor's state/invalidation/whatever happens, a
// Swap Promise can be inserted into LayerTreeHost/LayerTreeImpl, to track
// whether the compositor's reply to the new state/invaliadtion/whatever is
// completed in the compositor, i.e. the compositor knows it has been sent
// to its output or not.
//
// If the new compositor state is sent to the output, SwapPromise::DidSwap()
// will be called, and if the compositor fails to send its new state to the
// output, SwapPromise::DidNotSwap() will be called.
//
// Client wishes to use SwapPromise should have a subclass that defines
// the behavior of DidSwap() and DidNotSwap(). Notice that the promise can
// be broken at either main or impl thread, e.g. commit fails on main thread,
// new frame data has no actual damage so LayerTreeHostImpl::SwapBuffers()
// bails out early on impl thread, so don't assume that DidSwap() and
// DidNotSwap() are called at a particular thread. It is better to let the
// subclass carry thread-safe member data and operate on that member data in
// DidSwap() and DidNotSwap().
class CC_EXPORT SwapPromise {
 public:
  enum DidNotSwapReason {
    DID_NOT_SWAP_UNKNOWN,
    SWAP_FAILS,
    COMMIT_FAILS,
    SWAP_PROMISE_LIST_OVERFLOW,
  };

  SwapPromise() {}
  virtual ~SwapPromise() {}

  virtual void DidSwap(CompositorFrameMetadata* metadata) = 0;
  virtual void DidNotSwap(DidNotSwapReason reason) = 0;
};

}  // namespace cc

#endif  // CC_BASE_SWAP_PROMISE_H_
