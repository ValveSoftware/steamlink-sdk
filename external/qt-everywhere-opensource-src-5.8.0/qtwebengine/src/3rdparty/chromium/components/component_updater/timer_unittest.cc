// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "components/component_updater/timer.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::string;

namespace component_updater {

class ComponentUpdaterTimerTest : public testing::Test {
 public:
  ComponentUpdaterTimerTest() {}
  ~ComponentUpdaterTimerTest() override {}

 private:
  base::MessageLoopForUI message_loop_;
};

TEST_F(ComponentUpdaterTimerTest, Start) {
  class TimerClientFake {
   public:
    TimerClientFake(int max_count, const base::Closure& quit_closure)
        : max_count_(max_count), quit_closure_(quit_closure), count_(0) {}

    void OnTimerEvent() {
      ++count_;
      if (count_ >= max_count_)
        quit_closure_.Run();
    }

    int count() const { return count_; }

   private:
    const int max_count_;
    const base::Closure quit_closure_;

    int count_;
  };

  base::RunLoop run_loop;
  TimerClientFake timer_client_fake(3, run_loop.QuitClosure());
  EXPECT_EQ(0, timer_client_fake.count());

  Timer timer;
  const base::TimeDelta delay(base::TimeDelta::FromMilliseconds(1));
  timer.Start(delay, delay, base::Bind(&TimerClientFake::OnTimerEvent,
                                       base::Unretained(&timer_client_fake)));
  run_loop.Run();
  timer.Stop();

  EXPECT_EQ(3, timer_client_fake.count());
}

}  // namespace component_updater
