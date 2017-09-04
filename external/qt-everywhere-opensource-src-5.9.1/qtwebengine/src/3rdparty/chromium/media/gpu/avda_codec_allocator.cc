// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/avda_codec_allocator.h"

#include <stddef.h>

#include <memory>

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/sys_info.h"
#include "base/task_runner_util.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "media/base/limits.h"
#include "media/base/media.h"
#include "media/base/timestamp_constants.h"

namespace media {

namespace {

// Give tasks on the construction thread 800ms before considering them hung.
// MediaCodec.configure() calls typically take 100-200ms on a N5, so 800ms is
// expected to very rarely result in false positives. Also, false positives have
// low impact because we resume using the thread if its apparently hung task
// completes.
constexpr base::TimeDelta kHungTaskDetectionTimeout =
    base::TimeDelta::FromMilliseconds(800);

}  // namespace

AVDACodecAllocator::TestInformation::TestInformation() {}
AVDACodecAllocator::TestInformation::~TestInformation() {}

AVDACodecAllocator::HangDetector::HangDetector(base::TickClock* tick_clock)
    : tick_clock_(tick_clock) {}

void AVDACodecAllocator::HangDetector::WillProcessTask(
    const base::PendingTask& pending_task) {
  base::AutoLock l(lock_);
  task_start_time_ = tick_clock_->NowTicks();
}

void AVDACodecAllocator::HangDetector::DidProcessTask(
    const base::PendingTask& pending_task) {
  base::AutoLock l(lock_);
  task_start_time_ = base::TimeTicks();
}

bool AVDACodecAllocator::HangDetector::IsThreadLikelyHung() {
  base::AutoLock l(lock_);
  if (task_start_time_.is_null())
    return false;

  return (tick_clock_->NowTicks() - task_start_time_) >
         kHungTaskDetectionTimeout;
}

// Make sure the construction threads are started for |avda|.
bool AVDACodecAllocator::StartThread(AndroidVideoDecodeAccelerator* avda) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Cancel any pending StopThreadTask()s because we need the threads now.
  weak_this_factory_.InvalidateWeakPtrs();

  // Try to start all threads if they haven't been started.  Remember that
  // threads fail to start fairly often.
  for (size_t i = 0; i < threads_.size(); i++) {
    if (threads_[i]->thread.IsRunning())
      continue;

    if (!threads_[i]->thread.Start())
      continue;

    // Register the hang detector to observe the thread's MessageLoop.
    threads_[i]->thread.task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&base::MessageLoop::AddTaskObserver,
                   base::Unretained(threads_[i]->thread.message_loop()),
                   &threads_[i]->hang_detector));
  }

  // Make sure that the construction thread started, else refuse to run.
  // If other threads fail to start, then we'll post to the GPU main thread for
  // those cases.  SW allocation failures are much less rare, so this usually
  // just costs us the latency of doing the codec allocation on the main thread.
  if (!threads_[TaskType::AUTO_CODEC]->thread.IsRunning())
    return false;

  thread_avda_instances_.insert(avda);
  UMA_HISTOGRAM_ENUMERATION("Media.AVDA.NumAVDAInstances",
                            thread_avda_instances_.size(),
                            31);  // PRESUBMIT_IGNORE_UMA_MAX
  return true;
}

void AVDACodecAllocator::StopThread(AndroidVideoDecodeAccelerator* avda) {
  DCHECK(thread_checker_.CalledOnValidThread());

  thread_avda_instances_.erase(avda);
  // Post a task to stop the thread through the thread's task runner and back
  // to this thread. This ensures that all pending tasks are run first. If the
  // thread is hung we don't post a task to avoid leaking an unbounded number
  // of tasks on its queue. If the thread is not hung, but appears to be, it
  // will stay alive until next time an AVDA tries to stop it. We're
  // guaranteed to not run StopThreadTask() when the thread is hung because if
  // an AVDA queues tasks after DoNothing(), the StopThreadTask() reply will
  // be canceled by invalidating its weak pointer.
  base::WaitableEvent* event =
      (test_info_ ? test_info_->stop_event_.get() : nullptr);
  if (!thread_avda_instances_.empty()) {
    // If we aren't stopping, then signal immediately.
    if (event)
      event->Signal();
    return;
  }

  for (size_t i = 0; i < threads_.size(); i++) {
    if (threads_[i]->thread.IsRunning() &&
        !threads_[i]->hang_detector.IsThreadLikelyHung()) {
      threads_[i]->thread.task_runner()->PostTaskAndReply(
          FROM_HERE, base::Bind(&base::DoNothing),
          base::Bind(&AVDACodecAllocator::StopThreadTask,
                     weak_this_factory_.GetWeakPtr(), i,
                     (i == TaskType::AUTO_CODEC ? event : nullptr)));
    }
  }
}

// Return the task runner for tasks of type |type|.  If that thread failed
// to start, then fall back to the GPU main thread.
scoped_refptr<base::SingleThreadTaskRunner> AVDACodecAllocator::TaskRunnerFor(
    TaskType task_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::Thread& thread = threads_[task_type]->thread;

  // Fail over to the main thread if this thread failed to start.  Note that
  // if the AUTO_CODEC thread fails to start, then AVDA init will fail.
  // We won't fall back autodetection to the main thread, even without a
  // special case here.
  if (!thread.IsRunning())
    return base::ThreadTaskRunnerHandle::Get();

  return thread.task_runner();
}

// Returns a hint about whether the construction thread has hung for
// |task_type|.  Note that if a thread isn't started, then we'll just return
// "not hung", since it'll run on the current thread anyway.  The hang
// detector will see no pending jobs in that case, so it's automatic.
bool AVDACodecAllocator::IsThreadLikelyHung(TaskType task_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return threads_[task_type]->hang_detector.IsThreadLikelyHung();
}

bool AVDACodecAllocator::IsAnyRegisteredAVDA() {
  return !thread_avda_instances_.empty();
}

AVDACodecAllocator::TaskType AVDACodecAllocator::TaskTypeForAllocation() {
  if (!IsThreadLikelyHung(TaskType::AUTO_CODEC))
    return TaskType::AUTO_CODEC;

  if (!IsThreadLikelyHung(TaskType::SW_CODEC))
    return TaskType::SW_CODEC;

  // If nothing is working, then we can't allocate anyway.
  return TaskType::FAILED_CODEC;
}

base::Thread& AVDACodecAllocator::GetThreadForTesting(TaskType task_type) {
  return threads_[task_type]->thread;
}

AVDACodecAllocator::AVDACodecAllocator(TestInformation* test_info)
    : test_info_(test_info), weak_this_factory_(this) {
  // If we're not provided with one, use real time.
  // Note that we'll leak this, but that's okay since we're a singleton.
  base::TickClock* tick_clock = nullptr;
  if (!test_info_)
    tick_clock = new base::DefaultTickClock();
  else
    tick_clock = test_info_->tick_clock_.get();

  // Create threads with names / indices that match up with TaskType.
  DCHECK_EQ(threads_.size(), TaskType::AUTO_CODEC);
  threads_.push_back(new ThreadAndHangDetector("AVDAAutoThread", tick_clock));
  DCHECK_EQ(threads_.size(), TaskType::SW_CODEC);
  threads_.push_back(new ThreadAndHangDetector("AVDASWThread", tick_clock));
}

AVDACodecAllocator::~AVDACodecAllocator() {
  // Only tests should reach here.  Shut down threads so that we guarantee that
  // nothing will use the threads.
  for (size_t i = 0; i < threads_.size(); i++) {
    if (!threads_[i]->thread.IsRunning())
      continue;
    threads_[i]->thread.Stop();
  }
}

void AVDACodecAllocator::StopThreadTask(size_t index,
                                        base::WaitableEvent* event) {
  threads_[index]->thread.Stop();
  if (event)
    event->Signal();
}

}  // namespace media
