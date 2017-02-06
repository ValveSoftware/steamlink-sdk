// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "storage/browser/fileapi/timed_task_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

using storage::TimedTaskHelper;

namespace content {

namespace {

class Embedder {
 public:
  Embedder()
      : timer_(base::ThreadTaskRunnerHandle::Get().get()),
        timer_fired_(false) {}

  void OnTimerFired() {
    timer_fired_ = true;
  }

  TimedTaskHelper* timer() { return &timer_; }
  bool timer_fired() const { return timer_fired_; }

 private:
  TimedTaskHelper timer_;
  bool timer_fired_;
};

}  // namespace

TEST(TimedTaskHelper, FireTimerWhenAlive) {
  base::MessageLoop message_loop;
  Embedder embedder;

  ASSERT_FALSE(embedder.timer_fired());
  ASSERT_FALSE(embedder.timer()->IsRunning());

  embedder.timer()->Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(0),
      base::Bind(&Embedder::OnTimerFired, base::Unretained(&embedder)));

  ASSERT_TRUE(embedder.timer()->IsRunning());
  embedder.timer()->Reset();
  ASSERT_TRUE(embedder.timer()->IsRunning());

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(embedder.timer_fired());
}

TEST(TimedTaskHelper, FireTimerWhenAlreadyDeleted) {
  base::MessageLoop message_loop;

  // Run message loop after embedder is already deleted to make sure callback
  // doesn't cause a crash for use after free.
  {
    Embedder embedder;

    ASSERT_FALSE(embedder.timer_fired());
    ASSERT_FALSE(embedder.timer()->IsRunning());

    embedder.timer()->Start(
        FROM_HERE,
        base::TimeDelta::FromSeconds(0),
        base::Bind(&Embedder::OnTimerFired, base::Unretained(&embedder)));

    ASSERT_TRUE(embedder.timer()->IsRunning());
  }

  // At this point the callback is still in the message queue but
  // embedder is gone.
  base::RunLoop().RunUntilIdle();
}

}  // namespace content
