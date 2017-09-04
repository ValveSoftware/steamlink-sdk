// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_CHANNEL_MAIN_H_
#define CC_TREES_CHANNEL_MAIN_H_

#include "cc/base/cc_export.h"
#include "cc/base/completion_event.h"
#include "cc/input/browser_controls_state.h"
#include "cc/output/compositor_frame_sink.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/scheduler/commit_earlyout_reason.h"
#include "cc/trees/proxy_common.h"

namespace cc {
class LayerTreeHostInProcess;
class LayerTreeMutator;

// ChannelMain and ChannelImpl provide an abstract communication layer for
// the main and impl side of the compositor.
//
// The communication sequence between the 2 sides is:
//
// LayerTreeHostInProcess<-->ProxyMain<-->ChannelMain
//                                      |
//                                      |
//                               ChannelImpl<-->ProxyImpl<-->LayerTreeHostImpl

class CC_EXPORT ChannelMain {
 public:
  virtual ~ChannelMain() {}

  // Interface for commands sent to ProxyImpl
  virtual void UpdateBrowserControlsStateOnImpl(
      BrowserControlsState constraints,
      BrowserControlsState current,
      bool animate) = 0;
  virtual void InitializeCompositorFrameSinkOnImpl(
      CompositorFrameSink* compositor_frame_sink) = 0;
  virtual void InitializeMutatorOnImpl(
      std::unique_ptr<LayerTreeMutator> mutator) = 0;
  virtual void MainThreadHasStoppedFlingingOnImpl() = 0;
  virtual void SetInputThrottledUntilCommitOnImpl(bool is_throttled) = 0;
  virtual void SetDeferCommitsOnImpl(bool defer_commits) = 0;
  virtual void SetVisibleOnImpl(bool visible) = 0;
  virtual void ReleaseCompositorFrameSinkOnImpl(
      CompletionEvent* completion) = 0;
  virtual void MainFrameWillHappenOnImplForTesting(
      CompletionEvent* completion,
      bool* main_frame_will_happen) = 0;
  virtual void SetNeedsRedrawOnImpl(const gfx::Rect& damage_rect) = 0;
  virtual void SetNeedsCommitOnImpl() = 0;
  virtual void BeginMainFrameAbortedOnImpl(
      CommitEarlyOutReason reason,
      base::TimeTicks main_thread_start_time,
      std::vector<std::unique_ptr<SwapPromise>> swap_promises) = 0;
  virtual void NotifyReadyToCommitOnImpl(
      CompletionEvent* completion,
      LayerTreeHostInProcess* layer_tree_host,
      base::TimeTicks main_thread_start_time,
      bool hold_commit_for_activation) = 0;

  // Must be called before using the channel.
  virtual void SynchronouslyInitializeImpl(
      LayerTreeHostInProcess* layer_tree_host) = 0;

  // Must be called before deleting the channel.
  virtual void SynchronouslyCloseImpl() = 0;
};

}  // namespace cc

#endif  // CC_TREES_CHANNEL_MAIN_H_
