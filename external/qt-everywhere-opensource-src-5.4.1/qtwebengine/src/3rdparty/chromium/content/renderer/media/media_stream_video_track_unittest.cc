// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_checker_impl.h"
#include "content/child/child_process.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/mock_media_stream_video_sink.h"
#include "content/renderer/media/mock_media_stream_video_source.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

const uint8 kBlackValue = 0x00;
const uint8 kColorValue = 0xAB;

ACTION_P(RunClosure, closure) {
  closure.Run();
}

class MediaStreamVideoTrackTest : public ::testing::Test {
 public:
  MediaStreamVideoTrackTest()
      : child_process_(new ChildProcess()),
        mock_source_(new MockMediaStreamVideoSource(false)),
        source_started_(false) {
    blink_source_.initialize(base::UTF8ToUTF16("dummy_source_id"),
                              blink::WebMediaStreamSource::TypeVideo,
                              base::UTF8ToUTF16("dummy_source_name"));
    blink_source_.setExtraData(mock_source_);
  }

  virtual ~MediaStreamVideoTrackTest() {
  }

  void DeliverVideoFrameAndWaitForRenderer(MockMediaStreamVideoSink* sink) {
    base::RunLoop run_loop;
    base::Closure quit_closure = run_loop.QuitClosure();
    EXPECT_CALL(*sink, OnVideoFrame()).WillOnce(
        RunClosure(quit_closure));
    scoped_refptr<media::VideoFrame> frame =
        media::VideoFrame::CreateColorFrame(
            gfx::Size(MediaStreamVideoSource::kDefaultWidth,
                      MediaStreamVideoSource::kDefaultHeight),
            kColorValue, kColorValue, kColorValue, base::TimeDelta());
    mock_source()->DeliverVideoFrame(frame);
    run_loop.Run();
  }

 protected:
  base::MessageLoop* io_message_loop() const {
    return child_process_->io_message_loop();
  }

  // Create a track that's associated with |mock_source_|.
  blink::WebMediaStreamTrack CreateTrack() {
    blink::WebMediaConstraints constraints;
    constraints.initialize();
    bool enabled = true;
    blink::WebMediaStreamTrack track =
        MediaStreamVideoTrack::CreateVideoTrack(
            mock_source_, constraints,
            MediaStreamSource::ConstraintsCallback(), enabled);
    if (!source_started_) {
      mock_source_->StartMockedSource();
      source_started_ = true;
    }
    return track;
  }

  MockMediaStreamVideoSource* mock_source() { return mock_source_; }
  const blink::WebMediaStreamSource& blink_source() const {
    return blink_source_;
  }

 private:
  base::MessageLoopForUI message_loop_;
  scoped_ptr<ChildProcess> child_process_;
  blink::WebMediaStreamSource blink_source_;
  // |mock_source_| is owned by |webkit_source_|.
  MockMediaStreamVideoSource* mock_source_;
  bool source_started_;
};

TEST_F(MediaStreamVideoTrackTest, AddAndRemoveSink) {
  MockMediaStreamVideoSink sink;
  blink::WebMediaStreamTrack track = CreateTrack();
  MediaStreamVideoSink::AddToVideoTrack(
      &sink, sink.GetDeliverFrameCB(), track);

  DeliverVideoFrameAndWaitForRenderer(&sink);
  EXPECT_EQ(1, sink.number_of_frames());

  DeliverVideoFrameAndWaitForRenderer(&sink);

  MediaStreamVideoSink::RemoveFromVideoTrack(&sink, track);

  scoped_refptr<media::VideoFrame> frame =
      media::VideoFrame::CreateBlackFrame(
          gfx::Size(MediaStreamVideoSource::kDefaultWidth,
                    MediaStreamVideoSource::kDefaultHeight));
  mock_source()->DeliverVideoFrame(frame);
  // Wait for the IO thread to complete delivering frames.
  io_message_loop()->RunUntilIdle();
  EXPECT_EQ(2, sink.number_of_frames());
}

class CheckThreadHelper {
 public:
  CheckThreadHelper(base::Closure callback, bool* correct)
      : callback_(callback),
        correct_(correct) {
  }

  ~CheckThreadHelper() {
    *correct_ = thread_checker_.CalledOnValidThread();
    callback_.Run();
  }

 private:
  base::Closure callback_;
  bool* correct_;
  base::ThreadCheckerImpl thread_checker_;
};

void CheckThreadVideoFrameReceiver(
    CheckThreadHelper* helper,
    const scoped_refptr<media::VideoFrame>& frame,
    const media::VideoCaptureFormat& format,
    const base::TimeTicks& estimated_capture_time) {
  // Do nothing.
}

// Checks that the callback given to the track is reset on the right thread.
TEST_F(MediaStreamVideoTrackTest, ResetCallbackOnThread) {
  MockMediaStreamVideoSink sink;
  blink::WebMediaStreamTrack track = CreateTrack();

  base::RunLoop run_loop;
  bool correct = false;
  MediaStreamVideoSink::AddToVideoTrack(
      &sink,
      base::Bind(
          &CheckThreadVideoFrameReceiver,
          base::Owned(new CheckThreadHelper(run_loop.QuitClosure(), &correct))),
      track);
  MediaStreamVideoSink::RemoveFromVideoTrack(&sink, track);
  run_loop.Run();
  EXPECT_TRUE(correct) << "Not called on correct thread.";
}

TEST_F(MediaStreamVideoTrackTest, SetEnabled) {
  MockMediaStreamVideoSink sink;
  blink::WebMediaStreamTrack track = CreateTrack();
  MediaStreamVideoSink::AddToVideoTrack(
      &sink, sink.GetDeliverFrameCB(), track);

  MediaStreamVideoTrack* video_track =
      MediaStreamVideoTrack::GetVideoTrack(track);

  DeliverVideoFrameAndWaitForRenderer(&sink);
  EXPECT_EQ(1, sink.number_of_frames());
  EXPECT_EQ(kColorValue, *sink.last_frame()->data(media::VideoFrame::kYPlane));

  video_track->SetEnabled(false);
  EXPECT_FALSE(sink.enabled());

  DeliverVideoFrameAndWaitForRenderer(&sink);
  EXPECT_EQ(2, sink.number_of_frames());
  EXPECT_EQ(kBlackValue, *sink.last_frame()->data(media::VideoFrame::kYPlane));

  video_track->SetEnabled(true);
  EXPECT_TRUE(sink.enabled());
  DeliverVideoFrameAndWaitForRenderer(&sink);
  EXPECT_EQ(3, sink.number_of_frames());
  EXPECT_EQ(kColorValue, *sink.last_frame()->data(media::VideoFrame::kYPlane));
  MediaStreamVideoSink::RemoveFromVideoTrack(&sink, track);
}

TEST_F(MediaStreamVideoTrackTest, SourceStopped) {
  MockMediaStreamVideoSink sink;
  blink::WebMediaStreamTrack track = CreateTrack();
  MediaStreamVideoSink::AddToVideoTrack(
      &sink, sink.GetDeliverFrameCB(), track);
  EXPECT_EQ(blink::WebMediaStreamSource::ReadyStateLive, sink.state());

  mock_source()->StopSource();
  EXPECT_EQ(blink::WebMediaStreamSource::ReadyStateEnded, sink.state());
  MediaStreamVideoSink::RemoveFromVideoTrack(&sink, track);
}

TEST_F(MediaStreamVideoTrackTest, StopLastTrack) {
  MockMediaStreamVideoSink sink1;
  blink::WebMediaStreamTrack track1 = CreateTrack();
  MediaStreamVideoSink::AddToVideoTrack(
      &sink1, sink1.GetDeliverFrameCB(), track1);
  EXPECT_EQ(blink::WebMediaStreamSource::ReadyStateLive, sink1.state());

  EXPECT_EQ(blink::WebMediaStreamSource::ReadyStateLive,
            blink_source().readyState());

  MockMediaStreamVideoSink sink2;
  blink::WebMediaStreamTrack track2 = CreateTrack();
  MediaStreamVideoSink::AddToVideoTrack(
      &sink2, sink2.GetDeliverFrameCB(), track2);
  EXPECT_EQ(blink::WebMediaStreamSource::ReadyStateLive, sink2.state());

  MediaStreamVideoTrack* native_track1 =
      MediaStreamVideoTrack::GetVideoTrack(track1);
  native_track1->Stop();
  EXPECT_EQ(blink::WebMediaStreamSource::ReadyStateEnded, sink1.state());
  EXPECT_EQ(blink::WebMediaStreamSource::ReadyStateLive,
              blink_source().readyState());
  MediaStreamVideoSink::RemoveFromVideoTrack(&sink1, track1);

  MediaStreamVideoTrack* native_track2 =
        MediaStreamVideoTrack::GetVideoTrack(track2);
  native_track2->Stop();
  EXPECT_EQ(blink::WebMediaStreamSource::ReadyStateEnded, sink2.state());
  EXPECT_EQ(blink::WebMediaStreamSource::ReadyStateEnded,
            blink_source().readyState());
  MediaStreamVideoSink::RemoveFromVideoTrack(&sink2, track2);
}

}  // namespace content
