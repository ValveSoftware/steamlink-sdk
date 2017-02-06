// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "content/child/child_process.h"
#include "content/renderer/media/media_stream_video_renderer_sink.h"
#include "content/renderer/media/mock_media_stream_registry.h"
#include "media/base/video_frame.h"
#include "media/renderers/gpu_video_accelerator_factories.h"
#include "media/renderers/mock_gpu_memory_buffer_video_frame_pool.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebHeap.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::InSequence;
using ::testing::Lt;
using ::testing::Mock;

namespace content {

ACTION_P(RunClosure, closure) {
  closure.Run();
}

static const std::string kTestStreamUrl = "stream_url";
static const std::string kTestVideoTrackId = "video_track_id";

class MediaStreamVideoRendererSinkTest : public testing::Test {
 public:
  MediaStreamVideoRendererSinkTest() {
    registry_.Init(kTestStreamUrl);
    registry_.AddVideoTrack(kTestVideoTrackId);

    // Extract the Blink Video Track for the MSVRSink.
    registry_.test_stream().videoTracks(video_tracks_);
    EXPECT_EQ(1u, video_tracks_.size());

    media_stream_video_renderer_sink_ = new MediaStreamVideoRendererSink(
        video_tracks_[0],
        base::Bind(&MediaStreamVideoRendererSinkTest::ErrorCallback,
                   base::Unretained(this)),
        base::Bind(&MediaStreamVideoRendererSinkTest::RepaintCallback,
                   base::Unretained(this)),
        message_loop_.task_runner(), message_loop_.task_runner().get(),
        nullptr /* gpu_factories */);

    EXPECT_TRUE(IsInStoppedState());
  }

  ~MediaStreamVideoRendererSinkTest() {
    media_stream_video_renderer_sink_ = nullptr;
    registry_.reset();
    blink::WebHeap::collectAllGarbageForTesting();

    // Let the message loop run to finish destroying the pool.
    base::RunLoop().RunUntilIdle();
  }

  MOCK_METHOD1(RepaintCallback, void(const scoped_refptr<media::VideoFrame>&));
  MOCK_METHOD0(ErrorCallback, void(void));

  bool IsInStartedState() const {
    return media_stream_video_renderer_sink_->state_ ==
           MediaStreamVideoRendererSink::STARTED;
  }
  bool IsInStoppedState() const {
    return media_stream_video_renderer_sink_->state_ ==
           MediaStreamVideoRendererSink::STOPPED;
  }
  bool IsInPausedState() const {
    return media_stream_video_renderer_sink_->state_ ==
           MediaStreamVideoRendererSink::PAUSED;
  }

  void OnVideoFrame(const scoped_refptr<media::VideoFrame>& frame) {
    media_stream_video_renderer_sink_->OnVideoFrame(frame,
                                                    base::TimeTicks::Now());
  }

  scoped_refptr<MediaStreamVideoRendererSink> media_stream_video_renderer_sink_;

  // A ChildProcess and a MessageLoopForUI are both needed to fool the Tracks
  // and Sources in |registry_| into believing they are on the right threads.
  base::MessageLoopForUI message_loop_;
  const ChildProcess child_process_;

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks_;
  MockMediaStreamRegistry registry_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaStreamVideoRendererSinkTest);
};

// Checks that the initialization-destruction sequence works fine.
TEST_F(MediaStreamVideoRendererSinkTest, StartStop) {
  EXPECT_TRUE(IsInStoppedState());

  media_stream_video_renderer_sink_->Start();
  EXPECT_TRUE(IsInStartedState());

  media_stream_video_renderer_sink_->Pause();
  EXPECT_TRUE(IsInPausedState());

  media_stream_video_renderer_sink_->Resume();
  EXPECT_TRUE(IsInStartedState());

  media_stream_video_renderer_sink_->Stop();
  EXPECT_TRUE(IsInStoppedState());
}

// Sends 2 frames and expect them as WebM contained encoded data in writeData().
TEST_F(MediaStreamVideoRendererSinkTest, EncodeVideoFrames) {
  media_stream_video_renderer_sink_->Start();

  InSequence s;
  const scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::CreateBlackFrame(gfx::Size(160, 80));

  EXPECT_CALL(*this, RepaintCallback(video_frame)).Times(1);
  OnVideoFrame(video_frame);

  media_stream_video_renderer_sink_->Stop();
}

class MediaStreamVideoRendererSinkAsyncAddFrameReadyTest
    : public MediaStreamVideoRendererSinkTest {
 public:
  MediaStreamVideoRendererSinkAsyncAddFrameReadyTest() {
    media_stream_video_renderer_sink_->SetGpuMemoryBufferVideoForTesting(
        new media::MockGpuMemoryBufferVideoFramePool(&frame_ready_cbs_));
  }

 protected:
  std::vector<base::Closure> frame_ready_cbs_;
};

TEST_F(MediaStreamVideoRendererSinkAsyncAddFrameReadyTest,
       CreateHardwareFrames) {
  media_stream_video_renderer_sink_->Start();

  InSequence s;
  const scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::CreateBlackFrame(gfx::Size(160, 80));
  OnVideoFrame(video_frame);
  message_loop_.RunUntilIdle();
  ASSERT_EQ(1u, frame_ready_cbs_.size());

  EXPECT_CALL(*this, RepaintCallback(video_frame)).Times(1);
  frame_ready_cbs_[0].Run();
  message_loop_.RunUntilIdle();

  media_stream_video_renderer_sink_->Stop();
}

class MediaStreamVideoRendererSinkTransparencyTest
    : public MediaStreamVideoRendererSinkTest {
 public:
  MediaStreamVideoRendererSinkTransparencyTest() {
    media_stream_video_renderer_sink_ = new MediaStreamVideoRendererSink(
        video_tracks_[0],
        base::Bind(&MediaStreamVideoRendererSinkTest::ErrorCallback,
                   base::Unretained(this)),
        base::Bind(&MediaStreamVideoRendererSinkTransparencyTest::
                       VerifyTransparentFrame,
                   base::Unretained(this)),
        message_loop_.task_runner(), message_loop_.task_runner().get(),
        nullptr /* gpu_factories */);
  }

  void VerifyTransparentFrame(const scoped_refptr<media::VideoFrame>& frame) {
    EXPECT_EQ(media::PIXEL_FORMAT_YV12A, frame->format());
  }
};

TEST_F(MediaStreamVideoRendererSinkTransparencyTest,
       SendTransparentFrame) {
  media_stream_video_renderer_sink_->Start();

  InSequence s;
  const gfx::Size kSize(10, 10);
  const base::TimeDelta kTimestamp = base::TimeDelta();
  const scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::CreateFrame(media::PIXEL_FORMAT_YV12A, kSize,
                                     gfx::Rect(kSize), kSize, kTimestamp);
  OnVideoFrame(video_frame);
  message_loop_.RunUntilIdle();

  media_stream_video_renderer_sink_->Stop();
}

}  // namespace content
