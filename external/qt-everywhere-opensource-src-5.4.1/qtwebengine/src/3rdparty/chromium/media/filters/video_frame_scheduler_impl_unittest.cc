// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/base/test_helpers.h"
#include "media/base/video_frame.h"
#include "media/filters/video_frame_scheduler_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

using testing::_;

// NOTE: millisecond-level resolution is used for times as real delayed tasks
// are posted. Don't use large values if you want to keep tests running fast.
class VideoFrameSchedulerImplTest : public testing::Test {
 public:
  VideoFrameSchedulerImplTest()
      : scheduler_(message_loop_.message_loop_proxy(),
                   base::Bind(&VideoFrameSchedulerImplTest::OnDisplay,
                              base::Unretained(this))),
        tick_clock_(new base::SimpleTestTickClock()) {
    scheduler_.SetTickClockForTesting(scoped_ptr<base::TickClock>(tick_clock_));
  }

  virtual ~VideoFrameSchedulerImplTest() {}

  MOCK_METHOD1(OnDisplay, void(const scoped_refptr<VideoFrame>&));
  MOCK_METHOD2(OnFrameDone,
               void(const scoped_refptr<VideoFrame>&,
                    VideoFrameScheduler::Reason));

  void Schedule(const scoped_refptr<VideoFrame>& frame, int64 target_ms) {
    scheduler_.ScheduleVideoFrame(
        frame,
        base::TimeTicks() + base::TimeDelta::FromMilliseconds(target_ms),
        base::Bind(&VideoFrameSchedulerImplTest::OnFrameDone,
                   base::Unretained(this)));
  }

  void RunUntilTimeHasElapsed(int64 ms) {
    WaitableMessageLoopEvent waiter;
    message_loop_.PostDelayedTask(
        FROM_HERE, waiter.GetClosure(), base::TimeDelta::FromMilliseconds(ms));
    waiter.RunAndWait();
  }

  void AdvanceTime(int64 ms) {
    tick_clock_->Advance(base::TimeDelta::FromMilliseconds(ms));
  }

  void Reset() {
    scheduler_.Reset();
  }

 private:
  base::MessageLoop message_loop_;
  VideoFrameSchedulerImpl scheduler_;
  base::SimpleTestTickClock* tick_clock_;  // Owned by |scheduler_|.

  DISALLOW_COPY_AND_ASSIGN(VideoFrameSchedulerImplTest);
};

TEST_F(VideoFrameSchedulerImplTest, ImmediateDisplay) {
  scoped_refptr<VideoFrame> frame =
      VideoFrame::CreateBlackFrame(gfx::Size(8, 8));
  Schedule(frame, 0);

  EXPECT_CALL(*this, OnDisplay(frame));
  EXPECT_CALL(*this, OnFrameDone(frame, VideoFrameScheduler::DISPLAYED));
  RunUntilTimeHasElapsed(0);
}

TEST_F(VideoFrameSchedulerImplTest, EventualDisplay) {
  scoped_refptr<VideoFrame> frame =
      VideoFrame::CreateBlackFrame(gfx::Size(8, 8));
  Schedule(frame, 10);

  // Nothing should happen.
  RunUntilTimeHasElapsed(10);

  // Now we should get the frame.
  EXPECT_CALL(*this, OnDisplay(frame));
  EXPECT_CALL(*this, OnFrameDone(frame, VideoFrameScheduler::DISPLAYED));
  AdvanceTime(10);
  RunUntilTimeHasElapsed(10);
}

TEST_F(VideoFrameSchedulerImplTest, DroppedFrame) {
  scoped_refptr<VideoFrame> dropped =
      VideoFrame::CreateBlackFrame(gfx::Size(8, 8));
  scoped_refptr<VideoFrame> displayed =
      VideoFrame::CreateBlackFrame(gfx::Size(8, 8));
  Schedule(dropped, 10);
  Schedule(displayed, 20);

  // The frame past its deadline will get dropped.
  EXPECT_CALL(*this, OnDisplay(displayed));
  EXPECT_CALL(*this, OnFrameDone(dropped, VideoFrameScheduler::DROPPED));
  EXPECT_CALL(*this, OnFrameDone(displayed, VideoFrameScheduler::DISPLAYED));
  AdvanceTime(20);
  RunUntilTimeHasElapsed(20);
}

TEST_F(VideoFrameSchedulerImplTest, SingleFrameLate) {
  scoped_refptr<VideoFrame> frame =
      VideoFrame::CreateBlackFrame(gfx::Size(8, 8));
  Schedule(frame, 10);

  // Despite frame being late it should still get displayed as it's the only
  // one.
  EXPECT_CALL(*this, OnDisplay(frame));
  EXPECT_CALL(*this, OnFrameDone(frame, VideoFrameScheduler::DISPLAYED));
  AdvanceTime(20);
  RunUntilTimeHasElapsed(20);
}

TEST_F(VideoFrameSchedulerImplTest, ManyFramesLate) {
  scoped_refptr<VideoFrame> dropped =
      VideoFrame::CreateBlackFrame(gfx::Size(8, 8));
  scoped_refptr<VideoFrame> displayed =
      VideoFrame::CreateBlackFrame(gfx::Size(8, 8));
  Schedule(dropped, 10);
  Schedule(displayed, 20);

  // Despite both being late, the scheduler should always displays the latest
  // expired frame.
  EXPECT_CALL(*this, OnDisplay(displayed));
  EXPECT_CALL(*this, OnFrameDone(dropped, VideoFrameScheduler::DROPPED));
  EXPECT_CALL(*this, OnFrameDone(displayed, VideoFrameScheduler::DISPLAYED));
  AdvanceTime(30);
  RunUntilTimeHasElapsed(30);
}

TEST_F(VideoFrameSchedulerImplTest, Reset) {
  scoped_refptr<VideoFrame> frame =
      VideoFrame::CreateBlackFrame(gfx::Size(8, 8));
  Schedule(frame, 10);

  // Despite being on time, frame callback isn't run.
  EXPECT_CALL(*this, OnFrameDone(_, _)).Times(0);
  AdvanceTime(10);
  Reset();
  RunUntilTimeHasElapsed(10);
}

}  // namespace media
