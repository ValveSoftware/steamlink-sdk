// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_SINGLE_THREAD_PROXY_H_
#define CC_TREES_SINGLE_THREAD_PROXY_H_

#include <limits>

#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "cc/output/begin_frame_args.h"
#include "cc/scheduler/scheduler.h"
#include "cc/trees/blocking_task_runner.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/proxy.h"
#include "cc/trees/task_runner_provider.h"

namespace cc {

class AnimationEvents;
class BeginFrameSource;
class ContextProvider;
class LayerTreeHost;
class LayerTreeHostSingleThreadClient;

class CC_EXPORT SingleThreadProxy : public Proxy,
                                    NON_EXPORTED_BASE(LayerTreeHostImplClient),
                                    SchedulerClient {
 public:
  static std::unique_ptr<Proxy> Create(
      LayerTreeHost* layer_tree_host,
      LayerTreeHostSingleThreadClient* client,
      TaskRunnerProvider* task_runner_provider_);
  ~SingleThreadProxy() override;

  // Proxy implementation
  void FinishAllRendering() override;
  bool IsStarted() const override;
  bool CommitToActiveTree() const override;
  void SetOutputSurface(OutputSurface* output_surface) override;
  void ReleaseOutputSurface() override;
  void SetVisible(bool visible) override;
  const RendererCapabilities& GetRendererCapabilities() const override;
  void SetNeedsAnimate() override;
  void SetNeedsUpdateLayers() override;
  void SetNeedsCommit() override;
  void SetNeedsRedraw(const gfx::Rect& damage_rect) override;
  void SetNextCommitWaitsForActivation() override;
  void NotifyInputThrottledUntilCommit() override {}
  void SetDeferCommits(bool defer_commits) override;
  bool CommitRequested() const override;
  bool BeginMainFrameRequested() const override;
  void MainThreadHasStoppedFlinging() override {}
  void Start(
      std::unique_ptr<BeginFrameSource> external_begin_frame_source) override;
  void Stop() override;
  void SetMutator(std::unique_ptr<LayerTreeMutator> mutator) override;
  bool SupportsImplScrolling() const override;
  bool MainFrameWillHappenForTesting() override;
  void UpdateTopControlsState(TopControlsState constraints,
                              TopControlsState current,
                              bool animate) override;

  // SchedulerClient implementation
  void WillBeginImplFrame(const BeginFrameArgs& args) override;
  void DidFinishImplFrame() override;
  void ScheduledActionSendBeginMainFrame(const BeginFrameArgs& args) override;
  DrawResult ScheduledActionDrawAndSwapIfPossible() override;
  DrawResult ScheduledActionDrawAndSwapForced() override;
  void ScheduledActionCommit() override;
  void ScheduledActionActivateSyncTree() override;
  void ScheduledActionBeginOutputSurfaceCreation() override;
  void ScheduledActionPrepareTiles() override;
  void ScheduledActionInvalidateOutputSurface() override;
  void SendBeginMainFrameNotExpectedSoon() override;

  // LayerTreeHostImplClient implementation
  void UpdateRendererCapabilitiesOnImplThread() override;
  void DidLoseOutputSurfaceOnImplThread() override;
  void CommitVSyncParameters(base::TimeTicks timebase,
                             base::TimeDelta interval) override;
  void SetBeginFrameSource(BeginFrameSource* source) override;
  void SetEstimatedParentDrawTime(base::TimeDelta draw_time) override;
  void DidSwapBuffersOnImplThread() override;
  void DidSwapBuffersCompleteOnImplThread() override;
  void OnCanDrawStateChanged(bool can_draw) override;
  void NotifyReadyToActivate() override;
  void NotifyReadyToDraw() override;
  void SetNeedsRedrawOnImplThread() override;
  void SetNeedsRedrawRectOnImplThread(const gfx::Rect& dirty_rect) override;
  void SetNeedsOneBeginImplFrameOnImplThread() override;
  void SetNeedsPrepareTilesOnImplThread() override;
  void SetNeedsCommitOnImplThread() override;
  void SetVideoNeedsBeginFrames(bool needs_begin_frames) override;
  void PostAnimationEventsToMainThreadOnImplThread(
      std::unique_ptr<AnimationEvents> events) override;
  bool IsInsideDraw() override;
  void RenewTreePriority() override {}
  void PostDelayedAnimationTaskOnImplThread(const base::Closure& task,
                                            base::TimeDelta delay) override {}
  void DidActivateSyncTree() override;
  void WillPrepareTiles() override;
  void DidPrepareTiles() override;
  void DidCompletePageScaleAnimationOnImplThread() override;
  void OnDrawForOutputSurface(bool resourceless_software_draw) override;

  void RequestNewOutputSurface();

  // Called by the legacy path where RenderWidget does the scheduling.
  void CompositeImmediately(base::TimeTicks frame_begin_time);

 protected:
  SingleThreadProxy(LayerTreeHost* layer_tree_host,
                    LayerTreeHostSingleThreadClient* client,
                    TaskRunnerProvider* task_runner_provider);

 private:
  void BeginMainFrame(const BeginFrameArgs& begin_frame_args);
  void BeginMainFrameAbortedOnImplThread(CommitEarlyOutReason reason);
  void DoBeginMainFrame(const BeginFrameArgs& begin_frame_args);
  void DoCommit();
  DrawResult DoComposite(LayerTreeHostImpl::FrameData* frame);
  void DoSwap();
  void DidCommitAndDrawFrame();
  void CommitComplete();

  bool ShouldComposite() const;
  void ScheduleRequestNewOutputSurface();

  // Accessed on main thread only.
  LayerTreeHost* layer_tree_host_;
  LayerTreeHostSingleThreadClient* client_;

  TaskRunnerProvider* task_runner_provider_;

  // Used on the Thread, but checked on main thread during
  // initialization/shutdown.
  std::unique_ptr<LayerTreeHostImpl> layer_tree_host_impl_;
  RendererCapabilities renderer_capabilities_for_main_thread_;

  // Accessed from both threads.
  std::unique_ptr<BeginFrameSource> external_begin_frame_source_;
  std::unique_ptr<BeginFrameSource> unthrottled_begin_frame_source_;
  std::unique_ptr<SyntheticBeginFrameSource> synthetic_begin_frame_source_;
  std::unique_ptr<Scheduler> scheduler_on_impl_thread_;

  std::unique_ptr<BlockingTaskRunner::CapturePostTasks>
      commit_blocking_task_runner_;
  bool next_frame_is_newly_committed_frame_;

#if DCHECK_IS_ON()
  bool inside_impl_frame_;
#endif
  bool inside_draw_;
  bool defer_commits_;
  bool animate_requested_;
  bool commit_requested_;
  bool inside_synchronous_composite_;

  // True if a request to the LayerTreeHostClient to create an output surface
  // is still outstanding.
  bool output_surface_creation_requested_;

  // This is the callback for the scheduled RequestNewOutputSurface.
  base::CancelableClosure output_surface_creation_callback_;

  base::WeakPtrFactory<SingleThreadProxy> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SingleThreadProxy);
};

// For use in the single-threaded case. In debug builds, it pretends that the
// code is running on the impl thread to satisfy assertion checks.
class DebugScopedSetImplThread {
 public:
#if DCHECK_IS_ON()
  explicit DebugScopedSetImplThread(TaskRunnerProvider* task_runner_provider)
      : task_runner_provider_(task_runner_provider) {
    previous_value_ = task_runner_provider_->impl_thread_is_overridden_;
    task_runner_provider_->SetCurrentThreadIsImplThread(true);
  }
#else
  explicit DebugScopedSetImplThread(TaskRunnerProvider* task_runner_provider) {}
#endif

  ~DebugScopedSetImplThread() {
#if DCHECK_IS_ON()
    task_runner_provider_->SetCurrentThreadIsImplThread(previous_value_);
#endif
  }

 private:
#if DCHECK_IS_ON()
  bool previous_value_;
  TaskRunnerProvider* task_runner_provider_;
#endif

  DISALLOW_COPY_AND_ASSIGN(DebugScopedSetImplThread);
};

// For use in the single-threaded case. In debug builds, it pretends that the
// code is running on the main thread to satisfy assertion checks.
class DebugScopedSetMainThread {
 public:
#if DCHECK_IS_ON()
  explicit DebugScopedSetMainThread(TaskRunnerProvider* task_runner_provider)
      : task_runner_provider_(task_runner_provider) {
    previous_value_ = task_runner_provider_->impl_thread_is_overridden_;
    task_runner_provider_->SetCurrentThreadIsImplThread(false);
  }
#else
  explicit DebugScopedSetMainThread(TaskRunnerProvider* task_runner_provider) {}
#endif

  ~DebugScopedSetMainThread() {
#if DCHECK_IS_ON()
    task_runner_provider_->SetCurrentThreadIsImplThread(previous_value_);
#endif
  }

 private:
#if DCHECK_IS_ON()
  bool previous_value_;
  TaskRunnerProvider* task_runner_provider_;
#endif

  DISALLOW_COPY_AND_ASSIGN(DebugScopedSetMainThread);
};

// For use in the single-threaded case. In debug builds, it pretends that the
// code is running on the impl thread and that the main thread is blocked to
// satisfy assertion checks
class DebugScopedSetImplThreadAndMainThreadBlocked {
 public:
  explicit DebugScopedSetImplThreadAndMainThreadBlocked(
      TaskRunnerProvider* task_runner_provider)
      : impl_thread_(task_runner_provider),
        main_thread_blocked_(task_runner_provider) {}

 private:
  DebugScopedSetImplThread impl_thread_;
  DebugScopedSetMainThreadBlocked main_thread_blocked_;

  DISALLOW_COPY_AND_ASSIGN(DebugScopedSetImplThreadAndMainThreadBlocked);
};

}  // namespace cc

#endif  // CC_TREES_SINGLE_THREAD_PROXY_H_
