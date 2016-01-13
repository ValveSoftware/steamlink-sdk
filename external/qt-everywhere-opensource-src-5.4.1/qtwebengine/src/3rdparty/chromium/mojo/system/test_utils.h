// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_TEST_UTILS_H_
#define MOJO_SYSTEM_TEST_UTILS_H_

#include <stdint.h>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/task_runner.h"
#include "base/threading/thread.h"
#include "base/time/time.h"

namespace tracked_objects {
class Location;
}

namespace mojo {
namespace system {
namespace test {

// Posts the given task (to the given task runner) and waits for it to complete.
// (Note: Doesn't spin the current thread's message loop, so if you're careless
// this could easily deadlock.)
void PostTaskAndWait(scoped_refptr<base::TaskRunner> task_runner,
                     const tracked_objects::Location& from_here,
                     const base::Closure& task);

// A timeout smaller than |TestTimeouts::tiny_timeout()|. Warning: This may lead
// to flakiness, but this is unavoidable if, e.g., you're trying to ensure that
// functions with timeouts are reasonably accurate. We want this to be as small
// as possible without causing too much flakiness.
base::TimeDelta EpsilonTimeout();

// Stopwatch -------------------------------------------------------------------

// A simple "stopwatch" for measuring time elapsed from a given starting point.
class Stopwatch {
 public:
  Stopwatch() {}
  ~Stopwatch() {}

  void Start() {
    start_time_ = base::TimeTicks::HighResNow();
  }

  base::TimeDelta Elapsed() {
    return base::TimeTicks::HighResNow() - start_time_;
  }

 private:
  base::TimeTicks start_time_;

  DISALLOW_COPY_AND_ASSIGN(Stopwatch);
};

// TestIOThread ----------------------------------------------------------------

class TestIOThread {
 public:
  enum Mode { kAutoStart, kManualStart };
  explicit TestIOThread(Mode mode);
  // Stops the I/O thread if necessary.
  ~TestIOThread();

  // |Start()|/|Stop()| should only be called from the main (creation) thread.
  // After |Stop()|, |Start()| may be called again to start a new I/O thread.
  // |Stop()| may be called even when the I/O thread is not started.
  void Start();
  void Stop();

  void PostTask(const tracked_objects::Location& from_here,
                const base::Closure& task);
  void PostTaskAndWait(const tracked_objects::Location& from_here,
                       const base::Closure& task);

  base::MessageLoopForIO* message_loop() {
    return static_cast<base::MessageLoopForIO*>(io_thread_.message_loop());
  }

  scoped_refptr<base::TaskRunner> task_runner() {
    return message_loop()->message_loop_proxy();
  }

 private:
  base::Thread io_thread_;
  bool io_thread_started_;

  DISALLOW_COPY_AND_ASSIGN(TestIOThread);
};

}  // namespace test
}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_TEST_UTILS_H_
