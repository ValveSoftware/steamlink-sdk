// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <mmsystem.h>

#include "base/basictypes.h"
#include "base/environment.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/test/test_timeouts.h"
#include "base/win/scoped_com_initializer.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/win/audio_low_latency_input_win.h"
#include "media/audio/win/core_audio_util_win.h"
#include "media/base/seekable_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::win::ScopedCOMInitializer;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::Gt;
using ::testing::NotNull;

namespace media {

ACTION_P3(CheckCountAndPostQuitTask, count, limit, loop) {
  if (++*count >= limit) {
    loop->PostTask(FROM_HERE, base::MessageLoop::QuitClosure());
  }
}

class MockAudioInputCallback : public AudioInputStream::AudioInputCallback {
 public:
  MOCK_METHOD4(OnData,
               void(AudioInputStream* stream,
                    const AudioBus* src,
                    uint32 hardware_delay_bytes,
                    double volume));
  MOCK_METHOD1(OnError, void(AudioInputStream* stream));
};

class FakeAudioInputCallback : public AudioInputStream::AudioInputCallback {
 public:
  FakeAudioInputCallback()
      : error_(false),
        data_event_(false, false),
        num_received_audio_frames_(0) {}

  bool error() const { return error_; }
  int num_received_audio_frames() const { return num_received_audio_frames_; }

  // Waits until OnData() is called on another thread.
  void WaitForData() {
    data_event_.Wait();
  }

  virtual void OnData(AudioInputStream* stream,
                      const AudioBus* src,
                      uint32 hardware_delay_bytes,
                      double volume) OVERRIDE {
    EXPECT_NE(hardware_delay_bytes, 0u);
    num_received_audio_frames_ += src->frames();
    data_event_.Signal();
  }

  virtual void OnError(AudioInputStream* stream) OVERRIDE {
    error_ = true;
  }

 private:
  int num_received_audio_frames_;
  base::WaitableEvent data_event_;
  bool error_;

  DISALLOW_COPY_AND_ASSIGN(FakeAudioInputCallback);
};

// This audio sink implementation should be used for manual tests only since
// the recorded data is stored on a raw binary data file.
class WriteToFileAudioSink : public AudioInputStream::AudioInputCallback {
 public:
  // Allocate space for ~10 seconds of data @ 48kHz in stereo:
  // 2 bytes per sample, 2 channels, 10ms @ 48kHz, 10 seconds <=> 1920000 bytes.
  static const size_t kMaxBufferSize = 2 * 2 * 480 * 100 * 10;

  explicit WriteToFileAudioSink(const char* file_name, int bits_per_sample)
      : bits_per_sample_(bits_per_sample),
        buffer_(0, kMaxBufferSize),
        bytes_to_write_(0) {
    base::FilePath file_path;
    EXPECT_TRUE(PathService::Get(base::DIR_EXE, &file_path));
    file_path = file_path.AppendASCII(file_name);
    binary_file_ = base::OpenFile(file_path, "wb");
    DLOG_IF(ERROR, !binary_file_) << "Failed to open binary PCM data file.";
    VLOG(0) << ">> Output file: " << file_path.value() << " has been created.";
    VLOG(0) << "bits_per_sample_:" << bits_per_sample_;
  }

  virtual ~WriteToFileAudioSink() {
    size_t bytes_written = 0;
    while (bytes_written < bytes_to_write_) {
      const uint8* chunk;
      int chunk_size;

      // Stop writing if no more data is available.
      if (!buffer_.GetCurrentChunk(&chunk, &chunk_size))
        break;

      // Write recorded data chunk to the file and prepare for next chunk.
      fwrite(chunk, 1, chunk_size, binary_file_);
      buffer_.Seek(chunk_size);
      bytes_written += chunk_size;
    }
    base::CloseFile(binary_file_);
  }

  // AudioInputStream::AudioInputCallback implementation.
  virtual void OnData(AudioInputStream* stream,
                      const AudioBus* src,
                      uint32 hardware_delay_bytes,
                      double volume) {
    EXPECT_EQ(bits_per_sample_, 16);
    const int num_samples = src->frames() * src->channels();
    scoped_ptr<int16> interleaved(new int16[num_samples]);
    const int bytes_per_sample = sizeof(*interleaved);
    src->ToInterleaved(src->frames(), bytes_per_sample, interleaved.get());

    // Store data data in a temporary buffer to avoid making blocking
    // fwrite() calls in the audio callback. The complete buffer will be
    // written to file in the destructor.
    const int size = bytes_per_sample * num_samples;
    if (buffer_.Append((const uint8*)interleaved.get(), size)) {
      bytes_to_write_ += size;
    }
  }

  virtual void OnError(AudioInputStream* stream) {}

 private:
  int bits_per_sample_;
  media::SeekableBuffer buffer_;
  FILE* binary_file_;
  size_t bytes_to_write_;
};

// Convenience method which ensures that we are not running on the build
// bots and that at least one valid input device can be found. We also
// verify that we are not running on XP since the low-latency (WASAPI-
// based) version requires Windows Vista or higher.
static bool CanRunAudioTests(AudioManager* audio_man) {
  if (!CoreAudioUtil::IsSupported()) {
    LOG(WARNING) << "This tests requires Windows Vista or higher.";
    return false;
  }
  // TODO(henrika): note that we use Wave today to query the number of
  // existing input devices.
  bool input = audio_man->HasAudioInputDevices();
  LOG_IF(WARNING, !input) << "No input device detected.";
  return input;
}

// Convenience method which creates a default AudioInputStream object but
// also allows the user to modify the default settings.
class AudioInputStreamWrapper {
 public:
  explicit AudioInputStreamWrapper(AudioManager* audio_manager)
      : com_init_(ScopedCOMInitializer::kMTA),
        audio_man_(audio_manager),
        default_params_(
            audio_manager->GetInputStreamParameters(
                  AudioManagerBase::kDefaultDeviceId)) {
    EXPECT_EQ(format(), AudioParameters::AUDIO_PCM_LOW_LATENCY);
    frames_per_buffer_ = default_params_.frames_per_buffer();
    // We expect the default buffer size to be a 10ms buffer.
    EXPECT_EQ(frames_per_buffer_, sample_rate() / 100);
  }

  ~AudioInputStreamWrapper() {}

  // Creates AudioInputStream object using default parameters.
  AudioInputStream* Create() {
    return CreateInputStream();
  }

  // Creates AudioInputStream object using non-default parameters where the
  // frame size is modified.
  AudioInputStream* Create(int frames_per_buffer) {
    frames_per_buffer_ = frames_per_buffer;
    return CreateInputStream();
  }

  AudioParameters::Format format() const { return default_params_.format(); }
  int channels() const {
    return ChannelLayoutToChannelCount(default_params_.channel_layout());
  }
  int bits_per_sample() const { return default_params_.bits_per_sample(); }
  int sample_rate() const { return default_params_.sample_rate(); }
  int frames_per_buffer() const { return frames_per_buffer_; }

 private:
  AudioInputStream* CreateInputStream() {
    AudioInputStream* ais = audio_man_->MakeAudioInputStream(
        AudioParameters(format(), default_params_.channel_layout(),
                        default_params_.input_channels(),
                        sample_rate(), bits_per_sample(), frames_per_buffer_,
                        default_params_.effects()),
        AudioManagerBase::kDefaultDeviceId);
    EXPECT_TRUE(ais);
    return ais;
  }

  ScopedCOMInitializer com_init_;
  AudioManager* audio_man_;
  const AudioParameters default_params_;
  int frames_per_buffer_;
};

// Convenience method which creates a default AudioInputStream object.
static AudioInputStream* CreateDefaultAudioInputStream(
    AudioManager* audio_manager) {
  AudioInputStreamWrapper aisw(audio_manager);
  AudioInputStream* ais = aisw.Create();
  return ais;
}

class ScopedAudioInputStream {
 public:
  explicit ScopedAudioInputStream(AudioInputStream* stream)
      : stream_(stream) {}

  ~ScopedAudioInputStream() {
    if (stream_)
      stream_->Close();
  }

  void Close() {
    if (stream_)
      stream_->Close();
    stream_ = NULL;
  }

  AudioInputStream* operator->() {
    return stream_;
  }

  AudioInputStream* get() const { return stream_; }

  void Reset(AudioInputStream* new_stream) {
    Close();
    stream_ = new_stream;
  }

 private:
  AudioInputStream* stream_;

  DISALLOW_COPY_AND_ASSIGN(ScopedAudioInputStream);
};

// Verify that we can retrieve the current hardware/mixing sample rate
// for all available input devices.
TEST(WinAudioInputTest, WASAPIAudioInputStreamHardwareSampleRate) {
  scoped_ptr<AudioManager> audio_manager(AudioManager::CreateForTesting());
  if (!CanRunAudioTests(audio_manager.get()))
    return;

  ScopedCOMInitializer com_init(ScopedCOMInitializer::kMTA);

  // Retrieve a list of all available input devices.
  media::AudioDeviceNames device_names;
  audio_manager->GetAudioInputDeviceNames(&device_names);

  // Scan all available input devices and repeat the same test for all of them.
  for (media::AudioDeviceNames::const_iterator it = device_names.begin();
       it != device_names.end(); ++it) {
    // Retrieve the hardware sample rate given a specified audio input device.
    AudioParameters params = WASAPIAudioInputStream::GetInputStreamParameters(
        it->unique_id);
    EXPECT_GE(params.sample_rate(), 0);
  }
}

// Test Create(), Close() calling sequence.
TEST(WinAudioInputTest, WASAPIAudioInputStreamCreateAndClose) {
  scoped_ptr<AudioManager> audio_manager(AudioManager::CreateForTesting());
  if (!CanRunAudioTests(audio_manager.get()))
    return;
  ScopedAudioInputStream ais(
      CreateDefaultAudioInputStream(audio_manager.get()));
  ais.Close();
}

// Test Open(), Close() calling sequence.
TEST(WinAudioInputTest, WASAPIAudioInputStreamOpenAndClose) {
  scoped_ptr<AudioManager> audio_manager(AudioManager::CreateForTesting());
  if (!CanRunAudioTests(audio_manager.get()))
    return;
  ScopedAudioInputStream ais(
      CreateDefaultAudioInputStream(audio_manager.get()));
  EXPECT_TRUE(ais->Open());
  ais.Close();
}

// Test Open(), Start(), Close() calling sequence.
TEST(WinAudioInputTest, WASAPIAudioInputStreamOpenStartAndClose) {
  scoped_ptr<AudioManager> audio_manager(AudioManager::CreateForTesting());
  if (!CanRunAudioTests(audio_manager.get()))
    return;
  ScopedAudioInputStream ais(
      CreateDefaultAudioInputStream(audio_manager.get()));
  EXPECT_TRUE(ais->Open());
  MockAudioInputCallback sink;
  ais->Start(&sink);
  ais.Close();
}

// Test Open(), Start(), Stop(), Close() calling sequence.
TEST(WinAudioInputTest, WASAPIAudioInputStreamOpenStartStopAndClose) {
  scoped_ptr<AudioManager> audio_manager(AudioManager::CreateForTesting());
  if (!CanRunAudioTests(audio_manager.get()))
    return;
  ScopedAudioInputStream ais(
      CreateDefaultAudioInputStream(audio_manager.get()));
  EXPECT_TRUE(ais->Open());
  MockAudioInputCallback sink;
  ais->Start(&sink);
  ais->Stop();
  ais.Close();
}

// Test some additional calling sequences.
TEST(WinAudioInputTest, WASAPIAudioInputStreamMiscCallingSequences) {
  scoped_ptr<AudioManager> audio_manager(AudioManager::CreateForTesting());
  if (!CanRunAudioTests(audio_manager.get()))
    return;
  ScopedAudioInputStream ais(
      CreateDefaultAudioInputStream(audio_manager.get()));
  WASAPIAudioInputStream* wais =
      static_cast<WASAPIAudioInputStream*>(ais.get());

  // Open(), Open() should fail the second time.
  EXPECT_TRUE(ais->Open());
  EXPECT_FALSE(ais->Open());

  MockAudioInputCallback sink;

  // Start(), Start() is a valid calling sequence (second call does nothing).
  ais->Start(&sink);
  EXPECT_TRUE(wais->started());
  ais->Start(&sink);
  EXPECT_TRUE(wais->started());

  // Stop(), Stop() is a valid calling sequence (second call does nothing).
  ais->Stop();
  EXPECT_FALSE(wais->started());
  ais->Stop();
  EXPECT_FALSE(wais->started());
  ais.Close();
}

TEST(WinAudioInputTest, WASAPIAudioInputStreamTestPacketSizes) {
  scoped_ptr<AudioManager> audio_manager(AudioManager::CreateForTesting());
  if (!CanRunAudioTests(audio_manager.get()))
    return;

  int count = 0;
  base::MessageLoopForUI loop;

  // 10 ms packet size.

  // Create default WASAPI input stream which records in stereo using
  // the shared mixing rate. The default buffer size is 10ms.
  AudioInputStreamWrapper aisw(audio_manager.get());
  ScopedAudioInputStream ais(aisw.Create());
  EXPECT_TRUE(ais->Open());

  MockAudioInputCallback sink;

  // Derive the expected size in bytes of each recorded packet.
  uint32 bytes_per_packet = aisw.channels() * aisw.frames_per_buffer() *
      (aisw.bits_per_sample() / 8);

  // We use 10ms packets and will run the test until ten packets are received.
  // All should contain valid packets of the same size and a valid delay
  // estimate.
  EXPECT_CALL(sink, OnData(ais.get(), NotNull(), Gt(bytes_per_packet), _))
      .Times(AtLeast(10))
      .WillRepeatedly(CheckCountAndPostQuitTask(&count, 10, &loop));
  ais->Start(&sink);
  loop.Run();
  ais->Stop();

  // Store current packet size (to be used in the subsequent tests).
  int frames_per_buffer_10ms = aisw.frames_per_buffer();

  ais.Close();

  // 20 ms packet size.

  count = 0;
  ais.Reset(aisw.Create(2 * frames_per_buffer_10ms));
  EXPECT_TRUE(ais->Open());
  bytes_per_packet = aisw.channels() * aisw.frames_per_buffer() *
      (aisw.bits_per_sample() / 8);

  EXPECT_CALL(sink, OnData(ais.get(), NotNull(), Gt(bytes_per_packet), _))
      .Times(AtLeast(10))
      .WillRepeatedly(CheckCountAndPostQuitTask(&count, 10, &loop));
  ais->Start(&sink);
  loop.Run();
  ais->Stop();
  ais.Close();

  // 5 ms packet size.

  count = 0;
  ais.Reset(aisw.Create(frames_per_buffer_10ms / 2));
  EXPECT_TRUE(ais->Open());
  bytes_per_packet = aisw.channels() * aisw.frames_per_buffer() *
    (aisw.bits_per_sample() / 8);

  EXPECT_CALL(sink, OnData(ais.get(), NotNull(), Gt(bytes_per_packet), _))
      .Times(AtLeast(10))
      .WillRepeatedly(CheckCountAndPostQuitTask(&count, 10, &loop));
  ais->Start(&sink);
  loop.Run();
  ais->Stop();
  ais.Close();
}

// Test that we can capture a stream in loopback.
TEST(WinAudioInputTest, WASAPIAudioInputStreamLoopback) {
  scoped_ptr<AudioManager> audio_manager(AudioManager::CreateForTesting());
  if (!audio_manager->HasAudioOutputDevices() || !CoreAudioUtil::IsSupported())
    return;

  AudioParameters params = audio_manager->GetInputStreamParameters(
      AudioManagerBase::kLoopbackInputDeviceId);
  EXPECT_EQ(params.effects(), 0);

  AudioParameters output_params =
      audio_manager->GetOutputStreamParameters(std::string());
  EXPECT_EQ(params.sample_rate(), output_params.sample_rate());
  EXPECT_EQ(params.channel_layout(), output_params.channel_layout());

  ScopedAudioInputStream stream(audio_manager->MakeAudioInputStream(
      params, AudioManagerBase::kLoopbackInputDeviceId));
  ASSERT_TRUE(stream->Open());
  FakeAudioInputCallback sink;
  stream->Start(&sink);
  ASSERT_FALSE(sink.error());

  sink.WaitForData();
  stream.Close();

  EXPECT_GT(sink.num_received_audio_frames(), 0);
  EXPECT_FALSE(sink.error());
}

// This test is intended for manual tests and should only be enabled
// when it is required to store the captured data on a local file.
// By default, GTest will print out YOU HAVE 1 DISABLED TEST.
// To include disabled tests in test execution, just invoke the test program
// with --gtest_also_run_disabled_tests or set the GTEST_ALSO_RUN_DISABLED_TESTS
// environment variable to a value greater than 0.
TEST(WinAudioInputTest, DISABLED_WASAPIAudioInputStreamRecordToFile) {
  scoped_ptr<AudioManager> audio_manager(AudioManager::CreateForTesting());
  if (!CanRunAudioTests(audio_manager.get()))
    return;

  // Name of the output PCM file containing captured data. The output file
  // will be stored in the directory containing 'media_unittests.exe'.
  // Example of full name: \src\build\Debug\out_stereo_10sec.pcm.
  const char* file_name = "out_stereo_10sec.pcm";

  AudioInputStreamWrapper aisw(audio_manager.get());
  ScopedAudioInputStream ais(aisw.Create());
  EXPECT_TRUE(ais->Open());

  VLOG(0) << ">> Sample rate: " << aisw.sample_rate() << " [Hz]";
  WriteToFileAudioSink file_sink(file_name, aisw.bits_per_sample());
  VLOG(0) << ">> Speak into the default microphone while recording.";
  ais->Start(&file_sink);
  base::PlatformThread::Sleep(TestTimeouts::action_timeout());
  ais->Stop();
  VLOG(0) << ">> Recording has stopped.";
  ais.Close();
}

}  // namespace media
