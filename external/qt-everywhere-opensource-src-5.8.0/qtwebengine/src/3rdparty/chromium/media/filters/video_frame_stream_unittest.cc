// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "media/base/fake_demuxer_stream.h"
#include "media/base/gmock_callback_support.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/base/timestamp_constants.h"
#include "media/filters/decoder_stream.h"
#include "media/filters/fake_video_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Assign;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;

static const int kNumConfigs = 4;
static const int kNumBuffersInOneConfig = 5;

namespace media {

struct VideoFrameStreamTestParams {
  VideoFrameStreamTestParams(bool is_encrypted,
                             int decoding_delay,
                             int parallel_decoding)
      : is_encrypted(is_encrypted),
        decoding_delay(decoding_delay),
        parallel_decoding(parallel_decoding) {}

  bool is_encrypted;
  int decoding_delay;
  int parallel_decoding;
};

class VideoFrameStreamTest
    : public testing::Test,
      public testing::WithParamInterface<VideoFrameStreamTestParams> {
 public:
  VideoFrameStreamTest()
      : demuxer_stream_(new FakeDemuxerStream(kNumConfigs,
                                              kNumBuffersInOneConfig,
                                              GetParam().is_encrypted)),
        cdm_context_(new StrictMock<MockCdmContext>()),
        decryptor_(new NiceMock<MockDecryptor>()),
        decoder1_(
            new FakeVideoDecoder(GetParam().decoding_delay,
                                 GetParam().parallel_decoding,
                                 base::Bind(
                                     &VideoFrameStreamTest::OnBytesDecoded,
                                     base::Unretained(this)))),
        decoder2_(
            new FakeVideoDecoder(GetParam().decoding_delay,
                                 GetParam().parallel_decoding,
                                 base::Bind(
                                     &VideoFrameStreamTest::OnBytesDecoded,
                                     base::Unretained(this)))),
        decoder3_(
            new FakeVideoDecoder(GetParam().decoding_delay,
                                 GetParam().parallel_decoding,
                                 base::Bind(
                                     &VideoFrameStreamTest::OnBytesDecoded,
                                     base::Unretained(this)))),
        is_initialized_(false),
        num_decoded_frames_(0),
        pending_initialize_(false),
        pending_read_(false),
        pending_reset_(false),
        pending_stop_(false),
        num_decoded_bytes_unreported_(0),
        has_no_key_(false) {
    ScopedVector<VideoDecoder> decoders;
    decoders.push_back(decoder1_);
    decoders.push_back(decoder2_);
    decoders.push_back(decoder3_);

    video_frame_stream_.reset(new VideoFrameStream(
        message_loop_.task_runner(), std::move(decoders), new MediaLog()));

    EXPECT_CALL(*cdm_context_, GetDecryptor())
        .WillRepeatedly(Return(decryptor_.get()));

    // Decryptor can only decrypt (not decrypt-and-decode) so that
    // DecryptingDemuxerStream will be used.
    EXPECT_CALL(*decryptor_, InitializeVideoDecoder(_, _))
        .WillRepeatedly(RunCallback<1>(false));
    EXPECT_CALL(*decryptor_, Decrypt(_, _, _))
        .WillRepeatedly(Invoke(this, &VideoFrameStreamTest::Decrypt));
  }

  ~VideoFrameStreamTest() {
    // Check that the pipeline statistics callback was fired correctly.
    EXPECT_EQ(num_decoded_bytes_unreported_, 0);

    is_initialized_ = false;
    decoder1_ = NULL;
    decoder2_ = NULL;
    decoder3_ = NULL;
    video_frame_stream_.reset();
    base::RunLoop().RunUntilIdle();

    DCHECK(!pending_initialize_);
    DCHECK(!pending_read_);
    DCHECK(!pending_reset_);
    DCHECK(!pending_stop_);
  }

  MOCK_METHOD1(OnNewSpliceBuffer, void(base::TimeDelta));
  MOCK_METHOD0(OnWaitingForDecryptionKey, void(void));

  void OnStatistics(const PipelineStatistics& statistics) {
    num_decoded_bytes_unreported_ -= statistics.video_bytes_decoded;
  }

  void OnBytesDecoded(int count) {
    num_decoded_bytes_unreported_ += count;
  }

  void OnInitialized(bool success) {
    DCHECK(!pending_read_);
    DCHECK(!pending_reset_);
    DCHECK(pending_initialize_);
    pending_initialize_ = false;

    is_initialized_ = success;
    if (!success) {
      decoder1_ = NULL;
      decoder2_ = NULL;
      decoder3_ = NULL;
    }
  }

  void InitializeVideoFrameStream() {
    pending_initialize_ = true;
    video_frame_stream_->Initialize(
        demuxer_stream_.get(), base::Bind(&VideoFrameStreamTest::OnInitialized,
                                          base::Unretained(this)),
        cdm_context_.get(),
        base::Bind(&VideoFrameStreamTest::OnStatistics, base::Unretained(this)),
        base::Bind(&VideoFrameStreamTest::OnWaitingForDecryptionKey,
                   base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  // Fake Decrypt() function used by DecryptingDemuxerStream. It does nothing
  // but removes the DecryptConfig to make the buffer unencrypted.
  void Decrypt(Decryptor::StreamType stream_type,
               const scoped_refptr<DecoderBuffer>& encrypted,
               const Decryptor::DecryptCB& decrypt_cb) {
    DCHECK(encrypted->decrypt_config());
    if (has_no_key_) {
      decrypt_cb.Run(Decryptor::kNoKey, NULL);
      return;
    }

    DCHECK_EQ(stream_type, Decryptor::kVideo);
    scoped_refptr<DecoderBuffer> decrypted =
        DecoderBuffer::CopyFrom(encrypted->data(), encrypted->data_size());
    if (encrypted->is_key_frame())
      decrypted->set_is_key_frame(true);
    decrypted->set_timestamp(encrypted->timestamp());
    decrypted->set_duration(encrypted->duration());
    decrypt_cb.Run(Decryptor::kSuccess, decrypted);
  }

  // Callback for VideoFrameStream::Read().
  void FrameReady(VideoFrameStream::Status status,
                  const scoped_refptr<VideoFrame>& frame) {
    DCHECK(pending_read_);
    frame_read_ = frame;
    last_read_status_ = status;
    if (frame.get() &&
        !frame->metadata()->IsTrue(VideoFrameMetadata::END_OF_STREAM)) {
      num_decoded_frames_++;
    }
    pending_read_ = false;
  }

  void OnReset() {
    DCHECK(!pending_read_);
    DCHECK(pending_reset_);
    pending_reset_ = false;
  }

  void ReadOneFrame() {
    frame_read_ = NULL;
    pending_read_ = true;
    video_frame_stream_->Read(base::Bind(
        &VideoFrameStreamTest::FrameReady, base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  void ReadUntilPending() {
    do {
      ReadOneFrame();
    } while (!pending_read_);
  }

  void ReadAllFrames(int expected_decoded_frames) {
    do {
      ReadOneFrame();
    } while (frame_read_.get() &&
             !frame_read_->metadata()->IsTrue(
                 VideoFrameMetadata::END_OF_STREAM));

    DCHECK_EQ(expected_decoded_frames, num_decoded_frames_);
  }

  void ReadAllFrames() {
    // No frames should have been dropped.
    ReadAllFrames(kNumConfigs * kNumBuffersInOneConfig);
  }

  enum PendingState {
    NOT_PENDING,
    DEMUXER_READ_NORMAL,
    DEMUXER_READ_CONFIG_CHANGE,
    DECRYPTOR_NO_KEY,
    DECODER_INIT,
    DECODER_REINIT,
    DECODER_DECODE,
    DECODER_RESET
  };

  void EnterPendingState(PendingState state) {
    EnterPendingState(state, decoder1_);
  }

  void EnterPendingState(PendingState state, FakeVideoDecoder* decoder) {
    DCHECK_NE(state, NOT_PENDING);
    switch (state) {
      case DEMUXER_READ_NORMAL:
        demuxer_stream_->HoldNextRead();
        ReadUntilPending();
        break;

      case DEMUXER_READ_CONFIG_CHANGE:
        demuxer_stream_->HoldNextConfigChangeRead();
        ReadUntilPending();
        break;

      case DECRYPTOR_NO_KEY:
        if (GetParam().is_encrypted)
          EXPECT_CALL(*this, OnWaitingForDecryptionKey());
        has_no_key_ = true;
        ReadOneFrame();
        break;

      case DECODER_INIT:
        decoder->HoldNextInit();
        InitializeVideoFrameStream();
        break;

      case DECODER_REINIT:
        decoder->HoldNextInit();
        ReadUntilPending();
        break;

      case DECODER_DECODE:
        decoder->HoldDecode();
        ReadUntilPending();
        break;

      case DECODER_RESET:
        decoder->HoldNextReset();
        pending_reset_ = true;
        video_frame_stream_->Reset(base::Bind(&VideoFrameStreamTest::OnReset,
                                              base::Unretained(this)));
        base::RunLoop().RunUntilIdle();
        break;

      case NOT_PENDING:
        NOTREACHED();
        break;
    }
  }

  void SatisfyPendingCallback(PendingState state) {
    SatisfyPendingCallback(state, decoder1_);
  }

  void SatisfyPendingCallback(PendingState state, FakeVideoDecoder* decoder) {
    DCHECK_NE(state, NOT_PENDING);
    switch (state) {
      case DEMUXER_READ_NORMAL:
      case DEMUXER_READ_CONFIG_CHANGE:
        demuxer_stream_->SatisfyRead();
        break;

      // This is only interesting to test during VideoFrameStream destruction.
      // There's no need to satisfy a callback.
      case DECRYPTOR_NO_KEY:
        NOTREACHED();
        break;

      case DECODER_INIT:
        decoder->SatisfyInit();
        break;

      case DECODER_REINIT:
        decoder->SatisfyInit();
        break;

      case DECODER_DECODE:
        decoder->SatisfyDecode();
        break;

      case DECODER_RESET:
        decoder->SatisfyReset();
        break;

      case NOT_PENDING:
        NOTREACHED();
        break;
    }

    base::RunLoop().RunUntilIdle();
  }

  void Initialize() {
    EnterPendingState(DECODER_INIT);
    SatisfyPendingCallback(DECODER_INIT);
  }

  void Read() {
    EnterPendingState(DECODER_DECODE);
    SatisfyPendingCallback(DECODER_DECODE);
  }

  void Reset() {
    EnterPendingState(DECODER_RESET);
    SatisfyPendingCallback(DECODER_RESET);
  }

  void ReadUntilDecoderReinitialized(FakeVideoDecoder* decoder) {
    EnterPendingState(DECODER_REINIT, decoder);
    SatisfyPendingCallback(DECODER_REINIT, decoder);
  }

  base::MessageLoop message_loop_;

  std::unique_ptr<VideoFrameStream> video_frame_stream_;
  std::unique_ptr<FakeDemuxerStream> demuxer_stream_;
  std::unique_ptr<StrictMock<MockCdmContext>> cdm_context_;

  // Use NiceMock since we don't care about most of calls on the decryptor,
  // e.g. RegisterNewKeyCB().
  std::unique_ptr<NiceMock<MockDecryptor>> decryptor_;

  // Three decoders are needed to test that decoder fallback can occur more than
  // once on a config change. They are owned by |video_frame_stream_|.
  FakeVideoDecoder* decoder1_;
  FakeVideoDecoder* decoder2_;
  FakeVideoDecoder* decoder3_;

  bool is_initialized_;
  int num_decoded_frames_;
  bool pending_initialize_;
  bool pending_read_;
  bool pending_reset_;
  bool pending_stop_;
  int num_decoded_bytes_unreported_;
  scoped_refptr<VideoFrame> frame_read_;
  VideoFrameStream::Status last_read_status_;

  // Decryptor has no key to decrypt a frame.
  bool has_no_key_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoFrameStreamTest);
};

INSTANTIATE_TEST_CASE_P(
    Clear,
    VideoFrameStreamTest,
    ::testing::Values(
        VideoFrameStreamTestParams(false, 0, 1),
        VideoFrameStreamTestParams(false, 3, 1),
        VideoFrameStreamTestParams(false, 7, 1)));

INSTANTIATE_TEST_CASE_P(
    Encrypted,
    VideoFrameStreamTest,
    ::testing::Values(
        VideoFrameStreamTestParams(true, 7, 1)));

INSTANTIATE_TEST_CASE_P(
    Clear_Parallel,
    VideoFrameStreamTest,
    ::testing::Values(
        VideoFrameStreamTestParams(false, 0, 3),
        VideoFrameStreamTestParams(false, 2, 3)));


TEST_P(VideoFrameStreamTest, Initialization) {
  Initialize();
}

TEST_P(VideoFrameStreamTest, AllDecoderInitializationFails) {
  decoder1_->SimulateFailureToInit();
  decoder2_->SimulateFailureToInit();
  decoder3_->SimulateFailureToInit();
  Initialize();
  EXPECT_FALSE(is_initialized_);
}

TEST_P(VideoFrameStreamTest, PartialDecoderInitializationFails) {
  decoder1_->SimulateFailureToInit();
  decoder2_->SimulateFailureToInit();
  Initialize();
  EXPECT_TRUE(is_initialized_);
}

TEST_P(VideoFrameStreamTest, ReadOneFrame) {
  Initialize();
  Read();
}

TEST_P(VideoFrameStreamTest, ReadAllFrames) {
  Initialize();
  ReadAllFrames();
}

TEST_P(VideoFrameStreamTest, Read_AfterReset) {
  Initialize();
  Reset();
  Read();
  Reset();
  Read();
}

TEST_P(VideoFrameStreamTest, Read_BlockedDemuxer) {
  Initialize();
  demuxer_stream_->HoldNextRead();
  ReadOneFrame();
  EXPECT_TRUE(pending_read_);

  int demuxed_buffers = 0;

  // Pass frames from the demuxer to the VideoFrameStream until the first read
  // request is satisfied.
  while (pending_read_) {
    ++demuxed_buffers;
    demuxer_stream_->SatisfyReadAndHoldNext();
    base::RunLoop().RunUntilIdle();
  }

  EXPECT_EQ(std::min(GetParam().decoding_delay + 1, kNumBuffersInOneConfig + 1),
            demuxed_buffers);

  // At this point the stream is waiting on read from the demuxer, but there is
  // no pending read from the stream. The stream should be blocked if we try
  // reading from it again.
  ReadUntilPending();

  demuxer_stream_->SatisfyRead();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(pending_read_);
}

TEST_P(VideoFrameStreamTest, Read_BlockedDemuxerAndDecoder) {
  // Test applies only when the decoder allows multiple parallel requests.
  if (GetParam().parallel_decoding == 1)
    return;

  Initialize();
  demuxer_stream_->HoldNextRead();
  decoder1_->HoldDecode();
  ReadOneFrame();
  EXPECT_TRUE(pending_read_);

  int demuxed_buffers = 0;

  // Pass frames from the demuxer to the VideoFrameStream until the first read
  // request is satisfied, while always keeping one decode request pending.
  while (pending_read_) {
    ++demuxed_buffers;
    demuxer_stream_->SatisfyReadAndHoldNext();
    base::RunLoop().RunUntilIdle();

    // Always keep one decode request pending.
    if (demuxed_buffers > 1) {
      decoder1_->SatisfySingleDecode();
      base::RunLoop().RunUntilIdle();
    }
  }

  ReadUntilPending();
  EXPECT_TRUE(pending_read_);

  // Unblocking one decode request should unblock read even when demuxer is
  // still blocked.
  decoder1_->SatisfySingleDecode();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(pending_read_);

  // Stream should still be blocked on the demuxer after unblocking the decoder.
  decoder1_->SatisfyDecode();
  ReadUntilPending();
  EXPECT_TRUE(pending_read_);

  // Verify that the stream has returned all frames that have been demuxed,
  // accounting for the decoder delay.
  EXPECT_EQ(demuxed_buffers - GetParam().decoding_delay, num_decoded_frames_);

  // Unblocking the demuxer will unblock the stream.
  demuxer_stream_->SatisfyRead();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(pending_read_);
}

TEST_P(VideoFrameStreamTest, Read_DuringEndOfStreamDecode) {
  // Test applies only when the decoder allows multiple parallel requests, and
  // they are not satisfied in a single batch.
  if (GetParam().parallel_decoding == 1 || GetParam().decoding_delay != 0)
    return;

  Initialize();
  decoder1_->HoldDecode();

  // Read all of the frames up to end of stream. Since parallel decoding is
  // enabled, the end of stream buffer will be sent to the decoder immediately,
  // but we don't satisfy it yet.
  for (int configuration = 0; configuration < kNumConfigs; configuration++) {
    for (int frame = 0; frame < kNumBuffersInOneConfig; frame++) {
      ReadOneFrame();
      while (pending_read_) {
        decoder1_->SatisfySingleDecode();
        base::RunLoop().RunUntilIdle();
      }
    }
  }

  // Read() again. The callback must be delayed until the decode completes.
  ReadOneFrame();
  ASSERT_TRUE(pending_read_);

  // Satisfy decoding of the end of stream buffer. The read should complete.
  decoder1_->SatisfySingleDecode();
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(pending_read_);
  EXPECT_EQ(last_read_status_, VideoFrameStream::OK);

  // The read output should indicate end of stream.
  ASSERT_TRUE(frame_read_.get());
  EXPECT_TRUE(
      frame_read_->metadata()->IsTrue(VideoFrameMetadata::END_OF_STREAM));
}

// No Reset() before initialization is successfully completed.
TEST_P(VideoFrameStreamTest, Reset_AfterInitialization) {
  Initialize();
  Reset();
  Read();
}

TEST_P(VideoFrameStreamTest, Reset_DuringReinitialization) {
  Initialize();
  EnterPendingState(DECODER_REINIT);
  // VideoDecoder::Reset() is not called when we reset during reinitialization.
  pending_reset_ = true;
  video_frame_stream_->Reset(
      base::Bind(&VideoFrameStreamTest::OnReset, base::Unretained(this)));
  SatisfyPendingCallback(DECODER_REINIT);
  Read();
}

TEST_P(VideoFrameStreamTest, Reset_AfterReinitialization) {
  Initialize();
  EnterPendingState(DECODER_REINIT);
  SatisfyPendingCallback(DECODER_REINIT);
  Reset();
  Read();
}

TEST_P(VideoFrameStreamTest, Reset_DuringDemuxerRead_Normal) {
  Initialize();
  EnterPendingState(DEMUXER_READ_NORMAL);
  EnterPendingState(DECODER_RESET);
  SatisfyPendingCallback(DEMUXER_READ_NORMAL);
  SatisfyPendingCallback(DECODER_RESET);
  Read();
}

TEST_P(VideoFrameStreamTest, Reset_DuringDemuxerRead_ConfigChange) {
  Initialize();
  EnterPendingState(DEMUXER_READ_CONFIG_CHANGE);
  EnterPendingState(DECODER_RESET);
  SatisfyPendingCallback(DEMUXER_READ_CONFIG_CHANGE);
  SatisfyPendingCallback(DECODER_RESET);
  Read();
}

TEST_P(VideoFrameStreamTest, Reset_DuringNormalDecoderDecode) {
  Initialize();
  EnterPendingState(DECODER_DECODE);
  EnterPendingState(DECODER_RESET);
  SatisfyPendingCallback(DECODER_DECODE);
  SatisfyPendingCallback(DECODER_RESET);
  Read();
}

TEST_P(VideoFrameStreamTest, Reset_AfterNormalRead) {
  Initialize();
  Read();
  Reset();
  Read();
}

TEST_P(VideoFrameStreamTest, Reset_AfterNormalReadWithActiveSplice) {
  video_frame_stream_->set_splice_observer(base::Bind(
      &VideoFrameStreamTest::OnNewSpliceBuffer, base::Unretained(this)));
  Initialize();

  // Send buffers with a splice timestamp, which sets the active splice flag.
  const base::TimeDelta splice_timestamp = base::TimeDelta();
  demuxer_stream_->set_splice_timestamp(splice_timestamp);
  EXPECT_CALL(*this, OnNewSpliceBuffer(splice_timestamp)).Times(AnyNumber());
  Read();

  // Issue an explicit Reset() and clear the splice timestamp.
  Reset();
  demuxer_stream_->set_splice_timestamp(kNoTimestamp());

  // Ensure none of the upcoming calls indicate they have a splice timestamp.
  EXPECT_CALL(*this, OnNewSpliceBuffer(_)).Times(0);
  Read();
}

TEST_P(VideoFrameStreamTest, Reset_AfterDemuxerRead_ConfigChange) {
  Initialize();
  EnterPendingState(DEMUXER_READ_CONFIG_CHANGE);
  SatisfyPendingCallback(DEMUXER_READ_CONFIG_CHANGE);
  Reset();
  Read();
}

TEST_P(VideoFrameStreamTest, Reset_AfterEndOfStream) {
  Initialize();
  ReadAllFrames();
  Reset();
  num_decoded_frames_ = 0;
  demuxer_stream_->SeekToStart();
  ReadAllFrames();
}

TEST_P(VideoFrameStreamTest, Reset_DuringNoKeyRead) {
  Initialize();
  EnterPendingState(DECRYPTOR_NO_KEY);
  Reset();
}

// In the following Destroy_* tests, |video_frame_stream_| is destroyed in
// VideoFrameStreamTest dtor.

TEST_P(VideoFrameStreamTest, Destroy_BeforeInitialization) {
}

TEST_P(VideoFrameStreamTest, Destroy_DuringInitialization) {
  EnterPendingState(DECODER_INIT);
}

TEST_P(VideoFrameStreamTest, Destroy_AfterInitialization) {
  Initialize();
}

TEST_P(VideoFrameStreamTest, Destroy_DuringReinitialization) {
  Initialize();
  EnterPendingState(DECODER_REINIT);
}

TEST_P(VideoFrameStreamTest, Destroy_AfterReinitialization) {
  Initialize();
  EnterPendingState(DECODER_REINIT);
  SatisfyPendingCallback(DECODER_REINIT);
}

TEST_P(VideoFrameStreamTest, Destroy_DuringDemuxerRead_Normal) {
  Initialize();
  EnterPendingState(DEMUXER_READ_NORMAL);
}

TEST_P(VideoFrameStreamTest, Destroy_DuringDemuxerRead_ConfigChange) {
  Initialize();
  EnterPendingState(DEMUXER_READ_CONFIG_CHANGE);
}

TEST_P(VideoFrameStreamTest, Destroy_DuringNormalDecoderDecode) {
  Initialize();
  EnterPendingState(DECODER_DECODE);
}

TEST_P(VideoFrameStreamTest, Destroy_AfterNormalRead) {
  Initialize();
  Read();
}

TEST_P(VideoFrameStreamTest, Destroy_AfterConfigChangeRead) {
  Initialize();
  EnterPendingState(DEMUXER_READ_CONFIG_CHANGE);
  SatisfyPendingCallback(DEMUXER_READ_CONFIG_CHANGE);
}

TEST_P(VideoFrameStreamTest, Destroy_DuringDecoderReinitialization) {
  Initialize();
  EnterPendingState(DECODER_REINIT);
}

TEST_P(VideoFrameStreamTest, Destroy_DuringNoKeyRead) {
  Initialize();
  EnterPendingState(DECRYPTOR_NO_KEY);
}

TEST_P(VideoFrameStreamTest, Destroy_DuringReset) {
  Initialize();
  EnterPendingState(DECODER_RESET);
}

TEST_P(VideoFrameStreamTest, Destroy_AfterReset) {
  Initialize();
  Reset();
}

TEST_P(VideoFrameStreamTest, Destroy_DuringRead_DuringReset) {
  Initialize();
  EnterPendingState(DECODER_DECODE);
  EnterPendingState(DECODER_RESET);
}

TEST_P(VideoFrameStreamTest, Destroy_AfterRead_DuringReset) {
  Initialize();
  EnterPendingState(DECODER_DECODE);
  EnterPendingState(DECODER_RESET);
  SatisfyPendingCallback(DECODER_DECODE);
}

TEST_P(VideoFrameStreamTest, Destroy_AfterRead_AfterReset) {
  Initialize();
  Read();
  Reset();
}

TEST_P(VideoFrameStreamTest, FallbackDecoder_SelectedOnInitialDecodeError) {
  Initialize();
  decoder1_->SimulateError();
  ReadOneFrame();

  // |video_frame_stream_| should have fallen back to |decoder2_|.
  ASSERT_FALSE(pending_read_);
  ASSERT_EQ(VideoFrameStream::OK, last_read_status_);

  // Can't check |decoder1_| right now, it might have been destroyed already.
  ASSERT_GT(decoder2_->total_bytes_decoded(), 0);

  // Verify no frame was dropped.
  ReadAllFrames();
}

TEST_P(VideoFrameStreamTest, FallbackDecoder_EndOfStreamReachedBeforeFallback) {
  // Only consider cases where there is a decoder delay. For test simplicity,
  // omit the parallel case.
  if (GetParam().decoding_delay == 0 || GetParam().parallel_decoding > 1)
    return;

  Initialize();
  decoder1_->HoldDecode();
  ReadOneFrame();

  // One buffer should have already pulled from the demuxer stream. Set the next
  // one to be an EOS.
  demuxer_stream_->SeekToEndOfStream();

  decoder1_->SatisfySingleDecode();
  base::RunLoop().RunUntilIdle();

  // |video_frame_stream_| should not have emited a frame.
  EXPECT_TRUE(pending_read_);

  // Pending buffers should contain a regular buffer and an EOS buffer.
  EXPECT_EQ(video_frame_stream_->get_pending_buffers_size_for_testing(), 2);

  decoder1_->SimulateError();
  base::RunLoop().RunUntilIdle();

  //  A frame should have been emited
  EXPECT_FALSE(pending_read_);
  EXPECT_EQ(last_read_status_, VideoFrameStream::OK);
  EXPECT_FALSE(
      frame_read_->metadata()->IsTrue(VideoFrameMetadata::END_OF_STREAM));
  EXPECT_GT(decoder2_->total_bytes_decoded(), 0);

  ReadOneFrame();

  EXPECT_FALSE(pending_read_);
  EXPECT_EQ(0, video_frame_stream_->get_fallback_buffers_size_for_testing());
  EXPECT_TRUE(
      frame_read_->metadata()->IsTrue(VideoFrameMetadata::END_OF_STREAM));
}

TEST_P(VideoFrameStreamTest, FallbackDecoder_DoesReinitializeStompPendingRead) {
  // Test only the case where there is no decoding delay and parallel decoding.
  if (GetParam().decoding_delay != 0 || GetParam().parallel_decoding <= 1)
    return;

  Initialize();
  decoder1_->HoldDecode();

  // Queue one read, defer the second.
  frame_read_ = nullptr;
  pending_read_ = true;
  video_frame_stream_->Read(
      base::Bind(&VideoFrameStreamTest::FrameReady, base::Unretained(this)));
  demuxer_stream_->HoldNextRead();

  // Force an error to occur on the first decode, but ensure it isn't propagated
  // until after the next read has been started.
  decoder1_->SimulateError();
  decoder2_->HoldDecode();

  // Complete the fallback to the second decoder with the read still pending.
  base::RunLoop().RunUntilIdle();

  // Can't check |decoder1_| right now, it might have been destroyed already.
  // Verify that there was nothing decoded until we kicked the decoder.
  EXPECT_EQ(decoder2_->total_bytes_decoded(), 0);
  decoder2_->SatisfyDecode();
  const int first_decoded_bytes = decoder2_->total_bytes_decoded();
  ASSERT_GT(first_decoded_bytes, 0);

  // Satisfy the previously pending read and ensure it is decoded.
  demuxer_stream_->SatisfyRead();
  base::RunLoop().RunUntilIdle();
  ASSERT_GT(decoder2_->total_bytes_decoded(), first_decoded_bytes);
}

TEST_P(VideoFrameStreamTest,
       FallbackDecoder_SelectedOnInitialDecodeError_Twice) {
  Initialize();
  decoder1_->SimulateError();
  decoder2_->HoldNextInit();
  ReadOneFrame();

  decoder2_->SatisfyInit();
  decoder2_->SimulateError();
  base::RunLoop().RunUntilIdle();

  // |video_frame_stream_| should have fallen back to |decoder3_|.
  ASSERT_FALSE(pending_read_);
  ASSERT_EQ(VideoFrameStream::OK, last_read_status_);

  // Can't check |decoder1_| or |decoder2_| right now, they might have been
  // destroyed already.
  ASSERT_GT(decoder3_->total_bytes_decoded(), 0);

  // Verify no frame was dropped.
  ReadAllFrames();
}

TEST_P(VideoFrameStreamTest, FallbackDecoder_ConfigChangeClearsPendingBuffers) {
  // Test case is only interesting if the decoder can receive a config change
  // before returning its first frame.
  if (GetParam().decoding_delay < kNumBuffersInOneConfig)
    return;

  Initialize();
  EnterPendingState(DEMUXER_READ_CONFIG_CHANGE);
  ASSERT_GT(video_frame_stream_->get_pending_buffers_size_for_testing(), 0);

  SatisfyPendingCallback(DEMUXER_READ_CONFIG_CHANGE);
  ASSERT_EQ(video_frame_stream_->get_pending_buffers_size_for_testing(), 0);
  EXPECT_FALSE(pending_read_);

  ReadAllFrames();
}

TEST_P(VideoFrameStreamTest, FallbackDecoder_ErrorDuringConfigChangeFlushing) {
  // Test case is only interesting if the decoder can receive a config change
  // before returning its first frame.
  if (GetParam().decoding_delay < kNumBuffersInOneConfig)
    return;

  Initialize();
  EnterPendingState(DEMUXER_READ_CONFIG_CHANGE);
  EXPECT_GT(video_frame_stream_->get_pending_buffers_size_for_testing(), 0);

  decoder1_->HoldDecode();
  SatisfyPendingCallback(DEMUXER_READ_CONFIG_CHANGE);

  // The flush request should have been sent and held.
  EXPECT_EQ(video_frame_stream_->get_pending_buffers_size_for_testing(), 0);
  EXPECT_TRUE(pending_read_);

  // Triggering an error here will cause the frames in |decoder1_| to be lost.
  // There are no pending buffers buffers to give to give to |decoder2_| due to
  // crbug.com/603713.
  decoder1_->SimulateError();
  base::RunLoop().RunUntilIdle();

  // We want to make sure that |decoder2_| can decode the rest of the frames
  // in the demuxer stream.
  ReadAllFrames(kNumBuffersInOneConfig * (kNumConfigs - 1));
}

TEST_P(VideoFrameStreamTest, FallbackDecoder_PendingBuffersIsFilledAndCleared) {
  // Test applies only when there is a decoder delay, and the decoder will not
  // receive a config change before outputing its first frame. Parallel decoding
  // is also disabled in this test case, for readability and simplicity of the
  // unit test.
  if (GetParam().decoding_delay == 0 ||
      GetParam().decoding_delay > kNumBuffersInOneConfig ||
      GetParam().parallel_decoding > 1) {
    return;
  }
  Initialize();

  // Block on demuxer read and decoder decode so we can step through.
  demuxer_stream_->HoldNextRead();
  decoder1_->HoldDecode();
  ReadOneFrame();

  int demuxer_reads_satisfied = 0;
  // Send back and requests buffers until the next one would fill the decoder
  // delay.
  while (demuxer_reads_satisfied < GetParam().decoding_delay - 1) {
    // Send a buffer back.
    demuxer_stream_->SatisfyReadAndHoldNext();
    base::RunLoop().RunUntilIdle();
    ++demuxer_reads_satisfied;

    // Decode one buffer.
    decoder1_->SatisfySingleDecode();
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(pending_read_);
    EXPECT_EQ(demuxer_reads_satisfied,
              video_frame_stream_->get_pending_buffers_size_for_testing());
    // No fallback buffers should be queued up yet.
    EXPECT_EQ(0, video_frame_stream_->get_fallback_buffers_size_for_testing());
  }

  // Hold the init before triggering the error, to verify internal state.
  demuxer_stream_->SatisfyReadAndHoldNext();
  ++demuxer_reads_satisfied;
  decoder2_->HoldNextInit();
  decoder1_->SimulateError();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(pending_read_);
  EXPECT_EQ(demuxer_reads_satisfied,
            video_frame_stream_->get_pending_buffers_size_for_testing());

  decoder2_->SatisfyInit();
  decoder2_->HoldDecode();
  base::RunLoop().RunUntilIdle();

  // Make sure the pending buffers have been transfered to fallback buffers.
  // One call to Decode() during the initialization process, so we expect one
  // buffer to already have been consumed from the fallback buffers.
  // Pending buffers should never go down (unless we encounter a config change)
  EXPECT_EQ(demuxer_reads_satisfied - 1,
            video_frame_stream_->get_fallback_buffers_size_for_testing());
  EXPECT_EQ(demuxer_reads_satisfied,
            video_frame_stream_->get_pending_buffers_size_for_testing());

  decoder2_->SatisfyDecode();
  base::RunLoop().RunUntilIdle();

  // Make sure all buffers consumed by |decoder2_| have come from the fallback.
  // Pending buffers should not have been cleared yet.
  EXPECT_EQ(0, video_frame_stream_->get_fallback_buffers_size_for_testing());
  EXPECT_EQ(demuxer_reads_satisfied,
            video_frame_stream_->get_pending_buffers_size_for_testing());
  EXPECT_EQ(video_frame_stream_->get_previous_decoder_for_testing(), decoder1_);
  EXPECT_TRUE(pending_read_);

  // Give the decoder one more buffer, enough to release a frame.
  demuxer_stream_->SatisfyReadAndHoldNext();
  base::RunLoop().RunUntilIdle();

  // New buffers should not have been added after the frame was released.
  EXPECT_EQ(video_frame_stream_->get_pending_buffers_size_for_testing(), 0);
  EXPECT_FALSE(pending_read_);

  demuxer_stream_->SatisfyRead();

  // Confirm no frames were dropped.
  ReadAllFrames();
}

TEST_P(VideoFrameStreamTest, FallbackDecoder_SelectedOnDecodeThenInitErrors) {
  Initialize();
  decoder1_->SimulateError();
  decoder2_->SimulateFailureToInit();
  ReadOneFrame();

  // |video_frame_stream_| should have fallen back to |decoder3_|
  ASSERT_FALSE(pending_read_);
  ASSERT_EQ(VideoFrameStream::OK, last_read_status_);

  // Can't check |decoder1_| or |decoder2_| right now, they might have been
  // destroyed already.
  ASSERT_GT(decoder3_->total_bytes_decoded(), 0);

  // Verify no frame was dropped.
  ReadAllFrames();
}

TEST_P(VideoFrameStreamTest, FallbackDecoder_SelectedOnInitThenDecodeErrors) {
  decoder1_->SimulateFailureToInit();
  decoder2_->HoldDecode();
  Initialize();
  ReadOneFrame();
  decoder2_->SimulateError();
  base::RunLoop().RunUntilIdle();

  // |video_frame_stream_| should have fallen back to |decoder3_|
  ASSERT_FALSE(pending_read_);
  ASSERT_EQ(VideoFrameStream::OK, last_read_status_);

  // Can't check |decoder1_| or |decoder2_| right now, they might have been
  // destroyed already.
  ASSERT_GT(decoder3_->total_bytes_decoded(), 0);

  // Verify no frame was dropped.
  ReadAllFrames();
}

TEST_P(VideoFrameStreamTest,
       FallbackDecoder_NotSelectedOnMidstreamDecodeError) {
  Initialize();
  ReadOneFrame();

  // Successfully received a frame.
  EXPECT_FALSE(pending_read_);
  ASSERT_GT(decoder1_->total_bytes_decoded(), 0);

  decoder1_->SimulateError();

  // The error must surface from Read() as DECODE_ERROR.
  while (last_read_status_ == VideoFrameStream::OK) {
    ReadOneFrame();
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(pending_read_);
  }

  // Verify the error was surfaced, rather than falling back to |decoder2_|.
  EXPECT_FALSE(pending_read_);
  ASSERT_EQ(decoder2_->total_bytes_decoded(), 0);
  ASSERT_EQ(VideoFrameStream::DECODE_ERROR, last_read_status_);
}

TEST_P(VideoFrameStreamTest, DecoderErrorWhenNotReading) {
  Initialize();
  decoder1_->HoldDecode();
  ReadOneFrame();
  EXPECT_TRUE(pending_read_);

  // Satisfy decode requests until we get the first frame out.
  while (pending_read_) {
    decoder1_->SatisfySingleDecode();
    base::RunLoop().RunUntilIdle();
  }

  // Trigger an error in the decoding.
  decoder1_->SimulateError();

  // The error must surface from Read() as DECODE_ERROR.
  while (last_read_status_ == VideoFrameStream::OK) {
    ReadOneFrame();
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(pending_read_);
  }
  EXPECT_EQ(VideoFrameStream::DECODE_ERROR, last_read_status_);
}

TEST_P(VideoFrameStreamTest, FallbackDecoderSelectedOnFailureToReinitialize) {
  Initialize();
  decoder1_->SimulateFailureToInit();
  // Holding decode, because large decoder delays might cause us to get rid of
  // |previous_decoder_| before we are in a pending state again.
  decoder2_->HoldDecode();
  ReadUntilDecoderReinitialized(decoder1_);
  ASSERT_TRUE(video_frame_stream_->get_previous_decoder_for_testing());
  decoder2_->SatisfyDecode();
  base::RunLoop().RunUntilIdle();
  ReadAllFrames();
  ASSERT_FALSE(video_frame_stream_->get_previous_decoder_for_testing());
}

TEST_P(VideoFrameStreamTest,
       FallbackDecoderSelectedOnFailureToReinitialize_Twice) {
  Initialize();
  decoder1_->SimulateFailureToInit();
  ReadUntilDecoderReinitialized(decoder1_);
  ReadOneFrame();
  decoder2_->SimulateFailureToInit();
  ReadUntilDecoderReinitialized(decoder2_);
  ReadAllFrames();
}

TEST_P(VideoFrameStreamTest, DecodeErrorAfterFallbackDecoderSelectionFails) {
  Initialize();
  decoder1_->SimulateFailureToInit();
  decoder2_->SimulateFailureToInit();
  decoder3_->SimulateFailureToInit();
  ReadUntilDecoderReinitialized(decoder1_);
  // The error will surface from Read() as DECODE_ERROR.
  while (last_read_status_ == VideoFrameStream::OK) {
    ReadOneFrame();
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(pending_read_);
  }
  EXPECT_EQ(VideoFrameStream::DECODE_ERROR, last_read_status_);
}

TEST_P(VideoFrameStreamTest, Destroy_DuringFallbackDecoderSelection) {
  Initialize();
  decoder1_->SimulateFailureToInit();
  EnterPendingState(DECODER_REINIT);
  decoder2_->HoldNextInit();
  SatisfyPendingCallback(DECODER_REINIT);
}

}  // namespace media
