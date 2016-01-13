// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"

#include <algorithm>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_pump_default.h"
#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/third_party/dynamic_annotations/dynamic_annotations.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread_local.h"
#include "base/time/time.h"
#include "base/tracked_objects.h"

#if defined(OS_MACOSX)
#include "base/message_loop/message_pump_mac.h"
#endif
#if defined(OS_POSIX) && !defined(OS_IOS)
#include "base/message_loop/message_pump_libevent.h"
#endif
#if defined(OS_ANDROID)
#include "base/message_loop/message_pump_android.h"
#endif
#if defined(USE_GLIB)
#include "base/message_loop/message_pump_glib.h"
#endif

namespace base {

namespace {

// A lazily created thread local storage for quick access to a thread's message
// loop, if one exists.  This should be safe and free of static constructors.
LazyInstance<base::ThreadLocalPointer<MessageLoop> >::Leaky lazy_tls_ptr =
    LAZY_INSTANCE_INITIALIZER;

// Logical events for Histogram profiling. Run with -message-loop-histogrammer
// to get an accounting of messages and actions taken on each thread.
const int kTaskRunEvent = 0x1;
#if !defined(OS_NACL)
const int kTimerEvent = 0x2;

// Provide range of message IDs for use in histogramming and debug display.
const int kLeastNonZeroMessageId = 1;
const int kMaxMessageId = 1099;
const int kNumberOfDistinctMessagesDisplayed = 1100;

// Provide a macro that takes an expression (such as a constant, or macro
// constant) and creates a pair to initalize an array of pairs.  In this case,
// our pair consists of the expressions value, and the "stringized" version
// of the expression (i.e., the exrpression put in quotes).  For example, if
// we have:
//    #define FOO 2
//    #define BAR 5
// then the following:
//    VALUE_TO_NUMBER_AND_NAME(FOO + BAR)
// will expand to:
//   {7, "FOO + BAR"}
// We use the resulting array as an argument to our histogram, which reads the
// number as a bucket identifier, and proceeds to use the corresponding name
// in the pair (i.e., the quoted string) when printing out a histogram.
#define VALUE_TO_NUMBER_AND_NAME(name) {name, #name},

const LinearHistogram::DescriptionPair event_descriptions_[] = {
  // Provide some pretty print capability in our histogram for our internal
  // messages.

  // A few events we handle (kindred to messages), and used to profile actions.
  VALUE_TO_NUMBER_AND_NAME(kTaskRunEvent)
  VALUE_TO_NUMBER_AND_NAME(kTimerEvent)

  {-1, NULL}  // The list must be null terminated, per API to histogram.
};
#endif  // !defined(OS_NACL)

bool enable_histogrammer_ = false;

MessageLoop::MessagePumpFactory* message_pump_for_ui_factory_ = NULL;

// Returns true if MessagePump::ScheduleWork() must be called one
// time for every task that is added to the MessageLoop incoming queue.
bool AlwaysNotifyPump(MessageLoop::Type type) {
#if defined(OS_ANDROID)
  return type == MessageLoop::TYPE_UI || type == MessageLoop::TYPE_JAVA;
#else
  return false;
#endif
}

#if defined(OS_IOS)
typedef MessagePumpIOSForIO MessagePumpForIO;
#elif defined(OS_NACL)
typedef MessagePumpDefault MessagePumpForIO;
#elif defined(OS_POSIX)
typedef MessagePumpLibevent MessagePumpForIO;
#endif

MessagePumpForIO* ToPumpIO(MessagePump* pump) {
  return static_cast<MessagePumpForIO*>(pump);
}

}  // namespace

//------------------------------------------------------------------------------

MessageLoop::TaskObserver::TaskObserver() {
}

MessageLoop::TaskObserver::~TaskObserver() {
}

MessageLoop::DestructionObserver::~DestructionObserver() {
}

//------------------------------------------------------------------------------

MessageLoop::MessageLoop(Type type)
    : type_(type),
      nestable_tasks_allowed_(true),
#if defined(OS_WIN)
      os_modal_loop_(false),
#endif  // OS_WIN
      message_histogram_(NULL),
      run_loop_(NULL) {
  Init();

  pump_ = CreateMessagePumpForType(type).Pass();
}

MessageLoop::MessageLoop(scoped_ptr<MessagePump> pump)
    : pump_(pump.Pass()),
      type_(TYPE_CUSTOM),
      nestable_tasks_allowed_(true),
#if defined(OS_WIN)
      os_modal_loop_(false),
#endif  // OS_WIN
      message_histogram_(NULL),
      run_loop_(NULL) {
  DCHECK(pump_.get());
  Init();
}

MessageLoop::~MessageLoop() {
  DCHECK_EQ(this, current());

  DCHECK(!run_loop_);

  // Clean up any unprocessed tasks, but take care: deleting a task could
  // result in the addition of more tasks (e.g., via DeleteSoon).  We set a
  // limit on the number of times we will allow a deleted task to generate more
  // tasks.  Normally, we should only pass through this loop once or twice.  If
  // we end up hitting the loop limit, then it is probably due to one task that
  // is being stubborn.  Inspect the queues to see who is left.
  bool did_work;
  for (int i = 0; i < 100; ++i) {
    DeletePendingTasks();
    ReloadWorkQueue();
    // If we end up with empty queues, then break out of the loop.
    did_work = DeletePendingTasks();
    if (!did_work)
      break;
  }
  DCHECK(!did_work);

  // Let interested parties have one last shot at accessing this.
  FOR_EACH_OBSERVER(DestructionObserver, destruction_observers_,
                    WillDestroyCurrentMessageLoop());

  thread_task_runner_handle_.reset();

  // Tell the incoming queue that we are dying.
  incoming_task_queue_->WillDestroyCurrentMessageLoop();
  incoming_task_queue_ = NULL;
  message_loop_proxy_ = NULL;

  // OK, now make it so that no one can find us.
  lazy_tls_ptr.Pointer()->Set(NULL);
}

// static
MessageLoop* MessageLoop::current() {
  // TODO(darin): sadly, we cannot enable this yet since people call us even
  // when they have no intention of using us.
  // DCHECK(loop) << "Ouch, did you forget to initialize me?";
  return lazy_tls_ptr.Pointer()->Get();
}

// static
void MessageLoop::EnableHistogrammer(bool enable) {
  enable_histogrammer_ = enable;
}

// static
bool MessageLoop::InitMessagePumpForUIFactory(MessagePumpFactory* factory) {
  if (message_pump_for_ui_factory_)
    return false;

  message_pump_for_ui_factory_ = factory;
  return true;
}

// static
scoped_ptr<MessagePump> MessageLoop::CreateMessagePumpForType(Type type) {
// TODO(rvargas): Get rid of the OS guards.
#if defined(USE_GLIB) && !defined(OS_NACL)
  typedef MessagePumpGlib MessagePumpForUI;
#elif defined(OS_LINUX) && !defined(OS_NACL)
  typedef MessagePumpLibevent MessagePumpForUI;
#endif

#if defined(OS_IOS) || defined(OS_MACOSX)
#define MESSAGE_PUMP_UI scoped_ptr<MessagePump>(MessagePumpMac::Create())
#elif defined(OS_NACL)
// Currently NaCl doesn't have a UI MessageLoop.
// TODO(abarth): Figure out if we need this.
#define MESSAGE_PUMP_UI scoped_ptr<MessagePump>()
#else
#define MESSAGE_PUMP_UI scoped_ptr<MessagePump>(new MessagePumpForUI())
#endif

  if (type == MessageLoop::TYPE_UI) {
    if (message_pump_for_ui_factory_)
      return message_pump_for_ui_factory_();
    return MESSAGE_PUMP_UI;
  }
  if (type == MessageLoop::TYPE_IO)
    return scoped_ptr<MessagePump>(new MessagePumpForIO());

#if defined(OS_ANDROID)
  if (type == MessageLoop::TYPE_JAVA)
    return scoped_ptr<MessagePump>(new MessagePumpForUI());
#endif

  DCHECK_EQ(MessageLoop::TYPE_DEFAULT, type);
  return scoped_ptr<MessagePump>(new MessagePumpDefault());
}

void MessageLoop::AddDestructionObserver(
    DestructionObserver* destruction_observer) {
  DCHECK_EQ(this, current());
  destruction_observers_.AddObserver(destruction_observer);
}

void MessageLoop::RemoveDestructionObserver(
    DestructionObserver* destruction_observer) {
  DCHECK_EQ(this, current());
  destruction_observers_.RemoveObserver(destruction_observer);
}

void MessageLoop::PostTask(
    const tracked_objects::Location& from_here,
    const Closure& task) {
  DCHECK(!task.is_null()) << from_here.ToString();
  incoming_task_queue_->AddToIncomingQueue(from_here, task, TimeDelta(), true);
}

void MessageLoop::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const Closure& task,
    TimeDelta delay) {
  DCHECK(!task.is_null()) << from_here.ToString();
  incoming_task_queue_->AddToIncomingQueue(from_here, task, delay, true);
}

void MessageLoop::PostNonNestableTask(
    const tracked_objects::Location& from_here,
    const Closure& task) {
  DCHECK(!task.is_null()) << from_here.ToString();
  incoming_task_queue_->AddToIncomingQueue(from_here, task, TimeDelta(), false);
}

void MessageLoop::PostNonNestableDelayedTask(
    const tracked_objects::Location& from_here,
    const Closure& task,
    TimeDelta delay) {
  DCHECK(!task.is_null()) << from_here.ToString();
  incoming_task_queue_->AddToIncomingQueue(from_here, task, delay, false);
}

void MessageLoop::Run() {
  RunLoop run_loop;
  run_loop.Run();
}

void MessageLoop::RunUntilIdle() {
  RunLoop run_loop;
  run_loop.RunUntilIdle();
}

void MessageLoop::QuitWhenIdle() {
  DCHECK_EQ(this, current());
  if (run_loop_) {
    run_loop_->quit_when_idle_received_ = true;
  } else {
    NOTREACHED() << "Must be inside Run to call Quit";
  }
}

void MessageLoop::QuitNow() {
  DCHECK_EQ(this, current());
  if (run_loop_) {
    pump_->Quit();
  } else {
    NOTREACHED() << "Must be inside Run to call Quit";
  }
}

bool MessageLoop::IsType(Type type) const {
  return type_ == type;
}

static void QuitCurrentWhenIdle() {
  MessageLoop::current()->QuitWhenIdle();
}

// static
Closure MessageLoop::QuitWhenIdleClosure() {
  return Bind(&QuitCurrentWhenIdle);
}

void MessageLoop::SetNestableTasksAllowed(bool allowed) {
  if (allowed) {
    // Kick the native pump just in case we enter a OS-driven nested message
    // loop.
    pump_->ScheduleWork();
  }
  nestable_tasks_allowed_ = allowed;
}

bool MessageLoop::NestableTasksAllowed() const {
  return nestable_tasks_allowed_;
}

bool MessageLoop::IsNested() {
  return run_loop_->run_depth_ > 1;
}

void MessageLoop::AddTaskObserver(TaskObserver* task_observer) {
  DCHECK_EQ(this, current());
  task_observers_.AddObserver(task_observer);
}

void MessageLoop::RemoveTaskObserver(TaskObserver* task_observer) {
  DCHECK_EQ(this, current());
  task_observers_.RemoveObserver(task_observer);
}

bool MessageLoop::is_running() const {
  DCHECK_EQ(this, current());
  return run_loop_ != NULL;
}

bool MessageLoop::IsHighResolutionTimerEnabledForTesting() {
  return incoming_task_queue_->IsHighResolutionTimerEnabledForTesting();
}

bool MessageLoop::IsIdleForTesting() {
  // We only check the imcoming queue|, since we don't want to lock the work
  // queue.
  return incoming_task_queue_->IsIdleForTesting();
}

//------------------------------------------------------------------------------

void MessageLoop::Init() {
  DCHECK(!current()) << "should only have one message loop per thread";
  lazy_tls_ptr.Pointer()->Set(this);

  incoming_task_queue_ = new internal::IncomingTaskQueue(this);
  message_loop_proxy_ =
      new internal::MessageLoopProxyImpl(incoming_task_queue_);
  thread_task_runner_handle_.reset(
      new ThreadTaskRunnerHandle(message_loop_proxy_));
}

void MessageLoop::RunHandler() {
  DCHECK_EQ(this, current());

  StartHistogrammer();

#if defined(OS_WIN)
  if (run_loop_->dispatcher_ && type() == TYPE_UI) {
    static_cast<MessagePumpForUI*>(pump_.get())->
        RunWithDispatcher(this, run_loop_->dispatcher_);
    return;
  }
#endif

  pump_->Run(this);
}

bool MessageLoop::ProcessNextDelayedNonNestableTask() {
  if (run_loop_->run_depth_ != 1)
    return false;

  if (deferred_non_nestable_work_queue_.empty())
    return false;

  PendingTask pending_task = deferred_non_nestable_work_queue_.front();
  deferred_non_nestable_work_queue_.pop();

  RunTask(pending_task);
  return true;
}

void MessageLoop::RunTask(const PendingTask& pending_task) {
  tracked_objects::TrackedTime start_time =
      tracked_objects::ThreadData::NowForStartOfRun(pending_task.birth_tally);

  TRACE_EVENT_FLOW_END1(TRACE_DISABLED_BY_DEFAULT("toplevel.flow"),
      "MessageLoop::PostTask", TRACE_ID_MANGLE(GetTaskTraceID(pending_task)),
      "queue_duration",
      (start_time - pending_task.EffectiveTimePosted()).InMilliseconds());
  // When tracing memory for posted tasks it's more valuable to attribute the
  // memory allocations to the source function than generically to "RunTask".
  TRACE_EVENT_WITH_MEMORY_TAG2(
      "toplevel", "MessageLoop::RunTask",
      pending_task.posted_from.function_name(),  // Name for memory tracking.
      "src_file", pending_task.posted_from.file_name(),
      "src_func", pending_task.posted_from.function_name());

  DCHECK(nestable_tasks_allowed_);
  // Execute the task and assume the worst: It is probably not reentrant.
  nestable_tasks_allowed_ = false;

  // Before running the task, store the program counter where it was posted
  // and deliberately alias it to ensure it is on the stack if the task
  // crashes. Be careful not to assume that the variable itself will have the
  // expected value when displayed by the optimizer in an optimized build.
  // Look at a memory dump of the stack.
  const void* program_counter =
      pending_task.posted_from.program_counter();
  debug::Alias(&program_counter);

  HistogramEvent(kTaskRunEvent);

  FOR_EACH_OBSERVER(TaskObserver, task_observers_,
                    WillProcessTask(pending_task));
  pending_task.task.Run();
  FOR_EACH_OBSERVER(TaskObserver, task_observers_,
                    DidProcessTask(pending_task));

  tracked_objects::ThreadData::TallyRunOnNamedThreadIfTracking(pending_task,
      start_time, tracked_objects::ThreadData::NowForEndOfRun());

  nestable_tasks_allowed_ = true;
}

bool MessageLoop::DeferOrRunPendingTask(const PendingTask& pending_task) {
  if (pending_task.nestable || run_loop_->run_depth_ == 1) {
    RunTask(pending_task);
    // Show that we ran a task (Note: a new one might arrive as a
    // consequence!).
    return true;
  }

  // We couldn't run the task now because we're in a nested message loop
  // and the task isn't nestable.
  deferred_non_nestable_work_queue_.push(pending_task);
  return false;
}

void MessageLoop::AddToDelayedWorkQueue(const PendingTask& pending_task) {
  // Move to the delayed work queue.
  delayed_work_queue_.push(pending_task);
}

bool MessageLoop::DeletePendingTasks() {
  bool did_work = !work_queue_.empty();
  while (!work_queue_.empty()) {
    PendingTask pending_task = work_queue_.front();
    work_queue_.pop();
    if (!pending_task.delayed_run_time.is_null()) {
      // We want to delete delayed tasks in the same order in which they would
      // normally be deleted in case of any funny dependencies between delayed
      // tasks.
      AddToDelayedWorkQueue(pending_task);
    }
  }
  did_work |= !deferred_non_nestable_work_queue_.empty();
  while (!deferred_non_nestable_work_queue_.empty()) {
    deferred_non_nestable_work_queue_.pop();
  }
  did_work |= !delayed_work_queue_.empty();

  // Historically, we always delete the task regardless of valgrind status. It's
  // not completely clear why we want to leak them in the loops above.  This
  // code is replicating legacy behavior, and should not be considered
  // absolutely "correct" behavior.  See TODO above about deleting all tasks
  // when it's safe.
  while (!delayed_work_queue_.empty()) {
    delayed_work_queue_.pop();
  }
  return did_work;
}

uint64 MessageLoop::GetTaskTraceID(const PendingTask& task) {
  return (static_cast<uint64>(task.sequence_num) << 32) |
         ((static_cast<uint64>(reinterpret_cast<intptr_t>(this)) << 32) >> 32);
}

void MessageLoop::ReloadWorkQueue() {
  // We can improve performance of our loading tasks from the incoming queue to
  // |*work_queue| by waiting until the last minute (|*work_queue| is empty) to
  // load. That reduces the number of locks-per-task significantly when our
  // queues get large.
  if (work_queue_.empty())
    incoming_task_queue_->ReloadWorkQueue(&work_queue_);
}

void MessageLoop::ScheduleWork(bool was_empty) {
  // The Android UI message loop needs to get notified each time
  // a task is added to the incoming queue.
  if (was_empty || AlwaysNotifyPump(type_))
    pump_->ScheduleWork();
}

//------------------------------------------------------------------------------
// Method and data for histogramming events and actions taken by each instance
// on each thread.

void MessageLoop::StartHistogrammer() {
#if !defined(OS_NACL)  // NaCl build has no metrics code.
  if (enable_histogrammer_ && !message_histogram_
      && StatisticsRecorder::IsActive()) {
    DCHECK(!thread_name_.empty());
    message_histogram_ = LinearHistogram::FactoryGetWithRangeDescription(
        "MsgLoop:" + thread_name_,
        kLeastNonZeroMessageId, kMaxMessageId,
        kNumberOfDistinctMessagesDisplayed,
        message_histogram_->kHexRangePrintingFlag,
        event_descriptions_);
  }
#endif
}

void MessageLoop::HistogramEvent(int event) {
#if !defined(OS_NACL)
  if (message_histogram_)
    message_histogram_->Add(event);
#endif
}

bool MessageLoop::DoWork() {
  if (!nestable_tasks_allowed_) {
    // Task can't be executed right now.
    return false;
  }

  for (;;) {
    ReloadWorkQueue();
    if (work_queue_.empty())
      break;

    // Execute oldest task.
    do {
      PendingTask pending_task = work_queue_.front();
      work_queue_.pop();
      if (!pending_task.delayed_run_time.is_null()) {
        AddToDelayedWorkQueue(pending_task);
        // If we changed the topmost task, then it is time to reschedule.
        if (delayed_work_queue_.top().task.Equals(pending_task.task))
          pump_->ScheduleDelayedWork(pending_task.delayed_run_time);
      } else {
        if (DeferOrRunPendingTask(pending_task))
          return true;
      }
    } while (!work_queue_.empty());
  }

  // Nothing happened.
  return false;
}

bool MessageLoop::DoDelayedWork(TimeTicks* next_delayed_work_time) {
  if (!nestable_tasks_allowed_ || delayed_work_queue_.empty()) {
    recent_time_ = *next_delayed_work_time = TimeTicks();
    return false;
  }

  // When we "fall behind," there will be a lot of tasks in the delayed work
  // queue that are ready to run.  To increase efficiency when we fall behind,
  // we will only call Time::Now() intermittently, and then process all tasks
  // that are ready to run before calling it again.  As a result, the more we
  // fall behind (and have a lot of ready-to-run delayed tasks), the more
  // efficient we'll be at handling the tasks.

  TimeTicks next_run_time = delayed_work_queue_.top().delayed_run_time;
  if (next_run_time > recent_time_) {
    recent_time_ = TimeTicks::Now();  // Get a better view of Now();
    if (next_run_time > recent_time_) {
      *next_delayed_work_time = next_run_time;
      return false;
    }
  }

  PendingTask pending_task = delayed_work_queue_.top();
  delayed_work_queue_.pop();

  if (!delayed_work_queue_.empty())
    *next_delayed_work_time = delayed_work_queue_.top().delayed_run_time;

  return DeferOrRunPendingTask(pending_task);
}

bool MessageLoop::DoIdleWork() {
  if (ProcessNextDelayedNonNestableTask())
    return true;

  if (run_loop_->quit_when_idle_received_)
    pump_->Quit();

  return false;
}

void MessageLoop::GetQueueingInformation(size_t* queue_size,
                                         TimeDelta* queueing_delay) {
  *queue_size = work_queue_.size();
  if (*queue_size == 0) {
    *queueing_delay = TimeDelta();
    return;
  }

  const PendingTask& next_to_run = work_queue_.front();
  tracked_objects::Duration duration =
      tracked_objects::TrackedTime::Now() - next_to_run.EffectiveTimePosted();
  *queueing_delay = TimeDelta::FromMilliseconds(duration.InMilliseconds());
}

void MessageLoop::DeleteSoonInternal(const tracked_objects::Location& from_here,
                                     void(*deleter)(const void*),
                                     const void* object) {
  PostNonNestableTask(from_here, Bind(deleter, object));
}

void MessageLoop::ReleaseSoonInternal(
    const tracked_objects::Location& from_here,
    void(*releaser)(const void*),
    const void* object) {
  PostNonNestableTask(from_here, Bind(releaser, object));
}

#if !defined(OS_NACL)
//------------------------------------------------------------------------------
// MessageLoopForUI

#if defined(OS_ANDROID)
void MessageLoopForUI::Start() {
  // No Histogram support for UI message loop as it is managed by Java side
  static_cast<MessagePumpForUI*>(pump_.get())->Start(this);
}
#endif

#if defined(OS_IOS)
void MessageLoopForUI::Attach() {
  static_cast<MessagePumpUIApplication*>(pump_.get())->Attach(this);
}
#endif

#if defined(OS_WIN)
void MessageLoopForUI::AddObserver(Observer* observer) {
  static_cast<MessagePumpWin*>(pump_.get())->AddObserver(observer);
}

void MessageLoopForUI::RemoveObserver(Observer* observer) {
  static_cast<MessagePumpWin*>(pump_.get())->RemoveObserver(observer);
}
#endif  // defined(OS_WIN)

#if defined(USE_OZONE) || (defined(OS_CHROMEOS) && !defined(USE_GLIB))
bool MessageLoopForUI::WatchFileDescriptor(
    int fd,
    bool persistent,
    MessagePumpLibevent::Mode mode,
    MessagePumpLibevent::FileDescriptorWatcher *controller,
    MessagePumpLibevent::Watcher *delegate) {
  return static_cast<MessagePumpLibevent*>(pump_.get())->WatchFileDescriptor(
      fd,
      persistent,
      mode,
      controller,
      delegate);
}
#endif

#endif  // !defined(OS_NACL)

//------------------------------------------------------------------------------
// MessageLoopForIO

#if !defined(OS_NACL)
void MessageLoopForIO::AddIOObserver(
    MessageLoopForIO::IOObserver* io_observer) {
  ToPumpIO(pump_.get())->AddIOObserver(io_observer);
}

void MessageLoopForIO::RemoveIOObserver(
    MessageLoopForIO::IOObserver* io_observer) {
  ToPumpIO(pump_.get())->RemoveIOObserver(io_observer);
}

#if defined(OS_WIN)
void MessageLoopForIO::RegisterIOHandler(HANDLE file, IOHandler* handler) {
  ToPumpIO(pump_.get())->RegisterIOHandler(file, handler);
}

bool MessageLoopForIO::RegisterJobObject(HANDLE job, IOHandler* handler) {
  return ToPumpIO(pump_.get())->RegisterJobObject(job, handler);
}

bool MessageLoopForIO::WaitForIOCompletion(DWORD timeout, IOHandler* filter) {
  return ToPumpIO(pump_.get())->WaitForIOCompletion(timeout, filter);
}
#elif defined(OS_POSIX)
bool MessageLoopForIO::WatchFileDescriptor(int fd,
                                           bool persistent,
                                           Mode mode,
                                           FileDescriptorWatcher *controller,
                                           Watcher *delegate) {
  return ToPumpIO(pump_.get())->WatchFileDescriptor(
      fd,
      persistent,
      mode,
      controller,
      delegate);
}
#endif

#endif  // !defined(OS_NACL)

}  // namespace base
