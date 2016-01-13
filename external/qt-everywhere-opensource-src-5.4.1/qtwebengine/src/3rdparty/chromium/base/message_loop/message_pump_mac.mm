// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/message_loop/message_pump_mac.h"

#include <dlfcn.h>
#import <Foundation/Foundation.h>

#include <limits>
#include <stack>

#include "base/format_macros.h"
#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/message_loop/timer_slack.h"
#include "base/metrics/histogram.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"

#if !defined(OS_IOS)
#import <AppKit/AppKit.h>
#endif  // !defined(OS_IOS)

namespace base {

namespace {

void CFRunLoopAddSourceToAllModes(CFRunLoopRef rl, CFRunLoopSourceRef source) {
  CFRunLoopAddSource(rl, source, kCFRunLoopCommonModes);
  CFRunLoopAddSource(rl, source, kMessageLoopExclusiveRunLoopMode);
}

void CFRunLoopRemoveSourceFromAllModes(CFRunLoopRef rl,
                                       CFRunLoopSourceRef source) {
  CFRunLoopRemoveSource(rl, source, kCFRunLoopCommonModes);
  CFRunLoopRemoveSource(rl, source, kMessageLoopExclusiveRunLoopMode);
}

void CFRunLoopAddTimerToAllModes(CFRunLoopRef rl, CFRunLoopTimerRef timer) {
  CFRunLoopAddTimer(rl, timer, kCFRunLoopCommonModes);
  CFRunLoopAddTimer(rl, timer, kMessageLoopExclusiveRunLoopMode);
}

void CFRunLoopRemoveTimerFromAllModes(CFRunLoopRef rl,
                                      CFRunLoopTimerRef timer) {
  CFRunLoopRemoveTimer(rl, timer, kCFRunLoopCommonModes);
  CFRunLoopRemoveTimer(rl, timer, kMessageLoopExclusiveRunLoopMode);
}

void CFRunLoopAddObserverToAllModes(CFRunLoopRef rl,
                                    CFRunLoopObserverRef observer) {
  CFRunLoopAddObserver(rl, observer, kCFRunLoopCommonModes);
  CFRunLoopAddObserver(rl, observer, kMessageLoopExclusiveRunLoopMode);
}

void CFRunLoopRemoveObserverFromAllModes(CFRunLoopRef rl,
                                         CFRunLoopObserverRef observer) {
  CFRunLoopRemoveObserver(rl, observer, kCFRunLoopCommonModes);
  CFRunLoopRemoveObserver(rl, observer, kMessageLoopExclusiveRunLoopMode);
}

void NoOp(void* info) {
}

const CFTimeInterval kCFTimeIntervalMax =
    std::numeric_limits<CFTimeInterval>::max();

#if !defined(OS_IOS)
// Set to true if MessagePumpMac::Create() is called before NSApp is
// initialized.  Only accessed from the main thread.
bool g_not_using_cr_app = false;
#endif

// Call through to CFRunLoopTimerSetTolerance(), which is only available on
// OS X 10.9.
void SetTimerTolerance(CFRunLoopTimerRef timer, CFTimeInterval tolerance) {
  typedef void (*CFRunLoopTimerSetTolerancePtr)(CFRunLoopTimerRef timer,
      CFTimeInterval tolerance);

  static CFRunLoopTimerSetTolerancePtr settimertolerance_function_ptr;

  static dispatch_once_t get_timer_tolerance_function_ptr_once;
  dispatch_once(&get_timer_tolerance_function_ptr_once, ^{
      NSBundle* bundle =[NSBundle
        bundleWithPath:@"/System/Library/Frameworks/CoreFoundation.framework"];
      const char* path = [[bundle executablePath] fileSystemRepresentation];
      CHECK(path);
      void* library_handle = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
      CHECK(library_handle) << dlerror();
      settimertolerance_function_ptr =
          reinterpret_cast<CFRunLoopTimerSetTolerancePtr>(
              dlsym(library_handle, "CFRunLoopTimerSetTolerance"));

      dlclose(library_handle);
  });

  if (settimertolerance_function_ptr)
    settimertolerance_function_ptr(timer, tolerance);
}

}  // namespace

// static
const CFStringRef kMessageLoopExclusiveRunLoopMode =
    CFSTR("kMessageLoopExclusiveRunLoopMode");

// A scoper for autorelease pools created from message pump run loops.
// Avoids dirtying up the ScopedNSAutoreleasePool interface for the rare
// case where an autorelease pool needs to be passed in.
class MessagePumpScopedAutoreleasePool {
 public:
  explicit MessagePumpScopedAutoreleasePool(MessagePumpCFRunLoopBase* pump) :
      pool_(pump->CreateAutoreleasePool()) {
  }
   ~MessagePumpScopedAutoreleasePool() {
    [pool_ drain];
  }

 private:
  NSAutoreleasePool* pool_;
  DISALLOW_COPY_AND_ASSIGN(MessagePumpScopedAutoreleasePool);
};

// This class is used to instrument the MessagePump to gather various timing
// data about when the underlying run loop is entered, when it is waiting, and
// when it is servicing its delegate.
//
// The metrics are gathered as UMA-tracked histograms. To gather the data over
// time, sampling is used, such that a new histogram is created for each metric
// every |sampling_interval| for |sampling_duration|. After sampling is
// complete, this class deletes itself.
class MessagePumpInstrumentation {
 public:
  // Creates an instrument for the MessagePump on the current thread. Every
  // |sampling_interval|, a new histogram will be created to track the metrics
  // over time. After |sampling_duration|, this will delete itself, causing the
  // WeakPtr to go NULL.
  static WeakPtr<MessagePumpInstrumentation> Create(
      const TimeDelta& sampling_interval,
      const TimeDelta& sampling_duration) {
    MessagePumpInstrumentation* instrument =
        new MessagePumpInstrumentation(sampling_interval, sampling_duration);
    return instrument->weak_ptr_factory_.GetWeakPtr();
  }

  // Starts the timer that runs the sampling instrumentation. Can be called
  // multiple times as a noop.
  void StartIfNeeded() {
    if (timer_)
      return;

    sampling_start_time_ = generation_start_time_ = TimeTicks::Now();

    CFRunLoopTimerContext timer_context = { .info = this };
    timer_.reset(CFRunLoopTimerCreate(
        NULL,  // allocator
        (Time::Now() + sampling_interval_).ToCFAbsoluteTime(),
        sampling_interval_.InSecondsF(),
        0,  // flags
        0,  // order
        &MessagePumpInstrumentation::TimerFired,
        &timer_context));
    CFRunLoopAddTimerToAllModes(CFRunLoopGetCurrent(), timer_);
  }

  // Used to track kCFRunLoopEntry.
  void LoopEntered() {
    loop_run_times_.push(TimeTicks::Now());
  }

  // Used to track kCFRunLoopExit.
  void LoopExited() {
    TimeDelta duration = TimeTicks::Now() - loop_run_times_.top();
    loop_run_times_.pop();
    GetHistogram(LOOP_CYCLE)->AddTime(duration);
  }

  // Used to track kCFRunLoopBeforeWaiting.
  void WaitingStarted() {
    loop_wait_times_.push(TimeTicks::Now());
  }

  // Used to track kCFRunLoopAfterWaiting.
  void WaitingFinished() {
    TimeDelta duration = TimeTicks::Now() - loop_wait_times_.top();
    loop_wait_times_.pop();
    GetHistogram(LOOP_WAIT)->AddTime(duration);
  }

  // Used to track when the MessagePump will invoke its |delegate|.
  void WorkSourceEntered(MessagePump::Delegate* delegate) {
    work_source_times_.push(TimeTicks::Now());
    if (delegate) {
      size_t queue_size;
      TimeDelta queuing_delay;
      delegate->GetQueueingInformation(&queue_size, &queuing_delay);
      GetHistogram(QUEUE_SIZE)->Add(queue_size);
      GetHistogram(QUEUE_DELAY)->AddTime(queuing_delay);
    }
  }

  // Used to track the completion of servicing the MessagePump::Delegate.
  void WorkSourceExited() {
    TimeDelta duration = TimeTicks::Now() - work_source_times_.top();
    work_source_times_.pop();
    GetHistogram(WORK_SOURCE)->AddTime(duration);
  }

 private:
  enum HistogramEvent {
    // Time-based histograms:
    LOOP_CYCLE,  // LoopEntered/LoopExited
    LOOP_WAIT,  // WaitingStarted/WaitingEnded
    WORK_SOURCE,  // WorkSourceExited
    QUEUE_DELAY,  // WorkSourceEntered

    // Value-based histograms:
    // NOTE: Do not add value-based histograms before this event, only after.
    QUEUE_SIZE,  // WorkSourceEntered

    HISTOGRAM_EVENT_MAX,
  };

  MessagePumpInstrumentation(const TimeDelta& sampling_interval,
                             const TimeDelta& sampling_duration)
      : weak_ptr_factory_(this),
        sampling_interval_(sampling_interval),
        sampling_duration_(sampling_duration),
        sample_generation_(0) {
    // Create all the histogram objects that will be used for sampling.
    const char kHistogramName[] = "MessagePumpMac.%s.SampleMs.%" PRId64;
    for (TimeDelta i; i < sampling_duration_; i += sampling_interval_) {
      int64 sample = i.InMilliseconds();

      // Generate the time-based histograms.
      for (int j = LOOP_CYCLE; j < QUEUE_SIZE; ++j) {
        std::string name = StringPrintf(kHistogramName,
            NameForEnum(static_cast<HistogramEvent>(j)), sample);
        histograms_[j].push_back(
            Histogram::FactoryTimeGet(name, TimeDelta::FromMilliseconds(1),
                sampling_interval_, 50,
                HistogramBase::kUmaTargetedHistogramFlag));
      }

      // Generate the value-based histograms.
      for (int j = QUEUE_SIZE; j < HISTOGRAM_EVENT_MAX; ++j) {
        std::string name = StringPrintf(kHistogramName,
            NameForEnum(static_cast<HistogramEvent>(j)), sample);
        histograms_[j].push_back(
            Histogram::FactoryGet(name, 1, 10000, 50,
                HistogramBase::kUmaTargetedHistogramFlag));
      }
    }
  }

  ~MessagePumpInstrumentation() {
    if (timer_)
      CFRunLoopTimerInvalidate(timer_);
  }

  const char* NameForEnum(HistogramEvent event) {
    switch (event) {
      case LOOP_CYCLE: return "LoopCycle";
      case LOOP_WAIT: return "Waiting";
      case WORK_SOURCE: return "WorkSource";
      case QUEUE_DELAY: return "QueueingDelay";
      case QUEUE_SIZE: return "QueueSize";
      default:
        NOTREACHED();
        return NULL;
    }
  }

  static void TimerFired(CFRunLoopTimerRef timer, void* context) {
    static_cast<MessagePumpInstrumentation*>(context)->TimerFired();
  }

  // Called by the run loop when the sampling_interval_ has elapsed. Advances
  // the sample_generation_, which controls into which histogram data is
  // recorded, while recording and accounting for timer skew. Will delete this
  // object after |sampling_duration_| has elapsed.
  void TimerFired() {
    TimeTicks now = TimeTicks::Now();
    TimeDelta delta = now - generation_start_time_;

    // The timer fired, so advance the generation by at least one.
    ++sample_generation_;

    // To account for large timer skew/drift, advance the generation by any
    // more completed intervals.
    for (TimeDelta skew_advance = delta - sampling_interval_;
         skew_advance >= sampling_interval_;
         skew_advance -= sampling_interval_) {
      ++sample_generation_;
    }

    generation_start_time_ = now;
    if (now >= sampling_start_time_ + sampling_duration_)
      delete this;
  }

  HistogramBase* GetHistogram(HistogramEvent event) {
    DCHECK_LT(sample_generation_, histograms_[event].size());
    return histograms_[event][sample_generation_];
  }

  // Vends the pointer to the Create()or.
  WeakPtrFactory<MessagePumpInstrumentation> weak_ptr_factory_;

  // The interval and duration of the sampling.
  TimeDelta sampling_interval_;
  TimeDelta sampling_duration_;

  // The time at which sampling started.
  TimeTicks sampling_start_time_;

  // The timer that advances the sample_generation_ and sets the
  // generation_start_time_ for the current sample interval.
  base::ScopedCFTypeRef<CFRunLoopTimerRef> timer_;

  // The two-dimensional array of histograms. The first dimension is the
  // HistogramEvent type. The second is for the sampling intervals.
  std::vector<HistogramBase*> histograms_[HISTOGRAM_EVENT_MAX];

  // The index in the second dimension of histograms_, which controls in which
  // sampled histogram events are recorded.
  size_t sample_generation_;

  // The last time at which the timer fired. This is used to track timer skew
  // (i.e. it did not fire on time) and properly account for it when advancing
  // samle_generation_.
  TimeTicks generation_start_time_;

  // MessagePump activations can be nested. Use a stack for each of the
  // possibly reentrant HistogramEvent types to properly balance and calculate
  // the timing information.
  std::stack<TimeTicks> loop_run_times_;
  std::stack<TimeTicks> loop_wait_times_;
  std::stack<TimeTicks> work_source_times_;

  DISALLOW_COPY_AND_ASSIGN(MessagePumpInstrumentation);
};

// Must be called on the run loop thread.
MessagePumpCFRunLoopBase::MessagePumpCFRunLoopBase()
    : delegate_(NULL),
      delayed_work_fire_time_(kCFTimeIntervalMax),
      timer_slack_(base::TIMER_SLACK_NONE),
      nesting_level_(0),
      run_nesting_level_(0),
      deepest_nesting_level_(0),
      delegateless_work_(false),
      delegateless_idle_work_(false) {
  run_loop_ = CFRunLoopGetCurrent();
  CFRetain(run_loop_);

  // Set a repeating timer with a preposterous firing time and interval.  The
  // timer will effectively never fire as-is.  The firing time will be adjusted
  // as needed when ScheduleDelayedWork is called.
  CFRunLoopTimerContext timer_context = CFRunLoopTimerContext();
  timer_context.info = this;
  delayed_work_timer_ = CFRunLoopTimerCreate(NULL,                // allocator
                                             kCFTimeIntervalMax,  // fire time
                                             kCFTimeIntervalMax,  // interval
                                             0,                   // flags
                                             0,                   // priority
                                             RunDelayedWorkTimer,
                                             &timer_context);
  CFRunLoopAddTimerToAllModes(run_loop_, delayed_work_timer_);

  CFRunLoopSourceContext source_context = CFRunLoopSourceContext();
  source_context.info = this;
  source_context.perform = RunWorkSource;
  work_source_ = CFRunLoopSourceCreate(NULL,  // allocator
                                       1,     // priority
                                       &source_context);
  CFRunLoopAddSourceToAllModes(run_loop_, work_source_);

  source_context.perform = RunIdleWorkSource;
  idle_work_source_ = CFRunLoopSourceCreate(NULL,  // allocator
                                            2,     // priority
                                            &source_context);
  CFRunLoopAddSourceToAllModes(run_loop_, idle_work_source_);

  source_context.perform = RunNestingDeferredWorkSource;
  nesting_deferred_work_source_ = CFRunLoopSourceCreate(NULL,  // allocator
                                                        0,     // priority
                                                        &source_context);
  CFRunLoopAddSourceToAllModes(run_loop_, nesting_deferred_work_source_);

  CFRunLoopObserverContext observer_context = CFRunLoopObserverContext();
  observer_context.info = this;
  pre_wait_observer_ = CFRunLoopObserverCreate(NULL,  // allocator
                                               kCFRunLoopBeforeWaiting |
                                                  kCFRunLoopAfterWaiting,
                                               true,  // repeat
                                               0,     // priority
                                               StartOrEndWaitObserver,
                                               &observer_context);
  CFRunLoopAddObserverToAllModes(run_loop_, pre_wait_observer_);

  pre_source_observer_ = CFRunLoopObserverCreate(NULL,  // allocator
                                                 kCFRunLoopBeforeSources,
                                                 true,  // repeat
                                                 0,     // priority
                                                 PreSourceObserver,
                                                 &observer_context);
  CFRunLoopAddObserverToAllModes(run_loop_, pre_source_observer_);

  enter_exit_observer_ = CFRunLoopObserverCreate(NULL,  // allocator
                                                 kCFRunLoopEntry |
                                                     kCFRunLoopExit,
                                                 true,  // repeat
                                                 0,     // priority
                                                 EnterExitObserver,
                                                 &observer_context);
  CFRunLoopAddObserverToAllModes(run_loop_, enter_exit_observer_);
}

// Ideally called on the run loop thread.  If other run loops were running
// lower on the run loop thread's stack when this object was created, the
// same number of run loops must be running when this object is destroyed.
MessagePumpCFRunLoopBase::~MessagePumpCFRunLoopBase() {
  CFRunLoopRemoveObserverFromAllModes(run_loop_, enter_exit_observer_);
  CFRelease(enter_exit_observer_);

  CFRunLoopRemoveObserverFromAllModes(run_loop_, pre_source_observer_);
  CFRelease(pre_source_observer_);

  CFRunLoopRemoveObserverFromAllModes(run_loop_, pre_wait_observer_);
  CFRelease(pre_wait_observer_);

  CFRunLoopRemoveSourceFromAllModes(run_loop_, nesting_deferred_work_source_);
  CFRelease(nesting_deferred_work_source_);

  CFRunLoopRemoveSourceFromAllModes(run_loop_, idle_work_source_);
  CFRelease(idle_work_source_);

  CFRunLoopRemoveSourceFromAllModes(run_loop_, work_source_);
  CFRelease(work_source_);

  CFRunLoopRemoveTimerFromAllModes(run_loop_, delayed_work_timer_);
  CFRelease(delayed_work_timer_);

  CFRelease(run_loop_);
}

// Must be called on the run loop thread.
void MessagePumpCFRunLoopBase::Run(Delegate* delegate) {
  // nesting_level_ will be incremented in EnterExitRunLoop, so set
  // run_nesting_level_ accordingly.
  int last_run_nesting_level = run_nesting_level_;
  run_nesting_level_ = nesting_level_ + 1;

  Delegate* last_delegate = delegate_;
  SetDelegate(delegate);

  DoRun(delegate);

  // Restore the previous state of the object.
  SetDelegate(last_delegate);
  run_nesting_level_ = last_run_nesting_level;
}

void MessagePumpCFRunLoopBase::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;

  if (delegate) {
    // If any work showed up but could not be dispatched for want of a
    // delegate, set it up for dispatch again now that a delegate is
    // available.
    if (delegateless_work_) {
      CFRunLoopSourceSignal(work_source_);
      delegateless_work_ = false;
    }
    if (delegateless_idle_work_) {
      CFRunLoopSourceSignal(idle_work_source_);
      delegateless_idle_work_ = false;
    }
  }
}

void MessagePumpCFRunLoopBase::EnableInstrumentation() {
  instrumentation_ = MessagePumpInstrumentation::Create(
      TimeDelta::FromSeconds(1), TimeDelta::FromSeconds(15));
}

// May be called on any thread.
void MessagePumpCFRunLoopBase::ScheduleWork() {
  CFRunLoopSourceSignal(work_source_);
  CFRunLoopWakeUp(run_loop_);
}

// Must be called on the run loop thread.
void MessagePumpCFRunLoopBase::ScheduleDelayedWork(
    const TimeTicks& delayed_work_time) {
  TimeDelta delta = delayed_work_time - TimeTicks::Now();
  delayed_work_fire_time_ = CFAbsoluteTimeGetCurrent() + delta.InSecondsF();
  CFRunLoopTimerSetNextFireDate(delayed_work_timer_, delayed_work_fire_time_);
  if (timer_slack_ == TIMER_SLACK_MAXIMUM) {
    SetTimerTolerance(delayed_work_timer_, delta.InSecondsF() * 0.5);
  } else {
    SetTimerTolerance(delayed_work_timer_, 0);
  }
}

void MessagePumpCFRunLoopBase::SetTimerSlack(TimerSlack timer_slack) {
  timer_slack_ = timer_slack;
}

// Called from the run loop.
// static
void MessagePumpCFRunLoopBase::RunDelayedWorkTimer(CFRunLoopTimerRef timer,
                                                   void* info) {
  MessagePumpCFRunLoopBase* self = static_cast<MessagePumpCFRunLoopBase*>(info);

  // The timer won't fire again until it's reset.
  self->delayed_work_fire_time_ = kCFTimeIntervalMax;

  // CFRunLoopTimers fire outside of the priority scheme for CFRunLoopSources.
  // In order to establish the proper priority in which work and delayed work
  // are processed one for one, the timer used to schedule delayed work must
  // signal a CFRunLoopSource used to dispatch both work and delayed work.
  CFRunLoopSourceSignal(self->work_source_);
}

// Called from the run loop.
// static
void MessagePumpCFRunLoopBase::RunWorkSource(void* info) {
  MessagePumpCFRunLoopBase* self = static_cast<MessagePumpCFRunLoopBase*>(info);
  self->RunWork();
}

// Called by MessagePumpCFRunLoopBase::RunWorkSource.
bool MessagePumpCFRunLoopBase::RunWork() {
  if (!delegate_) {
    // This point can be reached with a NULL delegate_ if Run is not on the
    // stack but foreign code is spinning the CFRunLoop.  Arrange to come back
    // here when a delegate is available.
    delegateless_work_ = true;
    return false;
  }

  if (instrumentation_)
    instrumentation_->WorkSourceEntered(delegate_);

  // The NSApplication-based run loop only drains the autorelease pool at each
  // UI event (NSEvent).  The autorelease pool is not drained for each
  // CFRunLoopSource target that's run.  Use a local pool for any autoreleased
  // objects if the app is not currently handling a UI event to ensure they're
  // released promptly even in the absence of UI events.
  MessagePumpScopedAutoreleasePool autorelease_pool(this);

  // Call DoWork and DoDelayedWork once, and if something was done, arrange to
  // come back here again as long as the loop is still running.
  bool did_work = delegate_->DoWork();
  bool resignal_work_source = did_work;

  TimeTicks next_time;
  delegate_->DoDelayedWork(&next_time);
  if (!did_work) {
    // Determine whether there's more delayed work, and if so, if it needs to
    // be done at some point in the future or if it's already time to do it.
    // Only do these checks if did_work is false. If did_work is true, this
    // function, and therefore any additional delayed work, will get another
    // chance to run before the loop goes to sleep.
    bool more_delayed_work = !next_time.is_null();
    if (more_delayed_work) {
      TimeDelta delay = next_time - TimeTicks::Now();
      if (delay > TimeDelta()) {
        // There's more delayed work to be done in the future.
        ScheduleDelayedWork(next_time);
      } else {
        // There's more delayed work to be done, and its time is in the past.
        // Arrange to come back here directly as long as the loop is still
        // running.
        resignal_work_source = true;
      }
    }
  }

  if (resignal_work_source) {
    CFRunLoopSourceSignal(work_source_);
  }

  if (instrumentation_)
    instrumentation_->WorkSourceExited();

  return resignal_work_source;
}

// Called from the run loop.
// static
void MessagePumpCFRunLoopBase::RunIdleWorkSource(void* info) {
  MessagePumpCFRunLoopBase* self = static_cast<MessagePumpCFRunLoopBase*>(info);
  self->RunIdleWork();
}

// Called by MessagePumpCFRunLoopBase::RunIdleWorkSource.
bool MessagePumpCFRunLoopBase::RunIdleWork() {
  if (!delegate_) {
    // This point can be reached with a NULL delegate_ if Run is not on the
    // stack but foreign code is spinning the CFRunLoop.  Arrange to come back
    // here when a delegate is available.
    delegateless_idle_work_ = true;
    return false;
  }

  // The NSApplication-based run loop only drains the autorelease pool at each
  // UI event (NSEvent).  The autorelease pool is not drained for each
  // CFRunLoopSource target that's run.  Use a local pool for any autoreleased
  // objects if the app is not currently handling a UI event to ensure they're
  // released promptly even in the absence of UI events.
  MessagePumpScopedAutoreleasePool autorelease_pool(this);

  // Call DoIdleWork once, and if something was done, arrange to come back here
  // again as long as the loop is still running.
  bool did_work = delegate_->DoIdleWork();
  if (did_work) {
    CFRunLoopSourceSignal(idle_work_source_);
  }

  return did_work;
}

// Called from the run loop.
// static
void MessagePumpCFRunLoopBase::RunNestingDeferredWorkSource(void* info) {
  MessagePumpCFRunLoopBase* self = static_cast<MessagePumpCFRunLoopBase*>(info);
  self->RunNestingDeferredWork();
}

// Called by MessagePumpCFRunLoopBase::RunNestingDeferredWorkSource.
bool MessagePumpCFRunLoopBase::RunNestingDeferredWork() {
  if (!delegate_) {
    // This point can be reached with a NULL delegate_ if Run is not on the
    // stack but foreign code is spinning the CFRunLoop.  There's no sense in
    // attempting to do any work or signalling the work sources because
    // without a delegate, work is not possible.
    return false;
  }

  // Immediately try work in priority order.
  if (!RunWork()) {
    if (!RunIdleWork()) {
      return false;
    }
  } else {
    // Work was done.  Arrange for the loop to try non-nestable idle work on
    // a subsequent pass.
    CFRunLoopSourceSignal(idle_work_source_);
  }

  return true;
}

// Called before the run loop goes to sleep or exits, or processes sources.
void MessagePumpCFRunLoopBase::MaybeScheduleNestingDeferredWork() {
  // deepest_nesting_level_ is set as run loops are entered.  If the deepest
  // level encountered is deeper than the current level, a nested loop
  // (relative to the current level) ran since the last time nesting-deferred
  // work was scheduled.  When that situation is encountered, schedule
  // nesting-deferred work in case any work was deferred because nested work
  // was disallowed.
  if (deepest_nesting_level_ > nesting_level_) {
    deepest_nesting_level_ = nesting_level_;
    CFRunLoopSourceSignal(nesting_deferred_work_source_);
  }
}

// Called from the run loop.
// static
void MessagePumpCFRunLoopBase::StartOrEndWaitObserver(
    CFRunLoopObserverRef observer,
    CFRunLoopActivity activity,
    void* info) {
  MessagePumpCFRunLoopBase* self = static_cast<MessagePumpCFRunLoopBase*>(info);

  if (activity == kCFRunLoopAfterWaiting) {
    if (self->instrumentation_)
      self->instrumentation_->WaitingFinished();
    return;
  }

  // Attempt to do some idle work before going to sleep.
  self->RunIdleWork();

  // The run loop is about to go to sleep.  If any of the work done since it
  // started or woke up resulted in a nested run loop running,
  // nesting-deferred work may have accumulated.  Schedule it for processing
  // if appropriate.
  self->MaybeScheduleNestingDeferredWork();

  if (self->instrumentation_)
    self->instrumentation_->WaitingStarted();
}

// Called from the run loop.
// static
void MessagePumpCFRunLoopBase::PreSourceObserver(CFRunLoopObserverRef observer,
                                                 CFRunLoopActivity activity,
                                                 void* info) {
  MessagePumpCFRunLoopBase* self = static_cast<MessagePumpCFRunLoopBase*>(info);

  // The run loop has reached the top of the loop and is about to begin
  // processing sources.  If the last iteration of the loop at this nesting
  // level did not sleep or exit, nesting-deferred work may have accumulated
  // if a nested loop ran.  Schedule nesting-deferred work for processing if
  // appropriate.
  self->MaybeScheduleNestingDeferredWork();
}

// Called from the run loop.
// static
void MessagePumpCFRunLoopBase::EnterExitObserver(CFRunLoopObserverRef observer,
                                                 CFRunLoopActivity activity,
                                                 void* info) {
  MessagePumpCFRunLoopBase* self = static_cast<MessagePumpCFRunLoopBase*>(info);

  switch (activity) {
    case kCFRunLoopEntry:
      if (self->instrumentation_)
        self->instrumentation_->LoopEntered();

      ++self->nesting_level_;
      if (self->nesting_level_ > self->deepest_nesting_level_) {
        self->deepest_nesting_level_ = self->nesting_level_;
      }
      break;

    case kCFRunLoopExit:
      // Not all run loops go to sleep.  If a run loop is stopped before it
      // goes to sleep due to a CFRunLoopStop call, or if the timeout passed
      // to CFRunLoopRunInMode expires, the run loop may proceed directly from
      // handling sources to exiting without any sleep.  This most commonly
      // occurs when CFRunLoopRunInMode is passed a timeout of 0, causing it
      // to make a single pass through the loop and exit without sleep.  Some
      // native loops use CFRunLoop in this way.  Because StartOrEndWaitObserver
      // will not be called in these case, MaybeScheduleNestingDeferredWork
      // needs to be called here, as the run loop exits.
      //
      // MaybeScheduleNestingDeferredWork consults self->nesting_level_
      // to determine whether to schedule nesting-deferred work.  It expects
      // the nesting level to be set to the depth of the loop that is going
      // to sleep or exiting.  It must be called before decrementing the
      // value so that the value still corresponds to the level of the exiting
      // loop.
      self->MaybeScheduleNestingDeferredWork();
      --self->nesting_level_;

      if (self->instrumentation_)
        self->instrumentation_->LoopExited();
      break;

    default:
      break;
  }

  self->EnterExitRunLoop(activity);
}

// Called by MessagePumpCFRunLoopBase::EnterExitRunLoop.  The default
// implementation is a no-op.
void MessagePumpCFRunLoopBase::EnterExitRunLoop(CFRunLoopActivity activity) {
}

// Base version returns a standard NSAutoreleasePool.
AutoreleasePoolType* MessagePumpCFRunLoopBase::CreateAutoreleasePool() {
  return [[NSAutoreleasePool alloc] init];
}

MessagePumpCFRunLoop::MessagePumpCFRunLoop()
    : quit_pending_(false) {
}

MessagePumpCFRunLoop::~MessagePumpCFRunLoop() {}

// Called by MessagePumpCFRunLoopBase::DoRun.  If other CFRunLoopRun loops were
// running lower on the run loop thread's stack when this object was created,
// the same number of CFRunLoopRun loops must be running for the outermost call
// to Run.  Run/DoRun are reentrant after that point.
void MessagePumpCFRunLoop::DoRun(Delegate* delegate) {
  // This is completely identical to calling CFRunLoopRun(), except autorelease
  // pool management is introduced.
  int result;
  do {
    MessagePumpScopedAutoreleasePool autorelease_pool(this);
    result = CFRunLoopRunInMode(kCFRunLoopDefaultMode,
                                kCFTimeIntervalMax,
                                false);
  } while (result != kCFRunLoopRunStopped && result != kCFRunLoopRunFinished);
}

// Must be called on the run loop thread.
void MessagePumpCFRunLoop::Quit() {
  // Stop the innermost run loop managed by this MessagePumpCFRunLoop object.
  if (nesting_level() == run_nesting_level()) {
    // This object is running the innermost loop, just stop it.
    CFRunLoopStop(run_loop());
  } else {
    // There's another loop running inside the loop managed by this object.
    // In other words, someone else called CFRunLoopRunInMode on the same
    // thread, deeper on the stack than the deepest Run call.  Don't preempt
    // other run loops, just mark this object to quit the innermost Run as
    // soon as the other inner loops not managed by Run are done.
    quit_pending_ = true;
  }
}

// Called by MessagePumpCFRunLoopBase::EnterExitObserver.
void MessagePumpCFRunLoop::EnterExitRunLoop(CFRunLoopActivity activity) {
  if (activity == kCFRunLoopExit &&
      nesting_level() == run_nesting_level() &&
      quit_pending_) {
    // Quit was called while loops other than those managed by this object
    // were running further inside a run loop managed by this object.  Now
    // that all unmanaged inner run loops are gone, stop the loop running
    // just inside Run.
    CFRunLoopStop(run_loop());
    quit_pending_ = false;
  }
}

MessagePumpNSRunLoop::MessagePumpNSRunLoop()
    : keep_running_(true) {
  CFRunLoopSourceContext source_context = CFRunLoopSourceContext();
  source_context.perform = NoOp;
  quit_source_ = CFRunLoopSourceCreate(NULL,  // allocator
                                       0,     // priority
                                       &source_context);
  CFRunLoopAddSourceToAllModes(run_loop(), quit_source_);
}

MessagePumpNSRunLoop::~MessagePumpNSRunLoop() {
  CFRunLoopRemoveSourceFromAllModes(run_loop(), quit_source_);
  CFRelease(quit_source_);
}

void MessagePumpNSRunLoop::DoRun(Delegate* delegate) {
  while (keep_running_) {
    // NSRunLoop manages autorelease pools itself.
    [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                             beforeDate:[NSDate distantFuture]];
  }

  keep_running_ = true;
}

void MessagePumpNSRunLoop::Quit() {
  keep_running_ = false;
  CFRunLoopSourceSignal(quit_source_);
  CFRunLoopWakeUp(run_loop());
}

#if defined(OS_IOS)
MessagePumpUIApplication::MessagePumpUIApplication()
    : run_loop_(NULL) {
}

MessagePumpUIApplication::~MessagePumpUIApplication() {}

void MessagePumpUIApplication::DoRun(Delegate* delegate) {
  NOTREACHED();
}

void MessagePumpUIApplication::Quit() {
  NOTREACHED();
}

void MessagePumpUIApplication::Attach(Delegate* delegate) {
  DCHECK(!run_loop_);
  run_loop_ = new RunLoop();
  CHECK(run_loop_->BeforeRun());
  SetDelegate(delegate);
}

#else

MessagePumpNSApplication::MessagePumpNSApplication()
    : keep_running_(true),
      running_own_loop_(false) {
  EnableInstrumentation();
}

MessagePumpNSApplication::~MessagePumpNSApplication() {}

void MessagePumpNSApplication::DoRun(Delegate* delegate) {
  if (instrumentation_)
    instrumentation_->StartIfNeeded();

  bool last_running_own_loop_ = running_own_loop_;

  // NSApp must be initialized by calling:
  // [{some class which implements CrAppProtocol} sharedApplication]
  // Most likely candidates are CrApplication or BrowserCrApplication.
  // These can be initialized from C++ code by calling
  // RegisterCrApp() or RegisterBrowserCrApp().
  CHECK(NSApp);

  if (![NSApp isRunning]) {
    running_own_loop_ = false;
    // NSApplication manages autorelease pools itself when run this way.
    [NSApp run];
  } else {
    running_own_loop_ = true;
    NSDate* distant_future = [NSDate distantFuture];
    while (keep_running_) {
      MessagePumpScopedAutoreleasePool autorelease_pool(this);
      NSEvent* event = [NSApp nextEventMatchingMask:NSAnyEventMask
                                          untilDate:distant_future
                                             inMode:NSDefaultRunLoopMode
                                            dequeue:YES];
      if (event) {
        [NSApp sendEvent:event];
      }
    }
    keep_running_ = true;
  }

  running_own_loop_ = last_running_own_loop_;
}

void MessagePumpNSApplication::Quit() {
  if (!running_own_loop_) {
    [[NSApplication sharedApplication] stop:nil];
  } else {
    keep_running_ = false;
  }

  // Send a fake event to wake the loop up.
  [NSApp postEvent:[NSEvent otherEventWithType:NSApplicationDefined
                                      location:NSZeroPoint
                                 modifierFlags:0
                                     timestamp:0
                                  windowNumber:0
                                       context:NULL
                                       subtype:0
                                         data1:0
                                         data2:0]
           atStart:NO];
}

MessagePumpCrApplication::MessagePumpCrApplication() {
}

MessagePumpCrApplication::~MessagePumpCrApplication() {
}

// Prevents an autorelease pool from being created if the app is in the midst of
// handling a UI event because various parts of AppKit depend on objects that
// are created while handling a UI event to be autoreleased in the event loop.
// An example of this is NSWindowController. When a window with a window
// controller is closed it goes through a stack like this:
// (Several stack frames elided for clarity)
//
// #0 [NSWindowController autorelease]
// #1 DoAClose
// #2 MessagePumpCFRunLoopBase::DoWork()
// #3 [NSRunLoop run]
// #4 [NSButton performClick:]
// #5 [NSWindow sendEvent:]
// #6 [NSApp sendEvent:]
// #7 [NSApp run]
//
// -performClick: spins a nested run loop. If the pool created in DoWork was a
// standard NSAutoreleasePool, it would release the objects that were
// autoreleased into it once DoWork released it. This would cause the window
// controller, which autoreleased itself in frame #0, to release itself, and
// possibly free itself. Unfortunately this window controller controls the
// window in frame #5. When the stack is unwound to frame #5, the window would
// no longer exists and crashes may occur. Apple gets around this by never
// releasing the pool it creates in frame #4, and letting frame #7 clean it up
// when it cleans up the pool that wraps frame #7. When an autorelease pool is
// released it releases all other pools that were created after it on the
// autorelease pool stack.
//
// CrApplication is responsible for setting handlingSendEvent to true just
// before it sends the event through the event handling mechanism, and
// returning it to its previous value once the event has been sent.
AutoreleasePoolType* MessagePumpCrApplication::CreateAutoreleasePool() {
  if (MessagePumpMac::IsHandlingSendEvent())
    return nil;
  return MessagePumpNSApplication::CreateAutoreleasePool();
}

// static
bool MessagePumpMac::UsingCrApp() {
  DCHECK([NSThread isMainThread]);

  // If NSApp is still not initialized, then the subclass used cannot
  // be determined.
  DCHECK(NSApp);

  // The pump was created using MessagePumpNSApplication.
  if (g_not_using_cr_app)
    return false;

  return [NSApp conformsToProtocol:@protocol(CrAppProtocol)];
}

// static
bool MessagePumpMac::IsHandlingSendEvent() {
  DCHECK([NSApp conformsToProtocol:@protocol(CrAppProtocol)]);
  NSObject<CrAppProtocol>* app = static_cast<NSObject<CrAppProtocol>*>(NSApp);
  return [app isHandlingSendEvent];
}
#endif  // !defined(OS_IOS)

// static
MessagePump* MessagePumpMac::Create() {
  if ([NSThread isMainThread]) {
#if defined(OS_IOS)
    return new MessagePumpUIApplication;
#else
    if ([NSApp conformsToProtocol:@protocol(CrAppProtocol)])
      return new MessagePumpCrApplication;

    // The main-thread MessagePump implementations REQUIRE an NSApp.
    // Executables which have specific requirements for their
    // NSApplication subclass should initialize appropriately before
    // creating an event loop.
    [NSApplication sharedApplication];
    g_not_using_cr_app = true;
    return new MessagePumpNSApplication;
#endif
  }

  return new MessagePumpNSRunLoop;
}

}  // namespace base
