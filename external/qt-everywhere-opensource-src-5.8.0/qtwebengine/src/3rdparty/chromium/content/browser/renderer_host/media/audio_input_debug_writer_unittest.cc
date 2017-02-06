// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/files/file_util.h"
#include "base/sys_byteorder.h"
#include "content/browser/renderer_host/media/audio_input_debug_writer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "media/base/audio_bus.h"
#include "media/base/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

static const uint16_t kBytesPerSample = sizeof(uint16_t);
static const uint16_t kPcmEncoding = 1;
static const size_t kWavHeaderSize = 44;

uint16_t ReadLE2(const char* buf) {
  return static_cast<uint8_t>(buf[0]) | (static_cast<uint8_t>(buf[1]) << 8);
}

uint32_t ReadLE4(const char* buf) {
  return static_cast<uint8_t>(buf[0]) | (static_cast<uint8_t>(buf[1]) << 8) |
         (static_cast<uint8_t>(buf[2]) << 16) |
         (static_cast<uint8_t>(buf[3]) << 24);
}

}  // namespace

// <channel layout, sample rate, frames per buffer, number of buffer writes
typedef std::tr1::tuple<media::ChannelLayout, int, int, int>
    AudioInputDebugWriterTestData;

class AudioInputDebugWriterTest
    : public testing::TestWithParam<AudioInputDebugWriterTestData> {
 public:
  AudioInputDebugWriterTest()
      : params_(media::AudioParameters::Format::AUDIO_PCM_LINEAR,
                std::tr1::get<0>(GetParam()),
                std::tr1::get<1>(GetParam()),
                kBytesPerSample * 8,
                std::tr1::get<2>(GetParam())),
        writes_(std::tr1::get<3>(GetParam())),
        source_samples_(params_.frames_per_buffer() * params_.channels() *
                        writes_),
        source_interleaved_(source_samples_ ? new int16_t[source_samples_]
                                            : nullptr) {
    InitSourceInterleaved(source_interleaved_.get(), source_samples_);
  }

 protected:
  virtual ~AudioInputDebugWriterTest() {}

  static void InitSourceInterleaved(int16_t* source_interleaved,
                                    int source_samples) {
    if (source_samples) {
      // equal steps to cover int16_t range of values
      int16_t step = 0xffff / source_samples;
      int16_t val = std::numeric_limits<int16_t>::min();
      for (int i = 0; i < source_samples; ++i, val += step)
        source_interleaved[i] = val;
    }
  }

  static void VerifyHeader(const char(&wav_header)[kWavHeaderSize],
                           const media::AudioParameters& params,
                           int writes,
                           int64_t file_length) {
    uint32_t block_align = params.channels() * kBytesPerSample;
    uint32_t data_size =
        static_cast<uint32_t>(params.frames_per_buffer() * params.channels() *
                              writes * kBytesPerSample);
    // Offset Length  Content
    //  0      4     "RIFF"
    EXPECT_EQ(0, strncmp(wav_header, "RIFF", 4));
    //  4      4     <file length - 8>
    EXPECT_EQ(static_cast<uint64_t>(file_length - 8), ReadLE4(wav_header + 4));
    EXPECT_EQ(static_cast<uint32_t>(data_size + kWavHeaderSize - 8),
              ReadLE4(wav_header + 4));
    //  8      4     "WAVE"
    // 12      4     "fmt "
    EXPECT_EQ(0, strncmp(wav_header + 8, "WAVEfmt ", 8));
    // 16      4     <length of the fmt data> (=16)
    EXPECT_EQ(16U, ReadLE4(wav_header + 16));
    // 20      2     <WAVE file encoding tag>
    EXPECT_EQ(kPcmEncoding, ReadLE2(wav_header + 20));
    // 22      2     <channels>
    EXPECT_EQ(params.channels(), ReadLE2(wav_header + 22));
    // 24      4     <sample rate>
    EXPECT_EQ(static_cast<uint32_t>(params.sample_rate()),
              ReadLE4(wav_header + 24));
    // 28      4     <bytes per second> (sample rate * block align)
    EXPECT_EQ(static_cast<uint32_t>(params.sample_rate()) * block_align,
              ReadLE4(wav_header + 28));
    // 32      2     <block align>  (channels * bits per sample / 8)
    EXPECT_EQ(block_align, ReadLE2(wav_header + 32));
    // 34      2     <bits per sample>
    EXPECT_EQ(kBytesPerSample * 8, ReadLE2(wav_header + 34));
    // 36      4     "data"
    EXPECT_EQ(0, strncmp(wav_header + 36, "data", 4));
    // 40      4     <sample data size(n)>
    EXPECT_EQ(data_size, ReadLE4(wav_header + 40));
  }

  // |result_interleaved| is expected to be little-endian.
  static void VerifyDataRecording(const int16_t* source_interleaved,
                                  const int16_t* result_interleaved,
                                  int16_t source_samples) {
    // Allow mismatch by 1 due to rounding error in int->float->int
    // calculations.
    for (int i = 0; i < source_samples; ++i)
      EXPECT_LE(std::abs(static_cast<int16_t>(
                             base::ByteSwapToLE16(source_interleaved[i])) -
                         result_interleaved[i]),
                1)
          << "i = " << i << " source " << source_interleaved[i] << " result "
          << result_interleaved[i];
  }

  void VerifyRecording(const base::FilePath& file_path) {
    base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    EXPECT_TRUE(file.IsValid());

    char wav_header[kWavHeaderSize];
    EXPECT_EQ(file.Read(0, wav_header, kWavHeaderSize),
              static_cast<int>(kWavHeaderSize));
    VerifyHeader(wav_header, params_, writes_, file.GetLength());

    if (source_samples_ > 0) {
      std::unique_ptr<int16_t[]> result_interleaved(
          new int16_t[source_samples_]);
      memset(result_interleaved.get(), 0, source_samples_ * kBytesPerSample);

      // Recording is read from file as a byte sequence, so it stored as
      // little-endian.
      int read = file.Read(kWavHeaderSize,
                           reinterpret_cast<char*>(result_interleaved.get()),
                           source_samples_ * kBytesPerSample);
      EXPECT_EQ(static_cast<int>(file.GetLength() - kWavHeaderSize), read);

      VerifyDataRecording(source_interleaved_.get(), result_interleaved.get(),
                          source_samples_);
    }
  }

  void TestDoneOnFileThread(const base::Closure& callback) {
    DCHECK_CURRENTLY_ON(BrowserThread::FILE);

    // |writer| must be destroyed on FILE thread.
    input_debug_writer_.reset(nullptr);
    callback.Run();
  }

  void DoDebugRecording(base::File file) {
    if (!file.IsValid())
      return;

    input_debug_writer_.reset(
        new AudioInputDebugWriter(std::move(file), params_));
    // Write tasks are posted to BrowserThread::FILE.
    for (int i = 0; i < writes_; ++i) {
      std::unique_ptr<media::AudioBus> bus = media::AudioBus::Create(
          params_.channels(), params_.frames_per_buffer());

      bus->FromInterleaved(
          source_interleaved_.get() +
              i * params_.channels() * params_.frames_per_buffer(),
          params_.frames_per_buffer(), kBytesPerSample);

      input_debug_writer_->Write(std::move(bus));
    }

    media::WaitableMessageLoopEvent event;

    // Post a task to BrowserThread::FILE indicating that all the writes are
    // done.
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&AudioInputDebugWriterTest::TestDoneOnFileThread,
                   base::Unretained(this), event.GetClosure()));

    // Wait for TestDoneOnFileThread() to call event's closure.
    event.RunAndWait();
  }

 protected:
  TestBrowserThreadBundle thread_bundle_;

  // Writer under test.
  std::unique_ptr<AudioInputDebugWriter> input_debug_writer_;

  // AudioBus parameters.
  media::AudioParameters params_;

  // Number of times to write AudioBus to the file.
  int writes_;

  // Number of samples in the source data.
  int source_samples_;

  // Source data.
  std::unique_ptr<int16_t[]> source_interleaved_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioInputDebugWriterTest);
};

TEST_P(AudioInputDebugWriterTest, WaveRecordingTest) {
  base::FilePath file_path;
  EXPECT_TRUE(base::CreateTemporaryFile(&file_path));

  base::File file(file_path,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  EXPECT_TRUE(file.IsValid());

  DoDebugRecording(std::move(file));

  VerifyRecording(file_path);

  if (::testing::Test::HasFailure()) {
    LOG(ERROR) << "Test failed; keeping recording(s) at ["
               << file_path.value().c_str() << "].";
  } else {
    EXPECT_TRUE(base::DeleteFile(file_path, false));
  }
}

INSTANTIATE_TEST_CASE_P(
    AudioInputDebugWriterTest,
    AudioInputDebugWriterTest,
    // Using 10ms sframes per buffer everywhere.
    testing::Values(
        // No writes.
        std::tr1::make_tuple(media::ChannelLayout::CHANNEL_LAYOUT_MONO,
                             44100,
                             44100 / 100,
                             0),
        // 1 write of mono.
        std::tr1::make_tuple(media::ChannelLayout::CHANNEL_LAYOUT_MONO,
                             44100,
                             44100 / 100,
                             1),
        // 1 second of mono.
        std::tr1::make_tuple(media::ChannelLayout::CHANNEL_LAYOUT_MONO,
                             44100,
                             44100 / 100,
                             100),
        // 1 second of mono, higher rate.
        std::tr1::make_tuple(media::ChannelLayout::CHANNEL_LAYOUT_MONO,
                             48000,
                             48000 / 100,
                             100),
        // 1 second of stereo.
        std::tr1::make_tuple(media::ChannelLayout::CHANNEL_LAYOUT_STEREO,
                             44100,
                             44100 / 100,
                             100),
        // 15 seconds of stereo, higher rate.
        std::tr1::make_tuple(media::ChannelLayout::CHANNEL_LAYOUT_STEREO,
                             48000,
                             48000 / 100,
                             1500)));

}  // namespace content
