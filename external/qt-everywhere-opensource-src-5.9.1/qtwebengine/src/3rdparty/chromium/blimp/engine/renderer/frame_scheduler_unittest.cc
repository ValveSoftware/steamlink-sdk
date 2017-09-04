// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/renderer/frame_scheduler.h"

#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {
namespace engine {
namespace {

class FrameSchedulerTest : public testing::Test, public FrameSchedulerClient {
 public:
  FrameSchedulerTest() : scheduler_(base::ThreadTaskRunnerHandle::Get(), this) {
    scheduler_.set_frame_delay_for_testing(base::TimeDelta::FromSeconds(0));
  }
  ~FrameSchedulerTest() override {}

  // FrameSchedulerClient implementation.
  void StartFrameUpdate() override {
    num_frames_++;
    if (send_client_update_during_frame_) {
      scheduler_.DidSendFrameUpdateToClient();
      send_client_update_during_frame_ = false;
    }
  }

 protected:
  base::MessageLoop loop_;
  FrameScheduler scheduler_;
  int num_frames_ = 0;

  bool send_client_update_during_frame_ = false;
};

TEST_F(FrameSchedulerTest, FirstMainFrameRequest) {
  // The request for the first main frame should be responded to right away.
  scheduler_.ScheduleFrameUpdate();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, num_frames_);
}

TEST_F(FrameSchedulerTest, MultipleMainFrameRequestsResultInSingleFrame) {
  // Multiple main frame requests result in a single callback to the client.
  scheduler_.ScheduleFrameUpdate();
  scheduler_.ScheduleFrameUpdate();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, num_frames_);
}

TEST_F(FrameSchedulerTest, MainFrameBlockedOnAckFromClient) {
  // Request a main frame, start it and send an update to the client. The second
  // frame should remain blocked till an ack is received.
  scheduler_.ScheduleFrameUpdate();
  send_client_update_during_frame_ = true;
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, num_frames_);

  // Request a frame and ensure that the client is not asked to start it.
  scheduler_.ScheduleFrameUpdate();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, num_frames_);

  // Now we have an ack from the client. This should start another frame.
  scheduler_.DidReceiveFrameUpdateAck();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, num_frames_);
}

}  // namespace
}  // namespace engine
}  // namespace blimp
