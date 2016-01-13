// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_SCHEDULER_H_
#define CC_SCHEDULER_SCHEDULER_H_

#include <deque>
#include <string>

#include "base/basictypes.h"
#include "base/cancelable_callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "cc/base/cc_export.h"
#include "cc/output/begin_frame_args.h"
#include "cc/scheduler/delay_based_time_source.h"
#include "cc/scheduler/draw_result.h"
#include "cc/scheduler/scheduler_settings.h"
#include "cc/scheduler/scheduler_state_machine.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace cc {

class SchedulerClient {
 public:
  virtual void SetNeedsBeginFrame(bool enable) = 0;
  virtual void WillBeginImplFrame(const BeginFrameArgs& args) = 0;
  virtual void ScheduledActionSendBeginMainFrame() = 0;
  virtual DrawResult ScheduledActionDrawAndSwapIfPossible() = 0;
  virtual DrawResult ScheduledActionDrawAndSwapForced() = 0;
  virtual void ScheduledActionAnimate() = 0;
  virtual void ScheduledActionCommit() = 0;
  virtual void ScheduledActionUpdateVisibleTiles() = 0;
  virtual void ScheduledActionActivatePendingTree() = 0;
  virtual void ScheduledActionBeginOutputSurfaceCreation() = 0;
  virtual void ScheduledActionManageTiles() = 0;
  virtual void DidAnticipatedDrawTimeChange(base::TimeTicks time) = 0;
  virtual base::TimeDelta DrawDurationEstimate() = 0;
  virtual base::TimeDelta BeginMainFrameToCommitDurationEstimate() = 0;
  virtual base::TimeDelta CommitToActivateDurationEstimate() = 0;
  virtual void DidBeginImplFrameDeadline() = 0;

 protected:
  virtual ~SchedulerClient() {}
};

class CC_EXPORT Scheduler {
 public:
  static scoped_ptr<Scheduler> Create(
      SchedulerClient* client,
      const SchedulerSettings& scheduler_settings,
      int layer_tree_host_id,
      const scoped_refptr<base::SingleThreadTaskRunner>& impl_task_runner) {
    return make_scoped_ptr(new Scheduler(
        client, scheduler_settings, layer_tree_host_id, impl_task_runner));
  }

  virtual ~Scheduler();

  const SchedulerSettings& settings() const { return settings_; }

  void CommitVSyncParameters(base::TimeTicks timebase,
                             base::TimeDelta interval);
  void SetEstimatedParentDrawTime(base::TimeDelta draw_time);

  void SetCanStart();

  void SetVisible(bool visible);
  void SetCanDraw(bool can_draw);
  void NotifyReadyToActivate();

  void SetNeedsCommit();

  void SetNeedsRedraw();

  void SetNeedsAnimate();

  void SetNeedsManageTiles();

  void SetMaxSwapsPending(int max);
  void DidSwapBuffers();
  void SetSwapUsedIncompleteTile(bool used_incomplete_tile);
  void DidSwapBuffersComplete();

  void SetSmoothnessTakesPriority(bool smoothness_takes_priority);

  void NotifyReadyToCommit();
  void BeginMainFrameAborted(bool did_handle);

  void DidManageTiles();
  void DidLoseOutputSurface();
  void DidCreateAndInitializeOutputSurface();

  bool CommitPending() const { return state_machine_.CommitPending(); }
  bool RedrawPending() const { return state_machine_.RedrawPending(); }
  bool ManageTilesPending() const {
    return state_machine_.ManageTilesPending();
  }
  bool MainThreadIsInHighLatencyMode() const {
    return state_machine_.MainThreadIsInHighLatencyMode();
  }
  bool BeginImplFrameDeadlinePending() const {
    return !begin_impl_frame_deadline_task_.IsCancelled();
  }

  bool WillDrawIfNeeded() const;

  base::TimeTicks AnticipatedDrawTime() const;

  void NotifyBeginMainFrameStarted();

  base::TimeTicks LastBeginImplFrameTime();
  base::TimeDelta VSyncInterval() { return vsync_interval_; }
  base::TimeDelta EstimatedParentDrawTime() {
    return estimated_parent_draw_time_;
  }

  void BeginFrame(const BeginFrameArgs& args);
  void PostBeginRetroFrame();
  void BeginRetroFrame();
  void BeginUnthrottledFrame();

  void BeginImplFrame(const BeginFrameArgs& args);
  void OnBeginImplFrameDeadline();
  void PollForAnticipatedDrawTriggers();
  void PollToAdvanceCommitState();

  scoped_ptr<base::Value> AsValue() const;

  bool IsInsideAction(SchedulerStateMachine::Action action) {
    return inside_action_ == action;
  }

  bool IsBeginMainFrameSent() const;
  void SetContinuousPainting(bool continuous_painting) {
    state_machine_.SetContinuousPainting(continuous_painting);
  }

 protected:
  class CC_EXPORT SyntheticBeginFrameSource : public TimeSourceClient {
   public:
    SyntheticBeginFrameSource(Scheduler* scheduler,
                              base::SingleThreadTaskRunner* task_runner);
    virtual ~SyntheticBeginFrameSource();

    // Updates the phase and frequency of the timer.
    void CommitVSyncParameters(base::TimeTicks timebase,
                               base::TimeDelta interval);

    // Activates future BeginFrames and, if activating, pushes the most
    // recently missed BeginFrame to the back of a retroactive queue.
    void SetNeedsBeginFrame(bool needs_begin_frame,
                            std::deque<BeginFrameArgs>* begin_retro_frame_args);

    bool IsActive() const;

    // TimeSourceClient implementation of OnTimerTick triggers a BeginFrame.
    virtual void OnTimerTick() OVERRIDE;

    scoped_ptr<base::Value> AsValue() const;

   private:
    BeginFrameArgs CreateSyntheticBeginFrameArgs(base::TimeTicks frame_time);

    Scheduler* scheduler_;
    scoped_refptr<DelayBasedTimeSource> time_source_;
  };

  Scheduler(
      SchedulerClient* client,
      const SchedulerSettings& scheduler_settings,
      int layer_tree_host_id,
      const scoped_refptr<base::SingleThreadTaskRunner>& impl_task_runner);

  const SchedulerSettings settings_;
  SchedulerClient* client_;
  int layer_tree_host_id_;
  scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner_;

  base::TimeDelta vsync_interval_;
  base::TimeDelta estimated_parent_draw_time_;

  bool last_set_needs_begin_frame_;
  bool begin_unthrottled_frame_posted_;
  bool begin_retro_frame_posted_;
  std::deque<BeginFrameArgs> begin_retro_frame_args_;
  BeginFrameArgs begin_impl_frame_args_;

  scoped_ptr<SyntheticBeginFrameSource> synthetic_begin_frame_source_;

  base::Closure begin_retro_frame_closure_;
  base::Closure begin_unthrottled_frame_closure_;

  base::Closure begin_impl_frame_deadline_closure_;
  base::Closure poll_for_draw_triggers_closure_;
  base::Closure advance_commit_state_closure_;
  base::CancelableClosure begin_impl_frame_deadline_task_;
  base::CancelableClosure poll_for_draw_triggers_task_;
  base::CancelableClosure advance_commit_state_task_;

  SchedulerStateMachine state_machine_;
  bool inside_process_scheduled_actions_;
  SchedulerStateMachine::Action inside_action_;

 private:
  base::TimeTicks AdjustedBeginImplFrameDeadline(
      const BeginFrameArgs& args,
      base::TimeDelta draw_duration_estimate) const;
  void ScheduleBeginImplFrameDeadline(base::TimeTicks deadline);
  void SetupNextBeginFrameIfNeeded();
  void PostBeginRetroFrameIfNeeded();
  void SetupNextBeginFrameWhenVSyncThrottlingEnabled(bool needs_begin_frame);
  void SetupNextBeginFrameWhenVSyncThrottlingDisabled(bool needs_begin_frame);
  void SetupPollingMechanisms(bool needs_begin_frame);
  void DrawAndSwapIfPossible();
  void ProcessScheduledActions();
  bool CanCommitAndActivateBeforeDeadline() const;
  void AdvanceCommitStateIfPossible();
  bool IsBeginMainFrameSentOrStarted() const;
  void SetupSyntheticBeginFrames();

  base::WeakPtrFactory<Scheduler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Scheduler);
};

}  // namespace cc

#endif  // CC_SCHEDULER_SCHEDULER_H_
