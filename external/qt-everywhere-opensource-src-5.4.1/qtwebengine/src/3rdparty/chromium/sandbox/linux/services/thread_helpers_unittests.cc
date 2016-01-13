// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/services/thread_helpers.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/process_metrics.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "sandbox/linux/tests/unit_tests.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::PlatformThread;

namespace sandbox {

namespace {

int GetRaceTestIterations() {
  if (IsRunningOnValgrind()) {
    return 2;
  } else {
    return 1000;
  }
}

class ScopedProcSelfTask {
 public:
  ScopedProcSelfTask() : fd_(-1) {
    fd_ = open("/proc/self/task/", O_RDONLY | O_DIRECTORY);
    CHECK_LE(0, fd_);
  }

  ~ScopedProcSelfTask() { PCHECK(0 == IGNORE_EINTR(close(fd_))); }

  int fd() { return fd_; }

 private:
  int fd_;
  DISALLOW_COPY_AND_ASSIGN(ScopedProcSelfTask);
};

#if defined(THREAD_SANITIZER)
// These tests fail under ThreadSanitizer, see http://crbug.com/342305
#define MAYBE_IsSingleThreadedBasic DISABLED_IsSingleThreadedBasic
#define MAYBE_IsSingleThreadedIterated DISABLED_IsSingleThreadedIterated
#define MAYBE_IsSingleThreadedStartAndStop DISABLED_IsSingleThreadedStartAndStop
#else
#define MAYBE_IsSingleThreadedBasic IsSingleThreadedBasic
#define MAYBE_IsSingleThreadedIterated IsSingleThreadedIterated
#define MAYBE_IsSingleThreadedStartAndStop IsSingleThreadedStartAndStop
#endif

TEST(ThreadHelpers, MAYBE_IsSingleThreadedBasic) {
  ScopedProcSelfTask task;
  ASSERT_TRUE(ThreadHelpers::IsSingleThreaded(task.fd()));
  ASSERT_TRUE(ThreadHelpers::IsSingleThreaded(-1));

  base::Thread thread("sandbox_tests");
  ASSERT_TRUE(thread.Start());
  ASSERT_FALSE(ThreadHelpers::IsSingleThreaded(task.fd()));
  ASSERT_FALSE(ThreadHelpers::IsSingleThreaded(-1));
  // Explicitly stop the thread here to not pollute the next test.
  ASSERT_TRUE(ThreadHelpers::StopThreadAndWatchProcFS(task.fd(), &thread));
}

TEST(ThreadHelpers, MAYBE_IsSingleThreadedIterated) {
  ScopedProcSelfTask task;
  ASSERT_TRUE(ThreadHelpers::IsSingleThreaded(task.fd()));

  // Iterate to check for race conditions.
  for (int i = 0; i < GetRaceTestIterations(); ++i) {
    base::Thread thread("sandbox_tests");
    ASSERT_TRUE(thread.Start());
    ASSERT_FALSE(ThreadHelpers::IsSingleThreaded(task.fd()));
    // Explicitly stop the thread here to not pollute the next test.
    ASSERT_TRUE(ThreadHelpers::StopThreadAndWatchProcFS(task.fd(), &thread));
  }
}

TEST(ThreadHelpers, MAYBE_IsSingleThreadedStartAndStop) {
  ScopedProcSelfTask task;
  ASSERT_TRUE(ThreadHelpers::IsSingleThreaded(task.fd()));

  base::Thread thread("sandbox_tests");
  // This is testing for a race condition, so iterate.
  // Manually, this has been tested with more that 1M iterations.
  for (int i = 0; i < GetRaceTestIterations(); ++i) {
    ASSERT_TRUE(thread.Start());
    ASSERT_FALSE(ThreadHelpers::IsSingleThreaded(task.fd()));

    ASSERT_TRUE(ThreadHelpers::StopThreadAndWatchProcFS(task.fd(), &thread));
    ASSERT_TRUE(ThreadHelpers::IsSingleThreaded(task.fd()));
    ASSERT_EQ(1, base::GetNumberOfThreads(base::GetCurrentProcessHandle()));
  }
}

}  // namespace

}  // namespace sandbox
