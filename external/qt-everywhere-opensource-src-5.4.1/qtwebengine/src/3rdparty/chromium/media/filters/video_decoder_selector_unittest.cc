// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "media/base/gmock_callback_support.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/filters/decoder_selector.h"
#include "media/filters/decrypting_demuxer_stream.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::IsNull;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::StrictMock;

namespace media {

class VideoDecoderSelectorTest : public ::testing::Test {
 public:
  enum DecryptorCapability {
    kNoDecryptor,
    // Used to test Abort() during DecryptingVideoDecoder::Initialize() and
    // DecryptingDemuxerStream::Initialize(). We don't need this for normal
    // VideoDecoders since we use MockVideoDecoder.
    kHoldSetDecryptor,
    kDecryptOnly,
    kDecryptAndDecode
  };

  VideoDecoderSelectorTest()
      : demuxer_stream_(
            new StrictMock<MockDemuxerStream>(DemuxerStream::VIDEO)),
        decryptor_(new NiceMock<MockDecryptor>()),
        decoder_1_(new StrictMock<MockVideoDecoder>()),
        decoder_2_(new StrictMock<MockVideoDecoder>()) {
    all_decoders_.push_back(decoder_1_);
    all_decoders_.push_back(decoder_2_);
  }

  ~VideoDecoderSelectorTest() {
    if (selected_decoder_)
      selected_decoder_->Stop();

    message_loop_.RunUntilIdle();
  }

  MOCK_METHOD1(SetDecryptorReadyCallback, void(const media::DecryptorReadyCB&));
  MOCK_METHOD2(OnDecoderSelected,
               void(VideoDecoder*, DecryptingDemuxerStream*));

  void MockOnDecoderSelected(
      scoped_ptr<VideoDecoder> decoder,
      scoped_ptr<DecryptingDemuxerStream> stream) {
    OnDecoderSelected(decoder.get(), stream.get());
    selected_decoder_ = decoder.Pass();
  }

  void UseClearStream() {
    demuxer_stream_->set_video_decoder_config(TestVideoConfig::Normal());
  }

  void UseEncryptedStream() {
    demuxer_stream_->set_video_decoder_config(
        TestVideoConfig::NormalEncrypted());
  }

  void InitializeDecoderSelector(DecryptorCapability decryptor_capability,
                                 int num_decoders) {
    SetDecryptorReadyCB set_decryptor_ready_cb;
    if (decryptor_capability != kNoDecryptor) {
      set_decryptor_ready_cb =
          base::Bind(&VideoDecoderSelectorTest::SetDecryptorReadyCallback,
                     base::Unretained(this));
    }

    if (decryptor_capability == kDecryptOnly ||
        decryptor_capability == kDecryptAndDecode) {
      EXPECT_CALL(*this, SetDecryptorReadyCallback(_))
          .WillRepeatedly(RunCallback<0>(decryptor_.get()));

      if (decryptor_capability == kDecryptOnly) {
        EXPECT_CALL(*decryptor_, InitializeVideoDecoder(_, _))
            .WillRepeatedly(RunCallback<1>(false));
      } else {
        EXPECT_CALL(*decryptor_, InitializeVideoDecoder(_, _))
            .WillRepeatedly(RunCallback<1>(true));
      }
    } else if (decryptor_capability == kHoldSetDecryptor) {
      // Set and cancel DecryptorReadyCB but the callback is never fired.
      EXPECT_CALL(*this, SetDecryptorReadyCallback(_))
          .Times(2);
    }

    DCHECK_GE(all_decoders_.size(), static_cast<size_t>(num_decoders));
    all_decoders_.erase(
        all_decoders_.begin() + num_decoders, all_decoders_.end());

    decoder_selector_.reset(new VideoDecoderSelector(
        message_loop_.message_loop_proxy(),
        all_decoders_.Pass(),
        set_decryptor_ready_cb));
  }

  void SelectDecoder() {
    decoder_selector_->SelectDecoder(
        demuxer_stream_.get(),
        false,
        base::Bind(&VideoDecoderSelectorTest::MockOnDecoderSelected,
                   base::Unretained(this)),
        base::Bind(&VideoDecoderSelectorTest::FrameReady,
                   base::Unretained(this)));
    message_loop_.RunUntilIdle();
  }

  void SelectDecoderAndAbort() {
    SelectDecoder();

    EXPECT_CALL(*this, OnDecoderSelected(IsNull(), IsNull()));
    decoder_selector_->Abort();
    message_loop_.RunUntilIdle();
  }

  void FrameReady(const scoped_refptr<VideoFrame>& frame) {
    NOTREACHED();
  }

  // Fixture members.
  scoped_ptr<VideoDecoderSelector> decoder_selector_;
  scoped_ptr<StrictMock<MockDemuxerStream> > demuxer_stream_;
  // Use NiceMock since we don't care about most of calls on the decryptor, e.g.
  // RegisterNewKeyCB().
  scoped_ptr<NiceMock<MockDecryptor> > decryptor_;
  StrictMock<MockVideoDecoder>* decoder_1_;
  StrictMock<MockVideoDecoder>* decoder_2_;
  ScopedVector<VideoDecoder> all_decoders_;

  scoped_ptr<VideoDecoder> selected_decoder_;

  base::MessageLoop message_loop_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoDecoderSelectorTest);
};

// Note:
// In all the tests, Stop() is expected to be called on a decoder if a decoder:
// - is pending initialization and DecoderSelector::Abort() is called, or
// - has been successfully initialized.

// The stream is not encrypted but we have no clear decoder. No decoder can be
// selected.
TEST_F(VideoDecoderSelectorTest, ClearStream_NoDecryptor_NoClearDecoder) {
  UseClearStream();
  InitializeDecoderSelector(kNoDecryptor, 0);

  EXPECT_CALL(*this, OnDecoderSelected(IsNull(), IsNull()));

  SelectDecoder();
}

// The stream is not encrypted and we have one clear decoder. The decoder
// will be selected.
TEST_F(VideoDecoderSelectorTest, ClearStream_NoDecryptor_OneClearDecoder) {
  UseClearStream();
  InitializeDecoderSelector(kNoDecryptor, 1);

  EXPECT_CALL(*decoder_1_, Initialize(_, _, _, _))
      .WillOnce(RunCallback<2>(PIPELINE_OK));
  EXPECT_CALL(*this, OnDecoderSelected(decoder_1_, IsNull()));
  EXPECT_CALL(*decoder_1_, Stop());

  SelectDecoder();
}

TEST_F(VideoDecoderSelectorTest,
       Abort_ClearStream_NoDecryptor_OneClearDecoder) {
  UseClearStream();
  InitializeDecoderSelector(kNoDecryptor, 1);

  EXPECT_CALL(*decoder_1_, Initialize(_, _, _, _));
  EXPECT_CALL(*decoder_1_, Stop());

  SelectDecoderAndAbort();
}

// The stream is not encrypted and we have multiple clear decoders. The first
// decoder that can decode the input stream will be selected.
TEST_F(VideoDecoderSelectorTest, ClearStream_NoDecryptor_MultipleClearDecoder) {
  UseClearStream();
  InitializeDecoderSelector(kNoDecryptor, 2);

  EXPECT_CALL(*decoder_1_, Initialize(_, _, _, _))
      .WillOnce(RunCallback<2>(DECODER_ERROR_NOT_SUPPORTED));
  EXPECT_CALL(*decoder_2_, Initialize(_, _, _, _))
      .WillOnce(RunCallback<2>(PIPELINE_OK));
  EXPECT_CALL(*this, OnDecoderSelected(decoder_2_, IsNull()));
  EXPECT_CALL(*decoder_2_, Stop());

  SelectDecoder();
}

TEST_F(VideoDecoderSelectorTest,
       Abort_ClearStream_NoDecryptor_MultipleClearDecoder) {
  UseClearStream();
  InitializeDecoderSelector(kNoDecryptor, 2);

  EXPECT_CALL(*decoder_1_, Initialize(_, _, _, _))
      .WillOnce(RunCallback<2>(DECODER_ERROR_NOT_SUPPORTED));
  EXPECT_CALL(*decoder_2_, Initialize(_, _, _, _));
  EXPECT_CALL(*decoder_2_, Stop());

  SelectDecoderAndAbort();
}

// There is a decryptor but the stream is not encrypted. The decoder will be
// selected.
TEST_F(VideoDecoderSelectorTest, ClearStream_HasDecryptor) {
  UseClearStream();
  InitializeDecoderSelector(kDecryptOnly, 1);

  EXPECT_CALL(*decoder_1_, Initialize(_, _, _, _))
      .WillOnce(RunCallback<2>(PIPELINE_OK));
  EXPECT_CALL(*this, OnDecoderSelected(decoder_1_, IsNull()));
  EXPECT_CALL(*decoder_1_, Stop());

  SelectDecoder();
}

TEST_F(VideoDecoderSelectorTest, Abort_ClearStream_HasDecryptor) {
  UseClearStream();
  InitializeDecoderSelector(kDecryptOnly, 1);

  EXPECT_CALL(*decoder_1_, Initialize(_, _, _, _));
  EXPECT_CALL(*decoder_1_, Stop());

  SelectDecoderAndAbort();
}

// The stream is encrypted and there's no decryptor. No decoder can be selected.
TEST_F(VideoDecoderSelectorTest, EncryptedStream_NoDecryptor) {
  UseEncryptedStream();
  InitializeDecoderSelector(kNoDecryptor, 1);

  EXPECT_CALL(*this, OnDecoderSelected(IsNull(), IsNull()));

  SelectDecoder();
}

// Decryptor can only do decryption and there's no decoder available. No decoder
// can be selected.
TEST_F(VideoDecoderSelectorTest, EncryptedStream_DecryptOnly_NoClearDecoder) {
  UseEncryptedStream();
  InitializeDecoderSelector(kDecryptOnly, 0);

  EXPECT_CALL(*this, OnDecoderSelected(IsNull(), IsNull()));

  SelectDecoder();
}

TEST_F(VideoDecoderSelectorTest,
       Abort_EncryptedStream_DecryptOnly_NoClearDecoder) {
  UseEncryptedStream();
  InitializeDecoderSelector(kHoldSetDecryptor, 0);

  SelectDecoderAndAbort();
}

// Decryptor can do decryption-only and there's a decoder available. The decoder
// will be selected and a DecryptingDemuxerStream will be created.
TEST_F(VideoDecoderSelectorTest, EncryptedStream_DecryptOnly_OneClearDecoder) {
  UseEncryptedStream();
  InitializeDecoderSelector(kDecryptOnly, 1);

  EXPECT_CALL(*decoder_1_, Initialize(_, _, _, _))
      .WillOnce(RunCallback<2>(PIPELINE_OK));
  EXPECT_CALL(*this, OnDecoderSelected(decoder_1_, NotNull()));
  EXPECT_CALL(*decoder_1_, Stop());

  SelectDecoder();
}

TEST_F(VideoDecoderSelectorTest,
       Abort_EncryptedStream_DecryptOnly_OneClearDecoder) {
  UseEncryptedStream();
  InitializeDecoderSelector(kDecryptOnly, 1);

  EXPECT_CALL(*decoder_1_, Initialize(_, _, _, _));
  EXPECT_CALL(*decoder_1_, Stop());

  SelectDecoderAndAbort();
}

// Decryptor can only do decryption and there are multiple decoders available.
// The first decoder that can decode the input stream will be selected and
// a DecryptingDemuxerStream will be created.
TEST_F(VideoDecoderSelectorTest,
       EncryptedStream_DecryptOnly_MultipleClearDecoder) {
  UseEncryptedStream();
  InitializeDecoderSelector(kDecryptOnly, 2);

  EXPECT_CALL(*decoder_1_, Initialize(_, _, _, _))
      .WillOnce(RunCallback<2>(DECODER_ERROR_NOT_SUPPORTED));
  EXPECT_CALL(*decoder_2_, Initialize(_, _, _, _))
      .WillOnce(RunCallback<2>(PIPELINE_OK));
  EXPECT_CALL(*this, OnDecoderSelected(decoder_2_, NotNull()));
  EXPECT_CALL(*decoder_2_, Stop());

  SelectDecoder();
}

TEST_F(VideoDecoderSelectorTest,
       Abort_EncryptedStream_DecryptOnly_MultipleClearDecoder) {
  UseEncryptedStream();
  InitializeDecoderSelector(kDecryptOnly, 2);

  EXPECT_CALL(*decoder_1_, Initialize(_, _, _, _))
      .WillOnce(RunCallback<2>(DECODER_ERROR_NOT_SUPPORTED));
  EXPECT_CALL(*decoder_2_, Initialize(_, _, _, _));
  EXPECT_CALL(*decoder_2_, Stop());

  SelectDecoderAndAbort();
}

// Decryptor can do decryption and decoding. A DecryptingVideoDecoder will be
// created and selected. The clear decoders should not be touched at all.
// No DecryptingDemuxerStream should to be created.
TEST_F(VideoDecoderSelectorTest, EncryptedStream_DecryptAndDecode) {
  UseEncryptedStream();
  InitializeDecoderSelector(kDecryptAndDecode, 1);

  EXPECT_CALL(*this, OnDecoderSelected(NotNull(), IsNull()));

  SelectDecoder();
}

TEST_F(VideoDecoderSelectorTest, Abort_EncryptedStream_DecryptAndDecode) {
  UseEncryptedStream();
  InitializeDecoderSelector(kHoldSetDecryptor, 1);

  SelectDecoderAndAbort();
}

}  // namespace media
