// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TIMERS_ALARM_TIMER_CHROMEOS_H_
#define COMPONENTS_TIMERS_ALARM_TIMER_CHROMEOS_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace base {
class MessageLoop;
struct PendingTask;
}

namespace timers {
// The class implements a timer that is capable of waking the system up from a
// suspended state.  For example, this is useful for running tasks that are
// needed for maintaining network connectivity, like sending heartbeat messages.
// Currently, this feature is only available on Chrome OS systems running linux
// version 3.11 or higher.  On all other platforms, the AlarmTimer behaves
// exactly the same way as a regular Timer.
class AlarmTimer : public base::Timer {
 public:
  ~AlarmTimer() override;

  bool can_wake_from_suspend() const { return can_wake_from_suspend_; }

  // Sets a hook that will be called when the timer fires and a task has been
  // queued on |origin_message_loop_|.  Used by tests to wait until a task is
  // pending in the MessageLoop.
  void SetTimerFiredCallbackForTest(base::Closure test_callback);

  // Timer overrides.
  void Stop() override;
  void Reset() override;

 protected:
  // The constructors for this class are protected because consumers should
  // instantiate one of the specialized sub-classes defined below instead.
  AlarmTimer(bool retain_user_task, bool is_repeating);
  AlarmTimer(const tracked_objects::Location& posted_from,
             base::TimeDelta delay,
             const base::Closure& user_task,
             bool is_repeating);

 private:
  // Common initialization that must be performed by both constructors.  This
  // really should live in a delegated constructor but the way base::Timer's
  // constructors are written makes it really hard to do so.
  void Init();

  // Will be called by the delegate to indicate that the timer has fired and
  // that the user task should be run.
  void OnTimerFired();

  // Called when |origin_message_loop_| will be destroyed.
  void WillDestroyCurrentMessageLoop();

  // Delegate that will manage actually setting the timer.
  class Delegate;
  scoped_refptr<Delegate> delegate_;

  // Keeps track of the user task we want to run.  A new one is constructed
  // every time Reset() is called.
  std::unique_ptr<base::PendingTask> pending_task_;

  // Tracks whether the timer has the ability to wake the system up from
  // suspend.  This is a runtime check because we won't know if the system
  // supports being woken up from suspend until the delegate actually tries to
  // set it up.
  bool can_wake_from_suspend_;

  // Pointer to the message loop that started the timer.  Used to track the
  // destruction of that message loop.
  base::MessageLoop* origin_message_loop_;

  // Observes |origin_message_loop_| and informs this class if it will be
  // destroyed.
  class MessageLoopObserver;
  std::unique_ptr<MessageLoopObserver> message_loop_observer_;

  base::WeakPtrFactory<AlarmTimer> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AlarmTimer);
};

// As its name suggests, a OneShotAlarmTimer runs a given task once.  It does
// not remember the task that was given to it after it has fired and does not
// repeat.  Useful for fire-and-forget tasks.
class OneShotAlarmTimer : public AlarmTimer {
 public:
  // Constructs a basic OneShotAlarmTimer.  An AlarmTimer constructed this way
  // requires that Start() is called before Reset() is called.
  OneShotAlarmTimer();
  ~OneShotAlarmTimer() override;
};

// A RepeatingAlarmTimer takes a task and delay and repeatedly runs the task
// using the specified delay as an interval between the runs until it is
// explicitly stopped.  It remembers both the task and the delay it was given
// after it fires.
class RepeatingAlarmTimer : public AlarmTimer {
 public:
  // Constructs a basic RepeatingAlarmTimer.  An AlarmTimer constructed this way
  // requires that Start() is called before Reset() is called.
  RepeatingAlarmTimer();

  // Constructs a RepeatingAlarmTimer with pre-populated parameters but does not
  // start it.  Useful if |user_task| or |delay| are not going to change.
  // Reset() can be called immediately after constructing an AlarmTimer in this
  // way.
  RepeatingAlarmTimer(const tracked_objects::Location& posted_from,
                      base::TimeDelta delay,
                      const base::Closure& user_task);

  ~RepeatingAlarmTimer() override;
};

// A SimpleAlarmTimer only fires once but remembers the task that it was given
// even after it has fired.  Useful if you want to run the same task multiple
// times but not at a regular interval.
class SimpleAlarmTimer : public AlarmTimer {
 public:
  // Constructs a basic SimpleAlarmTimer.  An AlarmTimer constructed this way
  // requires that Start() is called before Reset() is called.
  SimpleAlarmTimer();

  // Constructs a SimpleAlarmTimer with pre-populated parameters but does not
  // start it.  Useful if |user_task| or |delay| are not going to change.
  // Reset() can be called immediately after constructing an AlarmTimer in this
  // way.
  SimpleAlarmTimer(const tracked_objects::Location& posted_from,
                   base::TimeDelta delay,
                   const base::Closure& user_task);

  ~SimpleAlarmTimer() override;
};

}  // namespace timers

#endif  // COMPONENTS_TIMERS_ALARM_TIMER_CHROMEOS_H_
