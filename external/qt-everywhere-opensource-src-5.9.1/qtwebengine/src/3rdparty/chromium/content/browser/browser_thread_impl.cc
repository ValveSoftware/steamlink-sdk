// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_thread_impl.h"

#include <string.h>

#include <string>

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/profiler/scoped_tracker.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/threading/sequenced_worker_pool.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread_delegate.h"
#include "content/public/browser/content_browser_client.h"
#include "net/disk_cache/simple/simple_backend_impl.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#endif

namespace content {

namespace {

// Friendly names for the well-known threads.
static const char* const g_browser_thread_names[BrowserThread::ID_COUNT] = {
  "",  // UI (name assembled in browser_main.cc).
  "Chrome_DBThread",  // DB
  "Chrome_FileThread",  // FILE
  "Chrome_FileUserBlockingThread",  // FILE_USER_BLOCKING
  "Chrome_ProcessLauncherThread",  // PROCESS_LAUNCHER
  "Chrome_CacheThread",  // CACHE
  "Chrome_IOThread",  // IO
};

static const char* GetThreadName(BrowserThread::ID thread) {
  if (BrowserThread::UI < thread && thread < BrowserThread::ID_COUNT)
    return g_browser_thread_names[thread];
  if (thread == BrowserThread::UI)
    return "Chrome_UIThread";
  return "Unknown Thread";
}

// An implementation of SingleThreadTaskRunner to be used in conjunction
// with BrowserThread.
class BrowserThreadTaskRunner : public base::SingleThreadTaskRunner {
 public:
  explicit BrowserThreadTaskRunner(BrowserThread::ID identifier)
      : id_(identifier) {}

  // SingleThreadTaskRunner implementation.
  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       const base::Closure& task,
                       base::TimeDelta delay) override {
    return BrowserThread::PostDelayedTask(id_, from_here, task, delay);
  }

  bool PostNonNestableDelayedTask(const tracked_objects::Location& from_here,
                                  const base::Closure& task,
                                  base::TimeDelta delay) override {
    return BrowserThread::PostNonNestableDelayedTask(id_, from_here, task,
                                                     delay);
  }

  bool RunsTasksOnCurrentThread() const override {
    return BrowserThread::CurrentlyOn(id_);
  }

 protected:
  ~BrowserThreadTaskRunner() override {}

 private:
  BrowserThread::ID id_;
  DISALLOW_COPY_AND_ASSIGN(BrowserThreadTaskRunner);
};

// A separate helper is used just for the task runners, in order to avoid
// needing to initialize the globals to create a task runner.
struct BrowserThreadTaskRunners {
  BrowserThreadTaskRunners() {
    for (int i = 0; i < BrowserThread::ID_COUNT; ++i) {
      proxies[i] =
          new BrowserThreadTaskRunner(static_cast<BrowserThread::ID>(i));
    }
  }

  scoped_refptr<base::SingleThreadTaskRunner> proxies[BrowserThread::ID_COUNT];
};

base::LazyInstance<BrowserThreadTaskRunners>::Leaky g_task_runners =
    LAZY_INSTANCE_INITIALIZER;

struct BrowserThreadGlobals {
  BrowserThreadGlobals()
      : blocking_pool(
            new base::SequencedWorkerPool(3,
                                          "BrowserBlocking",
                                          base::TaskPriority::USER_VISIBLE)) {
    memset(threads, 0, BrowserThread::ID_COUNT * sizeof(threads[0]));
    memset(thread_ids, 0, BrowserThread::ID_COUNT * sizeof(thread_ids[0]));
    memset(thread_delegates, 0,
           BrowserThread::ID_COUNT * sizeof(thread_delegates[0]));
  }

  // This lock protects |threads| and |thread_ids|. Do not read or modify those
  // arrays without holding this lock. Do not block while holding this lock.
  base::Lock lock;

  // This array is protected by |lock|. IDs in this array are populated as soon
  // as their respective thread is started and are never reset.
  base::PlatformThreadId thread_ids[BrowserThread::ID_COUNT];

  // This array is protected by |lock|. The threads are not owned by this
  // array. Typically, the threads are owned on the UI thread by
  // BrowserMainLoop. BrowserThreadImpl objects remove themselves from this
  // array upon destruction.
  BrowserThreadImpl* threads[BrowserThread::ID_COUNT];

  // Only atomic operations are used on this array. The delegates are not owned
  // by this array, rather by whoever calls BrowserThread::SetDelegate.
  BrowserThreadDelegate* thread_delegates[BrowserThread::ID_COUNT];

  const scoped_refptr<base::SequencedWorkerPool> blocking_pool;
};

base::LazyInstance<BrowserThreadGlobals>::Leaky
    g_globals = LAZY_INSTANCE_INITIALIZER;

}  // namespace

BrowserThreadImpl::BrowserThreadImpl(ID identifier)
    : Thread(GetThreadName(identifier)), identifier_(identifier) {
  Initialize();

  // Unit tests may create multiple TestBrowserThreadBundles, causing the same
  // BrowserThread ID to be reinitialized. We explicitly clear the thread ID
  // here so Start() can sanity check.
  BrowserThreadGlobals& globals = g_globals.Get();
  base::AutoLock lock(globals.lock);
  globals.thread_ids[identifier] = base::kInvalidThreadId;
}

BrowserThreadImpl::BrowserThreadImpl(ID identifier,
                                     base::MessageLoop* message_loop)
    : Thread(GetThreadName(identifier)), identifier_(identifier) {
  SetMessageLoop(message_loop);
  Initialize();

  // If constructed with an explicit message loop, this is a fake BrowserThread
  // which runs on the current thread.
  BrowserThreadGlobals& globals = g_globals.Get();
  base::AutoLock lock(globals.lock);
  globals.thread_ids[identifier] = base::PlatformThread::CurrentId();
}

// static
void BrowserThreadImpl::ShutdownThreadPool() {
  // The goal is to make it impossible for chrome to 'infinite loop' during
  // shutdown, but to reasonably expect that all BLOCKING_SHUTDOWN tasks queued
  // during shutdown get run. There's nothing particularly scientific about the
  // number chosen.
  const int kMaxNewShutdownBlockingTasks = 1000;
  BrowserThreadGlobals& globals = g_globals.Get();
  globals.blocking_pool->Shutdown(kMaxNewShutdownBlockingTasks);
}

// static
void BrowserThreadImpl::FlushThreadPoolHelperForTesting() {
  // We don't want to create a pool if none exists.
  if (g_globals == nullptr)
    return;
  g_globals.Get().blocking_pool->FlushForTesting();
  disk_cache::SimpleBackendImpl::FlushWorkerPoolForTesting();
}

void BrowserThreadImpl::Init() {
  BrowserThreadGlobals& globals = g_globals.Get();

  if (BrowserThread::CurrentlyOn(BrowserThread::DB) ||
      BrowserThread::CurrentlyOn(BrowserThread::FILE) ||
      BrowserThread::CurrentlyOn(BrowserThread::FILE_USER_BLOCKING) ||
      BrowserThread::CurrentlyOn(BrowserThread::PROCESS_LAUNCHER) ||
      BrowserThread::CurrentlyOn(BrowserThread::CACHE)) {
    base::MessageLoop* message_loop = base::MessageLoop::current();
    message_loop->DisallowNesting();
    message_loop->DisallowTaskObservers();
  }

  using base::subtle::AtomicWord;
  AtomicWord* storage =
      reinterpret_cast<AtomicWord*>(&globals.thread_delegates[identifier_]);
  AtomicWord stored_pointer = base::subtle::NoBarrier_Load(storage);
  BrowserThreadDelegate* delegate =
      reinterpret_cast<BrowserThreadDelegate*>(stored_pointer);
  if (delegate)
    delegate->Init();
}

// We disable optimizations for this block of functions so the compiler doesn't
// merge them all together.
MSVC_DISABLE_OPTIMIZE()
MSVC_PUSH_DISABLE_WARNING(4748)

NOINLINE void BrowserThreadImpl::UIThreadRun(base::RunLoop* run_loop) {
  volatile int line_number = __LINE__;
  Thread::Run(run_loop);
  CHECK_GT(line_number, 0);
}

NOINLINE void BrowserThreadImpl::DBThreadRun(base::RunLoop* run_loop) {
  volatile int line_number = __LINE__;
  Thread::Run(run_loop);
  CHECK_GT(line_number, 0);
}

NOINLINE void BrowserThreadImpl::FileThreadRun(base::RunLoop* run_loop) {
  volatile int line_number = __LINE__;
  Thread::Run(run_loop);
  CHECK_GT(line_number, 0);
}

NOINLINE void BrowserThreadImpl::FileUserBlockingThreadRun(
    base::RunLoop* run_loop) {
  volatile int line_number = __LINE__;
  Thread::Run(run_loop);
  CHECK_GT(line_number, 0);
}

NOINLINE void BrowserThreadImpl::ProcessLauncherThreadRun(
    base::RunLoop* run_loop) {
  volatile int line_number = __LINE__;
  Thread::Run(run_loop);
  CHECK_GT(line_number, 0);
}

NOINLINE void BrowserThreadImpl::CacheThreadRun(base::RunLoop* run_loop) {
  volatile int line_number = __LINE__;
  Thread::Run(run_loop);
  CHECK_GT(line_number, 0);
}

NOINLINE void BrowserThreadImpl::IOThreadRun(base::RunLoop* run_loop) {
  volatile int line_number = __LINE__;
  Thread::Run(run_loop);
  CHECK_GT(line_number, 0);
}

MSVC_POP_WARNING()
MSVC_ENABLE_OPTIMIZE();

void BrowserThreadImpl::Run(base::RunLoop* run_loop) {
#if defined(OS_ANDROID)
  // Not to reset thread name to "Thread-???" by VM, attach VM with thread name.
  // Though it may create unnecessary VM thread objects, keeping thread name
  // gives more benefit in debugging in the platform.
  if (!thread_name().empty()) {
    base::android::AttachCurrentThreadWithName(thread_name());
  }
#endif

  BrowserThread::ID thread_id = ID_COUNT;
  CHECK(GetCurrentThreadIdentifier(&thread_id));
  CHECK_EQ(identifier_, thread_id);

  switch (identifier_) {
    case BrowserThread::UI:
      return UIThreadRun(run_loop);
    case BrowserThread::DB:
      return DBThreadRun(run_loop);
    case BrowserThread::FILE:
      return FileThreadRun(run_loop);
    case BrowserThread::FILE_USER_BLOCKING:
      return FileUserBlockingThreadRun(run_loop);
    case BrowserThread::PROCESS_LAUNCHER:
      return ProcessLauncherThreadRun(run_loop);
    case BrowserThread::CACHE:
      return CacheThreadRun(run_loop);
    case BrowserThread::IO:
      return IOThreadRun(run_loop);
    case BrowserThread::ID_COUNT:
      CHECK(false);  // This shouldn't actually be reached!
      break;
  }

  // |identifier_| must be set to a valid enum value in the constructor, so it
  // should be impossible to reach here.
  CHECK(false);
}

void BrowserThreadImpl::CleanUp() {
  BrowserThreadGlobals& globals = g_globals.Get();

  using base::subtle::AtomicWord;
  AtomicWord* storage =
      reinterpret_cast<AtomicWord*>(&globals.thread_delegates[identifier_]);
  AtomicWord stored_pointer = base::subtle::NoBarrier_Load(storage);
  BrowserThreadDelegate* delegate =
      reinterpret_cast<BrowserThreadDelegate*>(stored_pointer);

  if (delegate)
    delegate->CleanUp();

  // PostTaskHelper() accesses the message loop while holding this lock.
  // However, the message loop will soon be destructed without any locking. So
  // to prevent a race with accessing the message loop in PostTaskHelper(),
  // remove this thread from the global array now.
  base::AutoLock lock(globals.lock);
  globals.threads[identifier_] = nullptr;
}

void BrowserThreadImpl::Initialize() {
  BrowserThreadGlobals& globals = g_globals.Get();

  base::AutoLock lock(globals.lock);
  DCHECK_GE(identifier_, 0);
  DCHECK_LT(identifier_, ID_COUNT);
  DCHECK_EQ(globals.threads[identifier_], nullptr);
  globals.threads[identifier_] = this;
}

BrowserThreadImpl::~BrowserThreadImpl() {
  // All Thread subclasses must call Stop() in the destructor. This is
  // doubly important here as various bits of code check they are on
  // the right BrowserThread.
  Stop();

  BrowserThreadGlobals& globals = g_globals.Get();
  base::AutoLock lock(globals.lock);
  globals.threads[identifier_] = nullptr;
#ifndef NDEBUG
  // Double check that the threads are ordered correctly in the enumeration.
  for (int i = identifier_ + 1; i < ID_COUNT; ++i) {
    DCHECK(!globals.threads[i]) <<
        "Threads must be listed in the reverse order that they die";
  }
#endif
}

bool BrowserThreadImpl::Start() {
  return StartWithOptions(base::Thread::Options());
}

bool BrowserThreadImpl::StartWithOptions(const Options& options) {
  // The global thread table needs to be locked while a new thread is
  // starting, as the new thread can asynchronously start touching the
  // table (and other thread's message_loop).
  BrowserThreadGlobals& globals = g_globals.Get();
  base::AutoLock lock(globals.lock);
  DCHECK_EQ(globals.thread_ids[identifier_], base::kInvalidThreadId);
  bool result = Thread::StartWithOptions(options);
  globals.thread_ids[identifier_] = GetThreadId();
  return result;
}

bool BrowserThreadImpl::StartAndWaitForTesting() {
  if (!Start())
    return false;
  WaitUntilThreadStarted();
  return true;
}
// static
bool BrowserThreadImpl::PostTaskHelper(
    BrowserThread::ID identifier,
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay,
    bool nestable) {
  DCHECK_GE(identifier, 0);
  DCHECK_LT(identifier, ID_COUNT);
  // Optimization: to avoid unnecessary locks, we listed the ID enumeration in
  // order of lifetime.  So no need to lock if we know that the target thread
  // outlives current thread.
  // Note: since the array is so small, ok to loop instead of creating a map,
  // which would require a lock because std::map isn't thread safe, defeating
  // the whole purpose of this optimization.
  BrowserThread::ID current_thread = ID_COUNT;
  bool target_thread_outlives_current =
      GetCurrentThreadIdentifier(&current_thread) &&
      current_thread >= identifier;

  BrowserThreadGlobals& globals = g_globals.Get();
  if (!target_thread_outlives_current)
    globals.lock.Acquire();

  base::MessageLoop* message_loop =
      globals.threads[identifier] ? globals.threads[identifier]->message_loop()
                                  : nullptr;
  if (message_loop) {
    if (nestable) {
      message_loop->task_runner()->PostDelayedTask(from_here, task, delay);
    } else {
      message_loop->task_runner()->PostNonNestableDelayedTask(from_here, task,
                                                              delay);
    }
  }

  if (!target_thread_outlives_current)
    globals.lock.Release();

  return !!message_loop;
}

// static
bool BrowserThread::PostBlockingPoolTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  return g_globals.Get().blocking_pool->PostWorkerTask(from_here, task);
}

// static
bool BrowserThread::PostBlockingPoolTaskAndReply(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    const base::Closure& reply) {
  return g_globals.Get().blocking_pool->PostTaskAndReply(
      from_here, task, reply);
}

// static
bool BrowserThread::PostBlockingPoolSequencedTask(
    const std::string& sequence_token_name,
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  return g_globals.Get().blocking_pool->PostNamedSequencedWorkerTask(
      sequence_token_name, from_here, task);
}

// static
void BrowserThread::PostAfterStartupTask(
    const tracked_objects::Location& from_here,
    const scoped_refptr<base::TaskRunner>& task_runner,
    const base::Closure& task) {
  GetContentClient()->browser()->PostAfterStartupTask(from_here, task_runner,
                                                      task);
}

// static
base::SequencedWorkerPool* BrowserThread::GetBlockingPool() {
  return g_globals.Get().blocking_pool.get();
}

// static
bool BrowserThread::IsThreadInitialized(ID identifier) {
  if (g_globals == nullptr)
    return false;

  BrowserThreadGlobals& globals = g_globals.Get();
  base::AutoLock lock(globals.lock);
  DCHECK_GE(identifier, 0);
  DCHECK_LT(identifier, ID_COUNT);
  return globals.threads[identifier] != nullptr;
}

// static
bool BrowserThread::CurrentlyOn(ID identifier) {
  BrowserThreadGlobals& globals = g_globals.Get();
  base::AutoLock lock(globals.lock);
  DCHECK_GE(identifier, 0);
  DCHECK_LT(identifier, ID_COUNT);
  return base::PlatformThread::CurrentId() == globals.thread_ids[identifier];
}

// static
std::string BrowserThread::GetDCheckCurrentlyOnErrorMessage(ID expected) {
  std::string actual_name = base::PlatformThread::GetName();
  if (actual_name.empty())
    actual_name = "Unknown Thread";

  std::string result = "Must be called on ";
  result += GetThreadName(expected);
  result += "; actually called on ";
  result += actual_name;
  result += ".";
  return result;
}

// static
bool BrowserThread::IsMessageLoopValid(ID identifier) {
  if (g_globals == nullptr)
    return false;

  BrowserThreadGlobals& globals = g_globals.Get();
  base::AutoLock lock(globals.lock);
  DCHECK_GE(identifier, 0);
  DCHECK_LT(identifier, ID_COUNT);
  return globals.threads[identifier] &&
         globals.threads[identifier]->message_loop();
}

// static
bool BrowserThread::PostTask(ID identifier,
                             const tracked_objects::Location& from_here,
                             const base::Closure& task) {
  return BrowserThreadImpl::PostTaskHelper(
      identifier, from_here, task, base::TimeDelta(), true);
}

// static
bool BrowserThread::PostDelayedTask(ID identifier,
                                    const tracked_objects::Location& from_here,
                                    const base::Closure& task,
                                    base::TimeDelta delay) {
  return BrowserThreadImpl::PostTaskHelper(
      identifier, from_here, task, delay, true);
}

// static
bool BrowserThread::PostNonNestableTask(
    ID identifier,
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  return BrowserThreadImpl::PostTaskHelper(
      identifier, from_here, task, base::TimeDelta(), false);
}

// static
bool BrowserThread::PostNonNestableDelayedTask(
    ID identifier,
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  return BrowserThreadImpl::PostTaskHelper(
      identifier, from_here, task, delay, false);
}

// static
bool BrowserThread::PostTaskAndReply(
    ID identifier,
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    const base::Closure& reply) {
  return GetTaskRunnerForThread(identifier)
      ->PostTaskAndReply(from_here, task, reply);
}

// static
bool BrowserThread::GetCurrentThreadIdentifier(ID* identifier) {
  if (g_globals == nullptr)
    return false;

  base::MessageLoop* cur_message_loop = base::MessageLoop::current();
  BrowserThreadGlobals& globals = g_globals.Get();
  // Profiler to track potential contention on |globals.lock|. This only does
  // real work on canary and local dev builds, so the cost of having this here
  // should be minimal.
  tracked_objects::ScopedTracker tracking_profile(FROM_HERE);
  base::AutoLock lock(globals.lock);
  for (int i = 0; i < ID_COUNT; ++i) {
    if (globals.threads[i] &&
        globals.threads[i]->message_loop() == cur_message_loop) {
      *identifier = globals.threads[i]->identifier_;
      return true;
    }
  }

  return false;
}

// static
scoped_refptr<base::SingleThreadTaskRunner>
BrowserThread::GetTaskRunnerForThread(ID identifier) {
  return g_task_runners.Get().proxies[identifier];
}

void BrowserThread::SetDelegate(ID identifier,
                                BrowserThreadDelegate* delegate) {
  using base::subtle::AtomicWord;
  BrowserThreadGlobals& globals = g_globals.Get();
  AtomicWord* storage = reinterpret_cast<AtomicWord*>(
      &globals.thread_delegates[identifier]);
  AtomicWord old_pointer = base::subtle::NoBarrier_AtomicExchange(
      storage, reinterpret_cast<AtomicWord>(delegate));

  // This catches registration when previously registered.
  DCHECK(!delegate || !old_pointer);
}

}  // namespace content
