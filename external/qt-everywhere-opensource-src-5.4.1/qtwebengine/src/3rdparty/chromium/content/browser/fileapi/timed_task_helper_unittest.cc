// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/browser/fileapi/timed_task_helper.h"

using fileapi::TimedTaskHelper;

namespace content {

namespace {

class Embedder {
 public:
  Embedder()
      : timer_(base::MessageLoopProxy::current().get()), timer_fired_(false) {}

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
