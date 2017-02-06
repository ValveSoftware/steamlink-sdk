// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/timers/alarm_timer_chromeos.h"

#include <stdint.h>
#include <sys/timerfd.h>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/pending_task.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"

namespace timers {
namespace {
// This class represents the IO thread that the AlarmTimer::Delegate may use for
// watching file descriptors if it gets called from a thread that does not have
// a MessageLoopForIO.  It is a lazy global instance because it may not always
// be necessary.
class RtcAlarmIOThread : public base::Thread {
 public:
  RtcAlarmIOThread() : Thread("RTC Alarm IO Thread") {
    CHECK(
        StartWithOptions(base::Thread::Options(base::MessageLoop::TYPE_IO, 0)));
  }
  ~RtcAlarmIOThread() override { Stop(); }
};

base::LazyInstance<RtcAlarmIOThread> g_io_thread = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// Watches a MessageLoop and runs a callback if that MessageLoop will be
// destroyed.
class AlarmTimer::MessageLoopObserver
    : public base::MessageLoop::DestructionObserver {
 public:
  // Constructs a MessageLoopObserver that will observe |message_loop| and will
  // call |on_will_be_destroyed_callback| when |message_loop| is about to be
  // destroyed.
  MessageLoopObserver(base::MessageLoop* message_loop,
                      base::Closure on_will_be_destroyed_callback)
      : message_loop_(message_loop),
        on_will_be_destroyed_callback_(on_will_be_destroyed_callback) {
    DCHECK(message_loop_);
    message_loop_->AddDestructionObserver(this);
  }

  ~MessageLoopObserver() override {
    // If |message_loop_| was destroyed, then this class will have already
    // unregistered itself.  Doing it again will trigger a warning.
    if (message_loop_)
      message_loop_->RemoveDestructionObserver(this);
  }

  // base::MessageLoop::DestructionObserver override.
  void WillDestroyCurrentMessageLoop() override {
    message_loop_->RemoveDestructionObserver(this);
    message_loop_ = NULL;

    on_will_be_destroyed_callback_.Run();
  }

 private:
  // The MessageLoop that this class should watch.  Is a weak pointer.
  base::MessageLoop* message_loop_;

  // The callback to run when |message_loop_| will be destroyed.
  base::Closure on_will_be_destroyed_callback_;

  DISALLOW_COPY_AND_ASSIGN(MessageLoopObserver);
};

// This class manages a Real Time Clock (RTC) alarm, a feature that is available
// from linux version 3.11 onwards.  It creates a file descriptor for the RTC
// alarm timer and then watches that file descriptor to see when it can be read
// without blocking, indicating that the timer has fired.
//
// A major problem for this class is that watching file descriptors is only
// available on a MessageLoopForIO but there is no guarantee the timer is going
// to be created on one.  To get around this, the timer has a dedicated thread
// with a MessageLoopForIO that posts tasks back to the thread that started the
// timer.
class AlarmTimer::Delegate
    : public base::RefCountedThreadSafe<AlarmTimer::Delegate>,
      public base::MessageLoopForIO::Watcher {
 public:
  // Construct a Delegate for the AlarmTimer.  It should be safe to call
  // |on_timer_fired_callback| multiple times.
  explicit Delegate(base::Closure on_timer_fired_callback);

  // Returns true if the system timer managed by this delegate is capable of
  // waking the system from suspend.
  bool CanWakeFromSuspend();

  // Resets the timer to fire after |delay| has passed.  Cancels any
  // pre-existing delay.
  void Reset(base::TimeDelta delay);

  // Stops the currently running timer.  It should be safe to call this even if
  // the timer is not running.
  void Stop();

  // Sets a hook that will be called when the timer fires and a task has been
  // queued on |origin_task_runner_|.  Used by tests to wait until a task is
  // pending in the MessageLoop.
  void SetTimerFiredCallbackForTest(base::Closure test_callback);

  // base::MessageLoopForIO::Watcher overrides.
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

 private:
  friend class base::RefCountedThreadSafe<Delegate>;
  ~Delegate() override;

  // Actually performs the system calls to set up the timer.  This must be
  // called on a MessageLoopForIO.
  void ResetImpl(base::TimeDelta delay, int reset_sequence_number);

  // Callback that is run when the timer fires.  Must be run on
  // |origin_task_runner_|.
  void OnTimerFired(int reset_sequence_number);

  // File descriptor associated with the alarm timer.
  int alarm_fd_;

  // Task runner which initially started the timer.
  scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner_;

  // Callback that should be run when the timer fires.
  base::Closure on_timer_fired_callback_;

  // Hook used by tests to be notified when the timer has fired and a task has
  // been queued in the MessageLoop.
  base::Closure on_timer_fired_callback_for_test_;

  // Manages watching file descriptors.
  std::unique_ptr<base::MessageLoopForIO::FileDescriptorWatcher> fd_watcher_;

  // The sequence numbers of the last Reset() call handled respectively on
  // |origin_task_runner_| and on the MessageLoopForIO used for watching the
  // timer file descriptor.  Note that these can be the same MessageLoop.
  // OnTimerFired() runs |on_timer_fired_callback_| only if the sequence number
  // it receives from the MessageLoopForIO matches
  // |origin_reset_sequence_number_|.
  int origin_reset_sequence_number_;
  int io_reset_sequence_number_;

  DISALLOW_COPY_AND_ASSIGN(Delegate);
};

AlarmTimer::Delegate::Delegate(base::Closure on_timer_fired_callback)
    : alarm_fd_(timerfd_create(CLOCK_REALTIME_ALARM, 0)),
      on_timer_fired_callback_(on_timer_fired_callback),
      origin_reset_sequence_number_(0),
      io_reset_sequence_number_(0) {
  // The call to timerfd_create above may fail.  This is the only indication
  // that CLOCK_REALTIME_ALARM is not supported on this system.
  DPLOG_IF(INFO, (alarm_fd_ == -1))
      << "CLOCK_REALTIME_ALARM not supported on this system";
}

AlarmTimer::Delegate::~Delegate() {
  if (alarm_fd_ != -1)
    close(alarm_fd_);
}

bool AlarmTimer::Delegate::CanWakeFromSuspend() {
  return alarm_fd_ != -1;
}

void AlarmTimer::Delegate::Reset(base::TimeDelta delay) {
  // Get a task runner for the current message loop.  When the timer fires, we
  // will
  // post tasks to this proxy to let the parent timer know.
  origin_task_runner_ = base::ThreadTaskRunnerHandle::Get();

  // Increment the sequence number.  Used to invalidate any events that have
  // been queued but not yet run since the last time Reset() was called.
  origin_reset_sequence_number_++;

  // Calling timerfd_settime with a zero delay actually clears the timer so if
  // the user has requested a zero delay timer, we need to handle it
  // differently.  We queue the task here but we still go ahead and call
  // timerfd_settime with the zero delay anyway to cancel any previous delay
  // that might have been programmed.
  if (delay <= base::TimeDelta::FromMicroseconds(0)) {
    // The timerfd_settime documentation is vague on what happens when it is
    // passed a negative delay.  We can sidestep the issue by ensuring that
    // the delay is 0.
    delay = base::TimeDelta::FromMicroseconds(0);
    origin_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&Delegate::OnTimerFired, scoped_refptr<Delegate>(this),
                   origin_reset_sequence_number_));
  }

  // Run ResetImpl() on a MessageLoopForIO.
  if (base::MessageLoopForIO::IsCurrent()) {
    ResetImpl(delay, origin_reset_sequence_number_);
  } else {
    g_io_thread.Pointer()->task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&Delegate::ResetImpl, scoped_refptr<Delegate>(this), delay,
                   origin_reset_sequence_number_));
  }
}

void AlarmTimer::Delegate::Stop() {
  // Stop the RTC from a MessageLoopForIO.
  if (!base::MessageLoopForIO::IsCurrent()) {
    g_io_thread.Pointer()->task_runner()->PostTask(
        FROM_HERE, base::Bind(&Delegate::Stop, scoped_refptr<Delegate>(this)));
    return;
  }

  // Stop watching for events.
  fd_watcher_.reset();

  // Now clear the timer.
  DCHECK_NE(alarm_fd_, -1);
  itimerspec blank_time = {};
  if (timerfd_settime(alarm_fd_, 0, &blank_time, NULL) < 0)
    PLOG(ERROR) << "Unable to clear alarm time.  Timer may still fire.";
}

void AlarmTimer::Delegate::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK_EQ(alarm_fd_, fd);

  // Read from the fd to ack the event.
  char val[sizeof(uint64_t)];
  if (!base::ReadFromFD(alarm_fd_, val, sizeof(uint64_t)))
    PLOG(DFATAL) << "Unable to read from timer file descriptor.";

  // Make sure that the parent timer is informed on the proper message loop.
  if (origin_task_runner_->RunsTasksOnCurrentThread()) {
    OnTimerFired(io_reset_sequence_number_);
  } else {
    origin_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&Delegate::OnTimerFired, scoped_refptr<Delegate>(this),
                   io_reset_sequence_number_));
  }
}

void AlarmTimer::Delegate::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED();
}

void AlarmTimer::Delegate::SetTimerFiredCallbackForTest(
    base::Closure test_callback) {
  on_timer_fired_callback_for_test_ = test_callback;
}

void AlarmTimer::Delegate::ResetImpl(base::TimeDelta delay,
                                     int reset_sequence_number) {
  DCHECK(base::MessageLoopForIO::IsCurrent());
  DCHECK_NE(alarm_fd_, -1);

  // Store the sequence number in the IO thread variable.  When the timer
  // fires, we will bind this value to the OnTimerFired callback to ensure
  // that we do the right thing if the timer gets reset.
  io_reset_sequence_number_ = reset_sequence_number;

  // If we were already watching the fd, this will stop watching it.
  fd_watcher_.reset(new base::MessageLoopForIO::FileDescriptorWatcher);

  // Start watching the fd to see when the timer fires.
  if (!base::MessageLoopForIO::current()->WatchFileDescriptor(
          alarm_fd_, false, base::MessageLoopForIO::WATCH_READ,
          fd_watcher_.get(), this)) {
    LOG(ERROR) << "Error while attempting to watch file descriptor for RTC "
               << "alarm.  Timer will not fire.";
  }

  // Actually set the timer.  This will also clear the pre-existing timer, if
  // any.
  itimerspec alarm_time = {};
  alarm_time.it_value.tv_sec = delay.InSeconds();
  alarm_time.it_value.tv_nsec =
      (delay.InMicroseconds() % base::Time::kMicrosecondsPerSecond) *
      base::Time::kNanosecondsPerMicrosecond;
  if (timerfd_settime(alarm_fd_, 0, &alarm_time, NULL) < 0)
    PLOG(ERROR) << "Error while setting alarm time.  Timer will not fire";
}

void AlarmTimer::Delegate::OnTimerFired(int reset_sequence_number) {
  DCHECK(origin_task_runner_->RunsTasksOnCurrentThread());

  // If a test wants to be notified when this function is about to run, then
  // re-queue this task in the MessageLoop and run the test's callback.
  if (!on_timer_fired_callback_for_test_.is_null()) {
    origin_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&Delegate::OnTimerFired, scoped_refptr<Delegate>(this),
                   reset_sequence_number));

    on_timer_fired_callback_for_test_.Run();
    on_timer_fired_callback_for_test_.Reset();
    return;
  }

  // Check to make sure that the timer was not reset in the time between when
  // this task was queued to run and now.  If it was reset, then don't do
  // anything.
  if (reset_sequence_number != origin_reset_sequence_number_)
    return;

  on_timer_fired_callback_.Run();
}

AlarmTimer::AlarmTimer(bool retain_user_task, bool is_repeating)
    : base::Timer(retain_user_task, is_repeating),
      can_wake_from_suspend_(false),
      origin_message_loop_(NULL),
      weak_factory_(this) {
  Init();
}

AlarmTimer::AlarmTimer(const tracked_objects::Location& posted_from,
                       base::TimeDelta delay,
                       const base::Closure& user_task,
                       bool is_repeating)
    : base::Timer(posted_from, delay, user_task, is_repeating),
      can_wake_from_suspend_(false),
      origin_message_loop_(NULL),
      weak_factory_(this) {
  Init();
}

AlarmTimer::~AlarmTimer() {
  Stop();
}

void AlarmTimer::SetTimerFiredCallbackForTest(base::Closure test_callback) {
  delegate_->SetTimerFiredCallbackForTest(test_callback);
}

void AlarmTimer::Init() {
  delegate_ = make_scoped_refptr(new AlarmTimer::Delegate(
      base::Bind(&AlarmTimer::OnTimerFired, weak_factory_.GetWeakPtr())));
  can_wake_from_suspend_ = delegate_->CanWakeFromSuspend();
}

void AlarmTimer::Stop() {
  if (!base::Timer::is_running())
    return;

  if (!can_wake_from_suspend_) {
    base::Timer::Stop();
    return;
  }

  // Clear the running flag, stop the delegate, and delete the pending task.
  base::Timer::set_is_running(false);
  delegate_->Stop();
  pending_task_.reset();

  // Stop watching |origin_message_loop_|.
  origin_message_loop_ = NULL;
  message_loop_observer_.reset();

  if (!base::Timer::retain_user_task())
    base::Timer::set_user_task(base::Closure());
}

void AlarmTimer::Reset() {
  if (!can_wake_from_suspend_) {
    base::Timer::Reset();
    return;
  }

  DCHECK(!base::Timer::user_task().is_null());
  DCHECK(!origin_message_loop_ ||
         origin_message_loop_->task_runner()->RunsTasksOnCurrentThread());

  // Make sure that the timer will stop if the underlying message loop is
  // destroyed.
  if (!origin_message_loop_) {
    origin_message_loop_ = base::MessageLoop::current();
    message_loop_observer_.reset(new MessageLoopObserver(
        origin_message_loop_,
        base::Bind(&AlarmTimer::WillDestroyCurrentMessageLoop,
                   weak_factory_.GetWeakPtr())));
  }

  // Set up the pending task.
  if (base::Timer::GetCurrentDelay() > base::TimeDelta::FromMicroseconds(0)) {
    base::Timer::set_desired_run_time(base::TimeTicks::Now() +
                                      base::Timer::GetCurrentDelay());
    pending_task_.reset(new base::PendingTask(
        base::Timer::posted_from(), base::Timer::user_task(),
        base::Timer::desired_run_time(), true /* nestable */));
  } else {
    base::Timer::set_desired_run_time(base::TimeTicks());
    pending_task_.reset(new base::PendingTask(base::Timer::posted_from(),
                                              base::Timer::user_task()));
  }
  base::MessageLoop::current()->task_annotator()->DidQueueTask(
      "AlarmTimer::Reset", *pending_task_);

  // Now start up the timer.
  delegate_->Reset(base::Timer::GetCurrentDelay());
  base::Timer::set_is_running(true);
}

void AlarmTimer::WillDestroyCurrentMessageLoop() {
  Stop();
}

void AlarmTimer::OnTimerFired() {
  if (!base::Timer::IsRunning())
    return;

  DCHECK(pending_task_.get());

  // Take ownership of the pending user task, which is going to be cleared by
  // the Stop() or Reset() functions below.
  std::unique_ptr<base::PendingTask> pending_user_task(
      std::move(pending_task_));

  // Re-schedule or stop the timer as requested.
  if (base::Timer::is_repeating())
    Reset();
  else
    Stop();

  TRACE_TASK_EXECUTION("AlarmTimer::OnTimerFired", *pending_user_task);

  // Now run the user task.
  base::MessageLoop::current()->task_annotator()->RunTask("AlarmTimer::Reset",
                                                          *pending_user_task);
}

OneShotAlarmTimer::OneShotAlarmTimer() : AlarmTimer(false, false) {
}

OneShotAlarmTimer::~OneShotAlarmTimer() {
}

RepeatingAlarmTimer::RepeatingAlarmTimer() : AlarmTimer(true, true) {
}

RepeatingAlarmTimer::RepeatingAlarmTimer(
    const tracked_objects::Location& posted_from,
    base::TimeDelta delay,
    const base::Closure& user_task)
    : AlarmTimer(posted_from, delay, user_task, true) {
}

RepeatingAlarmTimer::~RepeatingAlarmTimer() {
}

SimpleAlarmTimer::SimpleAlarmTimer() : AlarmTimer(true, false) {
}

SimpleAlarmTimer::SimpleAlarmTimer(const tracked_objects::Location& posted_from,
                                   base::TimeDelta delay,
                                   const base::Closure& user_task)
    : AlarmTimer(posted_from, delay, user_task, false) {
}

SimpleAlarmTimer::~SimpleAlarmTimer() {
}

}  // namespace timers
