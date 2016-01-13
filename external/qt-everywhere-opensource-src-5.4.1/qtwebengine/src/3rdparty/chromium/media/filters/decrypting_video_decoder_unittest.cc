// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/message_loop/message_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/base/gmock_callback_support.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/base/video_frame.h"
#include "media/filters/decrypting_video_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace media {

const uint8 kFakeKeyId[] = { 0x4b, 0x65, 0x79, 0x20, 0x49, 0x44 };
const uint8 kFakeIv[DecryptConfig::kDecryptionKeySize] = { 0 };
const int kDecodingDelay = 3;

// Create a fake non-empty encrypted buffer.
static scoped_refptr<DecoderBuffer> CreateFakeEncryptedBuffer() {
  const int buffer_size = 16;  // Need a non-empty buffer;
  scoped_refptr<DecoderBuffer> buffer(new DecoderBuffer(buffer_size));
  buffer->set_decrypt_config(scoped_ptr<DecryptConfig>(new DecryptConfig(
      std::string(reinterpret_cast<const char*>(kFakeKeyId),
                  arraysize(kFakeKeyId)),
      std::string(reinterpret_cast<const char*>(kFakeIv), arraysize(kFakeIv)),
      std::vector<SubsampleEntry>())));
  return buffer;
}

// Use anonymous namespace here to prevent the actions to be defined multiple
// times across multiple test files. Sadly we can't use static for them.
namespace {

ACTION_P(RunCallbackIfNotNull, param) {
  if (!arg0.is_null())
    arg0.Run(param);
}

ACTION_P2(ResetAndRunCallback, callback, param) {
  base::ResetAndReturn(callback).Run(param);
}

}  // namespace

class DecryptingVideoDecoderTest : public testing::Test {
 public:
  DecryptingVideoDecoderTest()
      : decoder_(new DecryptingVideoDecoder(
            message_loop_.message_loop_proxy(),
            base::Bind(
                &DecryptingVideoDecoderTest::RequestDecryptorNotification,
                base::Unretained(this)))),
        decryptor_(new StrictMock<MockDecryptor>()),
        num_decrypt_and_decode_calls_(0),
        num_frames_in_decryptor_(0),
        encrypted_buffer_(CreateFakeEncryptedBuffer()),
        decoded_video_frame_(VideoFrame::CreateBlackFrame(
            TestVideoConfig::NormalCodedSize())),
        null_video_frame_(scoped_refptr<VideoFrame>()) {
    EXPECT_CALL(*this, RequestDecryptorNotification(_))
        .WillRepeatedly(RunCallbackIfNotNull(decryptor_.get()));
  }

  virtual ~DecryptingVideoDecoderTest() {
    Stop();
  }

  // Initializes the |decoder_| and expects |status|. Note the initialization
  // can succeed or fail.
  void InitializeAndExpectStatus(const VideoDecoderConfig& config,
                                 PipelineStatus status) {
    decoder_->Initialize(config, false, NewExpectedStatusCB(status),
                         base::Bind(&DecryptingVideoDecoderTest::FrameReady,
                                    base::Unretained(this)));
    message_loop_.RunUntilIdle();
  }

  // Initialize the |decoder_| and expects it to succeed.
  void Initialize() {
    EXPECT_CALL(*decryptor_, InitializeVideoDecoder(_, _))
        .WillOnce(RunCallback<1>(true));
    EXPECT_CALL(*decryptor_, RegisterNewKeyCB(Decryptor::kVideo, _))
        .WillOnce(SaveArg<1>(&key_added_cb_));

    InitializeAndExpectStatus(TestVideoConfig::NormalEncrypted(), PIPELINE_OK);
  }

  // Reinitialize the |decoder_| and expects it to succeed.
  void Reinitialize() {
    EXPECT_CALL(*decryptor_, DeinitializeDecoder(Decryptor::kVideo));
    EXPECT_CALL(*decryptor_, InitializeVideoDecoder(_, _))
        .WillOnce(RunCallback<1>(true));
    EXPECT_CALL(*decryptor_, RegisterNewKeyCB(Decryptor::kVideo, _))
        .WillOnce(SaveArg<1>(&key_added_cb_));

    InitializeAndExpectStatus(TestVideoConfig::LargeEncrypted(), PIPELINE_OK);
  }

  // Decode |buffer| and expect DecodeDone to get called with |status|.
  void DecodeAndExpect(const scoped_refptr<DecoderBuffer>& buffer,
                       VideoDecoder::Status status) {
    EXPECT_CALL(*this, DecodeDone(status));
    decoder_->Decode(buffer,
                     base::Bind(&DecryptingVideoDecoderTest::DecodeDone,
                                base::Unretained(this)));
    message_loop_.RunUntilIdle();
  }

  // Helper function to simulate the decrypting and decoding process in the
  // |decryptor_| with a decoding delay of kDecodingDelay buffers.
  void DecryptAndDecodeVideo(const scoped_refptr<DecoderBuffer>& encrypted,
                             const Decryptor::VideoDecodeCB& video_decode_cb) {
    num_decrypt_and_decode_calls_++;
    if (!encrypted->end_of_stream())
      num_frames_in_decryptor_++;

    if (num_decrypt_and_decode_calls_ <= kDecodingDelay ||
        num_frames_in_decryptor_ == 0) {
      video_decode_cb.Run(Decryptor::kNeedMoreData,
                          scoped_refptr<VideoFrame>());
      return;
    }

    num_frames_in_decryptor_--;
    video_decode_cb.Run(Decryptor::kSuccess, decoded_video_frame_);
  }

  // Sets up expectations and actions to put DecryptingVideoDecoder in an
  // active normal decoding state.
  void EnterNormalDecodingState() {
    EXPECT_CALL(*decryptor_, DecryptAndDecodeVideo(_, _)).WillRepeatedly(
        Invoke(this, &DecryptingVideoDecoderTest::DecryptAndDecodeVideo));
    EXPECT_CALL(*this, FrameReady(decoded_video_frame_));
    for (int i = 0; i < kDecodingDelay + 1; ++i)
      DecodeAndExpect(encrypted_buffer_, VideoDecoder::kOk);
  }

  // Sets up expectations and actions to put DecryptingVideoDecoder in an end
  // of stream state. This function must be called after
  // EnterNormalDecodingState() to work.
  void EnterEndOfStreamState() {
    // The codec in the |decryptor_| will be flushed.
    EXPECT_CALL(*this, FrameReady(decoded_video_frame_))
        .Times(kDecodingDelay);
    DecodeAndExpect(DecoderBuffer::CreateEOSBuffer(), VideoDecoder::kOk);
    EXPECT_EQ(0, num_frames_in_decryptor_);
  }

  // Make the video decode callback pending by saving and not firing it.
  void EnterPendingDecodeState() {
    EXPECT_TRUE(pending_video_decode_cb_.is_null());
    EXPECT_CALL(*decryptor_, DecryptAndDecodeVideo(encrypted_buffer_, _))
        .WillOnce(SaveArg<1>(&pending_video_decode_cb_));

    decoder_->Decode(encrypted_buffer_,
                     base::Bind(&DecryptingVideoDecoderTest::DecodeDone,
                                base::Unretained(this)));
    message_loop_.RunUntilIdle();
    // Make sure the Decode() on the decoder triggers a DecryptAndDecode() on
    // the decryptor.
    EXPECT_FALSE(pending_video_decode_cb_.is_null());
  }

  void EnterWaitingForKeyState() {
    EXPECT_CALL(*decryptor_, DecryptAndDecodeVideo(_, _))
        .WillRepeatedly(RunCallback<1>(Decryptor::kNoKey, null_video_frame_));
    decoder_->Decode(encrypted_buffer_,
                     base::Bind(&DecryptingVideoDecoderTest::DecodeDone,
                                base::Unretained(this)));
    message_loop_.RunUntilIdle();
  }

  void AbortPendingVideoDecodeCB() {
    if (!pending_video_decode_cb_.is_null()) {
      base::ResetAndReturn(&pending_video_decode_cb_).Run(
          Decryptor::kSuccess, scoped_refptr<VideoFrame>(NULL));
    }
  }

  void AbortAllPendingCBs() {
    if (!pending_init_cb_.is_null()) {
      ASSERT_TRUE(pending_video_decode_cb_.is_null());
      base::ResetAndReturn(&pending_init_cb_).Run(false);
      return;
    }

    AbortPendingVideoDecodeCB();
  }

  void Reset() {
    EXPECT_CALL(*decryptor_, ResetDecoder(Decryptor::kVideo))
        .WillRepeatedly(InvokeWithoutArgs(
            this, &DecryptingVideoDecoderTest::AbortPendingVideoDecodeCB));

    decoder_->Reset(NewExpectedClosure());
    message_loop_.RunUntilIdle();
  }

  void Stop() {
    EXPECT_CALL(*decryptor_, DeinitializeDecoder(Decryptor::kVideo))
        .WillRepeatedly(InvokeWithoutArgs(
            this, &DecryptingVideoDecoderTest::AbortAllPendingCBs));

    decoder_->Stop();
    message_loop_.RunUntilIdle();
  }

  MOCK_METHOD1(RequestDecryptorNotification, void(const DecryptorReadyCB&));

  MOCK_METHOD1(FrameReady, void(const scoped_refptr<VideoFrame>&));
  MOCK_METHOD1(DecodeDone, void(VideoDecoder::Status));

  base::MessageLoop message_loop_;
  scoped_ptr<DecryptingVideoDecoder> decoder_;
  scoped_ptr<StrictMock<MockDecryptor> > decryptor_;

  // Variables to help the |decryptor_| to simulate decoding delay and flushing.
  int num_decrypt_and_decode_calls_;
  int num_frames_in_decryptor_;

  Decryptor::DecoderInitCB pending_init_cb_;
  Decryptor::NewKeyCB key_added_cb_;
  Decryptor::VideoDecodeCB pending_video_decode_cb_;

  // Constant buffer/frames.
  scoped_refptr<DecoderBuffer> encrypted_buffer_;
  scoped_refptr<VideoFrame> decoded_video_frame_;
  scoped_refptr<VideoFrame> null_video_frame_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DecryptingVideoDecoderTest);
};

TEST_F(DecryptingVideoDecoderTest, Initialize_Normal) {
  Initialize();
}

TEST_F(DecryptingVideoDecoderTest, Initialize_NullDecryptor) {
  EXPECT_CALL(*this, RequestDecryptorNotification(_))
      .WillRepeatedly(RunCallbackIfNotNull(static_cast<Decryptor*>(NULL)));
  InitializeAndExpectStatus(TestVideoConfig::NormalEncrypted(),
                            DECODER_ERROR_NOT_SUPPORTED);
}

TEST_F(DecryptingVideoDecoderTest, Initialize_Failure) {
  EXPECT_CALL(*decryptor_, InitializeVideoDecoder(_, _))
      .WillRepeatedly(RunCallback<1>(false));
  EXPECT_CALL(*decryptor_, RegisterNewKeyCB(Decryptor::kVideo, _))
      .WillRepeatedly(SaveArg<1>(&key_added_cb_));

  InitializeAndExpectStatus(TestVideoConfig::NormalEncrypted(),
                            DECODER_ERROR_NOT_SUPPORTED);
}

TEST_F(DecryptingVideoDecoderTest, Reinitialize_Normal) {
  Initialize();
  EnterNormalDecodingState();
  Reinitialize();
}

TEST_F(DecryptingVideoDecoderTest, Reinitialize_Failure) {
  Initialize();
  EnterNormalDecodingState();

  EXPECT_CALL(*decryptor_, DeinitializeDecoder(Decryptor::kVideo));
  EXPECT_CALL(*decryptor_, InitializeVideoDecoder(_, _))
      .WillOnce(RunCallback<1>(false));

  // Reinitialize() expects the reinitialization to succeed. Call
  // InitializeAndExpectStatus() directly to test the reinitialization failure.
  InitializeAndExpectStatus(TestVideoConfig::NormalEncrypted(),
                            DECODER_ERROR_NOT_SUPPORTED);
}

// Test normal decrypt and decode case.
TEST_F(DecryptingVideoDecoderTest, DecryptAndDecode_Normal) {
  Initialize();
  EnterNormalDecodingState();
}

// Test the case where the decryptor returns error when doing decrypt and
// decode.
TEST_F(DecryptingVideoDecoderTest, DecryptAndDecode_DecodeError) {
  Initialize();

  EXPECT_CALL(*decryptor_, DecryptAndDecodeVideo(_, _))
      .WillRepeatedly(RunCallback<1>(Decryptor::kError,
                                     scoped_refptr<VideoFrame>(NULL)));

  DecodeAndExpect(encrypted_buffer_, VideoDecoder::kDecodeError);

  // After a decode error occurred, all following decode returns kDecodeError.
  DecodeAndExpect(encrypted_buffer_, VideoDecoder::kDecodeError);
}

// Test the case where the decryptor receives end-of-stream buffer.
TEST_F(DecryptingVideoDecoderTest, DecryptAndDecode_EndOfStream) {
  Initialize();
  EnterNormalDecodingState();
  EnterEndOfStreamState();
}

// Test the case where the a key is added when the decryptor is in
// kWaitingForKey state.
TEST_F(DecryptingVideoDecoderTest, KeyAdded_DuringWaitingForKey) {
  Initialize();
  EnterWaitingForKeyState();

  EXPECT_CALL(*decryptor_, DecryptAndDecodeVideo(_, _))
      .WillRepeatedly(RunCallback<1>(Decryptor::kSuccess,
                                     decoded_video_frame_));
  EXPECT_CALL(*this, FrameReady(decoded_video_frame_));
  EXPECT_CALL(*this, DecodeDone(VideoDecoder::kOk));
  key_added_cb_.Run();
  message_loop_.RunUntilIdle();
}

// Test the case where the a key is added when the decryptor is in
// kPendingDecode state.
TEST_F(DecryptingVideoDecoderTest, KeyAdded_DruingPendingDecode) {
  Initialize();
  EnterPendingDecodeState();

  EXPECT_CALL(*decryptor_, DecryptAndDecodeVideo(_, _))
      .WillRepeatedly(RunCallback<1>(Decryptor::kSuccess,
                                     decoded_video_frame_));
  EXPECT_CALL(*this, FrameReady(decoded_video_frame_));
  EXPECT_CALL(*this, DecodeDone(VideoDecoder::kOk));
  // The video decode callback is returned after the correct decryption key is
  // added.
  key_added_cb_.Run();
  base::ResetAndReturn(&pending_video_decode_cb_).Run(Decryptor::kNoKey,
                                                      null_video_frame_);
  message_loop_.RunUntilIdle();
}

// Test resetting when the decoder is in kIdle state but has not decoded any
// frame.
TEST_F(DecryptingVideoDecoderTest, Reset_DuringIdleAfterInitialization) {
  Initialize();
  Reset();
}

// Test resetting when the decoder is in kIdle state after it has decoded one
// frame.
TEST_F(DecryptingVideoDecoderTest, Reset_DuringIdleAfterDecodedOneFrame) {
  Initialize();
  EnterNormalDecodingState();
  Reset();
}

// Test resetting when the decoder is in kPendingDecode state.
TEST_F(DecryptingVideoDecoderTest, Reset_DuringPendingDecode) {
  Initialize();
  EnterPendingDecodeState();

  EXPECT_CALL(*this, DecodeDone(VideoDecoder::kAborted));

  Reset();
}

// Test resetting when the decoder is in kWaitingForKey state.
TEST_F(DecryptingVideoDecoderTest, Reset_DuringWaitingForKey) {
  Initialize();
  EnterWaitingForKeyState();

  EXPECT_CALL(*this, DecodeDone(VideoDecoder::kAborted));

  Reset();
}

// Test resetting when the decoder has hit end of stream and is in
// kDecodeFinished state.
TEST_F(DecryptingVideoDecoderTest, Reset_AfterDecodeFinished) {
  Initialize();
  EnterNormalDecodingState();
  EnterEndOfStreamState();
  Reset();
}

// Test resetting after the decoder has been reset.
TEST_F(DecryptingVideoDecoderTest, Reset_AfterReset) {
  Initialize();
  EnterNormalDecodingState();
  Reset();
  Reset();
}

// Test stopping when the decoder is in kDecryptorRequested state.
TEST_F(DecryptingVideoDecoderTest, Stop_DuringDecryptorRequested) {
  DecryptorReadyCB decryptor_ready_cb;
  EXPECT_CALL(*this, RequestDecryptorNotification(_))
      .WillOnce(SaveArg<0>(&decryptor_ready_cb));
  decoder_->Initialize(TestVideoConfig::NormalEncrypted(),
                       false,
                       NewExpectedStatusCB(DECODER_ERROR_NOT_SUPPORTED),
                       base::Bind(&DecryptingVideoDecoderTest::FrameReady,
                                  base::Unretained(this)));
  message_loop_.RunUntilIdle();
  // |decryptor_ready_cb| is saved but not called here.
  EXPECT_FALSE(decryptor_ready_cb.is_null());

  // During stop, RequestDecryptorNotification() should be called with a NULL
  // callback to cancel the |decryptor_ready_cb|.
  EXPECT_CALL(*this, RequestDecryptorNotification(IsNullCallback()))
      .WillOnce(ResetAndRunCallback(&decryptor_ready_cb,
                                    reinterpret_cast<Decryptor*>(NULL)));
  Stop();
}

// Test stopping when the decoder is in kPendingDecoderInit state.
TEST_F(DecryptingVideoDecoderTest, Stop_DuringPendingDecoderInit) {
  EXPECT_CALL(*decryptor_, InitializeVideoDecoder(_, _))
      .WillOnce(SaveArg<1>(&pending_init_cb_));

  InitializeAndExpectStatus(TestVideoConfig::NormalEncrypted(),
                            DECODER_ERROR_NOT_SUPPORTED);
  EXPECT_FALSE(pending_init_cb_.is_null());

  Stop();
}

// Test stopping when the decoder is in kIdle state but has not decoded any
// frame.
TEST_F(DecryptingVideoDecoderTest, Stop_DuringIdleAfterInitialization) {
  Initialize();
  Stop();
}

// Test stopping when the decoder is in kIdle state after it has decoded one
// frame.
TEST_F(DecryptingVideoDecoderTest, Stop_DuringIdleAfterDecodedOneFrame) {
  Initialize();
  EnterNormalDecodingState();
  Stop();
}

// Test stopping when the decoder is in kPendingDecode state.
TEST_F(DecryptingVideoDecoderTest, Stop_DuringPendingDecode) {
  Initialize();
  EnterPendingDecodeState();

  EXPECT_CALL(*this, DecodeDone(VideoDecoder::kAborted));

  Stop();
}

// Test stopping when the decoder is in kWaitingForKey state.
TEST_F(DecryptingVideoDecoderTest, Stop_DuringWaitingForKey) {
  Initialize();
  EnterWaitingForKeyState();

  EXPECT_CALL(*this, DecodeDone(VideoDecoder::kAborted));

  Stop();
}

// Test stopping when the decoder has hit end of stream and is in
// kDecodeFinished state.
TEST_F(DecryptingVideoDecoderTest, Stop_AfterDecodeFinished) {
  Initialize();
  EnterNormalDecodingState();
  EnterEndOfStreamState();
  Stop();
}

// Test stopping when there is a pending reset on the decoder.
// Reset is pending because it cannot complete when the video decode callback
// is pending.
TEST_F(DecryptingVideoDecoderTest, Stop_DuringPendingReset) {
  Initialize();
  EnterPendingDecodeState();

  EXPECT_CALL(*decryptor_, ResetDecoder(Decryptor::kVideo));
  EXPECT_CALL(*this, DecodeDone(VideoDecoder::kAborted));

  decoder_->Reset(NewExpectedClosure());
  Stop();
}

// Test stopping after the decoder has been reset.
TEST_F(DecryptingVideoDecoderTest, Stop_AfterReset) {
  Initialize();
  EnterNormalDecodingState();
  Reset();
  Stop();
}

// Test stopping after the decoder has been stopped.
TEST_F(DecryptingVideoDecoderTest, Stop_AfterStop) {
  Initialize();
  EnterNormalDecodingState();
  Stop();
  Stop();
}

}  // namespace media
