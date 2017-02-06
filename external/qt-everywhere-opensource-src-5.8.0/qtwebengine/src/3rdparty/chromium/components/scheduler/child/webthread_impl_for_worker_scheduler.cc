// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/child/webthread_impl_for_worker_scheduler.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/default_tick_clock.h"
#include "components/scheduler/base/task_queue.h"
#include "components/scheduler/child/scheduler_tqm_delegate_impl.h"
#include "components/scheduler/child/web_scheduler_impl.h"
#include "components/scheduler/child/web_task_runner_impl.h"
#include "components/scheduler/child/worker_scheduler_impl.h"
#include "third_party/WebKit/public/platform/WebTraceLocation.h"

namespace scheduler {

WebThreadImplForWorkerScheduler::WebThreadImplForWorkerScheduler(
    const char* name)
    : WebThreadImplForWorkerScheduler(name, base::Thread::Options()) {}

WebThreadImplForWorkerScheduler::WebThreadImplForWorkerScheduler(
    const char* name,
    base::Thread::Options options)
    : thread_(new base::Thread(name ? name : std::string())) {
  bool started = thread_->StartWithOptions(options);
  CHECK(started);
  thread_task_runner_ = thread_->task_runner();
}

void WebThreadImplForWorkerScheduler::Init() {
  base::WaitableEvent completion(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&WebThreadImplForWorkerScheduler::InitOnThread,
                            base::Unretained(this), &completion));
  completion.Wait();
}

WebThreadImplForWorkerScheduler::~WebThreadImplForWorkerScheduler() {
  if (task_runner_delegate_) {
    base::WaitableEvent completion(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    // Restore the original task runner so that the thread can tear itself down.
    thread_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&WebThreadImplForWorkerScheduler::RestoreTaskRunnerOnThread,
                   base::Unretained(this), &completion));
    completion.Wait();
  }
  thread_->Stop();
}

void WebThreadImplForWorkerScheduler::InitOnThread(
    base::WaitableEvent* completion) {
  // TODO(alexclarke): Do we need to unify virtual time for workers and the
  // main thread?
  worker_scheduler_ = CreateWorkerScheduler();
  worker_scheduler_->Init();
  task_runner_ = worker_scheduler_->DefaultTaskRunner();
  idle_task_runner_ = worker_scheduler_->IdleTaskRunner();
  web_scheduler_.reset(new WebSchedulerImpl(
      worker_scheduler_.get(), worker_scheduler_->IdleTaskRunner(),
      worker_scheduler_->DefaultTaskRunner(),
      worker_scheduler_->DefaultTaskRunner()));
  base::MessageLoop::current()->AddDestructionObserver(this);
  web_task_runner_ = base::WrapUnique(new WebTaskRunnerImpl(task_runner_));
  completion->Signal();
}

void WebThreadImplForWorkerScheduler::RestoreTaskRunnerOnThread(
    base::WaitableEvent* completion) {
  task_runner_delegate_->RestoreDefaultTaskRunner();
  completion->Signal();
}

void WebThreadImplForWorkerScheduler::WillDestroyCurrentMessageLoop() {
  task_runner_ = nullptr;
  idle_task_runner_ = nullptr;
  web_scheduler_.reset();
  worker_scheduler_.reset();
}

std::unique_ptr<scheduler::WorkerScheduler>
WebThreadImplForWorkerScheduler::CreateWorkerScheduler() {
  task_runner_delegate_ = SchedulerTqmDelegateImpl::Create(
      thread_->message_loop(), base::WrapUnique(new base::DefaultTickClock()));
  return WorkerScheduler::Create(task_runner_delegate_);
}

blink::PlatformThreadId WebThreadImplForWorkerScheduler::threadId() const {
  return thread_->GetThreadId();
}

blink::WebScheduler* WebThreadImplForWorkerScheduler::scheduler() const {
  return web_scheduler_.get();
}

base::SingleThreadTaskRunner* WebThreadImplForWorkerScheduler::GetTaskRunner()
    const {
  return task_runner_.get();
}

SingleThreadIdleTaskRunner* WebThreadImplForWorkerScheduler::GetIdleTaskRunner()
    const {
  return idle_task_runner_.get();
}

blink::WebTaskRunner* WebThreadImplForWorkerScheduler::getWebTaskRunner() {
  return web_task_runner_.get();
}

void WebThreadImplForWorkerScheduler::AddTaskObserverInternal(
    base::MessageLoop::TaskObserver* observer) {
  worker_scheduler_->AddTaskObserver(observer);
}

void WebThreadImplForWorkerScheduler::RemoveTaskObserverInternal(
    base::MessageLoop::TaskObserver* observer) {
  worker_scheduler_->RemoveTaskObserver(observer);
}

}  // namespace scheduler
