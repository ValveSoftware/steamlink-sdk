// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_THREAD_PROXY_H_
#define CC_TREES_THREAD_PROXY_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "cc/animation/animation_events.h"
#include "cc/base/completion_event.h"
#include "cc/base/delayed_unique_notifier.h"
#include "cc/resources/resource_update_controller.h"
#include "cc/scheduler/scheduler.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/proxy.h"
#include "cc/trees/proxy_timing_history.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace cc {

class ContextProvider;
class InputHandlerClient;
class LayerTreeHost;
class ResourceUpdateQueue;
class Scheduler;
class ScopedThreadProxy;

class CC_EXPORT ThreadProxy : public Proxy,
                    NON_EXPORTED_BASE(LayerTreeHostImplClient),
                    NON_EXPORTED_BASE(SchedulerClient),
                    NON_EXPORTED_BASE(ResourceUpdateControllerClient) {
 public:
  static scoped_ptr<Proxy> Create(
      LayerTreeHost* layer_tree_host,
      scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner);

  virtual ~ThreadProxy();

  struct BeginMainFrameAndCommitState {
    BeginMainFrameAndCommitState();
    ~BeginMainFrameAndCommitState();

    base::TimeTicks monotonic_frame_begin_time;
    scoped_ptr<ScrollAndScaleSet> scroll_info;
    size_t memory_allocation_limit_bytes;
    int memory_allocation_priority_cutoff;
    bool evicted_ui_resources;
  };

  struct MainThreadOnly {
    MainThreadOnly(ThreadProxy* proxy, int layer_tree_host_id);
    ~MainThreadOnly();

    const int layer_tree_host_id;

    // Set only when SetNeedsAnimate is called.
    bool animate_requested;
    // Set only when SetNeedsCommit is called.
    bool commit_requested;
    // Set by SetNeedsAnimate, SetNeedsUpdateLayers, and SetNeedsCommit.
    bool commit_request_sent_to_impl_thread;

    bool started;
    bool manage_tiles_pending;
    bool can_cancel_commit;
    bool defer_commits;

    RendererCapabilities renderer_capabilities_main_thread_copy;

    scoped_ptr<BeginMainFrameAndCommitState> pending_deferred_commit;
    base::WeakPtrFactory<ThreadProxy> weak_factory;
  };

  // Accessed on the main thread, or when main thread is blocked.
  struct MainThreadOrBlockedMainThread {
    explicit MainThreadOrBlockedMainThread(LayerTreeHost* host);
    ~MainThreadOrBlockedMainThread();

    PrioritizedResourceManager* contents_texture_manager();

    LayerTreeHost* layer_tree_host;
    bool commit_waits_for_activation;
    bool main_thread_inside_commit;

    base::TimeTicks last_monotonic_frame_begin_time;
  };

  struct ReadbackRequest;

  struct CompositorThreadOnly {
    CompositorThreadOnly(ThreadProxy* proxy, int layer_tree_host_id);
    ~CompositorThreadOnly();

    const int layer_tree_host_id;

    // Copy of the main thread side contents texture manager for work
    // that needs to be done on the compositor thread.
    PrioritizedResourceManager* contents_texture_manager;

    scoped_ptr<Scheduler> scheduler;

    // Set when the main thread is waiting on a
    // ScheduledActionSendBeginMainFrame to be issued.
    CompletionEvent* begin_main_frame_sent_completion_event;

    // Set when the main thread is waiting on a commit to complete.
    CompletionEvent* commit_completion_event;

    // Set when the main thread is waiting on a pending tree activation.
    CompletionEvent* completion_event_for_commit_held_on_tree_activation;

    scoped_ptr<ResourceUpdateController> current_resource_update_controller;

    // Set when the next draw should post DidCommitAndDrawFrame to the main
    // thread.
    bool next_frame_is_newly_committed_frame;

    bool inside_draw;

    bool input_throttled_until_commit;

    // Set when we freeze animations to avoid checkerboarding.
    bool animations_frozen_until_next_draw;
    base::TimeTicks animation_time;

    // Whether a commit has been completed since the last time animations were
    // ticked. If this happens, we need to animate again.
    bool did_commit_after_animating;

    DelayedUniqueNotifier smoothness_priority_expiration_notifier;

    ProxyTimingHistory timing_history;

    scoped_ptr<LayerTreeHostImpl> layer_tree_host_impl;
    base::WeakPtrFactory<ThreadProxy> weak_factory;
  };

  const MainThreadOnly& main() const;
  const MainThreadOrBlockedMainThread& blocked_main() const;
  const CompositorThreadOnly& impl() const;

  // Proxy implementation
  virtual void FinishAllRendering() OVERRIDE;
  virtual bool IsStarted() const OVERRIDE;
  virtual void SetLayerTreeHostClientReady() OVERRIDE;
  virtual void SetVisible(bool visible) OVERRIDE;
  virtual const RendererCapabilities& GetRendererCapabilities() const OVERRIDE;
  virtual void SetNeedsAnimate() OVERRIDE;
  virtual void SetNeedsUpdateLayers() OVERRIDE;
  virtual void SetNeedsCommit() OVERRIDE;
  virtual void SetNeedsRedraw(const gfx::Rect& damage_rect) OVERRIDE;
  virtual void SetNextCommitWaitsForActivation() OVERRIDE;
  virtual void NotifyInputThrottledUntilCommit() OVERRIDE;
  virtual void SetDeferCommits(bool defer_commits) OVERRIDE;
  virtual bool CommitRequested() const OVERRIDE;
  virtual bool BeginMainFrameRequested() const OVERRIDE;
  virtual void MainThreadHasStoppedFlinging() OVERRIDE;
  virtual void Start() OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual size_t MaxPartialTextureUpdates() const OVERRIDE;
  virtual void ForceSerializeOnSwapBuffers() OVERRIDE;
  virtual void SetDebugState(const LayerTreeDebugState& debug_state) OVERRIDE;
  virtual scoped_ptr<base::Value> AsValue() const OVERRIDE;
  virtual bool CommitPendingForTesting() OVERRIDE;
  virtual scoped_ptr<base::Value> SchedulerAsValueForTesting() OVERRIDE;

  // LayerTreeHostImplClient implementation
  virtual void UpdateRendererCapabilitiesOnImplThread() OVERRIDE;
  virtual void DidLoseOutputSurfaceOnImplThread() OVERRIDE;
  virtual void CommitVSyncParameters(base::TimeTicks timebase,
                                     base::TimeDelta interval) OVERRIDE;
  virtual void SetEstimatedParentDrawTime(base::TimeDelta draw_time) OVERRIDE;
  virtual void BeginFrame(const BeginFrameArgs& args) OVERRIDE;
  virtual void SetMaxSwapsPendingOnImplThread(int max) OVERRIDE;
  virtual void DidSwapBuffersOnImplThread() OVERRIDE;
  virtual void DidSwapBuffersCompleteOnImplThread() OVERRIDE;
  virtual void OnCanDrawStateChanged(bool can_draw) OVERRIDE;
  virtual void NotifyReadyToActivate() OVERRIDE;
  // Please call these 3 functions through
  // LayerTreeHostImpl's SetNeedsRedraw(), SetNeedsRedrawRect() and
  // SetNeedsAnimate().
  virtual void SetNeedsRedrawOnImplThread() OVERRIDE;
  virtual void SetNeedsRedrawRectOnImplThread(const gfx::Rect& dirty_rect)
      OVERRIDE;
  virtual void SetNeedsAnimateOnImplThread() OVERRIDE;
  virtual void SetNeedsManageTilesOnImplThread() OVERRIDE;
  virtual void DidInitializeVisibleTileOnImplThread() OVERRIDE;
  virtual void SetNeedsCommitOnImplThread() OVERRIDE;
  virtual void PostAnimationEventsToMainThreadOnImplThread(
      scoped_ptr<AnimationEventsVector> queue) OVERRIDE;
  virtual bool ReduceContentsTextureMemoryOnImplThread(size_t limit_bytes,
                                                       int priority_cutoff)
      OVERRIDE;
  virtual bool IsInsideDraw() OVERRIDE;
  virtual void RenewTreePriority() OVERRIDE;
  virtual void PostDelayedScrollbarFadeOnImplThread(
      const base::Closure& start_fade,
      base::TimeDelta delay) OVERRIDE;
  virtual void DidActivatePendingTree() OVERRIDE;
  virtual void DidManageTiles() OVERRIDE;

  // SchedulerClient implementation
  virtual void SetNeedsBeginFrame(bool enable) OVERRIDE;
  virtual void WillBeginImplFrame(const BeginFrameArgs& args) OVERRIDE;
  virtual void ScheduledActionSendBeginMainFrame() OVERRIDE;
  virtual DrawResult ScheduledActionDrawAndSwapIfPossible() OVERRIDE;
  virtual DrawResult ScheduledActionDrawAndSwapForced() OVERRIDE;
  virtual void ScheduledActionAnimate() OVERRIDE;
  virtual void ScheduledActionCommit() OVERRIDE;
  virtual void ScheduledActionUpdateVisibleTiles() OVERRIDE;
  virtual void ScheduledActionActivatePendingTree() OVERRIDE;
  virtual void ScheduledActionBeginOutputSurfaceCreation() OVERRIDE;
  virtual void ScheduledActionManageTiles() OVERRIDE;
  virtual void DidAnticipatedDrawTimeChange(base::TimeTicks time) OVERRIDE;
  virtual base::TimeDelta DrawDurationEstimate() OVERRIDE;
  virtual base::TimeDelta BeginMainFrameToCommitDurationEstimate() OVERRIDE;
  virtual base::TimeDelta CommitToActivateDurationEstimate() OVERRIDE;
  virtual void DidBeginImplFrameDeadline() OVERRIDE;

  // ResourceUpdateControllerClient implementation
  virtual void ReadyToFinalizeTextureUpdates() OVERRIDE;

 protected:
  ThreadProxy(LayerTreeHost* layer_tree_host,
              scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner);

 private:
  // Called on main thread.
  void SetRendererCapabilitiesMainThreadCopy(
      const RendererCapabilities& capabilities);
  void BeginMainFrame(
      scoped_ptr<BeginMainFrameAndCommitState> begin_main_frame_state);
  void DidCommitAndDrawFrame();
  void DidCompleteSwapBuffers();
  void SetAnimationEvents(scoped_ptr<AnimationEventsVector> queue);
  void DidLoseOutputSurface();
  void CreateAndInitializeOutputSurface();
  void DidInitializeOutputSurface(bool success,
                                  const RendererCapabilities& capabilities);
  void SendCommitRequestToImplThreadIfNeeded();

  // Called on impl thread.
  struct CommitPendingRequest;
  struct SchedulerStateRequest;

  void StartCommitOnImplThread(CompletionEvent* completion,
                               ResourceUpdateQueue* queue);
  void BeginMainFrameAbortedOnImplThread(bool did_handle);
  void FinishAllRenderingOnImplThread(CompletionEvent* completion);
  void InitializeImplOnImplThread(CompletionEvent* completion);
  void SetLayerTreeHostClientReadyOnImplThread();
  void SetVisibleOnImplThread(CompletionEvent* completion, bool visible);
  void UpdateBackgroundAnimateTicking();
  void HasInitializedOutputSurfaceOnImplThread(
      CompletionEvent* completion,
      bool* has_initialized_output_surface);
  void DeleteContentsTexturesOnImplThread(CompletionEvent* completion);
  void InitializeOutputSurfaceOnImplThread(
      scoped_ptr<OutputSurface> output_surface);
  void FinishGLOnImplThread(CompletionEvent* completion);
  void LayerTreeHostClosedOnImplThread(CompletionEvent* completion);
  DrawResult DrawSwapInternal(bool forced_draw);
  void ForceSerializeOnSwapBuffersOnImplThread(CompletionEvent* completion);
  void CheckOutputSurfaceStatusOnImplThread();
  void CommitPendingOnImplThreadForTesting(CommitPendingRequest* request);
  void SchedulerAsValueOnImplThreadForTesting(SchedulerStateRequest* request);
  void AsValueOnImplThread(CompletionEvent* completion,
                           base::DictionaryValue* state) const;
  void SetSwapUsedIncompleteTileOnImplThread(bool used_incomplete_tile);
  void MainThreadHasStoppedFlingingOnImplThread();
  void SetInputThrottledUntilCommitOnImplThread(bool is_throttled);
  void SetDebugStateOnImplThread(const LayerTreeDebugState& debug_state);

  LayerTreeHost* layer_tree_host();
  const LayerTreeHost* layer_tree_host() const;

  // Use accessors instead of this variable directly.
  MainThreadOnly main_thread_only_vars_unsafe_;
  MainThreadOnly& main();

  // Use accessors instead of this variable directly.
  MainThreadOrBlockedMainThread main_thread_or_blocked_vars_unsafe_;
  MainThreadOrBlockedMainThread& blocked_main();

  // Use accessors instead of this variable directly.
  CompositorThreadOnly compositor_thread_vars_unsafe_;
  CompositorThreadOnly& impl();

  base::WeakPtr<ThreadProxy> main_thread_weak_ptr_;
  base::WeakPtr<ThreadProxy> impl_thread_weak_ptr_;

  DISALLOW_COPY_AND_ASSIGN(ThreadProxy);
};

}  // namespace cc

#endif  // CC_TREES_THREAD_PROXY_H_
