// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>

#include "base/android/build_info.h"
#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/sys_info.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/time/tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "media/base/media.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

class AndroidVideoDecodeAccelerator;

// AVDACodecAllocator manages threads for allocating and releasing MediaCodec
// instances.  These activities can hang, depending on android version, due
// to mediaserver bugs.  AVDACodecAllocator detects these cases, and reports
// on them to allow software fallback if the HW path is hung up.
class MEDIA_GPU_EXPORT AVDACodecAllocator {
 public:
  // For AVDAManager::TaskRunnerFor.  These are used as vector indices, so
  // please update AVDACodecAllocator's constructor if you add / change them.
  enum TaskType {
    // Task for an autodetected MediaCodec instance.
    AUTO_CODEC = 0,

    // Task for a software-codec-required MediaCodec.
    SW_CODEC = 1,

    // Special value to indicate "none".  This is not used as an array index. It
    // must, however, be positive to keep the enum unsigned.
    FAILED_CODEC = 99,
  };

  // Make sure the construction thread is started for |avda|.
  bool StartThread(AndroidVideoDecodeAccelerator* avda);

  void StopThread(AndroidVideoDecodeAccelerator* avda);

  // Return the task runner for tasks of type |type|.  If that thread failed
  // to start, then fall back to the GPU main thread.
  scoped_refptr<base::SingleThreadTaskRunner> TaskRunnerFor(TaskType task_type);

  // Returns a hint about whether the construction thread has hung for
  // |task_type|.  Note that if a thread isn't started, then we'll just return
  // "not hung", since it'll run on the current thread anyway.  The hang
  // detector will see no pending jobs in that case, so it's automatic.
  bool IsThreadLikelyHung(TaskType task_type);

  // Return true if and only if there is any AVDA registered.
  bool IsAnyRegisteredAVDA();

  // Return the task type to use for a new codec allocation, or FAILED_CODEC if
  // there is no thread that can support it.
  TaskType TaskTypeForAllocation();

  // Return a reference to the thread for unit tests.
  base::Thread& GetThreadForTesting(TaskType task_type);

 private:
  friend struct base::DefaultLazyInstanceTraits<AVDACodecAllocator>;
  friend class AVDACodecAllocatorTest;

  // Things that our unit test needs.  We guarantee that we'll access none of
  // it, from any thread, after we are destructed.
  struct TestInformation {
    TestInformation();
    ~TestInformation();
    // Optional clock source.
    std::unique_ptr<base::TickClock> tick_clock_;

    // Optional event that we'll signal when stopping the AUTO_CODEC thread.
    std::unique_ptr<base::WaitableEvent> stop_event_;
  };

  // |test_info| is owned by the unit test.
  AVDACodecAllocator(TestInformation* test_info = nullptr);
  ~AVDACodecAllocator();

  // Stop the thread indicated by |index|, then signal |event| if provided.
  void StopThreadTask(size_t index, base::WaitableEvent* event = nullptr);

  // All registered AVDA instances.
  std::set<AndroidVideoDecodeAccelerator*> thread_avda_instances_;

  class HangDetector : public base::MessageLoop::TaskObserver {
   public:
    HangDetector(base::TickClock* tick_clock);

    void WillProcessTask(const base::PendingTask& pending_task) override;
    void DidProcessTask(const base::PendingTask& pending_task) override;

    bool IsThreadLikelyHung();

   private:
    base::Lock lock_;
    // Non-null when a task is currently running.
    base::TimeTicks task_start_time_;

    base::TickClock* tick_clock_;

    DISALLOW_COPY_AND_ASSIGN(HangDetector);
  };

  // Handy combination of a thread and hang detector for it.
  struct ThreadAndHangDetector {
    ThreadAndHangDetector(const std::string& name, base::TickClock* tick_clock)
        : thread(name), hang_detector(tick_clock) {}
    base::Thread thread;
    HangDetector hang_detector;
  };

  // Threads for each of TaskType.  They are started / stopped as avda instances
  // show and and request them.  The vector indicies must match TaskType.
  std::vector<ThreadAndHangDetector*> threads_;

  base::ThreadChecker thread_checker_;

  // Optional, used for unit testing.  We do not own this.
  TestInformation* test_info_;

  // For canceling pending StopThreadTask()s.
  base::WeakPtrFactory<AVDACodecAllocator> weak_this_factory_;

  DISALLOW_COPY_AND_ASSIGN(AVDACodecAllocator);
};

}  // namespace media
