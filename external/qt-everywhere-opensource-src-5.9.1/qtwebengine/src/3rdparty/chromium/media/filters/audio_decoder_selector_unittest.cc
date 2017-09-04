// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "media/base/gmock_callback_support.h"
#include "media/base/media_util.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/filters/decoder_selector.h"
#include "media/filters/decrypting_demuxer_stream.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::InvokeWithoutArgs;
using ::testing::IsNull;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::StrictMock;

// Use anonymous namespace here to prevent the actions to be defined multiple
// times across multiple test files. Sadly we can't use static for them.
namespace {

MATCHER(EncryptedConfig, "") {
  return arg.is_encrypted();
}
MATCHER(ClearConfig, "") {
  return !arg.is_encrypted();
}

}  // namespace

namespace media {

class AudioDecoderSelectorTest : public ::testing::Test {
 public:
  enum DecryptorCapability {
    kNoCdm,        // No CDM. Only possible for clear stream.
    kNoDecryptor,  // CDM is available but Decryptor is not supported.
    kDecryptOnly,
    kDecryptAndDecode
  };

  AudioDecoderSelectorTest()
      : media_log_(new MediaLog()),
        traits_(media_log_),
        demuxer_stream_(
            new StrictMock<MockDemuxerStream>(DemuxerStream::AUDIO)),
        decoder_1_(new StrictMock<MockAudioDecoder>()),
        decoder_2_(new StrictMock<MockAudioDecoder>()) {
    all_decoders_.push_back(decoder_1_);
    all_decoders_.push_back(decoder_2_);
    // |cdm_context_| and |decryptor_| are conditionally created in
    // InitializeDecoderSelector().
  }

  ~AudioDecoderSelectorTest() { base::RunLoop().RunUntilIdle(); }

  MOCK_METHOD2(OnDecoderSelected,
               void(AudioDecoder*, DecryptingDemuxerStream*));

  void MockOnDecoderSelected(std::unique_ptr<AudioDecoder> decoder,
                             std::unique_ptr<DecryptingDemuxerStream> stream) {
    OnDecoderSelected(decoder.get(), stream.get());
    selected_decoder_ = std::move(decoder);
  }

  void UseClearStream() {
    AudioDecoderConfig clear_audio_config(kCodecVorbis, kSampleFormatPlanarF32,
                                          CHANNEL_LAYOUT_STEREO, 44100,
                                          EmptyExtraData(), Unencrypted());
    demuxer_stream_->set_audio_decoder_config(clear_audio_config);
  }

  void UseEncryptedStream() {
    AudioDecoderConfig encrypted_audio_config(
        kCodecVorbis, kSampleFormatPlanarF32, CHANNEL_LAYOUT_STEREO, 44100,
        EmptyExtraData(), AesCtrEncryptionScheme());
    demuxer_stream_->set_audio_decoder_config(encrypted_audio_config);
  }

  void InitializeDecoderSelector(DecryptorCapability decryptor_capability,
                                 int num_decoders) {
    if (decryptor_capability != kNoCdm) {
      cdm_context_.reset(new StrictMock<MockCdmContext>());

      if (decryptor_capability == kNoDecryptor) {
        EXPECT_CALL(*cdm_context_, GetDecryptor())
            .WillRepeatedly(Return(nullptr));
      } else {
        decryptor_.reset(new NiceMock<MockDecryptor>());
        EXPECT_CALL(*cdm_context_, GetDecryptor())
            .WillRepeatedly(Return(decryptor_.get()));
        EXPECT_CALL(*decryptor_, InitializeAudioDecoder(_, _))
            .WillRepeatedly(
                RunCallback<1>(decryptor_capability == kDecryptAndDecode));
      }
    }

    DCHECK_GE(all_decoders_.size(), static_cast<size_t>(num_decoders));
    all_decoders_.erase(
        all_decoders_.begin() + num_decoders, all_decoders_.end());

    decoder_selector_.reset(new AudioDecoderSelector(
        message_loop_.task_runner(), std::move(all_decoders_), media_log_));
  }

  void SelectDecoder() {
    decoder_selector_->SelectDecoder(
        &traits_, demuxer_stream_.get(), cdm_context_.get(),
        base::Bind(&AudioDecoderSelectorTest::MockOnDecoderSelected,
                   base::Unretained(this)),
        base::Bind(&AudioDecoderSelectorTest::OnDecoderOutput),
        base::Bind(&AudioDecoderSelectorTest::OnWaitingForDecryptionKey));
    base::RunLoop().RunUntilIdle();
  }

  void SelectDecoderAndDestroy() {
    SelectDecoder();

    EXPECT_CALL(*this, OnDecoderSelected(IsNull(), IsNull()));
    decoder_selector_.reset();
    base::RunLoop().RunUntilIdle();
  }

  static void OnDecoderOutput(const scoped_refptr<AudioBuffer>& output) {
    NOTREACHED();
  }

  static void OnWaitingForDecryptionKey() {
    NOTREACHED();
  }

  scoped_refptr<MediaLog> media_log_;

  // Stream traits specific to audio decoding.
  DecoderStreamTraits<DemuxerStream::AUDIO> traits_;

  // Declare |decoder_selector_| after |demuxer_stream_| and |decryptor_| since
  // |demuxer_stream_| and |decryptor_| should outlive |decoder_selector_|.
  std::unique_ptr<StrictMock<MockDemuxerStream>> demuxer_stream_;

  std::unique_ptr<StrictMock<MockCdmContext>> cdm_context_;

  // Use NiceMock since we don't care about most of calls on the decryptor, e.g.
  // RegisterNewKeyCB().
  std::unique_ptr<NiceMock<MockDecryptor>> decryptor_;

  std::unique_ptr<AudioDecoderSelector> decoder_selector_;

  StrictMock<MockAudioDecoder>* decoder_1_;
  StrictMock<MockAudioDecoder>* decoder_2_;
  ScopedVector<AudioDecoder> all_decoders_;
  std::unique_ptr<AudioDecoder> selected_decoder_;

  base::MessageLoop message_loop_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioDecoderSelectorTest);
};

// TODO(xhwang): Add kNoCdm tests for clear stream.

// The stream is not encrypted but we have no clear decoder. No decoder can be
// selected.
TEST_F(AudioDecoderSelectorTest, ClearStream_NoDecryptor_NoClearDecoder) {
  UseClearStream();
  InitializeDecoderSelector(kNoDecryptor, 0);

  EXPECT_CALL(*this, OnDecoderSelected(IsNull(), IsNull()));

  SelectDecoder();
}

// The stream is not encrypted and we have one clear decoder. The decoder
// will be selected.
TEST_F(AudioDecoderSelectorTest, ClearStream_NoDecryptor_OneClearDecoder) {
  UseClearStream();
  InitializeDecoderSelector(kNoDecryptor, 1);

  EXPECT_CALL(*decoder_1_, Initialize(ClearConfig(), _, _, _))
      .WillOnce(RunCallback<2>(true));
  EXPECT_CALL(*this, OnDecoderSelected(decoder_1_, IsNull()));

  SelectDecoder();
}

TEST_F(AudioDecoderSelectorTest,
       Destroy_ClearStream_NoDecryptor_OneClearDecoder) {
  UseClearStream();
  InitializeDecoderSelector(kNoDecryptor, 1);

  EXPECT_CALL(*decoder_1_, Initialize(ClearConfig(), _, _, _));

  SelectDecoderAndDestroy();
}

// The stream is not encrypted and we have multiple clear decoders. The first
// decoder that can decode the input stream will be selected.
TEST_F(AudioDecoderSelectorTest, ClearStream_NoDecryptor_MultipleClearDecoder) {
  UseClearStream();
  InitializeDecoderSelector(kNoDecryptor, 2);

  EXPECT_CALL(*decoder_1_, Initialize(ClearConfig(), _, _, _))
      .WillOnce(RunCallback<2>(false));
  EXPECT_CALL(*decoder_2_, Initialize(ClearConfig(), _, _, _))
      .WillOnce(RunCallback<2>(true));
  EXPECT_CALL(*this, OnDecoderSelected(decoder_2_, IsNull()));

  SelectDecoder();
}

TEST_F(AudioDecoderSelectorTest,
       Destroy_ClearStream_NoDecryptor_MultipleClearDecoder) {
  UseClearStream();
  InitializeDecoderSelector(kNoDecryptor, 2);

  EXPECT_CALL(*decoder_1_, Initialize(ClearConfig(), _, _, _))
      .WillOnce(RunCallback<2>(false));
  EXPECT_CALL(*decoder_2_, Initialize(ClearConfig(), _, _, _));

  SelectDecoderAndDestroy();
}

// There is a decryptor but the stream is not encrypted. The decoder will be
// selected.
TEST_F(AudioDecoderSelectorTest, ClearStream_HasDecryptor) {
  UseClearStream();
  InitializeDecoderSelector(kDecryptOnly, 1);

  EXPECT_CALL(*decoder_1_, Initialize(ClearConfig(), _, _, _))
      .WillOnce(RunCallback<2>(true));
  EXPECT_CALL(*this, OnDecoderSelected(decoder_1_, IsNull()));

  SelectDecoder();
}

TEST_F(AudioDecoderSelectorTest, Destroy_ClearStream_HasDecryptor) {
  UseClearStream();
  InitializeDecoderSelector(kDecryptOnly, 1);

  EXPECT_CALL(*decoder_1_, Initialize(ClearConfig(), _, _, _));

  SelectDecoderAndDestroy();
}

// The stream is encrypted and there's no decryptor. The decoder only supports
// clear streams so no decoder can be selected.
TEST_F(AudioDecoderSelectorTest, EncryptedStream_NoDecryptor_OneClearDecoder) {
  UseEncryptedStream();
  InitializeDecoderSelector(kNoDecryptor, 1);

  EXPECT_CALL(*decoder_1_, Initialize(EncryptedConfig(), _, _, _))
      .WillOnce(RunCallback<2>(false));
  EXPECT_CALL(*this, OnDecoderSelected(IsNull(), IsNull()));

  SelectDecoder();
}

TEST_F(AudioDecoderSelectorTest,
       Destroy_EncryptedStream_NoDecryptor_OneClearDecoder) {
  UseEncryptedStream();
  InitializeDecoderSelector(kNoDecryptor, 1);

  EXPECT_CALL(*decoder_1_, Initialize(EncryptedConfig(), _, _, _));

  SelectDecoderAndDestroy();
}

// The stream is encrypted and there's no decryptor. There are multiple decoders
// and the first one that supports encrypted streams is selected.
TEST_F(AudioDecoderSelectorTest, EncryptedStream_NoDecryptor_MultipleDecoders) {
  UseEncryptedStream();
  InitializeDecoderSelector(kNoDecryptor, 2);

  EXPECT_CALL(*decoder_1_, Initialize(EncryptedConfig(), _, _, _))
      .WillOnce(RunCallback<2>(false));
  EXPECT_CALL(*decoder_2_, Initialize(EncryptedConfig(), _, _, _))
      .WillOnce(RunCallback<2>(true));
  EXPECT_CALL(*this, OnDecoderSelected(decoder_2_, IsNull()));

  SelectDecoder();
}

TEST_F(AudioDecoderSelectorTest,
       Destroy_EncryptedStream_NoDecryptor_MultipleDecoders) {
  UseEncryptedStream();
  InitializeDecoderSelector(kNoDecryptor, 2);

  EXPECT_CALL(*decoder_1_, Initialize(EncryptedConfig(), _, _, _))
      .WillOnce(RunCallback<2>(false));
  EXPECT_CALL(*decoder_2_, Initialize(EncryptedConfig(), _, _, _));

  SelectDecoderAndDestroy();
}

// Decryptor can only do decryption and there's no decoder available. No decoder
// can be selected.
TEST_F(AudioDecoderSelectorTest, EncryptedStream_DecryptOnly_NoClearDecoder) {
  UseEncryptedStream();
  InitializeDecoderSelector(kDecryptOnly, 0);

  EXPECT_CALL(*this, OnDecoderSelected(IsNull(), IsNull()));

  SelectDecoder();
}

// Decryptor can do decryption-only and there's a decoder available. The decoder
// will be selected and a DecryptingDemuxerStream will be created.
TEST_F(AudioDecoderSelectorTest, EncryptedStream_DecryptOnly_OneClearDecoder) {
  UseEncryptedStream();
  InitializeDecoderSelector(kDecryptOnly, 1);

  EXPECT_CALL(*decoder_1_, Initialize(ClearConfig(), _, _, _))
      .WillOnce(RunCallback<2>(true));
  EXPECT_CALL(*this, OnDecoderSelected(decoder_1_, NotNull()));

  SelectDecoder();
}

TEST_F(AudioDecoderSelectorTest,
       Destroy_EncryptedStream_DecryptOnly_OneClearDecoder) {
  UseEncryptedStream();
  InitializeDecoderSelector(kDecryptOnly, 1);

  EXPECT_CALL(*decoder_1_, Initialize(ClearConfig(), _, _, _));

  SelectDecoderAndDestroy();
}

// Decryptor can only do decryption and there are multiple decoders available.
// The first decoder that can decode the input stream will be selected and
// a DecryptingDemuxerStream will be created.
TEST_F(AudioDecoderSelectorTest,
       EncryptedStream_DecryptOnly_MultipleClearDecoder) {
  UseEncryptedStream();
  InitializeDecoderSelector(kDecryptOnly, 2);

  EXPECT_CALL(*decoder_1_, Initialize(ClearConfig(), _, _, _))
      .WillOnce(RunCallback<2>(false));
  EXPECT_CALL(*decoder_2_, Initialize(ClearConfig(), _, _, _))
      .WillOnce(RunCallback<2>(true));
  EXPECT_CALL(*this, OnDecoderSelected(decoder_2_, NotNull()));

  SelectDecoder();
}

TEST_F(AudioDecoderSelectorTest,
       Destroy_EncryptedStream_DecryptOnly_MultipleClearDecoder) {
  UseEncryptedStream();
  InitializeDecoderSelector(kDecryptOnly, 2);

  EXPECT_CALL(*decoder_1_, Initialize(ClearConfig(), _, _, _))
      .WillOnce(RunCallback<2>(false));
  EXPECT_CALL(*decoder_2_, Initialize(ClearConfig(), _, _, _));

  SelectDecoderAndDestroy();
}

// Decryptor can do decryption and decoding.
TEST_F(AudioDecoderSelectorTest, EncryptedStream_DecryptAndDecode) {
  UseEncryptedStream();
  InitializeDecoderSelector(kDecryptAndDecode, 1);

#if !defined(OS_ANDROID)
  // A DecryptingVideoDecoder will be created and selected. The clear decoder
  // should not be touched at all. No DecryptingDemuxerStream should to be
  // created.
  EXPECT_CALL(*this, OnDecoderSelected(NotNull(), IsNull()));
#else
  // A DecryptingDemuxerStream will be created. The clear decoder will be
  // initialized and returned.
  EXPECT_CALL(*decoder_1_, Initialize(ClearConfig(), _, _, _))
      .WillOnce(RunCallback<2>(true));
  EXPECT_CALL(*this, OnDecoderSelected(NotNull(), NotNull()));
#endif

  SelectDecoder();
}

}  // namespace media
