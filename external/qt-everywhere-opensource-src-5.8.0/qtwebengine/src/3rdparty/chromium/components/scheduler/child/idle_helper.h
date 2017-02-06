// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_CHILD_IDLE_HELPER_H_
#define COMPONENTS_SCHEDULER_CHILD_IDLE_HELPER_H_

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "components/scheduler/base/cancelable_closure_holder.h"
#include "components/scheduler/base/task_queue_selector.h"
#include "components/scheduler/child/scheduler_helper.h"
#include "components/scheduler/child/single_thread_idle_task_runner.h"
#include "components/scheduler/scheduler_export.h"

namespace scheduler {

class SchedulerHelper;

// Common scheduler functionality for Idle tasks.
class SCHEDULER_EXPORT IdleHelper
    : public base::MessageLoop::TaskObserver,
      public SingleThreadIdleTaskRunner::Delegate {
 public:
  // Used to by scheduler implementations to customize idle behaviour.
  class SCHEDULER_EXPORT Delegate {
   public:
    Delegate();
    virtual ~Delegate();

    // If it's ok to enter a long idle period, return true.  Otherwise return
    // false and set next_long_idle_period_delay_out so we know when to try
    // again.
    virtual bool CanEnterLongIdlePeriod(
        base::TimeTicks now,
        base::TimeDelta* next_long_idle_period_delay_out) = 0;

    // Signals that the Long Idle Period hasn't started yet because the system
    // isn't quiescent.
    virtual void IsNotQuiescent() = 0;

    // Signals that we have started an Idle Period.
    virtual void OnIdlePeriodStarted() = 0;

    // Signals that we have finished an Idle Period.
    virtual void OnIdlePeriodEnded() = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  // Keep IdleHelper::IdlePeriodStateToString in sync with this enum.
  enum class IdlePeriodState {
    NOT_IN_IDLE_PERIOD,
    IN_SHORT_IDLE_PERIOD,
    IN_LONG_IDLE_PERIOD,
    IN_LONG_IDLE_PERIOD_WITH_MAX_DEADLINE,
    IN_LONG_IDLE_PERIOD_PAUSED,
    // Must be the last entry.
    IDLE_PERIOD_STATE_COUNT,
    FIRST_IDLE_PERIOD_STATE = NOT_IN_IDLE_PERIOD,
  };

  // The maximum length of an idle period.
  static const int kMaximumIdlePeriodMillis = 50;

  // |helper| and |delegate| are not owned by IdleHelper object and must
  // outlive it.
  IdleHelper(
      SchedulerHelper* helper,
      Delegate* delegate,
      const char* tracing_category,
      const char* disabled_by_default_tracing_category,
      const char* idle_period_tracing_name,
      base::TimeDelta required_quiescence_duration_before_long_idle_period);
  ~IdleHelper() override;

  // Returns the idle task runner. Tasks posted to this runner may be reordered
  // relative to other task types and may be starved for an arbitrarily long
  // time if no idle time is available.
  scoped_refptr<SingleThreadIdleTaskRunner> IdleTaskRunner();

  // If |required_quiescence_duration_before_long_idle_period_| is zero then
  // immediately initiate a long idle period, otherwise check if any tasks have
  // run recently and if so, check again after a delay of
  // |required_quiescence_duration_before_long_idle_period_|.
  // Calling this function will end any previous idle period immediately, and
  // potentially again later if
  // |required_quiescence_duration_before_long_idle_period_| is non-zero.
  // NOTE EndIdlePeriod will disable the long idle periods.
  void EnableLongIdlePeriod();

  // Start an idle period with a given idle period deadline.
  void StartIdlePeriod(IdlePeriodState new_idle_period_state,
                       base::TimeTicks now,
                       base::TimeTicks idle_period_deadline);

  // This will end an idle period either started with StartIdlePeriod or
  // EnableLongIdlePeriod.
  void EndIdlePeriod();

  // Returns true if a currently running idle task could exceed its deadline
  // without impacting user experience too much. This should only be used if
  // there is a task which cannot be pre-empted and is likely to take longer
  // than the largest expected idle task deadline. It should NOT be polled to
  // check whether more work can be performed on the current idle task after
  // its deadline has expired - post a new idle task for the continuation of the
  // work in this case.
  // Must be called from the thread this class was created on.
  bool CanExceedIdleDeadlineIfRequired() const;

  // Returns the deadline for the current idle task.
  base::TimeTicks CurrentIdleTaskDeadline() const;

  // SingleThreadIdleTaskRunner::Delegate implementation:
  void OnIdleTaskPosted() override;
  base::TimeTicks WillProcessIdleTask() override;
  void DidProcessIdleTask() override;

  // base::MessageLoop::TaskObserver implementation:
  void WillProcessTask(const base::PendingTask& pending_task) override;
  void DidProcessTask(const base::PendingTask& pending_task) override;

  IdlePeriodState SchedulerIdlePeriodState() const;
  static const char* IdlePeriodStateToString(IdlePeriodState state);

 private:
  friend class BaseIdleHelperTest;
  friend class IdleHelperTest;

  class State {
   public:
    State(SchedulerHelper* helper,
          Delegate* delegate,
          const char* tracing_category,
          const char* disabled_by_default_tracing_category,
          const char* idle_period_tracing_name);
    virtual ~State();

    void UpdateState(IdlePeriodState new_state,
                     base::TimeTicks new_deadline,
                     base::TimeTicks optional_now);
    bool IsIdlePeriodPaused() const;

    IdlePeriodState idle_period_state() const;
    base::TimeTicks idle_period_deadline() const;

    void TraceIdleIdleTaskStart();
    void TraceIdleIdleTaskEnd();

   private:
    void TraceEventIdlePeriodStateChange(IdlePeriodState new_state,
                                         bool new_running_idle_task,
                                         base::TimeTicks new_deadline,
                                         base::TimeTicks optional_now);

    SchedulerHelper* helper_;  // NOT OWNED
    Delegate* delegate_;       // NOT OWNED

    IdlePeriodState idle_period_state_;
    base::TimeTicks idle_period_deadline_;

    base::TimeTicks last_idle_task_trace_time_;
    bool idle_period_trace_event_started_;
    bool running_idle_task_for_tracing_;
    const char* tracing_category_;
    const char* disabled_by_default_tracing_category_;
    const char* idle_period_tracing_name_;

    DISALLOW_COPY_AND_ASSIGN(State);
  };

  // The minimum duration of an idle period.
  static const int kMinimumIdlePeriodDurationMillis = 1;

  // The minimum delay to wait between retrying to initiate a long idle time.
  static const int kRetryEnableLongIdlePeriodDelayMillis = 1;

  // Returns the new idle period state for the next long idle period. Fills in
  // |next_long_idle_period_delay_out| with the next time we should try to
  // initiate the next idle period.
  IdlePeriodState ComputeNewLongIdlePeriodState(
      const base::TimeTicks now,
      base::TimeDelta* next_long_idle_period_delay_out);

  bool ShouldWaitForQuiescence();
  void OnIdleTaskPostedOnMainThread();
  void UpdateLongIdlePeriodStateAfterIdleTask();

  void SetIdlePeriodState(IdlePeriodState new_state,
                          base::TimeTicks new_deadline,
                          base::TimeTicks optional_now);

  // Returns true if |state| represents being within an idle period state.
  static bool IsInIdlePeriod(IdlePeriodState state);
  // Returns true if |state| represents being within a long idle period state.
  static bool IsInLongIdlePeriod(IdlePeriodState state);

  SchedulerHelper* helper_;  // NOT OWNED
  Delegate* delegate_;       // NOT OWNED
  scoped_refptr<TaskQueue> idle_queue_;
  scoped_refptr<SingleThreadIdleTaskRunner> idle_task_runner_;

  CancelableClosureHolder enable_next_long_idle_period_closure_;
  CancelableClosureHolder on_idle_task_posted_closure_;

  State state_;

  base::TimeDelta required_quiescence_duration_before_long_idle_period_;

  const char* disabled_by_default_tracing_category_;

  base::WeakPtr<IdleHelper> weak_idle_helper_ptr_;
  base::WeakPtrFactory<IdleHelper> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(IdleHelper);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_CHILD_IDLE_HELPER_H_
