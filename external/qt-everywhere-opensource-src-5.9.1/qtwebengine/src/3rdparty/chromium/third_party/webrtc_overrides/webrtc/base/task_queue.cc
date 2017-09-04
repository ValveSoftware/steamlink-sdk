/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "third_party/webrtc_overrides/webrtc/base/task_queue.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/threading/thread.h"
#include "base/threading/thread_local.h"

namespace rtc {
namespace {

void RunTask(std::unique_ptr<QueuedTask> task) {
  if (!task->Run())
    task.release();
}

class PostAndReplyTask : public QueuedTask {
 public:
  PostAndReplyTask(
      std::unique_ptr<QueuedTask> task,
      std::unique_ptr<QueuedTask> reply,
      const scoped_refptr<base::SingleThreadTaskRunner>& reply_task_runner)
      : task_(std::move(task)),
        reply_(std::move(reply)),
        reply_task_runner_(reply_task_runner) {}

  ~PostAndReplyTask() override {}

 private:
  bool Run() override {
    if (!task_->Run())
      task_.release();

    reply_task_runner_->PostTask(FROM_HERE,
                                 base::Bind(&RunTask, base::Passed(&reply_)));
    return true;
  }

  std::unique_ptr<QueuedTask> task_;
  std::unique_ptr<QueuedTask> reply_;
  scoped_refptr<base::SingleThreadTaskRunner> reply_task_runner_;
};

// A lazily created thread local storage for quick access to a TaskQueue.
base::LazyInstance<base::ThreadLocalPointer<TaskQueue>>::Leaky lazy_tls_ptr =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

bool TaskQueue::IsCurrent() const {
  return Current() == this;
}

class TaskQueue::WorkerThread : public base::Thread {
 public:
  WorkerThread(const char* queue_name, TaskQueue* queue);
  ~WorkerThread() override;

 private:
  virtual void Init() override;

  TaskQueue* const queue_;
};

TaskQueue::WorkerThread::WorkerThread(const char* queue_name, TaskQueue* queue)
    : base::Thread(queue_name), queue_(queue) {}

void TaskQueue::WorkerThread::Init() {
  lazy_tls_ptr.Pointer()->Set(queue_);
}

TaskQueue::WorkerThread::~WorkerThread() {
  DCHECK(!Thread::IsRunning());
}

TaskQueue::TaskQueue(const char* queue_name)
    : thread_(
          std::unique_ptr<WorkerThread>(new WorkerThread(queue_name, this))) {
  DCHECK(queue_name);
  bool result = thread_->Start();
  CHECK(result);
}

TaskQueue::~TaskQueue() {
  DCHECK(!IsCurrent());
  thread_->Stop();
}

// static
TaskQueue* TaskQueue::Current() {
  return lazy_tls_ptr.Pointer()->Get();
}

// static
bool TaskQueue::IsCurrent(const char* queue_name) {
  TaskQueue* current = Current();
  return current && current->thread_->thread_name().compare(queue_name) == 0;
}

void TaskQueue::PostTask(std::unique_ptr<QueuedTask> task) {
  thread_->task_runner()->PostTask(FROM_HERE,
                                   base::Bind(&RunTask, base::Passed(&task)));
}

void TaskQueue::PostDelayedTask(std::unique_ptr<QueuedTask> task,
                                uint32_t milliseconds) {
  thread_->task_runner()->PostDelayedTask(
      FROM_HERE, base::Bind(&RunTask, base::Passed(&task)),
      base::TimeDelta::FromMilliseconds(milliseconds));
}

void TaskQueue::PostTaskAndReply(std::unique_ptr<QueuedTask> task,
                                 std::unique_ptr<QueuedTask> reply,
                                 TaskQueue* reply_queue) {
  PostTask(std::unique_ptr<QueuedTask>(new PostAndReplyTask(
      std::move(task), std::move(reply), reply_queue->thread_->task_runner())));
}

void TaskQueue::PostTaskAndReply(std::unique_ptr<QueuedTask> task,
                                 std::unique_ptr<QueuedTask> reply) {
  thread_->task_runner()->PostTaskAndReply(
      FROM_HERE, base::Bind(&RunTask, base::Passed(&task)),
      base::Bind(&RunTask, base::Passed(&reply)));
}

}  // namespace rtc
