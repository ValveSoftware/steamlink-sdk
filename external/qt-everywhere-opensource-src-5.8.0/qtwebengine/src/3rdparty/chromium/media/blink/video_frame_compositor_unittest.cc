// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/simple_test_tick_clock.h"
#include "cc/layers/video_frame_provider.h"
#include "media/base/video_frame.h"
#include "media/blink/video_frame_compositor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::DoAll;
using testing::Return;

namespace media {

ACTION_P(RunClosure, closure) {
  closure.Run();
}

class VideoFrameCompositorTest : public testing::Test,
                                 public cc::VideoFrameProvider::Client,
                                 public VideoRendererSink::RenderCallback {
 public:
  VideoFrameCompositorTest()
      : tick_clock_(new base::SimpleTestTickClock()),
        compositor_(new VideoFrameCompositor(message_loop.task_runner())),
        did_receive_frame_count_(0) {
    compositor_->SetVideoFrameProviderClient(this);
    compositor_->set_tick_clock_for_testing(
        std::unique_ptr<base::TickClock>(tick_clock_));
    // Disable background rendering by default.
    compositor_->set_background_rendering_for_testing(false);
  }

  ~VideoFrameCompositorTest() override {
    compositor_->SetVideoFrameProviderClient(nullptr);
  }

  scoped_refptr<VideoFrame> CreateOpaqueFrame() {
    gfx::Size size(8, 8);
    return VideoFrame::CreateFrame(PIXEL_FORMAT_YV12, size, gfx::Rect(size),
                                   size, base::TimeDelta());
  }

  VideoFrameCompositor* compositor() { return compositor_.get(); }
  int did_receive_frame_count() { return did_receive_frame_count_; }

 protected:
  // cc::VideoFrameProvider::Client implementation.
  void StopUsingProvider() override {}
  MOCK_METHOD0(StartRendering, void());
  MOCK_METHOD0(StopRendering, void());
  void DidReceiveFrame() override { ++did_receive_frame_count_; }

  // VideoRendererSink::RenderCallback implementation.
  MOCK_METHOD3(Render,
               scoped_refptr<VideoFrame>(base::TimeTicks,
                                         base::TimeTicks,
                                         bool));
  MOCK_METHOD0(OnFrameDropped, void());

  void StartVideoRendererSink() {
    EXPECT_CALL(*this, StartRendering());
    const bool had_current_frame = !!compositor_->GetCurrentFrame();
    compositor()->Start(this);
    // If we previously had a frame, we should still have one now.
    EXPECT_EQ(had_current_frame, !!compositor_->GetCurrentFrame());
    base::RunLoop().RunUntilIdle();
  }

  void StopVideoRendererSink(bool have_client) {
    if (have_client)
      EXPECT_CALL(*this, StopRendering());
    const bool had_current_frame = !!compositor_->GetCurrentFrame();
    compositor()->Stop();
    // If we previously had a frame, we should still have one now.
    EXPECT_EQ(had_current_frame, !!compositor_->GetCurrentFrame());
    base::RunLoop().RunUntilIdle();
  }

  void RenderFrame() {
    compositor()->GetCurrentFrame();
    compositor()->PutCurrentFrame();
  }

  base::MessageLoop message_loop;
  base::SimpleTestTickClock* tick_clock_;  // Owned by |compositor_|
  std::unique_ptr<VideoFrameCompositor> compositor_;

  int did_receive_frame_count_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameCompositorTest);
};

TEST_F(VideoFrameCompositorTest, InitialValues) {
  EXPECT_FALSE(compositor()->GetCurrentFrame().get());
}

TEST_F(VideoFrameCompositorTest, PaintSingleFrame) {
  scoped_refptr<VideoFrame> expected = VideoFrame::CreateEOSFrame();

  // Should notify compositor synchronously.
  EXPECT_EQ(0, did_receive_frame_count());
  compositor()->PaintSingleFrame(expected);
  scoped_refptr<VideoFrame> actual = compositor()->GetCurrentFrame();
  EXPECT_EQ(expected, actual);
  EXPECT_EQ(1, did_receive_frame_count());
}

TEST_F(VideoFrameCompositorTest, VideoRendererSinkFrameDropped) {
  scoped_refptr<VideoFrame> opaque_frame = CreateOpaqueFrame();

  EXPECT_CALL(*this, Render(_, _, _)).WillRepeatedly(Return(opaque_frame));
  StartVideoRendererSink();

  EXPECT_TRUE(
      compositor()->UpdateCurrentFrame(base::TimeTicks(), base::TimeTicks()));

  // Another call should trigger a dropped frame callback.
  EXPECT_CALL(*this, OnFrameDropped());
  EXPECT_FALSE(
      compositor()->UpdateCurrentFrame(base::TimeTicks(), base::TimeTicks()));

  // Ensure it always happens until the frame is rendered.
  EXPECT_CALL(*this, OnFrameDropped());
  EXPECT_FALSE(
      compositor()->UpdateCurrentFrame(base::TimeTicks(), base::TimeTicks()));

  // Call GetCurrentFrame() but not PutCurrentFrame()
  compositor()->GetCurrentFrame();

  // The frame should still register as dropped until PutCurrentFrame is called.
  EXPECT_CALL(*this, OnFrameDropped());
  EXPECT_FALSE(
      compositor()->UpdateCurrentFrame(base::TimeTicks(), base::TimeTicks()));

  RenderFrame();
  EXPECT_FALSE(
      compositor()->UpdateCurrentFrame(base::TimeTicks(), base::TimeTicks()));

  StopVideoRendererSink(true);
}

TEST_F(VideoFrameCompositorTest, VideoLayerShutdownWhileRendering) {
  EXPECT_CALL(*this, Render(_, _, true)).WillOnce(Return(nullptr));
  StartVideoRendererSink();
  compositor_->SetVideoFrameProviderClient(nullptr);
  StopVideoRendererSink(false);
}

TEST_F(VideoFrameCompositorTest, StartFiresBackgroundRender) {
  scoped_refptr<VideoFrame> opaque_frame = CreateOpaqueFrame();
  EXPECT_CALL(*this, Render(_, _, true)).WillRepeatedly(Return(opaque_frame));
  StartVideoRendererSink();
  StopVideoRendererSink(true);
}

TEST_F(VideoFrameCompositorTest, BackgroundRenderTicks) {
  scoped_refptr<VideoFrame> opaque_frame = CreateOpaqueFrame();
  compositor_->set_background_rendering_for_testing(true);

  base::RunLoop run_loop;
  EXPECT_CALL(*this, Render(_, _, true))
      .WillOnce(Return(opaque_frame))
      .WillOnce(
          DoAll(RunClosure(run_loop.QuitClosure()), Return(opaque_frame)));
  StartVideoRendererSink();
  run_loop.Run();

  // UpdateCurrentFrame() calls should indicate they are not synthetic.
  EXPECT_CALL(*this, Render(_, _, false)).WillOnce(Return(opaque_frame));
  EXPECT_FALSE(
      compositor()->UpdateCurrentFrame(base::TimeTicks(), base::TimeTicks()));

  // Background rendering should tick another render callback.
  StopVideoRendererSink(true);
}

TEST_F(VideoFrameCompositorTest,
       UpdateCurrentFrameWorksWhenBackgroundRendered) {
  scoped_refptr<VideoFrame> opaque_frame = CreateOpaqueFrame();
  compositor_->set_background_rendering_for_testing(true);

  // Background render a frame that succeeds immediately.
  EXPECT_CALL(*this, Render(_, _, true)).WillOnce(Return(opaque_frame));
  StartVideoRendererSink();

  // The background render completes immediately, so the next call to
  // UpdateCurrentFrame is expected to return true to account for the frame
  // rendered in the background.
  EXPECT_CALL(*this, Render(_, _, false))
      .WillOnce(Return(scoped_refptr<VideoFrame>(opaque_frame)));
  EXPECT_TRUE(
      compositor()->UpdateCurrentFrame(base::TimeTicks(), base::TimeTicks()));
  RenderFrame();

  // Second call to UpdateCurrentFrame will return false as no new frame has
  // been created since the last call.
  EXPECT_CALL(*this, Render(_, _, false))
      .WillOnce(Return(scoped_refptr<VideoFrame>(opaque_frame)));
  EXPECT_FALSE(
      compositor()->UpdateCurrentFrame(base::TimeTicks(), base::TimeTicks()));

  StopVideoRendererSink(true);
}

TEST_F(VideoFrameCompositorTest, GetCurrentFrameAndUpdateIfStale) {
  scoped_refptr<VideoFrame> opaque_frame_1 = CreateOpaqueFrame();
  scoped_refptr<VideoFrame> opaque_frame_2 = CreateOpaqueFrame();
  compositor_->set_background_rendering_for_testing(true);

  // |current_frame_| should be null at this point since we don't have a client
  // or a callback.
  ASSERT_FALSE(compositor()->GetCurrentFrameAndUpdateIfStale());

  // Starting the video renderer should return a single frame.
  EXPECT_CALL(*this, Render(_, _, true)).WillOnce(Return(opaque_frame_1));
  StartVideoRendererSink();

  // Since we have a client, this call should not call background render, even
  // if a lot of time has elapsed between calls.
  tick_clock_->Advance(base::TimeDelta::FromSeconds(1));
  ASSERT_EQ(opaque_frame_1, compositor()->GetCurrentFrameAndUpdateIfStale());

  // An update current frame call should stop background rendering.
  EXPECT_CALL(*this, Render(_, _, false)).WillOnce(Return(opaque_frame_2));
  EXPECT_TRUE(
      compositor()->UpdateCurrentFrame(base::TimeTicks(), base::TimeTicks()));

  // This call should still not call background render.
  ASSERT_EQ(opaque_frame_2, compositor()->GetCurrentFrameAndUpdateIfStale());

  testing::Mock::VerifyAndClearExpectations(this);

  // Clear our client, which means no mock function calls for Client.
  compositor()->SetVideoFrameProviderClient(nullptr);

  // This call should still not call background render, because we aren't in the
  // background rendering state yet.
  ASSERT_EQ(opaque_frame_2, compositor()->GetCurrentFrameAndUpdateIfStale());

  // Wait for background rendering to tick again.
  base::RunLoop run_loop;
  EXPECT_CALL(*this, Render(_, _, true))
      .WillOnce(
           DoAll(RunClosure(run_loop.QuitClosure()), Return(opaque_frame_1)))
      .WillOnce(Return(opaque_frame_2));
  run_loop.Run();

  // This call should still not call background render, because not enough time
  // has elapsed since the last background render call.
  ASSERT_EQ(opaque_frame_1, compositor()->GetCurrentFrameAndUpdateIfStale());

  // Advancing the tick clock should allow a new frame to be requested.
  tick_clock_->Advance(base::TimeDelta::FromMilliseconds(10));
  ASSERT_EQ(opaque_frame_2, compositor()->GetCurrentFrameAndUpdateIfStale());

  // Background rendering should tick another render callback.
  StopVideoRendererSink(false);
}

}  // namespace media
