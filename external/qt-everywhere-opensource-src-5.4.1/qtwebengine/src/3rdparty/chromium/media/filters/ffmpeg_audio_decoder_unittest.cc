// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "media/base/audio_buffer.h"
#include "media/base/decoder_buffer.h"
#include "media/base/mock_filters.h"
#include "media/base/test_data_util.h"
#include "media/base/test_helpers.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/ffmpeg_audio_decoder.h"
#include "media/filters/ffmpeg_glue.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::StrictMock;

namespace media {

class FFmpegAudioDecoderTest : public testing::Test {
 public:
  FFmpegAudioDecoderTest()
      : decoder_(new FFmpegAudioDecoder(message_loop_.message_loop_proxy(),
                                        LogCB())),
        pending_decode_(false),
        pending_reset_(false),
        last_decode_status_(AudioDecoder::kOk) {
    FFmpegGlue::InitializeFFmpeg();

    vorbis_extradata_ = ReadTestDataFile("vorbis-extradata");

    // Refer to media/test/data/README for details on vorbis test data.
    for (int i = 0; i < 4; ++i) {
      scoped_refptr<DecoderBuffer> buffer =
          ReadTestDataFile(base::StringPrintf("vorbis-packet-%d", i));

      if (i < 3) {
        buffer->set_timestamp(base::TimeDelta());
      } else {
        buffer->set_timestamp(base::TimeDelta::FromMicroseconds(2902));
      }

      buffer->set_duration(base::TimeDelta());
      encoded_audio_.push_back(buffer);
    }

    // Push in an EOS buffer.
    encoded_audio_.push_back(DecoderBuffer::CreateEOSBuffer());

    Initialize();
  }

  virtual ~FFmpegAudioDecoderTest() {
    EXPECT_FALSE(pending_decode_);
    EXPECT_FALSE(pending_reset_);
  }

  void Initialize() {
    AudioDecoderConfig config(kCodecVorbis,
                              kSampleFormatPlanarF32,
                              CHANNEL_LAYOUT_STEREO,
                              44100,
                              vorbis_extradata_->data(),
                              vorbis_extradata_->data_size(),
                              false);  // Not encrypted.
    decoder_->Initialize(config,
                         NewExpectedStatusCB(PIPELINE_OK),
                         base::Bind(&FFmpegAudioDecoderTest::OnDecoderOutput,
                                    base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  void SatisfyPendingDecode() {
    base::RunLoop().RunUntilIdle();
  }

  void Decode() {
    pending_decode_ = true;
    scoped_refptr<DecoderBuffer> buffer(encoded_audio_.front());
    encoded_audio_.pop_front();
    decoder_->Decode(buffer,
                     base::Bind(&FFmpegAudioDecoderTest::DecodeFinished,
                                base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(pending_decode_);
    EXPECT_EQ(AudioDecoder::kOk, last_decode_status_);
  }

  void Reset() {
    pending_reset_ = true;
    decoder_->Reset(base::Bind(
        &FFmpegAudioDecoderTest::ResetFinished, base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  void Stop() {
    decoder_->Stop();
    base::RunLoop().RunUntilIdle();
  }

  void OnDecoderOutput(const scoped_refptr<AudioBuffer>& buffer) {
    EXPECT_FALSE(buffer->end_of_stream());
    decoded_audio_.push_back(buffer);
  }

  void DecodeFinished(AudioDecoder::Status status) {
    EXPECT_TRUE(pending_decode_);
    pending_decode_ = false;

    last_decode_status_ = status;
  }

  void ResetFinished() {
    EXPECT_TRUE(pending_reset_);
    // Reset should always finish after Decode.
    EXPECT_FALSE(pending_decode_);

    pending_reset_ = false;
  }

  void ExpectDecodedAudio(size_t i, int64 timestamp, int64 duration) {
    EXPECT_LT(i, decoded_audio_.size());
    EXPECT_EQ(timestamp, decoded_audio_[i]->timestamp().InMicroseconds());
    EXPECT_EQ(duration, decoded_audio_[i]->duration().InMicroseconds());
  }

  base::MessageLoop message_loop_;
  scoped_ptr<FFmpegAudioDecoder> decoder_;
  bool pending_decode_;
  bool pending_reset_;

  scoped_refptr<DecoderBuffer> vorbis_extradata_;

  std::deque<scoped_refptr<DecoderBuffer> > encoded_audio_;
  std::deque<scoped_refptr<AudioBuffer> > decoded_audio_;
  AudioDecoder::Status last_decode_status_;
};

TEST_F(FFmpegAudioDecoderTest, Initialize) {
  AudioDecoderConfig config(kCodecVorbis,
                            kSampleFormatPlanarF32,
                            CHANNEL_LAYOUT_STEREO,
                            44100,
                            vorbis_extradata_->data(),
                            vorbis_extradata_->data_size(),
                            false);  // Not encrypted.
  Stop();
}

TEST_F(FFmpegAudioDecoderTest, ProduceAudioSamples) {
  // Vorbis requires N+1 packets to produce audio data for N packets.
  //
  // This will should result in the demuxer receiving three reads for two
  // requests to produce audio samples.
  Decode();
  Decode();
  Decode();
  Decode();

  ASSERT_EQ(3u, decoded_audio_.size());
  ExpectDecodedAudio(0, 0, 2902);
  ExpectDecodedAudio(1, 2902, 13061);
  ExpectDecodedAudio(2, 15963, 23219);

  // Call one more time with EOS.
  Decode();
  ASSERT_EQ(3u, decoded_audio_.size());
  Stop();
}

TEST_F(FFmpegAudioDecoderTest, PendingDecode_Stop) {
  Decode();
  Stop();
  SatisfyPendingDecode();
}

TEST_F(FFmpegAudioDecoderTest, PendingDecode_Reset) {
  Decode();
  Reset();
  SatisfyPendingDecode();
  Stop();
}

TEST_F(FFmpegAudioDecoderTest, PendingDecode_ResetStop) {
  Decode();
  Reset();
  Stop();
  SatisfyPendingDecode();
}

}  // namespace media
