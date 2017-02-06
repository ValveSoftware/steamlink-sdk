// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_THREADED_CHANNEL_H_
#define CC_TREES_THREADED_CHANNEL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/trees/channel_impl.h"
#include "cc/trees/channel_main.h"
#include "cc/trees/proxy_common.h"
#include "cc/trees/proxy_impl.h"
#include "cc/trees/proxy_main.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace cc {
class ChannelImpl;
class ChannelMain;
class LayerTreeHost;
class ProxyImpl;
class ProxyMain;

// An implementation of ChannelMain and ChannelImpl that sends commands between
// ProxyMain and ProxyImpl across thread boundaries.
//
// LayerTreeHost creates ThreadedChannel and passes the ownership to ProxyMain.
// The object life cycle and communication across threads is as follows:
//
//
//           Main Thread              |               Impl Thread
//   LayerTreeHost->InitializeProxy   |
//               |                    |
//        ProxyMain->Start()          |
//               |              ThreadedChannel
// ---------------------------------------------------------------------------
//  ChannelMain::SynchronouslyInitializeImpl ---PostTask---> ThreadedChannel::
//                                                    InitializeImplOnImpl
//                                                          |
//                                                   ProxyImpl::Create
//                                                          |
//                                                   ProxyImpl->Initialize()
//                                                          .
//                                                          .
//                                          ProxyImpl::ScheduledActionBegin
//                                                     OutputSurfaceCreation
//                                                          |
//                                         ChannelImpl::RequestNewOutputSurface
// ----------------------------------------------------------------------------
//                                                          |
// ProxyMain->RequestNewOutputSurface()<----PostTask--------
//              .
//              .
// ProxyMain->Stop()
//              |
// ---------------------------------------------------------------------------
// ChannelMain::SynchronouslyCloseImpl ---PostTask---> ThreadedChannel::
//                                                      CloseImplOnImpl
// ----------------------------------------------------------------------------
//
// ThreadedChannel is created and destroyed on the main thread but can be
// called from main or impl thread. It is safe for the Threadedchannel to be
// called on the impl thread because:
// 1) The only impl-threaded callers of ThreadedChannel are the ThreadedChannel
// itself and ProxyImpl which is created and owned by the ThreadedChannel.
// 2) ThreadedChannel blocks the main thread in
// ThreadedChannel::SynchronouslyCloseImpl to wait for the impl-thread teardown
// to complete, so there is no risk of any queued tasks calling it on the impl
// thread after it has been deleted on the main thread.

class CC_EXPORT ThreadedChannel : public ChannelMain, public ChannelImpl {
 public:
  static std::unique_ptr<ThreadedChannel> Create(
      ProxyMain* proxy_main,
      TaskRunnerProvider* task_runner_provider);

  ~ThreadedChannel() override;

  // ChannelMain Implementation
  void UpdateTopControlsStateOnImpl(TopControlsState constraints,
                                    TopControlsState current,
                                    bool animate) override;
  void InitializeOutputSurfaceOnImpl(OutputSurface* output_surface) override;
  void InitializeMutatorOnImpl(
      std::unique_ptr<LayerTreeMutator> mutator) override;
  void MainThreadHasStoppedFlingingOnImpl() override;
  void SetInputThrottledUntilCommitOnImpl(bool is_throttled) override;
  void SetDeferCommitsOnImpl(bool defer_commits) override;
  void SetNeedsCommitOnImpl() override;
  void BeginMainFrameAbortedOnImpl(
      CommitEarlyOutReason reason,
      base::TimeTicks main_thread_start_time) override;
  void SetNeedsRedrawOnImpl(const gfx::Rect& damage_rect) override;
  void SetVisibleOnImpl(bool visible) override;

  // Blocking calls to ProxyImpl
  void FinishAllRenderingOnImpl(CompletionEvent* completion) override;
  void ReleaseOutputSurfaceOnImpl(CompletionEvent* completion) override;
  void MainFrameWillHappenOnImplForTesting(
      CompletionEvent* completion,
      bool* main_frame_will_happen) override;
  void StartCommitOnImpl(CompletionEvent* completion,
                         LayerTreeHost* layer_tree_host,
                         base::TimeTicks main_thread_start_time,
                         bool hold_commit_for_activation) override;
  void SynchronouslyInitializeImpl(
      LayerTreeHost* layer_tree_host,
      std::unique_ptr<BeginFrameSource> external_begin_frame_source) override;
  void SynchronouslyCloseImpl() override;

  // ChannelImpl Implementation
  void DidCompleteSwapBuffers() override;
  void SetRendererCapabilitiesMainCopy(
      const RendererCapabilities& capabilities) override;
  void BeginMainFrameNotExpectedSoon() override;
  void DidCommitAndDrawFrame() override;
  void SetAnimationEvents(std::unique_ptr<AnimationEvents> events) override;
  void DidLoseOutputSurface() override;
  void RequestNewOutputSurface() override;
  void DidInitializeOutputSurface(
      bool success,
      const RendererCapabilities& capabilities) override;
  void DidCompletePageScaleAnimation() override;
  void BeginMainFrame(std::unique_ptr<BeginMainFrameAndCommitState>
                          begin_main_frame_state) override;

 protected:
  ThreadedChannel(ProxyMain* proxy_main,
                  TaskRunnerProvider* task_runner_provider);

  // Virtual for testing.
  virtual std::unique_ptr<ProxyImpl> CreateProxyImpl(
      ChannelImpl* channel_impl,
      LayerTreeHost* layer_tree_host,
      TaskRunnerProvider* task_runner_provider,
      std::unique_ptr<BeginFrameSource> external_begin_frame_source);

 private:
  // The members of this struct should be accessed on the main thread only.
  struct MainThreadOnly {
    explicit MainThreadOnly(ProxyMain* proxy_main);
    ~MainThreadOnly();

    base::WeakPtrFactory<ProxyMain> proxy_main_weak_factory;
    bool initialized;
  };

  // The members of this struct should be accessed on the impl thread only.
  struct CompositorThreadOnly {
    explicit CompositorThreadOnly(base::WeakPtr<ProxyMain> proxy_main_weak_ptr);
    ~CompositorThreadOnly();

    std::unique_ptr<ProxyImpl> proxy_impl;

    // We use a unique_ptr for the weak ptr factory here since the factory is
    // created after ProxyImpl is created in InitializeImplOnImpl. Since the
    // weak ptrs are needed only by the ThreadedChannel to safely post tasks on
    // ProxyImpl to be run on the impl thread, we avoid creating it in ProxyImpl
    // and ensure that it is destroyed before ProxyImpl during the impl-thread
    // tear down in CloseImplOnImpl.
    std::unique_ptr<base::WeakPtrFactory<ProxyImpl>> proxy_impl_weak_factory;

    // Used on the impl thread to queue calls to ProxyMain to be run on the main
    // thread. Since the weak pointer is invalidated after the impl-thread tear
    // down in SynchronouslyCloseImpl, this ensures that any tasks posted to
    // ProxyMain from the impl thread are abandoned after the impl side has been
    // destroyed.
    base::WeakPtr<ProxyMain> proxy_main_weak_ptr;
  };

  // Called on impl thread.
  void InitializeImplOnImpl(
      CompletionEvent* completion,
      LayerTreeHost* layer_tree_host,
      std::unique_ptr<BeginFrameSource> external_begin_frame_source);
  void CloseImplOnImpl(CompletionEvent* completion);

  bool IsInitialized() const;

  base::SingleThreadTaskRunner* MainThreadTaskRunner() const;
  base::SingleThreadTaskRunner* ImplThreadTaskRunner() const;
  bool IsMainThread() const;
  bool IsImplThread() const;

  TaskRunnerProvider* task_runner_provider_;

  MainThreadOnly& main();
  const MainThreadOnly& main() const;

  CompositorThreadOnly& impl();
  const CompositorThreadOnly& impl() const;

  // Use accessors instead of this variable directly.
  MainThreadOnly main_thread_only_vars_unsafe_;

  // Use accessors instead of this variable directly.
  CompositorThreadOnly compositor_thread_vars_unsafe_;

  // Used on the main thread to safely queue calls to ProxyImpl to be run on the
  // impl thread.
  base::WeakPtr<ProxyImpl> proxy_impl_weak_ptr_;

  DISALLOW_COPY_AND_ASSIGN(ThreadedChannel);
};

}  // namespace cc

#endif  // CC_TREES_THREADED_CHANNEL_H_
