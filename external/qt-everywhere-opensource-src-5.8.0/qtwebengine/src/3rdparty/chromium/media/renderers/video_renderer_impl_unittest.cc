// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/debug/stack_trace.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/base/data_buffer.h"
#include "media/base/gmock_callback_support.h"
#include "media/base/limits.h"
#include "media/base/mock_filters.h"
#include "media/base/null_video_sink.h"
#include "media/base/test_helpers.h"
#include "media/base/video_frame.h"
#include "media/base/wall_clock_time_source.h"
#include "media/renderers/mock_gpu_memory_buffer_video_frame_pool.h"
#include "media/renderers/video_renderer_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace media {

ACTION_P(RunClosure, closure) {
  closure.Run();
}

MATCHER_P(HasTimestamp, ms, "") {
  *result_listener << "has timestamp " << arg->timestamp().InMilliseconds();
  return arg->timestamp().InMilliseconds() == ms;
}

class VideoRendererImplTest : public testing::Test {
 public:
  VideoRendererImplTest()
      : tick_clock_(new base::SimpleTestTickClock()),
        decoder_(new NiceMock<MockVideoDecoder>()),
        demuxer_stream_(DemuxerStream::VIDEO) {
    ScopedVector<VideoDecoder> decoders;
    decoders.push_back(decoder_);

    null_video_sink_.reset(new NullVideoSink(
        false, base::TimeDelta::FromSecondsD(1.0 / 60),
        base::Bind(&MockCB::FrameReceived, base::Unretained(&mock_cb_)),
        message_loop_.task_runner()));

    renderer_.reset(new VideoRendererImpl(
        message_loop_.task_runner(), message_loop_.task_runner().get(),
        null_video_sink_.get(), std::move(decoders), true,
        nullptr,  // gpu_factories
        new MediaLog()));
    renderer_->SetTickClockForTesting(
        std::unique_ptr<base::TickClock>(tick_clock_));
    null_video_sink_->set_tick_clock_for_testing(tick_clock_);
    time_source_.set_tick_clock_for_testing(tick_clock_);

    // Start wallclock time at a non-zero value.
    AdvanceWallclockTimeInMs(12345);

    demuxer_stream_.set_video_decoder_config(TestVideoConfig::Normal());

    // We expect these to be called but we don't care how/when.
    EXPECT_CALL(demuxer_stream_, Read(_)).WillRepeatedly(
        RunCallback<0>(DemuxerStream::kOk,
                       scoped_refptr<DecoderBuffer>(new DecoderBuffer(0))));
  }

  virtual ~VideoRendererImplTest() {}

  void Initialize() {
    InitializeWithLowDelay(false);
  }

  void InitializeWithLowDelay(bool low_delay) {
    // Monitor decodes from the decoder.
    ON_CALL(*decoder_, Decode(_, _))
        .WillByDefault(Invoke(this, &VideoRendererImplTest::DecodeRequested));
    ON_CALL(*decoder_, Reset(_))
        .WillByDefault(Invoke(this, &VideoRendererImplTest::FlushRequested));

    // Initialize, we shouldn't have any reads.
    InitializeRenderer(low_delay, true);
  }

  void InitializeRenderer(bool low_delay, bool expect_success) {
    SCOPED_TRACE(base::StringPrintf("InitializeRenderer(%d)", expect_success));
    WaitableMessageLoopEvent event;
    CallInitialize(event.GetPipelineStatusCB(), low_delay, expect_success);
    event.RunAndWaitForStatus(expect_success ? PIPELINE_OK
                                             : DECODER_ERROR_NOT_SUPPORTED);
  }

  void CallInitialize(const PipelineStatusCB& status_cb,
                      bool low_delay,
                      bool expect_success) {
    if (low_delay)
      demuxer_stream_.set_liveness(DemuxerStream::LIVENESS_LIVE);
    EXPECT_CALL(*decoder_, Initialize(_, _, _, _, _))
        .WillOnce(
            DoAll(SaveArg<4>(&output_cb_), RunCallback<3>(expect_success)));
    EXPECT_CALL(mock_cb_, OnWaitingForDecryptionKey()).Times(0);
    renderer_->Initialize(&demuxer_stream_, nullptr, &mock_cb_,
                          base::Bind(&WallClockTimeSource::GetWallClockTimes,
                                     base::Unretained(&time_source_)),
                          status_cb);
  }

  void StartPlayingFrom(int milliseconds) {
    SCOPED_TRACE(base::StringPrintf("StartPlayingFrom(%d)", milliseconds));
    const base::TimeDelta media_time =
        base::TimeDelta::FromMilliseconds(milliseconds);
    time_source_.SetMediaTime(media_time);
    renderer_->StartPlayingFrom(media_time);
    base::RunLoop().RunUntilIdle();
  }

  void Flush() {
    SCOPED_TRACE("Flush()");
    WaitableMessageLoopEvent event;
    renderer_->Flush(event.GetClosure());
    event.RunAndWait();
  }

  void Destroy() {
    SCOPED_TRACE("Destroy()");
    renderer_.reset();
    base::RunLoop().RunUntilIdle();
  }

  // Parses a string representation of video frames and generates corresponding
  // VideoFrame objects in |decode_results_|.
  //
  // Syntax:
  //   nn - Queue a decoder buffer with timestamp nn * 1000us
  //   abort - Queue an aborted read
  //   error - Queue a decoder error
  //
  // Examples:
  //   A clip that is four frames long: "0 10 20 30"
  //   A clip that has a decode error: "60 70 error"
  void QueueFrames(const std::string& str) {
    for (const std::string& token :
         base::SplitString(str, " ", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_ALL)) {
      if (token == "abort") {
        scoped_refptr<VideoFrame> null_frame;
        QueueFrame(DecodeStatus::ABORTED, null_frame);
        continue;
      }

      if (token == "error") {
        scoped_refptr<VideoFrame> null_frame;
        QueueFrame(DecodeStatus::DECODE_ERROR, null_frame);
        continue;
      }

      int timestamp_in_ms = 0;
      if (base::StringToInt(token, &timestamp_in_ms)) {
        gfx::Size natural_size = TestVideoConfig::NormalCodedSize();
        scoped_refptr<VideoFrame> frame = VideoFrame::CreateFrame(
            PIXEL_FORMAT_YV12, natural_size, gfx::Rect(natural_size),
            natural_size, base::TimeDelta::FromMilliseconds(timestamp_in_ms));
        QueueFrame(DecodeStatus::OK, frame);
        continue;
      }

      CHECK(false) << "Unrecognized decoder buffer token: " << token;
    }
  }

  // Queues video frames to be served by the decoder during rendering.
  void QueueFrame(DecodeStatus status, scoped_refptr<VideoFrame> frame) {
    decode_results_.push_back(std::make_pair(status, frame));
  }

  bool IsReadPending() {
    return !decode_cb_.is_null();
  }

  void WaitForError(PipelineStatus expected) {
    SCOPED_TRACE(base::StringPrintf("WaitForError(%d)", expected));

    WaitableMessageLoopEvent event;
    PipelineStatusCB error_cb = event.GetPipelineStatusCB();
    EXPECT_CALL(mock_cb_, OnError(_))
        .WillOnce(Invoke(&error_cb, &PipelineStatusCB::Run));
    event.RunAndWaitForStatus(expected);
  }

  void WaitForEnded() {
    SCOPED_TRACE("WaitForEnded()");

    WaitableMessageLoopEvent event;
    EXPECT_CALL(mock_cb_, OnEnded()).WillOnce(RunClosure(event.GetClosure()));
    event.RunAndWait();
  }

  void WaitForPendingDecode() {
    SCOPED_TRACE("WaitForPendingDecode()");
    if (!decode_cb_.is_null())
      return;

    DCHECK(wait_for_pending_decode_cb_.is_null());

    WaitableMessageLoopEvent event;
    wait_for_pending_decode_cb_ = event.GetClosure();
    event.RunAndWait();

    DCHECK(!decode_cb_.is_null());
    DCHECK(wait_for_pending_decode_cb_.is_null());
  }

  void SatisfyPendingDecode() {
    CHECK(!decode_cb_.is_null());
    CHECK(!decode_results_.empty());

    // Post tasks for OutputCB and DecodeCB.
    scoped_refptr<VideoFrame> frame = decode_results_.front().second;
    if (frame.get())
      message_loop_.task_runner()->PostTask(FROM_HERE,
                                            base::Bind(output_cb_, frame));
    message_loop_.task_runner()->PostTask(
        FROM_HERE, base::Bind(base::ResetAndReturn(&decode_cb_),
                              decode_results_.front().first));
    decode_results_.pop_front();
  }

  void SatisfyPendingDecodeWithEndOfStream() {
    DCHECK(!decode_cb_.is_null());

    // Return EOS buffer to trigger EOS frame.
    EXPECT_CALL(demuxer_stream_, Read(_))
        .WillOnce(RunCallback<0>(DemuxerStream::kOk,
                                 DecoderBuffer::CreateEOSBuffer()));

    // Satify pending |decode_cb_| to trigger a new DemuxerStream::Read().
    message_loop_.task_runner()->PostTask(
        FROM_HERE,
        base::Bind(base::ResetAndReturn(&decode_cb_), DecodeStatus::OK));

    WaitForPendingDecode();

    message_loop_.task_runner()->PostTask(
        FROM_HERE,
        base::Bind(base::ResetAndReturn(&decode_cb_), DecodeStatus::OK));
  }

  void AdvanceWallclockTimeInMs(int time_ms) {
    DCHECK_EQ(&message_loop_, base::MessageLoop::current());
    base::AutoLock l(lock_);
    tick_clock_->Advance(base::TimeDelta::FromMilliseconds(time_ms));
  }

  void AdvanceTimeInMs(int time_ms) {
    DCHECK_EQ(&message_loop_, base::MessageLoop::current());
    base::AutoLock l(lock_);
    time_ += base::TimeDelta::FromMilliseconds(time_ms);
    time_source_.StopTicking();
    time_source_.SetMediaTime(time_);
    time_source_.StartTicking();
  }

  enum class UnderflowTestType {
    NORMAL,
    LOW_DELAY,
    CANT_READ_WITHOUT_STALLING
  };
  void BasicUnderflowTest(UnderflowTestType type) {
    InitializeWithLowDelay(type == UnderflowTestType::LOW_DELAY);
    if (type == UnderflowTestType::CANT_READ_WITHOUT_STALLING)
      ON_CALL(*decoder_, CanReadWithoutStalling()).WillByDefault(Return(false));

    QueueFrames("0 30 60 90");

    {
      WaitableMessageLoopEvent event;
      EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(0)));
      EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH))
          .WillOnce(RunClosure(event.GetClosure()));
      EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
      EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
      EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);
      StartPlayingFrom(0);
      event.RunAndWait();
      Mock::VerifyAndClearExpectations(&mock_cb_);
    }

    renderer_->OnTimeStateChanged(true);

    // Advance time slightly, but enough to exceed the duration of the last
    // frame.
    // Frames should be dropped and we should NOT signal having nothing.
    {
      SCOPED_TRACE("Waiting for frame drops");
      WaitableMessageLoopEvent event;

      // Note: Starting the TimeSource will cause the old VideoRendererImpl to
      // start rendering frames on its own thread, so the first frame may be
      // received.
      time_source_.StartTicking();
      EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(30))).Times(0);

      EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(60))).Times(0);
      EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(90)))
          .WillOnce(RunClosure(event.GetClosure()));
      EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
      AdvanceTimeInMs(91);

      event.RunAndWait();
      Mock::VerifyAndClearExpectations(&mock_cb_);
    }

    // Advance time more. Now we should signal having nothing. And put
    // the last frame up for display.
    {
      SCOPED_TRACE("Waiting for BUFFERING_HAVE_NOTHING");
      WaitableMessageLoopEvent event;
      EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_NOTHING))
          .WillOnce(RunClosure(event.GetClosure()));
      AdvanceTimeInMs(30);
      event.RunAndWait();
      Mock::VerifyAndClearExpectations(&mock_cb_);
    }

    // Simulate delayed buffering state callbacks.
    renderer_->OnTimeStateChanged(false);
    renderer_->OnTimeStateChanged(true);

    // Receiving end of stream should signal having enough.
    {
      SCOPED_TRACE("Waiting for BUFFERING_HAVE_ENOUGH");
      WaitableMessageLoopEvent event;
      EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH))
          .WillOnce(RunClosure(event.GetClosure()));
      EXPECT_CALL(mock_cb_, OnEnded());
      SatisfyPendingDecodeWithEndOfStream();
      event.RunAndWait();
    }

    Destroy();
  }

  void UnderflowRecoveryTest(UnderflowTestType type) {
    InitializeWithLowDelay(type == UnderflowTestType::LOW_DELAY);
    if (type == UnderflowTestType::CANT_READ_WITHOUT_STALLING)
      ON_CALL(*decoder_, CanReadWithoutStalling()).WillByDefault(Return(false));

    QueueFrames("0 20 40 60");
    {
      WaitableMessageLoopEvent event;
      EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(0)));
      EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH))
          .WillOnce(RunClosure(event.GetClosure()));
      EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
      EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
      EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);
      StartPlayingFrom(0);
      event.RunAndWait();
      Mock::VerifyAndClearExpectations(&mock_cb_);
    }

    renderer_->OnTimeStateChanged(true);
    time_source_.StartTicking();

    // Advance time, this should cause have nothing to be signaled.
    {
      SCOPED_TRACE("Waiting for BUFFERING_HAVE_NOTHING");
      WaitableMessageLoopEvent event;
      EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_NOTHING))
          .WillOnce(RunClosure(event.GetClosure()));
      EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(20))).Times(1);
      EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
      AdvanceTimeInMs(20);
      event.RunAndWait();
      Mock::VerifyAndClearExpectations(&mock_cb_);
    }

    AdvanceTimeInMs(59);
    EXPECT_EQ(3u, renderer_->frames_queued_for_testing());
    time_source_.StopTicking();
    renderer_->OnTimeStateChanged(false);
    EXPECT_EQ(0u, renderer_->frames_queued_for_testing());
    ASSERT_TRUE(IsReadPending());

    // Queue some frames, satisfy reads, and make sure expired frames are gone
    // when the renderer paints the first frame.
    {
      SCOPED_TRACE("Waiting for BUFFERING_HAVE_ENOUGH");
      WaitableMessageLoopEvent event;
      EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(80))).Times(1);
      EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH))
          .WillOnce(RunClosure(event.GetClosure()));
      if (type == UnderflowTestType::NORMAL)
        QueueFrames("80 100 120 140 160");
      else
        QueueFrames("40 60 80 90");
      SatisfyPendingDecode();
      event.RunAndWait();
    }

    Destroy();
  }

 protected:
  // Fixture members.
  std::unique_ptr<VideoRendererImpl> renderer_;
  base::SimpleTestTickClock* tick_clock_;  // Owned by |renderer_|.
  NiceMock<MockVideoDecoder>* decoder_;    // Owned by |renderer_|.
  NiceMock<MockDemuxerStream> demuxer_stream_;

  // Use StrictMock<T> to catch missing/extra callbacks.
  class MockCB : public MockRendererClient {
   public:
    MOCK_METHOD1(FrameReceived, void(const scoped_refptr<VideoFrame>&));
  };
  StrictMock<MockCB> mock_cb_;

  // Must be destroyed before |renderer_| since they share |tick_clock_|.
  std::unique_ptr<NullVideoSink> null_video_sink_;

  WallClockTimeSource time_source_;

  base::MessageLoop message_loop_;

 private:
  void DecodeRequested(const scoped_refptr<DecoderBuffer>& buffer,
                       const VideoDecoder::DecodeCB& decode_cb) {
    DCHECK_EQ(&message_loop_, base::MessageLoop::current());
    CHECK(decode_cb_.is_null());
    decode_cb_ = decode_cb;

    // Wake up WaitForPendingDecode() if needed.
    if (!wait_for_pending_decode_cb_.is_null())
      base::ResetAndReturn(&wait_for_pending_decode_cb_).Run();

    if (decode_results_.empty())
      return;

    SatisfyPendingDecode();
  }

  void FlushRequested(const base::Closure& callback) {
    DCHECK_EQ(&message_loop_, base::MessageLoop::current());
    decode_results_.clear();
    if (!decode_cb_.is_null()) {
      QueueFrames("abort");
      SatisfyPendingDecode();
    }

    message_loop_.task_runner()->PostTask(FROM_HERE, callback);
  }

  // Used to protect |time_|.
  base::Lock lock_;
  base::TimeDelta time_;

  // Used for satisfying reads.
  VideoDecoder::OutputCB output_cb_;
  VideoDecoder::DecodeCB decode_cb_;
  base::TimeDelta next_frame_timestamp_;

  // Run during DecodeRequested() to unblock WaitForPendingDecode().
  base::Closure wait_for_pending_decode_cb_;

  std::deque<std::pair<DecodeStatus, scoped_refptr<VideoFrame>>>
      decode_results_;

  DISALLOW_COPY_AND_ASSIGN(VideoRendererImplTest);
};

TEST_F(VideoRendererImplTest, DoNothing) {
  // Test that creation and deletion doesn't depend on calls to Initialize()
  // and/or Destroy().
}

TEST_F(VideoRendererImplTest, DestroyWithoutInitialize) {
  Destroy();
}

TEST_F(VideoRendererImplTest, Initialize) {
  Initialize();
  Destroy();
}

TEST_F(VideoRendererImplTest, InitializeAndStartPlayingFrom) {
  Initialize();
  QueueFrames("0 10 20 30");
  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(0)));
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH));
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);
  StartPlayingFrom(0);
  Destroy();
}

TEST_F(VideoRendererImplTest, InitializeAndEndOfStream) {
  Initialize();
  StartPlayingFrom(0);
  WaitForPendingDecode();
  {
    SCOPED_TRACE("Waiting for BUFFERING_HAVE_ENOUGH");
    WaitableMessageLoopEvent event;
    EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH))
        .WillOnce(RunClosure(event.GetClosure()));
    EXPECT_CALL(mock_cb_, OnEnded());
    SatisfyPendingDecodeWithEndOfStream();
    event.RunAndWait();
  }
  // Firing a time state changed to true should be ignored...
  renderer_->OnTimeStateChanged(true);
  EXPECT_FALSE(null_video_sink_->is_started());
  Destroy();
}

TEST_F(VideoRendererImplTest, DestroyWhileInitializing) {
  CallInitialize(NewExpectedStatusCB(PIPELINE_ERROR_ABORT), false, PIPELINE_OK);
  Destroy();
}

TEST_F(VideoRendererImplTest, DestroyWhileFlushing) {
  Initialize();
  QueueFrames("0 10 20 30");
  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(0)));
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH));
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);
  StartPlayingFrom(0);
  renderer_->Flush(NewExpectedClosure());
  Destroy();
}

TEST_F(VideoRendererImplTest, Play) {
  Initialize();
  QueueFrames("0 10 20 30");
  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(0)));
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH));
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);
  StartPlayingFrom(0);
  Destroy();
}

TEST_F(VideoRendererImplTest, FlushWithNothingBuffered) {
  Initialize();
  StartPlayingFrom(0);

  // We shouldn't expect a buffering state change since we never reached
  // BUFFERING_HAVE_ENOUGH.
  Flush();
  Destroy();
}

// Verify that the flush callback is invoked outside of VideoRenderer lock, so
// we should be able to call other renderer methods from the Flush callback.
static void VideoRendererImplTest_FlushDoneCB(VideoRendererImplTest* test,
                                              VideoRenderer* renderer,
                                              const base::Closure& success_cb) {
  test->QueueFrames("0 10 20 30");
  renderer->StartPlayingFrom(base::TimeDelta::FromSeconds(0));
  success_cb.Run();
}

TEST_F(VideoRendererImplTest, FlushCallbackNoLock) {
  Initialize();
  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(0)));
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);
  StartPlayingFrom(0);
  WaitableMessageLoopEvent event;
  renderer_->Flush(
      base::Bind(&VideoRendererImplTest_FlushDoneCB, base::Unretained(this),
                 base::Unretained(renderer_.get()), event.GetClosure()));
  event.RunAndWait();
  Destroy();
}

TEST_F(VideoRendererImplTest, DecodeError_Playing) {
  Initialize();
  QueueFrames("0 10 20 30");
  EXPECT_CALL(mock_cb_, FrameReceived(_)).Times(testing::AtLeast(1));

  // Consider the case that rendering is faster than we setup the test event.
  // In that case, when we run out of the frames, BUFFERING_HAVE_NOTHING will
  // be called.
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH));
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_NOTHING))
      .Times(testing::AtMost(1));
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);

  StartPlayingFrom(0);
  renderer_->OnTimeStateChanged(true);
  time_source_.StartTicking();
  AdvanceTimeInMs(10);

  QueueFrames("error");
  SatisfyPendingDecode();
  WaitForError(PIPELINE_ERROR_DECODE);
  Destroy();
}

TEST_F(VideoRendererImplTest, DecodeError_DuringStartPlayingFrom) {
  Initialize();
  QueueFrames("error");
  EXPECT_CALL(mock_cb_, OnError(PIPELINE_ERROR_DECODE));
  StartPlayingFrom(0);
  Destroy();
}

TEST_F(VideoRendererImplTest, StartPlayingFrom_Exact) {
  Initialize();
  QueueFrames("50 60 70 80 90");

  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(60)));
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH));
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);
  StartPlayingFrom(60);
  Destroy();
}

TEST_F(VideoRendererImplTest, StartPlayingFrom_RightBefore) {
  Initialize();
  QueueFrames("50 60 70 80 90");

  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(50)));
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH));
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);
  StartPlayingFrom(59);
  Destroy();
}

TEST_F(VideoRendererImplTest, StartPlayingFrom_RightAfter) {
  Initialize();
  QueueFrames("50 60 70 80 90");

  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(60)));
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH));
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);
  StartPlayingFrom(61);
  Destroy();
}

TEST_F(VideoRendererImplTest, StartPlayingFrom_LowDelay) {
  // In low-delay mode only one frame is required to finish preroll. But frames
  // prior to the start time will not be used.
  InitializeWithLowDelay(true);
  QueueFrames("0 10");

  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(10)));
  // Expect some amount of have enough/nothing due to only requiring one frame.
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH))
      .Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_NOTHING))
      .Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);
  StartPlayingFrom(10);

  QueueFrames("20");
  SatisfyPendingDecode();

  renderer_->OnTimeStateChanged(true);
  time_source_.StartTicking();

  WaitableMessageLoopEvent event;
  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(20)))
      .WillOnce(RunClosure(event.GetClosure()));
  AdvanceTimeInMs(20);
  event.RunAndWait();

  Destroy();
}

// Verify that a late decoder response doesn't break invariants in the renderer.
TEST_F(VideoRendererImplTest, DestroyDuringOutstandingRead) {
  Initialize();
  QueueFrames("0 10 20 30");
  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(0)));
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH));
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);
  StartPlayingFrom(0);

  // Check that there is an outstanding Read() request.
  EXPECT_TRUE(IsReadPending());

  Destroy();
}

TEST_F(VideoRendererImplTest, VideoDecoder_InitFailure) {
  InitializeRenderer(false, false);
  Destroy();
}

TEST_F(VideoRendererImplTest, Underflow) {
  BasicUnderflowTest(UnderflowTestType::NORMAL);
}

TEST_F(VideoRendererImplTest, Underflow_LowDelay) {
  BasicUnderflowTest(UnderflowTestType::LOW_DELAY);
}

TEST_F(VideoRendererImplTest, Underflow_CantReadWithoutStalling) {
  BasicUnderflowTest(UnderflowTestType::CANT_READ_WITHOUT_STALLING);
}

TEST_F(VideoRendererImplTest, UnderflowRecovery) {
  UnderflowRecoveryTest(UnderflowTestType::NORMAL);
}

TEST_F(VideoRendererImplTest, UnderflowRecovery_LowDelay) {
  UnderflowRecoveryTest(UnderflowTestType::LOW_DELAY);
}

TEST_F(VideoRendererImplTest, UnderflowRecovery_CantReadWithoutStalling) {
  UnderflowRecoveryTest(UnderflowTestType::CANT_READ_WITHOUT_STALLING);
}

// Verifies that the first frame is painted w/o rendering being started.
TEST_F(VideoRendererImplTest, RenderingStopsAfterFirstFrame) {
  InitializeWithLowDelay(true);
  QueueFrames("0");

  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH));
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnEnded()).Times(0);

  {
    SCOPED_TRACE("Waiting for first frame to be painted.");
    WaitableMessageLoopEvent event;

    EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(0)))
        .WillOnce(RunClosure(event.GetClosure()));
    StartPlayingFrom(0);

    EXPECT_TRUE(IsReadPending());
    SatisfyPendingDecodeWithEndOfStream();

    event.RunAndWait();
  }

  Destroy();
}

// Verifies that the sink is stopped after rendering the first frame if
// playback has started.
TEST_F(VideoRendererImplTest, RenderingStopsAfterOneFrameWithEOS) {
  InitializeWithLowDelay(true);
  QueueFrames("0");

  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(0))).Times(1);
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH));
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);

  {
    SCOPED_TRACE("Waiting for sink to stop.");
    WaitableMessageLoopEvent event;

    null_video_sink_->set_stop_cb(event.GetClosure());
    StartPlayingFrom(0);
    renderer_->OnTimeStateChanged(true);

    EXPECT_TRUE(IsReadPending());
    SatisfyPendingDecodeWithEndOfStream();
    WaitForEnded();

    renderer_->OnTimeStateChanged(false);
    event.RunAndWait();
  }

  Destroy();
}

// Tests the case where the video started and received a single Render() call,
// then the video was put into the background.
TEST_F(VideoRendererImplTest, RenderingStartedThenStopped) {
  Initialize();
  QueueFrames("0 30 60 90");

  // Start the sink and wait for the first callback.  Set statistics to a non
  // zero value, once we have some decoded frames they should be overwritten.
  PipelineStatistics last_pipeline_statistics;
  last_pipeline_statistics.video_frames_dropped = 1;
  {
    WaitableMessageLoopEvent event;
    EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH))
        .WillOnce(RunClosure(event.GetClosure()));
    EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(0)));
    EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
    EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);
    StartPlayingFrom(0);
    event.RunAndWait();
    Mock::VerifyAndClearExpectations(&mock_cb_);
  }

  // Consider the case that rendering is faster than we setup the test event.
  // In that case, when we run out of the frames, BUFFERING_HAVE_NOTHING will
  // be called. And then during SatisfyPendingDecodeWithEndOfStream,
  // BUFFER_HAVE_ENOUGH will be called again.
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH))
      .Times(testing::AtMost(1));
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_NOTHING))
      .Times(testing::AtMost(1));
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_))
      .WillRepeatedly(SaveArg<0>(&last_pipeline_statistics));
  renderer_->OnTimeStateChanged(true);
  time_source_.StartTicking();

  // Suspend all future callbacks and synthetically advance the media time,
  // because this is a background render, we won't underflow by waiting until
  // a pending read is ready.
  null_video_sink_->set_background_render(true);
  AdvanceTimeInMs(91);
  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(90)));
  WaitForPendingDecode();
  SatisfyPendingDecodeWithEndOfStream();

  // If this wasn't background rendering mode, this would result in two frames
  // being dropped, but since we set background render to true, none should be
  // reported
  EXPECT_EQ(0u, last_pipeline_statistics.video_frames_dropped);
  EXPECT_EQ(4u, last_pipeline_statistics.video_frames_decoded);
  EXPECT_EQ(115200, last_pipeline_statistics.video_memory_usage);

  AdvanceTimeInMs(30);
  WaitForEnded();
  Destroy();
}

// Tests the case where underflow evicts all frames before EOS.
TEST_F(VideoRendererImplTest, UnderflowEvictionBeforeEOS) {
  Initialize();
  QueueFrames("0 30 60 90 100");

  {
    SCOPED_TRACE("Waiting for BUFFERING_HAVE_ENOUGH");
    WaitableMessageLoopEvent event;
    EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH))
        .WillOnce(RunClosure(event.GetClosure()));
    EXPECT_CALL(mock_cb_, FrameReceived(_)).Times(AnyNumber());
    EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
    EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);
    EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
    StartPlayingFrom(0);
    event.RunAndWait();
  }

  {
    SCOPED_TRACE("Waiting for BUFFERING_HAVE_NOTHING");
    WaitableMessageLoopEvent event;
    EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_NOTHING))
        .WillOnce(RunClosure(event.GetClosure()));
    renderer_->OnTimeStateChanged(true);
    time_source_.StartTicking();
    event.RunAndWait();
  }

  WaitForPendingDecode();

  // Jump time far enough forward that no frames are valid.
  renderer_->OnTimeStateChanged(false);
  AdvanceTimeInMs(1000);
  time_source_.StopTicking();

  // Providing the end of stream packet should remove all frames and exit.
  SatisfyPendingDecodeWithEndOfStream();
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH));
  WaitForEnded();
  Destroy();
}

TEST_F(VideoRendererImplTest, StartPlayingFromThenFlushThenEOS) {
  Initialize();
  QueueFrames("0 30 60 90");

  WaitableMessageLoopEvent event;
  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(0)));
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH))
      .WillOnce(RunClosure(event.GetClosure()));
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);
  StartPlayingFrom(0);
  event.RunAndWait();

  // Cycle ticking so that we get a non-null reference time.
  time_source_.StartTicking();
  time_source_.StopTicking();

  // Flush and simulate a seek past EOS, where some error prevents the decoder
  // from returning any frames.
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_NOTHING));
  Flush();

  StartPlayingFrom(200);
  WaitForPendingDecode();
  SatisfyPendingDecodeWithEndOfStream();
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH));
  WaitForEnded();
  Destroy();
}

TEST_F(VideoRendererImplTest, FramesAreNotExpiredDuringPreroll) {
  Initialize();
  // !CanReadWithoutStalling() puts the renderer in state BUFFERING_HAVE_ENOUGH
  // after the first frame.
  ON_CALL(*decoder_, CanReadWithoutStalling()).WillByDefault(Return(false));
  // Set background rendering to simulate the first couple of Render() calls
  // by VFC.
  null_video_sink_->set_background_render(true);
  QueueFrames("0 10 20");
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH))
      .Times(testing::AtMost(1));
  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(0)));
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);
  StartPlayingFrom(0);

  renderer_->OnTimeStateChanged(true);
  time_source_.StartTicking();

  WaitableMessageLoopEvent event;
  // Frame "10" should not have been expired.
  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(10)))
      .WillOnce(RunClosure(event.GetClosure()));
  AdvanceTimeInMs(10);
  event.RunAndWait();

  Destroy();
}

TEST_F(VideoRendererImplTest, NaturalSizeChange) {
  Initialize();

  gfx::Size initial_size(8, 8);
  gfx::Size larger_size(16, 16);

  QueueFrame(DecodeStatus::OK,
             VideoFrame::CreateFrame(PIXEL_FORMAT_YV12, initial_size,
                                     gfx::Rect(initial_size), initial_size,
                                     base::TimeDelta::FromMilliseconds(0)));
  QueueFrame(DecodeStatus::OK,
             VideoFrame::CreateFrame(PIXEL_FORMAT_YV12, larger_size,
                                     gfx::Rect(larger_size), larger_size,
                                     base::TimeDelta::FromMilliseconds(10)));
  QueueFrame(DecodeStatus::OK,
             VideoFrame::CreateFrame(PIXEL_FORMAT_YV12, larger_size,
                                     gfx::Rect(larger_size), larger_size,
                                     base::TimeDelta::FromMilliseconds(20)));
  QueueFrame(DecodeStatus::OK,
             VideoFrame::CreateFrame(PIXEL_FORMAT_YV12, initial_size,
                                     gfx::Rect(initial_size), initial_size,
                                     base::TimeDelta::FromMilliseconds(30)));

  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH));
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);

  {
    // Callback is fired for the first frame.
    EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(initial_size));
    EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(0)));
    StartPlayingFrom(0);
    renderer_->OnTimeStateChanged(true);
    time_source_.StartTicking();
  }
  {
    // Callback should be fired once when switching to the larger size.
    EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(larger_size));
    WaitableMessageLoopEvent event;
    EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(10)))
        .WillOnce(RunClosure(event.GetClosure()));
    AdvanceTimeInMs(10);
    event.RunAndWait();
  }
  {
    // Called is not fired because frame size does not change.
    WaitableMessageLoopEvent event;
    EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(20)))
        .WillOnce(RunClosure(event.GetClosure()));
    AdvanceTimeInMs(10);
    event.RunAndWait();
  }
  {
    // Callback is fired once when switching to the larger size.
    EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(initial_size));
    WaitableMessageLoopEvent event;
    EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(30)))
        .WillOnce(RunClosure(event.GetClosure()));
    AdvanceTimeInMs(10);
    event.RunAndWait();
  }

  Destroy();
}

TEST_F(VideoRendererImplTest, OpacityChange) {
  Initialize();

  gfx::Size frame_size(8, 8);
  VideoPixelFormat opaque_format = PIXEL_FORMAT_YV12;
  VideoPixelFormat non_opaque_format = PIXEL_FORMAT_YV12A;

  QueueFrame(DecodeStatus::OK,
             VideoFrame::CreateFrame(non_opaque_format, frame_size,
                                     gfx::Rect(frame_size), frame_size,
                                     base::TimeDelta::FromMilliseconds(0)));
  QueueFrame(DecodeStatus::OK,
             VideoFrame::CreateFrame(non_opaque_format, frame_size,
                                     gfx::Rect(frame_size), frame_size,
                                     base::TimeDelta::FromMilliseconds(10)));
  QueueFrame(DecodeStatus::OK,
             VideoFrame::CreateFrame(opaque_format, frame_size,
                                     gfx::Rect(frame_size), frame_size,
                                     base::TimeDelta::FromMilliseconds(20)));
  QueueFrame(DecodeStatus::OK,
             VideoFrame::CreateFrame(opaque_format, frame_size,
                                     gfx::Rect(frame_size), frame_size,
                                     base::TimeDelta::FromMilliseconds(30)));

  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH));
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(frame_size)).Times(1);

  {
    // Callback is fired for the first frame.
    EXPECT_CALL(mock_cb_, OnVideoOpacityChange(false));
    EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(0)));
    StartPlayingFrom(0);
    renderer_->OnTimeStateChanged(true);
    time_source_.StartTicking();
  }
  {
    // Callback is not fired because opacity does not change.
    WaitableMessageLoopEvent event;
    EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(10)))
        .WillOnce(RunClosure(event.GetClosure()));
    AdvanceTimeInMs(10);
    event.RunAndWait();
  }
  {
    // Called is fired when opacity changes.
    EXPECT_CALL(mock_cb_, OnVideoOpacityChange(true));
    WaitableMessageLoopEvent event;
    EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(20)))
        .WillOnce(RunClosure(event.GetClosure()));
    AdvanceTimeInMs(10);
    event.RunAndWait();
  }
  {
    // Callback is not fired because opacity does not change.
    WaitableMessageLoopEvent event;
    EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(30)))
        .WillOnce(RunClosure(event.GetClosure()));
    AdvanceTimeInMs(10);
    event.RunAndWait();
  }

  Destroy();
}

class VideoRendererImplAsyncAddFrameReadyTest : public VideoRendererImplTest {
 public:
  VideoRendererImplAsyncAddFrameReadyTest() {
    std::unique_ptr<GpuMemoryBufferVideoFramePool> gpu_memory_buffer_pool(
        new MockGpuMemoryBufferVideoFramePool(&frame_ready_cbs_));
    renderer_->SetGpuMemoryBufferVideoForTesting(
        std::move(gpu_memory_buffer_pool));
  }

 protected:
  std::vector<base::Closure> frame_ready_cbs_;
};

TEST_F(VideoRendererImplAsyncAddFrameReadyTest, InitializeAndStartPlayingFrom) {
  Initialize();
  QueueFrames("0 10 20 30");
  EXPECT_CALL(mock_cb_, FrameReceived(HasTimestamp(0)));
  EXPECT_CALL(mock_cb_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH));
  EXPECT_CALL(mock_cb_, OnStatisticsUpdate(_)).Times(AnyNumber());
  EXPECT_CALL(mock_cb_, OnVideoNaturalSizeChange(_)).Times(1);
  EXPECT_CALL(mock_cb_, OnVideoOpacityChange(_)).Times(1);
  StartPlayingFrom(0);
  ASSERT_EQ(1u, frame_ready_cbs_.size());

  uint32_t frame_ready_index = 0;
  while (frame_ready_index < frame_ready_cbs_.size()) {
    frame_ready_cbs_[frame_ready_index++].Run();
    base::RunLoop().RunUntilIdle();
  }
  Destroy();
}

TEST_F(VideoRendererImplAsyncAddFrameReadyTest, WeakFactoryDiscardsOneFrame) {
  Initialize();
  QueueFrames("0 10 20 30");
  StartPlayingFrom(0);
  Flush();
  ASSERT_EQ(1u, frame_ready_cbs_.size());
  // This frame will be discarded.
  frame_ready_cbs_.front().Run();
  Destroy();
}

}  // namespace media
