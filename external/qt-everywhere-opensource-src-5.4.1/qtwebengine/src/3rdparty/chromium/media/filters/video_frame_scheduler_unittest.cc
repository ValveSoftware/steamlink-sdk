// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/debug/stack_trace.h"
#include "base/run_loop.h"
#include "media/base/video_frame.h"
#include "media/filters/clockless_video_frame_scheduler.h"
#include "media/filters/test_video_frame_scheduler.h"
#include "media/filters/video_frame_scheduler_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

static void DoNothing(const scoped_refptr<VideoFrame>& frame) {
}

static void CheckForReentrancy(std::string* stack_trace,
                               const scoped_refptr<VideoFrame>& frame,
                               VideoFrameScheduler::Reason reason) {
  *stack_trace = base::debug::StackTrace().ToString();
  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::MessageLoop::QuitClosure());
}

// Type parameterized test harness for validating API contract of
// VideoFrameScheduler implementations.
//
// NOTE: C++ requires using "this" for derived class templates when referencing
// class members.
template <typename T>
class VideoFrameSchedulerTest : public testing::Test {
 public:
  VideoFrameSchedulerTest() {}
  virtual ~VideoFrameSchedulerTest() {}

  base::MessageLoop message_loop_;
  T scheduler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoFrameSchedulerTest);
};

template <>
VideoFrameSchedulerTest<ClocklessVideoFrameScheduler>::VideoFrameSchedulerTest()
    : scheduler_(base::Bind(&DoNothing)) {
}

template <>
VideoFrameSchedulerTest<VideoFrameSchedulerImpl>::VideoFrameSchedulerTest()
    : scheduler_(message_loop_.message_loop_proxy(), base::Bind(&DoNothing)) {
}

TYPED_TEST_CASE_P(VideoFrameSchedulerTest);

TYPED_TEST_P(VideoFrameSchedulerTest, ScheduleVideoFrameIsntReentrant) {
  scoped_refptr<VideoFrame> frame =
      VideoFrame::CreateBlackFrame(gfx::Size(8, 8));

  std::string stack_trace;
  this->scheduler_.ScheduleVideoFrame(
      frame, base::TimeTicks(), base::Bind(&CheckForReentrancy, &stack_trace));
  EXPECT_TRUE(stack_trace.empty()) << "Reentracy detected:\n" << stack_trace;
}

REGISTER_TYPED_TEST_CASE_P(VideoFrameSchedulerTest,
                           ScheduleVideoFrameIsntReentrant);

INSTANTIATE_TYPED_TEST_CASE_P(ClocklessVideoFrameScheduler,
                              VideoFrameSchedulerTest,
                              ClocklessVideoFrameScheduler);
INSTANTIATE_TYPED_TEST_CASE_P(VideoFrameSchedulerImpl,
                              VideoFrameSchedulerTest,
                              VideoFrameSchedulerImpl);
INSTANTIATE_TYPED_TEST_CASE_P(TestVideoFrameScheduler,
                              VideoFrameSchedulerTest,
                              TestVideoFrameScheduler);

}  // namespace media
