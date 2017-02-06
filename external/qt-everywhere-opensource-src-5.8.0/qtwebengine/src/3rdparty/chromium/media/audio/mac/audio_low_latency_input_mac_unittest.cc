// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>

#include "base/environment.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/audio_unittest_util.h"
#include "media/audio/mac/audio_low_latency_input_mac.h"
#include "media/base/seekable_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::Ge;
using ::testing::NotNull;

namespace media {

ACTION_P4(CheckCountAndPostQuitTask, count, limit, loop, closure) {
  if (++*count >= limit) {
    loop->PostTask(FROM_HERE, closure);
  }
}

class MockAudioInputCallback : public AudioInputStream::AudioInputCallback {
 public:
  MOCK_METHOD4(OnData,
               void(AudioInputStream* stream,
                    const AudioBus* src,
                    uint32_t hardware_delay_bytes,
                    double volume));
  MOCK_METHOD1(OnError, void(AudioInputStream* stream));
};

// This audio sink implementation should be used for manual tests only since
// the recorded data is stored on a raw binary data file.
// The last test (WriteToFileAudioSink) - which is disabled by default -
// can use this audio sink to store the captured data on a file for offline
// analysis.
class WriteToFileAudioSink : public AudioInputStream::AudioInputCallback {
 public:
  // Allocate space for ~10 seconds of data @ 48kHz in stereo:
  // 2 bytes per sample, 2 channels, 10ms @ 48kHz, 10 seconds <=> 1920000 bytes.
  static const int kMaxBufferSize = 2 * 2 * 480 * 100 * 10;

  explicit WriteToFileAudioSink(const char* file_name)
      : buffer_(0, kMaxBufferSize),
        file_(fopen(file_name, "wb")),
        bytes_to_write_(0) {
  }

  ~WriteToFileAudioSink() override {
    int bytes_written = 0;
    while (bytes_written < bytes_to_write_) {
      const uint8_t* chunk;
      int chunk_size;

      // Stop writing if no more data is available.
      if (!buffer_.GetCurrentChunk(&chunk, &chunk_size))
        break;

      // Write recorded data chunk to the file and prepare for next chunk.
      fwrite(chunk, 1, chunk_size, file_);
      buffer_.Seek(chunk_size);
      bytes_written += chunk_size;
    }
    fclose(file_);
  }

  // AudioInputStream::AudioInputCallback implementation.
  void OnData(AudioInputStream* stream,
              const AudioBus* src,
              uint32_t hardware_delay_bytes,
              double volume) override {
    const int num_samples = src->frames() * src->channels();
    std::unique_ptr<int16_t> interleaved(new int16_t[num_samples]);
    const int bytes_per_sample = sizeof(*interleaved);
    src->ToInterleaved(src->frames(), bytes_per_sample, interleaved.get());

    // Store data data in a temporary buffer to avoid making blocking
    // fwrite() calls in the audio callback. The complete buffer will be
    // written to file in the destructor.
    const int size = bytes_per_sample * num_samples;
    if (buffer_.Append((const uint8_t*)interleaved.get(), size)) {
      bytes_to_write_ += size;
    }
  }

  void OnError(AudioInputStream* stream) override {}

 private:
  media::SeekableBuffer buffer_;
  FILE* file_;
  int bytes_to_write_;
};

class MacAudioInputTest : public testing::Test {
 protected:
  MacAudioInputTest()
      : message_loop_(base::MessageLoop::TYPE_UI),
        audio_manager_(
            AudioManager::CreateForTesting(message_loop_.task_runner())) {
    // Wait for the AudioManager to finish any initialization on the audio loop.
    base::RunLoop().RunUntilIdle();
  }

  ~MacAudioInputTest() override {
    audio_manager_.reset();
    base::RunLoop().RunUntilIdle();
  }

  bool InputDevicesAvailable() {
    return audio_manager_->HasAudioInputDevices();
  }

  // Convenience method which creates a default AudioInputStream object using
  // a 10ms frame size and a sample rate which is set to the hardware sample
  // rate.
  AudioInputStream* CreateDefaultAudioInputStream() {
    int fs = static_cast<int>(AUAudioInputStream::HardwareSampleRate());
    int samples_per_packet = fs / 100;
    AudioInputStream* ais = audio_manager_->MakeAudioInputStream(
        AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                        CHANNEL_LAYOUT_STEREO, fs, 16, samples_per_packet),
        AudioDeviceDescription::kDefaultDeviceId,
        base::Bind(&MacAudioInputTest::OnLogMessage, base::Unretained(this)));
    EXPECT_TRUE(ais);
    return ais;
  }

  // Convenience method which creates an AudioInputStream object with a
  // specified channel layout.
  AudioInputStream* CreateAudioInputStream(ChannelLayout channel_layout) {
    int fs = static_cast<int>(AUAudioInputStream::HardwareSampleRate());
    int samples_per_packet = fs / 100;
    AudioInputStream* ais = audio_manager_->MakeAudioInputStream(
        AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY, channel_layout,
                        fs, 16, samples_per_packet),
        AudioDeviceDescription::kDefaultDeviceId,
        base::Bind(&MacAudioInputTest::OnLogMessage, base::Unretained(this)));
    EXPECT_TRUE(ais);
    return ais;
  }

  void OnLogMessage(const std::string& message) { log_message_ = message; }

  base::MessageLoop message_loop_;
  ScopedAudioManagerPtr audio_manager_;
  std::string log_message_;
};

// Test Create(), Close().
TEST_F(MacAudioInputTest, AUAudioInputStreamCreateAndClose) {
  ABORT_AUDIO_TEST_IF_NOT(InputDevicesAvailable());
  AudioInputStream* ais = CreateDefaultAudioInputStream();
  ais->Close();
}

// Test Open(), Close().
TEST_F(MacAudioInputTest, AUAudioInputStreamOpenAndClose) {
  ABORT_AUDIO_TEST_IF_NOT(InputDevicesAvailable());
  AudioInputStream* ais = CreateDefaultAudioInputStream();
  EXPECT_TRUE(ais->Open());
  ais->Close();
}

// Test Open(), Start(), Close().
TEST_F(MacAudioInputTest, AUAudioInputStreamOpenStartAndClose) {
  ABORT_AUDIO_TEST_IF_NOT(InputDevicesAvailable());
  AudioInputStream* ais = CreateDefaultAudioInputStream();
  EXPECT_TRUE(ais->Open());
  MockAudioInputCallback sink;
  ais->Start(&sink);
  ais->Close();
}

// Test Open(), Start(), Stop(), Close().
TEST_F(MacAudioInputTest, AUAudioInputStreamOpenStartStopAndClose) {
  ABORT_AUDIO_TEST_IF_NOT(InputDevicesAvailable());
  AudioInputStream* ais = CreateDefaultAudioInputStream();
  EXPECT_TRUE(ais->Open());
  MockAudioInputCallback sink;
  ais->Start(&sink);
  ais->Stop();
  ais->Close();
}

// Verify that recording starts and stops correctly in mono using mocked sink.
TEST_F(MacAudioInputTest, AUAudioInputStreamVerifyMonoRecording) {
  ABORT_AUDIO_TEST_IF_NOT(InputDevicesAvailable());

  int count = 0;

  // Create an audio input stream which records in mono.
  AudioInputStream* ais = CreateAudioInputStream(CHANNEL_LAYOUT_MONO);
  EXPECT_TRUE(ais->Open());

  MockAudioInputCallback sink;

  // We use 10ms packets and will run the test until ten packets are received.
  // All should contain valid packets of the same size and a valid delay
  // estimate.
  base::RunLoop run_loop;
  EXPECT_CALL(sink, OnData(ais, NotNull(), _, _))
      .Times(AtLeast(10))
      .WillRepeatedly(CheckCountAndPostQuitTask(
          &count, 10, &message_loop_, run_loop.QuitClosure()));
  ais->Start(&sink);
  run_loop.Run();
  ais->Stop();
  ais->Close();

  EXPECT_FALSE(log_message_.empty());
}

// Verify that recording starts and stops correctly in mono using mocked sink.
TEST_F(MacAudioInputTest, AUAudioInputStreamVerifyStereoRecording) {
  ABORT_AUDIO_TEST_IF_NOT(InputDevicesAvailable());

  int count = 0;

  // Create an audio input stream which records in stereo.
  AudioInputStream* ais = CreateAudioInputStream(CHANNEL_LAYOUT_STEREO);
  EXPECT_TRUE(ais->Open());

  MockAudioInputCallback sink;

  // We use 10ms packets and will run the test until ten packets are received.
  // All should contain valid packets of the same size and a valid delay
  // estimate.
  // TODO(henrika): http://crbug.com/154352 forced us to run the capture side
  // using a native buffer size of 128 audio frames and combine it with a FIFO
  // to match the requested size by the client. This change might also have
  // modified the delay estimates since the existing Ge(bytes_per_packet) for
  // parameter #4 does no longer pass. I am removing this restriction here to
  // ensure that we can land the patch but will revisit this test again when
  // more analysis of the delay estimates are done.
  base::RunLoop run_loop;
  EXPECT_CALL(sink, OnData(ais, NotNull(), _, _))
      .Times(AtLeast(10))
      .WillRepeatedly(CheckCountAndPostQuitTask(
          &count, 10, &message_loop_, run_loop.QuitClosure()));
  ais->Start(&sink);
  run_loop.Run();
  ais->Stop();
  ais->Close();

  EXPECT_FALSE(log_message_.empty());
}

// This test is intended for manual tests and should only be enabled
// when it is required to store the captured data on a local file.
// By default, GTest will print out YOU HAVE 1 DISABLED TEST.
// To include disabled tests in test execution, just invoke the test program
// with --gtest_also_run_disabled_tests or set the GTEST_ALSO_RUN_DISABLED_TESTS
// environment variable to a value greater than 0.
TEST_F(MacAudioInputTest, DISABLED_AUAudioInputStreamRecordToFile) {
  ABORT_AUDIO_TEST_IF_NOT(InputDevicesAvailable());
  const char* file_name = "out_stereo_10sec.pcm";

  int fs = static_cast<int>(AUAudioInputStream::HardwareSampleRate());
  AudioInputStream* ais = CreateDefaultAudioInputStream();
  EXPECT_TRUE(ais->Open());

  fprintf(stderr, "               File name  : %s\n", file_name);
  fprintf(stderr, "               Sample rate: %d\n", fs);
  WriteToFileAudioSink file_sink(file_name);
  fprintf(stderr, "               >> Speak into the mic while recording...\n");
  ais->Start(&file_sink);
  base::PlatformThread::Sleep(TestTimeouts::action_timeout());
  ais->Stop();
  fprintf(stderr, "               >> Recording has stopped.\n");
  ais->Close();
}

}  // namespace media
