// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/audio_file_reader.h"

#include <memory>

#include "base/logging.h"
#include "base/macros.h"
#include "base/md5.h"
#include "build/build_config.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_hash.h"
#include "media/base/decoder_buffer.h"
#include "media/base/test_data_util.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/in_memory_url_protocol.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class AudioFileReaderTest : public testing::Test {
 public:
  AudioFileReaderTest() : packet_verification_disabled_(false) {}
  ~AudioFileReaderTest() override {}

  void Initialize(const char* filename) {
    data_ = ReadTestDataFile(filename);
    protocol_.reset(
        new InMemoryUrlProtocol(data_->data(), data_->data_size(), false));
    reader_.reset(new AudioFileReader(protocol_.get()));
  }

  // Reads and the entire file provided to Initialize().
  void ReadAndVerify(const char* expected_audio_hash, int expected_frames) {
    std::unique_ptr<AudioBus> decoded_audio_data =
        AudioBus::Create(reader_->channels(), reader_->GetNumberOfFrames());
    int actual_frames = reader_->Read(decoded_audio_data.get());
    ASSERT_LE(actual_frames, decoded_audio_data->frames());
    ASSERT_EQ(expected_frames, actual_frames);

    AudioHash audio_hash;
    audio_hash.Update(decoded_audio_data.get(), actual_frames);
    EXPECT_EQ(expected_audio_hash, audio_hash.ToString());
  }

  // Verify packets are consistent across demuxer runs.  Reads the first few
  // packets and then seeks back to the start timestamp and verifies that the
  // hashes match on the packets just read.
  void VerifyPackets() {
    const int kReads = 3;
    const int kTestPasses = 2;

    AVPacket packet;
    base::TimeDelta start_timestamp;
    std::vector<std::string> packet_md5_hashes_;
    for (int i = 0; i < kTestPasses; ++i) {
      for (int j = 0; j < kReads; ++j) {
        ASSERT_TRUE(reader_->ReadPacketForTesting(&packet));

        // Remove metadata from the packet data section before hashing.
        av_packet_split_side_data(&packet);

        // On the first pass save the MD5 hash of each packet, on subsequent
        // passes ensure it matches.
        const std::string md5_hash = base::MD5String(base::StringPiece(
            reinterpret_cast<char*>(packet.data), packet.size));
        if (i == 0) {
          packet_md5_hashes_.push_back(md5_hash);
          if (j == 0) {
            start_timestamp = ConvertFromTimeBase(
                reader_->codec_context_for_testing()->time_base, packet.pts);
          }
        } else {
          EXPECT_EQ(packet_md5_hashes_[j], md5_hash) << "j = " << j;
        }

        av_packet_unref(&packet);
      }
      ASSERT_TRUE(reader_->SeekForTesting(start_timestamp));
    }
  }

  void RunTest(const char* fn,
               const char* hash,
               int channels,
               int sample_rate,
               base::TimeDelta duration,
               int frames,
               int trimmed_frames) {
    Initialize(fn);
    ASSERT_TRUE(reader_->Open());
    EXPECT_EQ(channels, reader_->channels());
    EXPECT_EQ(sample_rate, reader_->sample_rate());
    EXPECT_EQ(duration.InMicroseconds(),
              reader_->GetDuration().InMicroseconds());
    EXPECT_EQ(frames, reader_->GetNumberOfFrames());
    if (!packet_verification_disabled_)
      ASSERT_NO_FATAL_FAILURE(VerifyPackets());
    ReadAndVerify(hash, trimmed_frames);
  }

  void RunTestFailingDemux(const char* fn) {
    Initialize(fn);
    EXPECT_FALSE(reader_->Open());
  }

  void RunTestFailingDecode(const char* fn) {
    Initialize(fn);
    EXPECT_TRUE(reader_->Open());
    std::unique_ptr<AudioBus> decoded_audio_data =
        AudioBus::Create(reader_->channels(), reader_->GetNumberOfFrames());
    EXPECT_EQ(reader_->Read(decoded_audio_data.get()), 0);
  }

  void disable_packet_verification() {
    packet_verification_disabled_ = true;
  }

 protected:
  scoped_refptr<DecoderBuffer> data_;
  std::unique_ptr<InMemoryUrlProtocol> protocol_;
  std::unique_ptr<AudioFileReader> reader_;
  bool packet_verification_disabled_;

  DISALLOW_COPY_AND_ASSIGN(AudioFileReaderTest);
};

TEST_F(AudioFileReaderTest, WithoutOpen) {
  Initialize("bear.ogv");
}

TEST_F(AudioFileReaderTest, InvalidFile) {
  RunTestFailingDemux("ten_byte_file");
}

TEST_F(AudioFileReaderTest, InfiniteDuration) {
  RunTestFailingDemux("bear-320x240-live.webm");
}

TEST_F(AudioFileReaderTest, WithVideo) {
  RunTest("bear.ogv",
          "-2.49,-0.75,0.38,1.60,0.70,-1.22,",
          2,
          44100,
          base::TimeDelta::FromMicroseconds(1011520),
          44609,
          44609);
}

TEST_F(AudioFileReaderTest, Vorbis) {
  RunTest("sfx.ogg",
          "4.36,4.81,4.84,4.45,4.61,4.63,",
          1,
          44100,
          base::TimeDelta::FromMicroseconds(350001),
          15436,
          15436);
}

TEST_F(AudioFileReaderTest, WaveU8) {
  RunTest("sfx_u8.wav",
          "-1.23,-1.57,-1.14,-0.91,-0.87,-0.07,",
          1,
          44100,
          base::TimeDelta::FromMicroseconds(288414),
          12720,
          12719);
}

TEST_F(AudioFileReaderTest, WaveS16LE) {
  RunTest("sfx_s16le.wav",
          "3.05,2.87,3.00,3.32,3.58,4.08,",
          1,
          44100,
          base::TimeDelta::FromMicroseconds(288414),
          12720,
          12719);
}

TEST_F(AudioFileReaderTest, WaveS24LE) {
  RunTest("sfx_s24le.wav",
          "3.03,2.86,2.99,3.31,3.57,4.06,",
          1,
          44100,
          base::TimeDelta::FromMicroseconds(288414),
          12720,
          12719);
}

TEST_F(AudioFileReaderTest, WaveF32LE) {
  RunTest("sfx_f32le.wav",
          "3.03,2.86,2.99,3.31,3.57,4.06,",
          1,
          44100,
          base::TimeDelta::FromMicroseconds(288414),
          12720,
          12719);
}

#if defined(USE_PROPRIETARY_CODECS)
TEST_F(AudioFileReaderTest, MP3) {
  RunTest("sfx.mp3",
          "1.30,2.72,4.56,5.08,3.74,2.03,",
          1,
          44100,
          base::TimeDelta::FromMicroseconds(313470),
          13825,
          11025);
}

TEST_F(AudioFileReaderTest, CorruptMP3) {
  // Disable packet verification since the file is corrupt and FFmpeg does not
  // make any guarantees on packet consistency in this case.
  disable_packet_verification();
  RunTest("corrupt.mp3",
          "-4.95,-2.95,-0.44,1.16,0.31,-2.21,",
          1,
          44100,
          base::TimeDelta::FromMicroseconds(1018801),
          44930,
          44928);
}

TEST_F(AudioFileReaderTest, AAC) {
  RunTest("sfx.m4a", "1.81,1.66,2.32,3.27,4.46,3.36,", 1, 44100,
          base::TimeDelta::FromMicroseconds(371660), 16391, 13312);
}

TEST_F(AudioFileReaderTest, MidStreamConfigChangesFail) {
  RunTestFailingDecode("midstream_config_change.mp3");
}
#endif

TEST_F(AudioFileReaderTest, VorbisInvalidChannelLayout) {
  RunTestFailingDemux("9ch.ogg");
}

TEST_F(AudioFileReaderTest, WaveValidFourChannelLayout) {
  RunTest("4ch.wav",
          "131.71,38.02,130.31,44.89,135.98,42.52,",
          4,
          44100,
          base::TimeDelta::FromMicroseconds(100001),
          4411,
          4410);
}

}  // namespace media
