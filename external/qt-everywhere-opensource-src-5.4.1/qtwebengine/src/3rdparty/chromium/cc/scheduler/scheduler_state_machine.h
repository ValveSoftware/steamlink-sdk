// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_SCHEDULER_STATE_MACHINE_H_
#define CC_SCHEDULER_SCHEDULER_STATE_MACHINE_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "cc/base/cc_export.h"
#include "cc/output/begin_frame_args.h"
#include "cc/scheduler/draw_result.h"
#include "cc/scheduler/scheduler_settings.h"

namespace base {
class Value;
}

namespace cc {

// The SchedulerStateMachine decides how to coordinate main thread activites
// like painting/running javascript with rendering and input activities on the
// impl thread.
//
// The state machine tracks internal state but is also influenced by external
// state.  Internal state includes things like whether a frame has been
// requested, while external state includes things like the current time being
// near to the vblank time.
//
// The scheduler seperates "what to do next" from the updating of its internal
// state to make testing cleaner.
class CC_EXPORT SchedulerStateMachine {
 public:
  // settings must be valid for the lifetime of this class.
  explicit SchedulerStateMachine(const SchedulerSettings& settings);

  enum OutputSurfaceState {
    OUTPUT_SURFACE_ACTIVE,
    OUTPUT_SURFACE_LOST,
    OUTPUT_SURFACE_CREATING,
    OUTPUT_SURFACE_WAITING_FOR_FIRST_COMMIT,
    OUTPUT_SURFACE_WAITING_FOR_FIRST_ACTIVATION,
  };
  static const char* OutputSurfaceStateToString(OutputSurfaceState state);

  // Note: BeginImplFrameState will always cycle through all the states in
  // order. Whether or not it actually waits or draws, it will at least try to
  // wait in BEGIN_IMPL_FRAME_STATE_INSIDE_BEGIN_FRAME and try to draw in
  // BEGIN_IMPL_FRAME_STATE_INSIDE_DEADLINE
  enum BeginImplFrameState {
    BEGIN_IMPL_FRAME_STATE_IDLE,
    BEGIN_IMPL_FRAME_STATE_BEGIN_FRAME_STARTING,
    BEGIN_IMPL_FRAME_STATE_INSIDE_BEGIN_FRAME,
    BEGIN_IMPL_FRAME_STATE_INSIDE_DEADLINE,
  };
  static const char* BeginImplFrameStateToString(BeginImplFrameState state);

  enum CommitState {
    COMMIT_STATE_IDLE,
    COMMIT_STATE_BEGIN_MAIN_FRAME_SENT,
    COMMIT_STATE_BEGIN_MAIN_FRAME_STARTED,
    COMMIT_STATE_READY_TO_COMMIT,
    COMMIT_STATE_WAITING_FOR_ACTIVATION,
    COMMIT_STATE_WAITING_FOR_FIRST_DRAW,
  };
  static const char* CommitStateToString(CommitState state);

  enum ForcedRedrawOnTimeoutState {
    FORCED_REDRAW_STATE_IDLE,
    FORCED_REDRAW_STATE_WAITING_FOR_COMMIT,
    FORCED_REDRAW_STATE_WAITING_FOR_ACTIVATION,
    FORCED_REDRAW_STATE_WAITING_FOR_DRAW,
  };
  static const char* ForcedRedrawOnTimeoutStateToString(
      ForcedRedrawOnTimeoutState state);

  bool CommitPending() const {
    return commit_state_ == COMMIT_STATE_BEGIN_MAIN_FRAME_SENT ||
           commit_state_ == COMMIT_STATE_BEGIN_MAIN_FRAME_STARTED ||
           commit_state_ == COMMIT_STATE_READY_TO_COMMIT;
  }
  CommitState commit_state() const { return commit_state_; }

  bool RedrawPending() const { return needs_redraw_; }
  bool ManageTilesPending() const { return needs_manage_tiles_; }

  enum Action {
    ACTION_NONE,
    ACTION_ANIMATE,
    ACTION_SEND_BEGIN_MAIN_FRAME,
    ACTION_COMMIT,
    ACTION_UPDATE_VISIBLE_TILES,
    ACTION_ACTIVATE_PENDING_TREE,
    ACTION_DRAW_AND_SWAP_IF_POSSIBLE,
    ACTION_DRAW_AND_SWAP_FORCED,
    ACTION_DRAW_AND_SWAP_ABORT,
    ACTION_BEGIN_OUTPUT_SURFACE_CREATION,
    ACTION_MANAGE_TILES,
  };
  static const char* ActionToString(Action action);

  scoped_ptr<base::Value> AsValue() const;

  Action NextAction() const;
  void UpdateState(Action action);

  // Indicates whether the impl thread needs a BeginImplFrame callback in order
  // to make progress.
  bool BeginFrameNeeded() const;

  // Indicates that we need to independently poll for new state and actions
  // because we can't expect a BeginImplFrame. This is mostly used to avoid
  // drawing repeat frames with the synchronous compositor without dropping
  // necessary actions on the floor.
  bool ShouldPollForAnticipatedDrawTriggers() const;

  // Indicates that the system has entered and left a BeginImplFrame callback.
  // The scheduler will not draw more than once in a given BeginImplFrame
  // callback nor send more than one BeginMainFrame message.
  void OnBeginImplFrame(const BeginFrameArgs& args);
  void OnBeginImplFrameDeadlinePending();
  void OnBeginImplFrameDeadline();
  void OnBeginImplFrameIdle();
  bool ShouldTriggerBeginImplFrameDeadlineEarly() const;
  BeginImplFrameState begin_impl_frame_state() const {
    return begin_impl_frame_state_;
  }

  // If the main thread didn't manage to produce a new frame in time for the
  // impl thread to draw, it is in a high latency mode.
  bool MainThreadIsInHighLatencyMode() const;

  // PollForAnticipatedDrawTriggers is used by the synchronous compositor to
  // avoid requesting BeginImplFrames when we won't actually draw but still
  // need to advance our state at vsync intervals.
  void DidEnterPollForAnticipatedDrawTriggers();
  void DidLeavePollForAnticipatedDrawTriggers();
  bool inside_poll_for_anticipated_draw_triggers() const {
    return inside_poll_for_anticipated_draw_triggers_;
  }

  // Indicates whether the LayerTreeHostImpl is visible.
  void SetVisible(bool visible);

  // Indicates that a redraw is required, either due to the impl tree changing
  // or the screen being damaged and simply needing redisplay.
  void SetNeedsRedraw();
  bool needs_redraw() const { return needs_redraw_; }

  void SetNeedsAnimate();
  bool needs_animate() const { return needs_animate_; }

  // Indicates that manage-tiles is required. This guarantees another
  // ManageTiles will occur shortly (even if no redraw is required).
  void SetNeedsManageTiles();

  // Sets how many swaps can be pending to the OutputSurface.
  void SetMaxSwapsPending(int max);

  // If the scheduler attempted to draw and swap, this provides feedback
  // regarding whether or not the swap actually occured. We might skip the
  // swap when there is not damage, for example.
  void DidSwapBuffers();

  // Indicates whether a redraw is required because we are currently rendering
  // with a low resolution or checkerboarded tile.
  void SetSwapUsedIncompleteTile(bool used_incomplete_tile);

  // Notification from the OutputSurface that a swap has been consumed.
  void DidSwapBuffersComplete();

  // Indicates whether to prioritize animation smoothness over new content
  // activation.
  void SetSmoothnessTakesPriority(bool smoothness_takes_priority);
  bool smoothness_takes_priority() const { return smoothness_takes_priority_; }

  // Indicates whether ACTION_DRAW_AND_SWAP_IF_POSSIBLE drew to the screen.
  void DidDrawIfPossibleCompleted(DrawResult result);

  // Indicates that a new commit flow needs to be performed, either to pull
  // updates from the main thread to the impl, or to push deltas from the impl
  // thread to main.
  void SetNeedsCommit();

  // Call this only in response to receiving an ACTION_SEND_BEGIN_MAIN_FRAME
  // from NextAction.
  // Indicates that all painting is complete.
  void NotifyReadyToCommit();

  // Call this only in response to receiving an ACTION_SEND_BEGIN_MAIN_FRAME
  // from NextAction if the client rejects the BeginMainFrame message.
  // If did_handle is false, then another commit will be retried soon.
  void BeginMainFrameAborted(bool did_handle);

  // Set that we can create the first OutputSurface and start the scheduler.
  void SetCanStart() { can_start_ = true; }

  void SetSkipNextBeginMainFrameToReduceLatency();

  // Indicates whether drawing would, at this time, make sense.
  // CanDraw can be used to suppress flashes or checkerboarding
  // when such behavior would be undesirable.
  void SetCanDraw(bool can);

  // Indicates that scheduled BeginMainFrame is started.
  void NotifyBeginMainFrameStarted();

  // Indicates that the pending tree is ready for activation.
  void NotifyReadyToActivate();

  bool has_pending_tree() const { return has_pending_tree_; }
  bool active_tree_needs_first_draw() const {
    return active_tree_needs_first_draw_;
  }

  void DidManageTiles();
  void DidLoseOutputSurface();
  void DidCreateAndInitializeOutputSurface();
  bool HasInitializedOutputSurface() const;

  // True if we need to abort draws to make forward progress.
  bool PendingDrawsShouldBeAborted() const;

  bool SupportsProactiveBeginFrame() const;

  void SetContinuousPainting(bool continuous_painting) {
    continuous_painting_ = continuous_painting;
  }

 protected:
  bool BeginFrameNeededToAnimateOrDraw() const;
  bool ProactiveBeginFrameWanted() const;

  // True if we need to force activations to make forward progress.
  bool PendingActivationsShouldBeForced() const;

  bool ShouldAnimate() const;
  bool ShouldBeginOutputSurfaceCreation() const;
  bool ShouldDrawForced() const;
  bool ShouldDraw() const;
  bool ShouldActivatePendingTree() const;
  bool ShouldUpdateVisibleTiles() const;
  bool ShouldSendBeginMainFrame() const;
  bool ShouldCommit() const;
  bool ShouldManageTiles() const;

  void AdvanceCurrentFrameNumber();
  bool HasSentBeginMainFrameThisFrame() const;
  bool HasUpdatedVisibleTilesThisFrame() const;
  bool HasRequestedSwapThisFrame() const;
  bool HasSwappedThisFrame() const;

  void UpdateStateOnCommit(bool commit_was_aborted);
  void UpdateStateOnActivation();
  void UpdateStateOnDraw(bool did_request_swap);
  void UpdateStateOnManageTiles();

  const SchedulerSettings settings_;

  OutputSurfaceState output_surface_state_;
  BeginImplFrameState begin_impl_frame_state_;
  CommitState commit_state_;
  ForcedRedrawOnTimeoutState forced_redraw_state_;

  BeginFrameArgs begin_impl_frame_args_;

  int commit_count_;
  int current_frame_number_;
  int last_frame_number_animate_performed_;
  int last_frame_number_swap_performed_;
  int last_frame_number_swap_requested_;
  int last_frame_number_begin_main_frame_sent_;
  int last_frame_number_update_visible_tiles_was_called_;

  // manage_tiles_funnel_ is "filled" each time ManageTiles is called
  // and "drained" on each BeginImplFrame. If the funnel gets too full,
  // we start throttling ACTION_MANAGE_TILES such that we average one
  // ManageTile per BeginImplFrame.
  int manage_tiles_funnel_;
  int consecutive_checkerboard_animations_;
  int max_pending_swaps_;
  int pending_swaps_;
  bool needs_redraw_;
  bool needs_animate_;
  bool needs_manage_tiles_;
  bool swap_used_incomplete_tile_;
  bool needs_commit_;
  bool inside_poll_for_anticipated_draw_triggers_;
  bool visible_;
  bool can_start_;
  bool can_draw_;
  bool has_pending_tree_;
  bool pending_tree_is_ready_for_activation_;
  bool active_tree_needs_first_draw_;
  bool did_create_and_initialize_first_output_surface_;
  bool smoothness_takes_priority_;
  bool skip_next_begin_main_frame_to_reduce_latency_;
  bool skip_begin_main_frame_to_reduce_latency_;
  bool continuous_painting_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SchedulerStateMachine);
};

}  // namespace cc

#endif  // CC_SCHEDULER_SCHEDULER_STATE_MACHINE_H_
