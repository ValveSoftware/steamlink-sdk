// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_buffer_converter.h"
#include "media/base/audio_hardware_config.h"
#include "media/base/audio_splicer.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/fake_audio_renderer_sink.h"
#include "media/base/gmock_callback_support.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/filters/audio_renderer_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::Time;
using ::base::TimeTicks;
using ::base::TimeDelta;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SaveArg;

namespace media {

// Constants to specify the type of audio data used.
static AudioCodec kCodec = kCodecVorbis;
static SampleFormat kSampleFormat = kSampleFormatPlanarF32;
static ChannelLayout kChannelLayout = CHANNEL_LAYOUT_STEREO;
static int kChannelCount = 2;
static int kChannels = ChannelLayoutToChannelCount(kChannelLayout);
static int kSamplesPerSecond = 44100;
// Use a different output sample rate so the AudioBufferConverter is invoked.
static int kOutputSamplesPerSecond = 48000;

// Constants for distinguishing between muted audio and playing audio when using
// ConsumeBufferedData(). Must match the type needed by kSampleFormat.
static float kMutedAudio = 0.0f;
static float kPlayingAudio = 0.5f;

static const int kDataSize = 1024;

ACTION_P(EnterPendingDecoderInitStateAction, test) {
  test->EnterPendingDecoderInitState(arg1);
}

class AudioRendererImplTest : public ::testing::Test {
 public:
  // Give the decoder some non-garbage media properties.
  AudioRendererImplTest()
      : hardware_config_(AudioParameters(), AudioParameters()),
        needs_stop_(true),
        demuxer_stream_(DemuxerStream::AUDIO),
        decoder_(new MockAudioDecoder()),
        last_time_update_(kNoTimestamp()) {
    AudioDecoderConfig audio_config(kCodec,
                                    kSampleFormat,
                                    kChannelLayout,
                                    kSamplesPerSecond,
                                    NULL,
                                    0,
                                    false);
    demuxer_stream_.set_audio_decoder_config(audio_config);

    // Used to save callbacks and run them at a later time.
    EXPECT_CALL(*decoder_, Decode(_, _))
        .WillRepeatedly(Invoke(this, &AudioRendererImplTest::DecodeDecoder));
    EXPECT_CALL(*decoder_, Reset(_))
        .WillRepeatedly(Invoke(this, &AudioRendererImplTest::ResetDecoder));

    // Mock out demuxer reads.
    EXPECT_CALL(demuxer_stream_, Read(_)).WillRepeatedly(
        RunCallback<0>(DemuxerStream::kOk,
                       scoped_refptr<DecoderBuffer>(new DecoderBuffer(0))));
    EXPECT_CALL(demuxer_stream_, SupportsConfigChanges())
        .WillRepeatedly(Return(true));
    AudioParameters out_params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                               kChannelLayout,
                               kOutputSamplesPerSecond,
                               SampleFormatToBytesPerChannel(kSampleFormat) * 8,
                               512);
    hardware_config_.UpdateOutputConfig(out_params);
    ScopedVector<AudioDecoder> decoders;
    decoders.push_back(decoder_);
    sink_ = new FakeAudioRendererSink();
    renderer_.reset(new AudioRendererImpl(message_loop_.message_loop_proxy(),
                                          sink_,
                                          decoders.Pass(),
                                          SetDecryptorReadyCB(),
                                          &hardware_config_));

    // Stub out time.
    renderer_->set_now_cb_for_testing(base::Bind(
        &AudioRendererImplTest::GetTime, base::Unretained(this)));
  }

  virtual ~AudioRendererImplTest() {
    SCOPED_TRACE("~AudioRendererImplTest()");
    if (needs_stop_) {
      WaitableMessageLoopEvent event;
      renderer_->Stop(event.GetClosure());
      event.RunAndWait();
    }
  }

  void ExpectUnsupportedAudioDecoder() {
    EXPECT_CALL(*decoder_, Initialize(_, _, _))
        .WillOnce(DoAll(SaveArg<2>(&output_cb_),
                        RunCallback<1>(DECODER_ERROR_NOT_SUPPORTED)));
  }

  MOCK_METHOD1(OnStatistics, void(const PipelineStatistics&));
  MOCK_METHOD0(OnUnderflow, void());
  MOCK_METHOD1(OnError, void(PipelineStatus));

  void OnAudioTimeCallback(TimeDelta current_time, TimeDelta max_time) {
    CHECK(current_time <= max_time);
    last_time_update_ = current_time;
  }

  void InitializeRenderer(const PipelineStatusCB& pipeline_status_cb) {
    renderer_->Initialize(
        &demuxer_stream_,
        pipeline_status_cb,
        base::Bind(&AudioRendererImplTest::OnStatistics,
                   base::Unretained(this)),
        base::Bind(&AudioRendererImplTest::OnUnderflow,
                   base::Unretained(this)),
        base::Bind(&AudioRendererImplTest::OnAudioTimeCallback,
                   base::Unretained(this)),
        ended_event_.GetClosure(),
        base::Bind(&AudioRendererImplTest::OnError,
                   base::Unretained(this)));
  }

  void Initialize() {
    EXPECT_CALL(*decoder_, Initialize(_, _, _))
        .WillOnce(DoAll(SaveArg<2>(&output_cb_),
                        RunCallback<1>(PIPELINE_OK)));
    EXPECT_CALL(*decoder_, Stop());
    InitializeWithStatus(PIPELINE_OK);

    next_timestamp_.reset(new AudioTimestampHelper(
        hardware_config_.GetOutputConfig().sample_rate()));
  }

  void InitializeWithStatus(PipelineStatus expected) {
    SCOPED_TRACE(base::StringPrintf("InitializeWithStatus(%d)", expected));

    WaitableMessageLoopEvent event;
    InitializeRenderer(event.GetPipelineStatusCB());
    event.RunAndWaitForStatus(expected);

    // We should have no reads.
    EXPECT_TRUE(decode_cb_.is_null());
  }

  void InitializeAndStop() {
    EXPECT_CALL(*decoder_, Initialize(_, _, _))
        .WillOnce(DoAll(SaveArg<2>(&output_cb_),
                        RunCallback<1>(PIPELINE_OK)));
    EXPECT_CALL(*decoder_, Stop());

    WaitableMessageLoopEvent event;
    InitializeRenderer(event.GetPipelineStatusCB());

    // Stop before we let the MessageLoop run, this simulates an interleaving
    // in which we end up calling Stop() while the OnDecoderSelected callback
    // is in flight.
    renderer_->Stop(NewExpectedClosure());
    event.RunAndWaitForStatus(PIPELINE_ERROR_ABORT);
    EXPECT_EQ(renderer_->state_, AudioRendererImpl::kStopped);
  }

  void InitializeAndStopDuringDecoderInit() {
    EXPECT_CALL(*decoder_, Initialize(_, _, _))
        .WillOnce(DoAll(SaveArg<2>(&output_cb_),
                        EnterPendingDecoderInitStateAction(this)));
    EXPECT_CALL(*decoder_, Stop());

    WaitableMessageLoopEvent event;
    InitializeRenderer(event.GetPipelineStatusCB());

    base::RunLoop().RunUntilIdle();
    DCHECK(!init_decoder_cb_.is_null());

    renderer_->Stop(NewExpectedClosure());
    base::ResetAndReturn(&init_decoder_cb_).Run(PIPELINE_OK);

    event.RunAndWaitForStatus(PIPELINE_ERROR_ABORT);
    EXPECT_EQ(renderer_->state_, AudioRendererImpl::kStopped);
  }

  void EnterPendingDecoderInitState(PipelineStatusCB cb) {
    init_decoder_cb_ = cb;
  }

  void Flush() {
    WaitableMessageLoopEvent flush_event;
    renderer_->Flush(flush_event.GetClosure());
    flush_event.RunAndWait();

    EXPECT_FALSE(IsReadPending());
  }

  void Preroll() {
    Preroll(0, PIPELINE_OK);
  }

  void Preroll(int timestamp_ms, PipelineStatus expected) {
    SCOPED_TRACE(base::StringPrintf("Preroll(%d, %d)", timestamp_ms, expected));

    TimeDelta timestamp = TimeDelta::FromMilliseconds(timestamp_ms);
    next_timestamp_->SetBaseTimestamp(timestamp);

    // Fill entire buffer to complete prerolling.
    WaitableMessageLoopEvent event;
    renderer_->Preroll(timestamp, event.GetPipelineStatusCB());
    WaitForPendingRead();
    DeliverRemainingAudio();
    event.RunAndWaitForStatus(PIPELINE_OK);
  }

  void StartRendering() {
    renderer_->StartRendering();
    renderer_->SetPlaybackRate(1.0f);
  }

  void StopRendering() {
    renderer_->StopRendering();
  }

  void Seek() {
    StopRendering();
    Flush();
    Preroll();
  }

  void WaitForEnded() {
    SCOPED_TRACE("WaitForEnded()");
    ended_event_.RunAndWait();
  }

  bool IsReadPending() const {
    return !decode_cb_.is_null();
  }

  void WaitForPendingRead() {
    SCOPED_TRACE("WaitForPendingRead()");
    if (!decode_cb_.is_null())
      return;

    DCHECK(wait_for_pending_decode_cb_.is_null());

    WaitableMessageLoopEvent event;
    wait_for_pending_decode_cb_ = event.GetClosure();
    event.RunAndWait();

    DCHECK(!decode_cb_.is_null());
    DCHECK(wait_for_pending_decode_cb_.is_null());
  }

  // Delivers |size| frames with value kPlayingAudio to |renderer_|.
  void SatisfyPendingRead(int size) {
    CHECK_GT(size, 0);
    CHECK(!decode_cb_.is_null());

    scoped_refptr<AudioBuffer> buffer =
        MakeAudioBuffer<float>(kSampleFormat,
                               kChannelLayout,
                               kChannelCount,
                               kSamplesPerSecond,
                               kPlayingAudio,
                               0.0f,
                               size,
                               next_timestamp_->GetTimestamp());
    next_timestamp_->AddFrames(size);

    DeliverBuffer(AudioDecoder::kOk, buffer);
  }

  void DeliverEndOfStream() {
    DCHECK(!decode_cb_.is_null());

    // Return EOS buffer to trigger EOS frame.
    EXPECT_CALL(demuxer_stream_, Read(_))
        .WillOnce(RunCallback<0>(DemuxerStream::kOk,
                                 DecoderBuffer::CreateEOSBuffer()));

    // Satify pending |decode_cb_| to trigger a new DemuxerStream::Read().
    message_loop_.PostTask(
        FROM_HERE,
        base::Bind(base::ResetAndReturn(&decode_cb_), AudioDecoder::kOk));

    WaitForPendingRead();

    message_loop_.PostTask(
        FROM_HERE,
        base::Bind(base::ResetAndReturn(&decode_cb_), AudioDecoder::kOk));

    message_loop_.RunUntilIdle();
  }

  // Delivers frames until |renderer_|'s internal buffer is full and no longer
  // has pending reads.
  void DeliverRemainingAudio() {
    SatisfyPendingRead(frames_remaining_in_buffer());
  }

  // Attempts to consume |requested_frames| frames from |renderer_|'s internal
  // buffer, returning true if all |requested_frames| frames were consumed,
  // false if less than |requested_frames| frames were consumed.
  //
  // |muted| is optional and if passed will get set if the value of
  // the consumed data is muted audio.
  bool ConsumeBufferedData(int requested_frames, bool* muted) {
    scoped_ptr<AudioBus> bus =
        AudioBus::Create(kChannels, std::max(requested_frames, 1));
    int frames_read;
    if (!sink_->Render(bus.get(), 0, &frames_read)) {
      if (muted)
        *muted = true;
      return false;
    }

    if (muted)
      *muted = frames_read < 1 || bus->channel(0)[0] == kMutedAudio;
    return frames_read == requested_frames;
  }

  // Attempts to consume all data available from the renderer.  Returns the
  // number of frames read.  Since time is frozen, the audio delay will increase
  // as frames come in.
  int ConsumeAllBufferedData() {
    int frames_read = 0;
    int total_frames_read = 0;

    scoped_ptr<AudioBus> bus = AudioBus::Create(kChannels, 1024);

    do {
      TimeDelta audio_delay = TimeDelta::FromMicroseconds(
          total_frames_read * Time::kMicrosecondsPerSecond /
          static_cast<float>(hardware_config_.GetOutputConfig().sample_rate()));

      frames_read = renderer_->Render(
          bus.get(), audio_delay.InMilliseconds());
      total_frames_read += frames_read;
    } while (frames_read > 0);

    return total_frames_read;
  }

  int frames_buffered() {
    return renderer_->algorithm_->frames_buffered();
  }

  int buffer_capacity() {
    return renderer_->algorithm_->QueueCapacity();
  }

  int frames_remaining_in_buffer() {
    // This can happen if too much data was delivered, in which case the buffer
    // will accept the data but not increase capacity.
    if (frames_buffered() > buffer_capacity()) {
      return 0;
    }
    return buffer_capacity() - frames_buffered();
  }

  void CallResumeAfterUnderflow() {
    renderer_->ResumeAfterUnderflow();
  }

  TimeDelta CalculatePlayTime(int frames_filled) {
    return TimeDelta::FromMicroseconds(
        frames_filled * Time::kMicrosecondsPerSecond /
        renderer_->audio_parameters_.sample_rate());
  }

  void EndOfStreamTest(float playback_rate) {
    Initialize();
    Preroll();
    StartRendering();
    renderer_->SetPlaybackRate(playback_rate);

    // Drain internal buffer, we should have a pending read.
    int total_frames = frames_buffered();
    int frames_filled = ConsumeAllBufferedData();
    WaitForPendingRead();

    // Due to how the cross-fade algorithm works we won't get an exact match
    // between the ideal and expected number of frames consumed.  In the faster
    // than normal playback case, more frames are created than should exist and
    // vice versa in the slower than normal playback case.
    const float kEpsilon = 0.20 * (total_frames / playback_rate);
    EXPECT_NEAR(frames_filled, total_frames / playback_rate, kEpsilon);

    // Figure out how long until the ended event should fire.
    TimeDelta audio_play_time = CalculatePlayTime(frames_filled);
    DVLOG(1) << "audio_play_time = " << audio_play_time.InSecondsF();

    // Fulfill the read with an end-of-stream packet.  We shouldn't report ended
    // nor have a read until we drain the internal buffer.
    DeliverEndOfStream();

    // Advance time half way without an ended expectation.
    AdvanceTime(audio_play_time / 2);
    ConsumeBufferedData(frames_buffered(), NULL);

    // Advance time by other half and expect the ended event.
    AdvanceTime(audio_play_time / 2);
    ConsumeBufferedData(frames_buffered(), NULL);
    WaitForEnded();
  }

  void AdvanceTime(TimeDelta time) {
    base::AutoLock auto_lock(lock_);
    time_ += time;
  }

  void force_config_change() {
    renderer_->OnConfigChange();
  }

  int converter_input_frames_left() const {
    return renderer_->buffer_converter_->input_frames_left_for_testing();
  }

  bool splicer_has_next_buffer() const {
    return renderer_->splicer_->HasNextBuffer();
  }

  base::TimeDelta last_time_update() const {
    return last_time_update_;
  }

  // Fixture members.
  base::MessageLoop message_loop_;
  scoped_ptr<AudioRendererImpl> renderer_;
  scoped_refptr<FakeAudioRendererSink> sink_;
  AudioHardwareConfig hardware_config_;

  // Whether or not the test needs the destructor to call Stop() on
  // |renderer_| at destruction.
  bool needs_stop_;

 private:
  TimeTicks GetTime() {
    base::AutoLock auto_lock(lock_);
    return time_;
  }

  void DecodeDecoder(const scoped_refptr<DecoderBuffer>& buffer,
                     const AudioDecoder::DecodeCB& decode_cb) {
    // We shouldn't ever call Read() after Stop():
    EXPECT_TRUE(stop_decoder_cb_.is_null());

    // TODO(scherkus): Make this a DCHECK after threading semantics are fixed.
    if (base::MessageLoop::current() != &message_loop_) {
      message_loop_.PostTask(FROM_HERE, base::Bind(
          &AudioRendererImplTest::DecodeDecoder,
          base::Unretained(this), buffer, decode_cb));
      return;
    }

    CHECK(decode_cb_.is_null()) << "Overlapping decodes are not permitted";
    decode_cb_ = decode_cb;

    // Wake up WaitForPendingRead() if needed.
    if (!wait_for_pending_decode_cb_.is_null())
      base::ResetAndReturn(&wait_for_pending_decode_cb_).Run();
  }

  void ResetDecoder(const base::Closure& reset_cb) {
    if (!decode_cb_.is_null()) {
      // |reset_cb| will be called in DeliverBuffer(), after the decoder is
      // flushed.
      reset_cb_ = reset_cb;
      return;
    }

    message_loop_.PostTask(FROM_HERE, reset_cb);
  }

  void DeliverBuffer(AudioDecoder::Status status,
                     const scoped_refptr<AudioBuffer>& buffer) {
    CHECK(!decode_cb_.is_null());
    if (buffer && !buffer->end_of_stream())
      output_cb_.Run(buffer);
    base::ResetAndReturn(&decode_cb_).Run(status);

    if (!reset_cb_.is_null())
      base::ResetAndReturn(&reset_cb_).Run();

    message_loop_.RunUntilIdle();
  }

  MockDemuxerStream demuxer_stream_;
  MockAudioDecoder* decoder_;

  // Used for stubbing out time in the audio callback thread.
  base::Lock lock_;
  TimeTicks time_;

  // Used for satisfying reads.
  AudioDecoder::OutputCB output_cb_;
  AudioDecoder::DecodeCB decode_cb_;
  base::Closure reset_cb_;
  scoped_ptr<AudioTimestampHelper> next_timestamp_;

  WaitableMessageLoopEvent ended_event_;

  // Run during DecodeDecoder() to unblock WaitForPendingRead().
  base::Closure wait_for_pending_decode_cb_;
  base::Closure stop_decoder_cb_;

  PipelineStatusCB init_decoder_cb_;
  base::TimeDelta last_time_update_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererImplTest);
};

TEST_F(AudioRendererImplTest, Initialize_Successful) {
  Initialize();
}

TEST_F(AudioRendererImplTest, Initialize_DecoderInitFailure) {
  ExpectUnsupportedAudioDecoder();
  InitializeWithStatus(DECODER_ERROR_NOT_SUPPORTED);
}

TEST_F(AudioRendererImplTest, Preroll) {
  Initialize();
  Preroll();
}

TEST_F(AudioRendererImplTest, StartRendering) {
  Initialize();
  Preroll();
  StartRendering();

  // Drain internal buffer, we should have a pending read.
  EXPECT_TRUE(ConsumeBufferedData(frames_buffered(), NULL));
  WaitForPendingRead();
}

TEST_F(AudioRendererImplTest, EndOfStream) {
  EndOfStreamTest(1.0);
}

TEST_F(AudioRendererImplTest, EndOfStream_FasterPlaybackSpeed) {
  EndOfStreamTest(2.0);
}

TEST_F(AudioRendererImplTest, EndOfStream_SlowerPlaybackSpeed) {
  EndOfStreamTest(0.5);
}

TEST_F(AudioRendererImplTest, Underflow) {
  Initialize();
  Preroll();

  int initial_capacity = buffer_capacity();

  StartRendering();

  // Drain internal buffer, we should have a pending read.
  EXPECT_TRUE(ConsumeBufferedData(frames_buffered(), NULL));
  WaitForPendingRead();

  // Verify the next FillBuffer() call triggers the underflow callback
  // since the decoder hasn't delivered any data after it was drained.
  EXPECT_CALL(*this, OnUnderflow());
  EXPECT_FALSE(ConsumeBufferedData(kDataSize, NULL));

  renderer_->ResumeAfterUnderflow();

  // Verify after resuming that we're still not getting data.
  bool muted = false;
  EXPECT_EQ(0, frames_buffered());
  EXPECT_FALSE(ConsumeBufferedData(kDataSize, &muted));
  EXPECT_TRUE(muted);

  // Verify that the buffer capacity increased as a result of the underflow.
  EXPECT_GT(buffer_capacity(), initial_capacity);

  // Deliver data, we should get non-muted audio.
  DeliverRemainingAudio();
  EXPECT_TRUE(ConsumeBufferedData(kDataSize, &muted));
  EXPECT_FALSE(muted);
}

TEST_F(AudioRendererImplTest, Underflow_CapacityResetsAfterFlush) {
  Initialize();
  Preroll();

  int initial_capacity = buffer_capacity();

  StartRendering();

  // Drain internal buffer, we should have a pending read.
  EXPECT_TRUE(ConsumeBufferedData(frames_buffered(), NULL));
  WaitForPendingRead();

  // Verify the next FillBuffer() call triggers the underflow callback
  // since the decoder hasn't delivered any data after it was drained.
  EXPECT_CALL(*this, OnUnderflow());
  EXPECT_FALSE(ConsumeBufferedData(kDataSize, NULL));

  // Verify that the buffer capacity increased as a result of resuming after
  // underflow.
  EXPECT_EQ(buffer_capacity(), initial_capacity);
  renderer_->ResumeAfterUnderflow();
  EXPECT_GT(buffer_capacity(), initial_capacity);

  // Verify that the buffer capacity is restored to the |initial_capacity|.
  DeliverEndOfStream();
  Flush();
  EXPECT_EQ(buffer_capacity(), initial_capacity);
}

TEST_F(AudioRendererImplTest, Underflow_FlushWhileUnderflowed) {
  Initialize();
  Preroll();
  StartRendering();

  // Drain internal buffer, we should have a pending read.
  EXPECT_TRUE(ConsumeBufferedData(frames_buffered(), NULL));
  WaitForPendingRead();

  // Verify the next FillBuffer() call triggers the underflow callback
  // since the decoder hasn't delivered any data after it was drained.
  EXPECT_CALL(*this, OnUnderflow());
  EXPECT_FALSE(ConsumeBufferedData(kDataSize, NULL));

  // Verify that we can still Flush() before entering the rebuffering state.
  DeliverEndOfStream();
  Flush();
}

TEST_F(AudioRendererImplTest, Underflow_EndOfStream) {
  Initialize();
  Preroll();
  StartRendering();

  // Figure out how long until the ended event should fire.  Since
  // ConsumeBufferedData() doesn't provide audio delay information, the time
  // until the ended event fires is equivalent to the longest buffered section,
  // which is the initial frames_buffered() read.
  TimeDelta time_until_ended = CalculatePlayTime(frames_buffered());

  // Drain internal buffer, we should have a pending read.
  EXPECT_TRUE(ConsumeBufferedData(frames_buffered(), NULL));
  WaitForPendingRead();

  // Verify the next FillBuffer() call triggers the underflow callback
  // since the decoder hasn't delivered any data after it was drained.
  EXPECT_CALL(*this, OnUnderflow());
  EXPECT_FALSE(ConsumeBufferedData(kDataSize, NULL));

  // Deliver a little bit of data.
  SatisfyPendingRead(kDataSize);
  WaitForPendingRead();

  // Verify we're getting muted audio during underflow.  Note: Since resampling
  // is active, the number of frames_buffered() won't always match kDataSize.
  bool muted = false;
  const int kInitialFramesBuffered = 1114;
  EXPECT_EQ(kInitialFramesBuffered, frames_buffered());
  EXPECT_FALSE(ConsumeBufferedData(kInitialFramesBuffered, &muted));
  EXPECT_TRUE(muted);

  // Now deliver end of stream, we should get our little bit of data back.
  DeliverEndOfStream();
  const int kNextFramesBuffered = 1408;
  EXPECT_EQ(kNextFramesBuffered, frames_buffered());
  EXPECT_TRUE(ConsumeBufferedData(kNextFramesBuffered, &muted));
  EXPECT_FALSE(muted);

  // Attempt to read to make sure we're truly at the end of stream.
  AdvanceTime(time_until_ended);
  EXPECT_FALSE(ConsumeBufferedData(kDataSize, &muted));
  EXPECT_TRUE(muted);
  WaitForEnded();
}

TEST_F(AudioRendererImplTest, Underflow_ResumeFromCallback) {
  Initialize();
  Preroll();
  StartRendering();

  // Drain internal buffer, we should have a pending read.
  EXPECT_TRUE(ConsumeBufferedData(frames_buffered(), NULL));
  WaitForPendingRead();

  // Verify the next FillBuffer() call triggers the underflow callback
  // since the decoder hasn't delivered any data after it was drained.
  EXPECT_CALL(*this, OnUnderflow())
      .WillOnce(Invoke(this, &AudioRendererImplTest::CallResumeAfterUnderflow));
  EXPECT_FALSE(ConsumeBufferedData(kDataSize, NULL));

  // Verify after resuming that we're still not getting data.
  bool muted = false;
  EXPECT_EQ(0, frames_buffered());
  EXPECT_FALSE(ConsumeBufferedData(kDataSize, &muted));
  EXPECT_TRUE(muted);

  // Deliver data, we should get non-muted audio.
  DeliverRemainingAudio();
  EXPECT_TRUE(ConsumeBufferedData(kDataSize, &muted));
  EXPECT_FALSE(muted);
}

TEST_F(AudioRendererImplTest, Underflow_SetPlaybackRate) {
  Initialize();
  Preroll();
  StartRendering();

  // Drain internal buffer, we should have a pending read.
  EXPECT_TRUE(ConsumeBufferedData(frames_buffered(), NULL));
  WaitForPendingRead();

  EXPECT_EQ(FakeAudioRendererSink::kPlaying, sink_->state());

  // Verify the next FillBuffer() call triggers the underflow callback
  // since the decoder hasn't delivered any data after it was drained.
  EXPECT_CALL(*this, OnUnderflow())
      .WillOnce(Invoke(this, &AudioRendererImplTest::CallResumeAfterUnderflow));
  EXPECT_FALSE(ConsumeBufferedData(kDataSize, NULL));
  EXPECT_EQ(0, frames_buffered());

  EXPECT_EQ(FakeAudioRendererSink::kPlaying, sink_->state());

  // Simulate playback being paused.
  renderer_->SetPlaybackRate(0);

  EXPECT_EQ(FakeAudioRendererSink::kPaused, sink_->state());

  // Deliver data to resolve the underflow.
  DeliverRemainingAudio();

  EXPECT_EQ(FakeAudioRendererSink::kPaused, sink_->state());

  // Simulate playback being resumed.
  renderer_->SetPlaybackRate(1);

  EXPECT_EQ(FakeAudioRendererSink::kPlaying, sink_->state());
}

TEST_F(AudioRendererImplTest, Underflow_PausePlay) {
  Initialize();
  Preroll();
  StartRendering();

  // Drain internal buffer, we should have a pending read.
  EXPECT_TRUE(ConsumeBufferedData(frames_buffered(), NULL));
  WaitForPendingRead();

  EXPECT_EQ(FakeAudioRendererSink::kPlaying, sink_->state());

  // Verify the next FillBuffer() call triggers the underflow callback
  // since the decoder hasn't delivered any data after it was drained.
  EXPECT_CALL(*this, OnUnderflow())
      .WillOnce(Invoke(this, &AudioRendererImplTest::CallResumeAfterUnderflow));
  EXPECT_FALSE(ConsumeBufferedData(kDataSize, NULL));
  EXPECT_EQ(0, frames_buffered());

  EXPECT_EQ(FakeAudioRendererSink::kPlaying, sink_->state());

  // Simulate playback being paused, and then played again.
  renderer_->SetPlaybackRate(0.0);
  renderer_->SetPlaybackRate(1.0);

  // Deliver data to resolve the underflow.
  DeliverRemainingAudio();

  // We should have resumed playing now.
  EXPECT_EQ(FakeAudioRendererSink::kPlaying, sink_->state());
}

TEST_F(AudioRendererImplTest, PendingRead_Flush) {
  Initialize();

  Preroll();
  StartRendering();

  // Partially drain internal buffer so we get a pending read.
  EXPECT_TRUE(ConsumeBufferedData(frames_buffered() / 2, NULL));
  WaitForPendingRead();

  StopRendering();

  EXPECT_TRUE(IsReadPending());

  // Start flushing.
  WaitableMessageLoopEvent flush_event;
  renderer_->Flush(flush_event.GetClosure());

  SatisfyPendingRead(kDataSize);

  flush_event.RunAndWait();

  EXPECT_FALSE(IsReadPending());

  // Preroll again to a different timestamp and verify it completed normally.
  Preroll(1000, PIPELINE_OK);
}

TEST_F(AudioRendererImplTest, PendingRead_Stop) {
  Initialize();

  Preroll();
  StartRendering();

  // Partially drain internal buffer so we get a pending read.
  EXPECT_TRUE(ConsumeBufferedData(frames_buffered() / 2, NULL));
  WaitForPendingRead();

  StopRendering();

  EXPECT_TRUE(IsReadPending());

  WaitableMessageLoopEvent stop_event;
  renderer_->Stop(stop_event.GetClosure());
  needs_stop_ = false;

  SatisfyPendingRead(kDataSize);

  stop_event.RunAndWait();

  EXPECT_FALSE(IsReadPending());
}

TEST_F(AudioRendererImplTest, PendingFlush_Stop) {
  Initialize();

  Preroll();
  StartRendering();

  // Partially drain internal buffer so we get a pending read.
  EXPECT_TRUE(ConsumeBufferedData(frames_buffered() / 2, NULL));
  WaitForPendingRead();

  StopRendering();

  EXPECT_TRUE(IsReadPending());

  // Start flushing.
  WaitableMessageLoopEvent flush_event;
  renderer_->Flush(flush_event.GetClosure());

  SatisfyPendingRead(kDataSize);

  WaitableMessageLoopEvent event;
  renderer_->Stop(event.GetClosure());
  event.RunAndWait();
  needs_stop_ = false;
}

TEST_F(AudioRendererImplTest, InitializeThenStop) {
  InitializeAndStop();
}

TEST_F(AudioRendererImplTest, InitializeThenStopDuringDecoderInit) {
  InitializeAndStopDuringDecoderInit();
}

TEST_F(AudioRendererImplTest, ConfigChangeDrainsConverter) {
  Initialize();
  Preroll();
  StartRendering();

  // Drain internal buffer, we should have a pending read.
  EXPECT_TRUE(ConsumeBufferedData(frames_buffered(), NULL));
  WaitForPendingRead();

  // Deliver a little bit of data.  Use an odd data size to ensure there is data
  // left in the AudioBufferConverter.  Ensure no buffers are in the splicer.
  SatisfyPendingRead(2053);
  EXPECT_FALSE(splicer_has_next_buffer());
  EXPECT_GT(converter_input_frames_left(), 0);

  // Force a config change and then ensure all buffered data has been put into
  // the splicer.
  force_config_change();
  EXPECT_TRUE(splicer_has_next_buffer());
  EXPECT_EQ(0, converter_input_frames_left());
}

TEST_F(AudioRendererImplTest, TimeUpdatesOnFirstBuffer) {
  Initialize();
  Preroll();
  StartRendering();

  AudioTimestampHelper timestamp_helper(kOutputSamplesPerSecond);
  EXPECT_EQ(kNoTimestamp(), last_time_update());

  // Preroll() should be buffered some data, consume half of it now.
  int frames_to_consume = frames_buffered() / 2;
  EXPECT_TRUE(ConsumeBufferedData(frames_to_consume, NULL));
  WaitForPendingRead();
  base::RunLoop().RunUntilIdle();

  // ConsumeBufferedData() uses an audio delay of zero, so ensure we received
  // a time update that's equal to |kFramesToConsume| from above.
  timestamp_helper.SetBaseTimestamp(base::TimeDelta());
  timestamp_helper.AddFrames(frames_to_consume);
  EXPECT_EQ(timestamp_helper.GetTimestamp(), last_time_update());

  // The next time update should match the remaining frames_buffered(), but only
  // after running the message loop.
  frames_to_consume = frames_buffered();
  EXPECT_TRUE(ConsumeBufferedData(frames_to_consume, NULL));
  EXPECT_EQ(timestamp_helper.GetTimestamp(), last_time_update());

  base::RunLoop().RunUntilIdle();
  timestamp_helper.AddFrames(frames_to_consume);
  EXPECT_EQ(timestamp_helper.GetTimestamp(), last_time_update());
}

TEST_F(AudioRendererImplTest, ImmediateEndOfStream) {
  Initialize();
  {
    SCOPED_TRACE("Preroll()");
    WaitableMessageLoopEvent event;
    renderer_->Preroll(base::TimeDelta(), event.GetPipelineStatusCB());
    WaitForPendingRead();
    DeliverEndOfStream();
    event.RunAndWaitForStatus(PIPELINE_OK);
  }
  StartRendering();

  // Read a single frame. We shouldn't be able to satisfy it.
  EXPECT_FALSE(ConsumeBufferedData(1, NULL));
  WaitForEnded();
}

TEST_F(AudioRendererImplTest, OnRenderErrorCausesDecodeError) {
  Initialize();
  Preroll();
  StartRendering();

  EXPECT_CALL(*this, OnError(PIPELINE_ERROR_DECODE));
  sink_->OnRenderError();
}

// Test for AudioRendererImpl calling Pause()/Play() on the sink when the
// playback rate is set to zero and non-zero.
TEST_F(AudioRendererImplTest, SetPlaybackRate) {
  Initialize();
  Preroll();

  // Rendering hasn't started. Sink should always be paused.
  EXPECT_EQ(FakeAudioRendererSink::kPaused, sink_->state());
  renderer_->SetPlaybackRate(0.0f);
  EXPECT_EQ(FakeAudioRendererSink::kPaused, sink_->state());
  renderer_->SetPlaybackRate(1.0f);
  EXPECT_EQ(FakeAudioRendererSink::kPaused, sink_->state());

  // Rendering has started with non-zero rate. Rate changes will affect sink
  // state.
  renderer_->StartRendering();
  EXPECT_EQ(FakeAudioRendererSink::kPlaying, sink_->state());
  renderer_->SetPlaybackRate(0.0f);
  EXPECT_EQ(FakeAudioRendererSink::kPaused, sink_->state());
  renderer_->SetPlaybackRate(1.0f);
  EXPECT_EQ(FakeAudioRendererSink::kPlaying, sink_->state());

  // Rendering has stopped. Sink should be paused.
  renderer_->StopRendering();
  EXPECT_EQ(FakeAudioRendererSink::kPaused, sink_->state());

  // Start rendering with zero playback rate. Sink should be paused until
  // non-zero rate is set.
  renderer_->SetPlaybackRate(0.0f);
  renderer_->StartRendering();
  EXPECT_EQ(FakeAudioRendererSink::kPaused, sink_->state());
  renderer_->SetPlaybackRate(1.0f);
  EXPECT_EQ(FakeAudioRendererSink::kPlaying, sink_->state());
}

}  // namespace media
