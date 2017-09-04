// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/bind.h"
#include "base/format_macros.h"
#include "base/macros.h"
#include "base/md5.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/sys_byteorder.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_hash.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_util.h"
#include "media/base/test_data_util.h"
#include "media/base/test_helpers.h"
#include "media/base/timestamp_constants.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/audio_file_reader.h"
#include "media/filters/ffmpeg_audio_decoder.h"
#include "media/filters/in_memory_url_protocol.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#include "media/base/android/media_codec_util.h"
#include "media/filters/android/media_codec_audio_decoder.h"

#if defined(USE_PROPRIETARY_CODECS)
#include "media/formats/mpeg/adts_stream_parser.h"
#endif

// Helper macro to skip the test if MediaCodec is not available.
#define SKIP_TEST_IF_NO_MEDIA_CODEC()                                \
  do {                                                               \
    if (GetParam().decoder_type == MEDIA_CODEC) {                    \
      if (!MediaCodecUtil::IsMediaCodecAvailable()) {                \
        VLOG(0) << "Could not run test - no MediaCodec on device.";  \
        return;                                                      \
      }                                                              \
      if (GetParam().codec == kCodecOpus &&                          \
          base::android::BuildInfo::GetInstance()->sdk_int() < 21) { \
        VLOG(0) << "Could not run test - Opus is not supported";     \
        return;                                                      \
      }                                                              \
    }                                                                \
  } while (0)
#else
#define SKIP_TEST_IF_NO_MEDIA_CODEC() \
  do {                                \
  } while (0)
#endif  // !defined(OS_ANDROID)

namespace media {

// The number of packets to read and then decode from each file.
static const size_t kDecodeRuns = 3;

enum AudioDecoderType {
  FFMPEG,
#if defined(OS_ANDROID)
  MEDIA_CODEC,
#endif
};

struct DecodedBufferExpectations {
  const int64_t timestamp;
  const int64_t duration;
  const char* hash;
};

struct DecoderTestData {
  const AudioDecoderType decoder_type;
  const AudioCodec codec;
  const char* filename;
  const DecodedBufferExpectations* expectations;
  const int first_packet_pts;
  const int samples_per_second;
  const ChannelLayout channel_layout;
};

// Tells gtest how to print our DecoderTestData structure.
std::ostream& operator<<(std::ostream& os, const DecoderTestData& data) {
  return os << data.filename;
}

// Marks negative timestamp buffers for discard or transfers FFmpeg's built in
// discard metadata in favor of setting DiscardPadding on the DecoderBuffer.
// Allows better testing of AudioDiscardHelper usage.
static void SetDiscardPadding(AVPacket* packet,
                              const scoped_refptr<DecoderBuffer> buffer,
                              double samples_per_second) {
  // Discard negative timestamps.
  if (buffer->timestamp() + buffer->duration() < base::TimeDelta()) {
    buffer->set_discard_padding(
        std::make_pair(kInfiniteDuration, base::TimeDelta()));
    return;
  }
  if (buffer->timestamp() < base::TimeDelta()) {
    buffer->set_discard_padding(
        std::make_pair(-buffer->timestamp(), base::TimeDelta()));
    return;
  }

  // If the timestamp is positive, try to use FFmpeg's discard data.
  int skip_samples_size = 0;
  const uint32_t* skip_samples_ptr =
      reinterpret_cast<const uint32_t*>(av_packet_get_side_data(
          packet, AV_PKT_DATA_SKIP_SAMPLES, &skip_samples_size));
  if (skip_samples_size < 4)
    return;
  buffer->set_discard_padding(std::make_pair(
      base::TimeDelta::FromSecondsD(base::ByteSwapToLE32(*skip_samples_ptr) /
                                    samples_per_second),
      base::TimeDelta()));
}

class AudioDecoderTest : public testing::TestWithParam<DecoderTestData> {
 public:
  AudioDecoderTest()
      : pending_decode_(false),
        pending_reset_(false),
        last_decode_status_(DecodeStatus::DECODE_ERROR) {
    switch (GetParam().decoder_type) {
      case FFMPEG:
        decoder_.reset(new FFmpegAudioDecoder(message_loop_.task_runner(),
                                              new MediaLog()));
        break;
#if defined(OS_ANDROID)
      case MEDIA_CODEC:
        decoder_.reset(new MediaCodecAudioDecoder(message_loop_.task_runner()));
        break;
#endif
    }
  }

  virtual ~AudioDecoderTest() {
    EXPECT_FALSE(pending_decode_);
    EXPECT_FALSE(pending_reset_);
  }

 protected:
  void DecodeBuffer(const scoped_refptr<DecoderBuffer>& buffer) {
    ASSERT_FALSE(pending_decode_);
    pending_decode_ = true;
    last_decode_status_ = DecodeStatus::DECODE_ERROR;

    base::RunLoop run_loop;
    decoder_->Decode(
        buffer, base::Bind(&AudioDecoderTest::DecodeFinished,
                           base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    ASSERT_FALSE(pending_decode_);
  }

  void SendEndOfStream() { DecodeBuffer(DecoderBuffer::CreateEOSBuffer()); }

  void Initialize() {
    // Load the test data file.
    data_ = ReadTestDataFile(GetParam().filename);
    protocol_.reset(
        new InMemoryUrlProtocol(data_->data(), data_->data_size(), false));
    reader_.reset(new AudioFileReader(protocol_.get()));
    ASSERT_TRUE(reader_->OpenDemuxerForTesting());

    // Load the first packet and check its timestamp.
    AVPacket packet;
    ASSERT_TRUE(reader_->ReadPacketForTesting(&packet));
    EXPECT_EQ(GetParam().first_packet_pts, packet.pts);
    start_timestamp_ = ConvertFromTimeBase(
        reader_->GetAVStreamForTesting()->time_base, packet.pts);

    // Seek back to the beginning.
    ASSERT_TRUE(reader_->SeekForTesting(start_timestamp_));

    AudioDecoderConfig config;
    ASSERT_TRUE(AVCodecContextToAudioDecoderConfig(
        reader_->codec_context_for_testing(), Unencrypted(), &config));

#if defined(OS_ANDROID) && defined(USE_PROPRIETARY_CODECS)
    // MEDIA_CODEC type requires config->extra_data() for AAC codec. For ADTS
    // streams we need to extract it with a separate procedure.
    if (GetParam().decoder_type == MEDIA_CODEC &&
        GetParam().codec == kCodecAAC && config.extra_data().empty()) {
      int sample_rate;
      ChannelLayout channel_layout;
      std::vector<uint8_t> extra_data;
      ASSERT_GT(ADTSStreamParser().ParseFrameHeader(
                    packet.data, packet.size, nullptr, &sample_rate,
                    &channel_layout, nullptr, nullptr, &extra_data),
                0);
      config.Initialize(kCodecAAC, kSampleFormatS16, channel_layout,
                        sample_rate, extra_data, Unencrypted(),
                        base::TimeDelta(), 0);
      ASSERT_FALSE(config.extra_data().empty());
    }
#endif

    av_packet_unref(&packet);

    EXPECT_EQ(GetParam().codec, config.codec());
    EXPECT_EQ(GetParam().samples_per_second, config.samples_per_second());
    EXPECT_EQ(GetParam().channel_layout, config.channel_layout());

    InitializeDecoder(config);
  }

  void InitializeDecoder(const AudioDecoderConfig& config) {
    InitializeDecoderWithResult(config, true);
  }

  void InitializeDecoderWithResult(const AudioDecoderConfig& config,
                                   bool success) {
    decoder_->Initialize(
        config, nullptr, NewExpectedBoolCB(success),
        base::Bind(&AudioDecoderTest::OnDecoderOutput, base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  void Decode() {
    AVPacket packet;
    ASSERT_TRUE(reader_->ReadPacketForTesting(&packet));

    // Split out packet metadata before making a copy.
    av_packet_split_side_data(&packet);

    scoped_refptr<DecoderBuffer> buffer =
        DecoderBuffer::CopyFrom(packet.data, packet.size);
    buffer->set_timestamp(ConvertFromTimeBase(
        reader_->GetAVStreamForTesting()->time_base, packet.pts));
    buffer->set_duration(ConvertFromTimeBase(
        reader_->GetAVStreamForTesting()->time_base, packet.duration));
    if (packet.flags & AV_PKT_FLAG_KEY)
      buffer->set_is_key_frame(true);

    // Don't set discard padding for Opus, it already has discard behavior set
    // based on the codec delay in the AudioDecoderConfig.
    if (GetParam().decoder_type == FFMPEG && GetParam().codec != kCodecOpus)
      SetDiscardPadding(&packet, buffer, GetParam().samples_per_second);

    // DecodeBuffer() shouldn't need the original packet since it uses the copy.
    av_packet_unref(&packet);
    DecodeBuffer(buffer);
  }

  void Reset() {
    ASSERT_FALSE(pending_reset_);
    pending_reset_ = true;
    decoder_->Reset(
        base::Bind(&AudioDecoderTest::ResetFinished, base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
    ASSERT_FALSE(pending_reset_);
  }

  void Seek(base::TimeDelta seek_time) {
    Reset();
    decoded_audio_.clear();
    ASSERT_TRUE(reader_->SeekForTesting(seek_time));
  }

  void OnDecoderOutput(const scoped_refptr<AudioBuffer>& buffer) {
    EXPECT_FALSE(buffer->end_of_stream());
    decoded_audio_.push_back(buffer);
  }

  void DecodeFinished(const base::Closure& quit_closure, DecodeStatus status) {
    EXPECT_TRUE(pending_decode_);
    EXPECT_FALSE(pending_reset_);
    pending_decode_ = false;
    last_decode_status_ = status;
    quit_closure.Run();
  }

  void ResetFinished() {
    EXPECT_TRUE(pending_reset_);
    EXPECT_FALSE(pending_decode_);
    pending_reset_ = false;
  }

  // Generates an MD5 hash of the audio signal.  Should not be used for checks
  // across platforms as audio varies slightly across platforms.
  std::string GetDecodedAudioMD5(size_t i) {
    CHECK_LT(i, decoded_audio_.size());
    const scoped_refptr<AudioBuffer>& buffer = decoded_audio_[i];

    std::unique_ptr<AudioBus> output =
        AudioBus::Create(buffer->channel_count(), buffer->frame_count());
    buffer->ReadFrames(buffer->frame_count(), 0, 0, output.get());

    base::MD5Context context;
    base::MD5Init(&context);
    for (int ch = 0; ch < output->channels(); ++ch) {
      base::MD5Update(
          &context,
          base::StringPiece(reinterpret_cast<char*>(output->channel(ch)),
                            output->frames() * sizeof(*output->channel(ch))));
    }
    base::MD5Digest digest;
    base::MD5Final(&digest, &context);
    return base::MD5DigestToBase16(digest);
  }

  // Android MediaCodec returns wrong timestamps (shifted one frame forward)
  // for AAC before Android L. Skip the timestamp check in this situation.
  bool SkipBufferTimestampCheck() const {
#if defined(OS_ANDROID)
    return (base::android::BuildInfo::GetInstance()->sdk_int() < 21) &&
           GetParam().decoder_type == MEDIA_CODEC &&
           GetParam().codec == kCodecAAC;
#else
    return false;
#endif
  }

  void ExpectDecodedAudio(size_t i, const std::string& exact_hash) {
    CHECK_LT(i, decoded_audio_.size());
    const scoped_refptr<AudioBuffer>& buffer = decoded_audio_[i];

    const DecodedBufferExpectations& sample_info = GetParam().expectations[i];

    // Android MediaCodec returns wrong timestamps (shifted one frame forward)
    // for AAC before Android L. Ignore sample_info.timestamp in this situation.
    if (!SkipBufferTimestampCheck())
      EXPECT_EQ(sample_info.timestamp, buffer->timestamp().InMicroseconds());
    EXPECT_EQ(sample_info.duration, buffer->duration().InMicroseconds());
    EXPECT_FALSE(buffer->end_of_stream());

    std::unique_ptr<AudioBus> output =
        AudioBus::Create(buffer->channel_count(), buffer->frame_count());
    buffer->ReadFrames(buffer->frame_count(), 0, 0, output.get());

    // Generate a lossy hash of the audio used for comparison across platforms.
    AudioHash audio_hash;
    audio_hash.Update(output.get(), output->frames());
    EXPECT_TRUE(audio_hash.IsEquivalent(sample_info.hash, 0.02))
        << "Audio hashes differ. Expected: " << sample_info.hash
        << " Actual: " << audio_hash.ToString();

    if (!exact_hash.empty()) {
      EXPECT_EQ(exact_hash, GetDecodedAudioMD5(i));

      // Verify different hashes are being generated.  None of our test data
      // files have audio that hashes out exactly the same.
      if (i > 0)
        EXPECT_NE(exact_hash, GetDecodedAudioMD5(i - 1));
    }
  }

  size_t decoded_audio_size() const { return decoded_audio_.size(); }
  base::TimeDelta start_timestamp() const { return start_timestamp_; }
  const scoped_refptr<AudioBuffer>& decoded_audio(size_t i) {
    return decoded_audio_[i];
  }
  DecodeStatus last_decode_status() const { return last_decode_status_; }

 private:
  base::MessageLoop message_loop_;
  scoped_refptr<DecoderBuffer> data_;
  std::unique_ptr<InMemoryUrlProtocol> protocol_;
  std::unique_ptr<AudioFileReader> reader_;

  std::unique_ptr<AudioDecoder> decoder_;
  bool pending_decode_;
  bool pending_reset_;
  DecodeStatus last_decode_status_;

  std::deque<scoped_refptr<AudioBuffer> > decoded_audio_;
  base::TimeDelta start_timestamp_;

  DISALLOW_COPY_AND_ASSIGN(AudioDecoderTest);
};

class FFmpegAudioDecoderBehavioralTest : public AudioDecoderTest {};

TEST_P(AudioDecoderTest, Initialize) {
  SKIP_TEST_IF_NO_MEDIA_CODEC();
  ASSERT_NO_FATAL_FAILURE(Initialize());
}

// Verifies decode audio as well as the Decode() -> Reset() sequence.
TEST_P(AudioDecoderTest, ProduceAudioSamples) {
  SKIP_TEST_IF_NO_MEDIA_CODEC();
  ASSERT_NO_FATAL_FAILURE(Initialize());

  // Run the test multiple times with a seek back to the beginning in between.
  std::vector<std::string> decoded_audio_md5_hashes;
  for (int i = 0; i < 2; ++i) {
    // Run decoder until we get at least |kDecodeRuns| output buffers.
    // Keeping Decode() in a loop seems to be the simplest way to guarantee that
    // the predefined number of output buffers are produced without draining
    // (i.e. decoding EOS).
    do {
      Decode();
      ASSERT_EQ(last_decode_status(), DecodeStatus::OK);
    } while (decoded_audio_size() < kDecodeRuns);

    // With MediaCodecAudioDecoder the output buffers might appear after
    // some delay. Since we keep decoding in a loop, the number of output
    // buffers when they eventually appear might exceed |kDecodeRuns|.
    ASSERT_LE(kDecodeRuns, decoded_audio_size());

    // On the first pass record the exact MD5 hash for each decoded buffer.
    if (i == 0) {
      for (size_t j = 0; j < kDecodeRuns; ++j)
        decoded_audio_md5_hashes.push_back(GetDecodedAudioMD5(j));
    }

    // On the first pass verify the basic audio hash and sample info.  On the
    // second, verify the exact MD5 sum for each packet.  It shouldn't change.
    for (size_t j = 0; j < kDecodeRuns; ++j) {
      SCOPED_TRACE(base::StringPrintf("i = %d, j = %" PRIuS, i, j));
      ExpectDecodedAudio(j, i == 0 ? "" : decoded_audio_md5_hashes[j]);
    }

    SendEndOfStream();

    // Seek back to the beginning.  Calls Reset() on the decoder.
    Seek(start_timestamp());
  }
}

TEST_P(AudioDecoderTest, Decode) {
  SKIP_TEST_IF_NO_MEDIA_CODEC();
  ASSERT_NO_FATAL_FAILURE(Initialize());
  Decode();
  EXPECT_EQ(DecodeStatus::OK, last_decode_status());
}

TEST_P(AudioDecoderTest, Reset) {
  SKIP_TEST_IF_NO_MEDIA_CODEC();
  ASSERT_NO_FATAL_FAILURE(Initialize());
  Reset();
}

TEST_P(AudioDecoderTest, NoTimestamp) {
  SKIP_TEST_IF_NO_MEDIA_CODEC();
  ASSERT_NO_FATAL_FAILURE(Initialize());
  scoped_refptr<DecoderBuffer> buffer(new DecoderBuffer(0));
  buffer->set_timestamp(kNoTimestamp);
  DecodeBuffer(buffer);
  EXPECT_EQ(DecodeStatus::DECODE_ERROR, last_decode_status());
}

const DecodedBufferExpectations kBearOpusExpectations[] = {
    {500, 3500, "-0.26,0.87,1.36,0.84,-0.30,-1.22,"},
    {4000, 10000, "0.09,0.23,0.21,0.03,-0.17,-0.24,"},
    {14000, 10000, "0.10,0.24,0.23,0.04,-0.14,-0.23,"},
};

#if defined(OS_ANDROID)
#if defined(USE_PROPRIETARY_CODECS)
const DecodedBufferExpectations kSfxAdtsMcExpectations[] = {
    {0, 23219, "-1.80,-1.49,-0.23,1.11,1.54,-0.11,"},
    {23219, 23219, "-1.90,-1.53,-0.15,1.28,1.23,-0.33,"},
    {46439, 23219, "0.54,0.88,2.19,3.54,3.24,1.63,"},
};

const DecodedBufferExpectations kHeAacMcExpectations[] = {
    {0, 42666, "-1.76,-0.12,1.72,1.45,0.10,-1.32,"},
    {42666, 42666, "-1.78,-0.13,1.70,1.44,0.09,-1.32,"},
    {85333, 42666, "-1.78,-0.13,1.70,1.44,0.08,-1.33,"},
};
#endif

const DecoderTestData kMediaCodecTests[] = {
    {MEDIA_CODEC, kCodecOpus, "bear-opus.ogg", kBearOpusExpectations, 24, 48000,
     CHANNEL_LAYOUT_STEREO},
#if defined(USE_PROPRIETARY_CODECS)
    {MEDIA_CODEC, kCodecAAC, "sfx.adts", kSfxAdtsMcExpectations, 0, 44100,
     CHANNEL_LAYOUT_MONO},
    {MEDIA_CODEC, kCodecAAC, "bear-audio-implicit-he-aac-v2.aac",
     kHeAacMcExpectations, 0, 24000, CHANNEL_LAYOUT_MONO},
#endif
};

INSTANTIATE_TEST_CASE_P(MediaCodecAudioDecoderTest,
                        AudioDecoderTest,
                        testing::ValuesIn(kMediaCodecTests));
#endif  // defined(OS_ANDROID)

#if defined(USE_PROPRIETARY_CODECS)
const DecodedBufferExpectations kSfxMp3Expectations[] = {
    {0, 1065, "2.81,3.99,4.53,4.10,3.08,2.46,"},
    {1065, 26122, "-3.81,-4.14,-3.90,-3.36,-3.03,-3.23,"},
    {27188, 26122, "4.24,3.95,4.22,4.78,5.13,4.93,"},
};

const DecodedBufferExpectations kSfxAdtsExpectations[] = {
    {0, 23219, "-1.90,-1.53,-0.15,1.28,1.23,-0.33,"},
    {23219, 23219, "0.54,0.88,2.19,3.54,3.24,1.63,"},
    {46439, 23219, "1.42,1.69,2.95,4.23,4.02,2.36,"},
};
#endif

const DecodedBufferExpectations kSfxFlacExpectations[] = {
    {0, 104489, "-2.42,-1.12,0.71,1.70,1.09,-0.68,"},
    {104489, 104489, "-1.99,-0.67,1.18,2.19,1.60,-0.16,"},
    {208979, 79433, "2.84,2.70,3.23,4.06,4.59,4.44,"},
};

const DecodedBufferExpectations kSfxWaveExpectations[] = {
    {0, 23219, "-1.23,-0.87,0.47,1.85,1.88,0.29,"},
    {23219, 23219, "0.75,1.10,2.43,3.78,3.53,1.93,"},
    {46439, 23219, "1.27,1.56,2.83,4.13,3.87,2.23,"},
};

const DecodedBufferExpectations kFourChannelWaveExpectations[] = {
    {0, 11609, "-1.68,1.68,0.89,-3.45,1.52,1.15,"},
    {11609, 11609, "43.26,9.06,18.27,35.98,19.45,7.46,"},
    {23219, 11609, "36.37,9.45,16.04,27.67,18.81,10.15,"},
};

const DecodedBufferExpectations kSfxOggExpectations[] = {
    {0, 13061, "-0.33,1.25,2.86,3.26,2.09,0.14,"},
    {13061, 23219, "-2.79,-2.42,-1.06,0.33,0.93,-0.64,"},
    {36281, 23219, "-1.19,-0.80,0.57,1.97,2.08,0.51,"},
};

const DecodedBufferExpectations kBearOgvExpectations[] = {
    {0, 13061, "-1.25,0.10,2.11,2.29,1.50,-0.68,"},
    {13061, 23219, "-1.80,-1.41,-0.13,1.30,1.65,0.01,"},
    {36281, 23219, "-1.43,-1.25,0.11,1.29,1.86,0.14,"},
};

#if defined(OPUS_FIXED_POINT)
const DecodedBufferExpectations kSfxOpusExpectations[] = {
    {0, 13500, "-2.70,-1.41,-0.78,-1.27,-2.56,-3.73,"},
    {13500, 20000, "5.48,5.93,6.05,5.83,5.54,5.46,"},
    {33500, 20000, "-3.44,-3.34,-3.57,-4.11,-4.74,-5.13,"},
};
#else
const DecodedBufferExpectations kSfxOpusExpectations[] = {
    {0, 13500, "-2.70,-1.41,-0.78,-1.27,-2.56,-3.73,"},
    {13500, 20000, "5.48,5.93,6.04,5.83,5.54,5.45,"},
    {33500, 20000, "-3.45,-3.35,-3.57,-4.12,-4.74,-5.14,"},
};
#endif

const DecoderTestData kFFmpegTests[] = {
#if defined(USE_PROPRIETARY_CODECS)
    {FFMPEG, kCodecMP3, "sfx.mp3", kSfxMp3Expectations, 0, 44100,
     CHANNEL_LAYOUT_MONO},
    {FFMPEG, kCodecAAC, "sfx.adts", kSfxAdtsExpectations, 0, 44100,
     CHANNEL_LAYOUT_MONO},
#endif
    {FFMPEG, kCodecFLAC, "sfx.flac", kSfxFlacExpectations, 0, 44100,
     CHANNEL_LAYOUT_MONO},
    {FFMPEG, kCodecPCM, "sfx_f32le.wav", kSfxWaveExpectations, 0, 44100,
     CHANNEL_LAYOUT_MONO},
    {FFMPEG, kCodecPCM, "4ch.wav", kFourChannelWaveExpectations, 0, 44100,
     CHANNEL_LAYOUT_QUAD},
    {FFMPEG, kCodecVorbis, "sfx.ogg", kSfxOggExpectations, 0, 44100,
     CHANNEL_LAYOUT_MONO},
    // Note: bear.ogv is incorrectly muxed such that valid samples are given
    // negative timestamps, this marks them for discard per the ogg vorbis spec.
    {FFMPEG, kCodecVorbis, "bear.ogv", kBearOgvExpectations, -704, 44100,
     CHANNEL_LAYOUT_STEREO},
    {FFMPEG, kCodecOpus, "sfx-opus.ogg", kSfxOpusExpectations, -312, 48000,
     CHANNEL_LAYOUT_MONO},
    {FFMPEG, kCodecOpus, "bear-opus.ogg", kBearOpusExpectations, 24, 48000,
     CHANNEL_LAYOUT_STEREO},
};

// Dummy data for behavioral tests.
const DecoderTestData kFFmpegBehavioralTest[] = {
    {FFMPEG, kUnknownAudioCodec, "", NULL, 0, 0, CHANNEL_LAYOUT_NONE},
};

INSTANTIATE_TEST_CASE_P(FFmpegAudioDecoderTest,
                        AudioDecoderTest,
                        testing::ValuesIn(kFFmpegTests));
INSTANTIATE_TEST_CASE_P(FFmpegAudioDecoderBehavioralTest,
                        FFmpegAudioDecoderBehavioralTest,
                        testing::ValuesIn(kFFmpegBehavioralTest));

}  // namespace media
