// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/services/thread_helpers.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"

namespace sandbox {

namespace {

bool IsSingleThreadedImpl(int proc_self_task) {
  CHECK_LE(0, proc_self_task);
  struct stat task_stat;
  int fstat_ret = fstat(proc_self_task, &task_stat);
  PCHECK(0 == fstat_ret);

  // At least "..", "." and the current thread should be present.
  CHECK_LE(3UL, task_stat.st_nlink);
  // Counting threads via /proc/self/task could be racy. For the purpose of
  // determining if the current proces is monothreaded it works: if at any
  // time it becomes monothreaded, it'll stay so.
  return task_stat.st_nlink == 3;
}

}  // namespace

bool ThreadHelpers::IsSingleThreaded(int proc_self_task) {
  DCHECK_LE(-1, proc_self_task);
  if (-1 == proc_self_task) {
    const int task_fd = open("/proc/self/task/", O_RDONLY | O_DIRECTORY);
    PCHECK(0 <= task_fd);
    const bool result = IsSingleThreadedImpl(task_fd);
    PCHECK(0 == IGNORE_EINTR(close(task_fd)));
    return result;
  } else {
    return IsSingleThreadedImpl(proc_self_task);
  }
}

bool ThreadHelpers::StopThreadAndWatchProcFS(int proc_self_task,
                                             base::Thread* thread) {
  DCHECK_LE(0, proc_self_task);
  DCHECK(thread);
  const base::PlatformThreadId thread_id = thread->thread_id();
  const std::string thread_id_dir_str = base::IntToString(thread_id) + "/";

  // The kernel is at liberty to wake the thread id futex before updating
  // /proc. Following Stop(), the thread is joined, but entries in /proc may
  // not have been updated.
  thread->Stop();

  unsigned int iterations = 0;
  bool thread_present_in_procfs = true;
  // Poll /proc with an exponential back-off, sleeping 2^iterations nanoseconds
  // in nanosleep(2).
  // Note: the clock may not allow for nanosecond granularity, in this case the
  // first iterations would sleep a tiny bit more instead, which would not
  // change the calculations significantly.
  while (thread_present_in_procfs) {
    struct stat task_stat;
    const int fstat_ret =
        fstatat(proc_self_task, thread_id_dir_str.c_str(), &task_stat, 0);
    if (fstat_ret < 0) {
      PCHECK(ENOENT == errno);
      // The thread disappeared from /proc, we're done.
      thread_present_in_procfs = false;
      break;
    }
    // Increase the waiting time exponentially.
    struct timespec ts = {0, 1L << iterations /* nanoseconds */};
    PCHECK(0 == HANDLE_EINTR(nanosleep(&ts, &ts)));
    ++iterations;

    // Crash after 30 iterations, which means having spent roughly 2s in
    // nanosleep(2) cumulatively.
    CHECK_GT(30U, iterations);
    // In practice, this never goes through more than a couple iterations. In
    // debug mode, crash after 64ms (+ eventually 25 times the granularity of
    // the clock) in nanosleep(2).
    DCHECK_GT(25U, iterations);
  }

  return true;
}

}  // namespace sandbox
