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
#include "base/time/time.h"
#include "base/win/scoped_com_initializer.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager.h"
#include "media/audio/mock_audio_source_callback.h"
#include "media/audio/win/audio_low_latency_output_win.h"
#include "media/audio/win/core_audio_util_win.h"
#include "media/base/decoder_buffer.h"
#include "media/base/seekable_buffer.h"
#include "media/base/test_data_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gmock_mutant.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::Between;
using ::testing::CreateFunctor;
using ::testing::DoAll;
using ::testing::Gt;
using ::testing::InvokeWithoutArgs;
using ::testing::NotNull;
using ::testing::Return;
using base::win::ScopedCOMInitializer;

namespace media {

static const char kSpeechFile_16b_s_48k[] = "speech_16b_stereo_48kHz.raw";
static const char kSpeechFile_16b_s_44k[] = "speech_16b_stereo_44kHz.raw";
static const size_t kFileDurationMs = 20000;
static const size_t kNumFileSegments = 2;
static const int kBitsPerSample = 16;
static const size_t kMaxDeltaSamples = 1000;
static const char kDeltaTimeMsFileName[] = "delta_times_ms.txt";

MATCHER_P(HasValidDelay, value, "") {
  // It is difficult to come up with a perfect test condition for the delay
  // estimation. For now, verify that the produced output delay is always
  // larger than the selected buffer size.
  return arg.hardware_delay_bytes >= value.hardware_delay_bytes;
}

// Used to terminate a loop from a different thread than the loop belongs to.
// |loop| should be a MessageLoopProxy.
ACTION_P(QuitLoop, loop) {
  loop->PostTask(FROM_HERE, base::MessageLoop::QuitClosure());
}

// This audio source implementation should be used for manual tests only since
// it takes about 20 seconds to play out a file.
class ReadFromFileAudioSource : public AudioOutputStream::AudioSourceCallback {
 public:
  explicit ReadFromFileAudioSource(const std::string& name)
    : pos_(0),
      previous_call_time_(base::TimeTicks::Now()),
      text_file_(NULL),
      elements_to_write_(0) {
    // Reads a test file from media/test/data directory.
    file_ = ReadTestDataFile(name);

    // Creates an array that will store delta times between callbacks.
    // The content of this array will be written to a text file at
    // destruction and can then be used for off-line analysis of the exact
    // timing of callbacks. The text file will be stored in media/test/data.
    delta_times_.reset(new int[kMaxDeltaSamples]);
  }

  virtual ~ReadFromFileAudioSource() {
    // Get complete file path to output file in directory containing
    // media_unittests.exe.
    base::FilePath file_name;
    EXPECT_TRUE(PathService::Get(base::DIR_EXE, &file_name));
    file_name = file_name.AppendASCII(kDeltaTimeMsFileName);

    EXPECT_TRUE(!text_file_);
    text_file_ = base::OpenFile(file_name, "wt");
    DLOG_IF(ERROR, !text_file_) << "Failed to open log file.";

    // Write the array which contains delta times to a text file.
    size_t elements_written = 0;
    while (elements_written < elements_to_write_) {
      fprintf(text_file_, "%d\n", delta_times_[elements_written]);
      ++elements_written;
    }

    base::CloseFile(text_file_);
  }

  // AudioOutputStream::AudioSourceCallback implementation.
  virtual int OnMoreData(AudioBus* audio_bus,
                         AudioBuffersState buffers_state) {
    // Store time difference between two successive callbacks in an array.
    // These values will be written to a file in the destructor.
    const base::TimeTicks now_time = base::TimeTicks::Now();
    const int diff = (now_time - previous_call_time_).InMilliseconds();
    previous_call_time_ = now_time;
    if (elements_to_write_ < kMaxDeltaSamples) {
      delta_times_[elements_to_write_] = diff;
      ++elements_to_write_;
    }

    int max_size =
        audio_bus->frames() * audio_bus->channels() * kBitsPerSample / 8;

    // Use samples read from a data file and fill up the audio buffer
    // provided to us in the callback.
    if (pos_ + static_cast<int>(max_size) > file_size())
      max_size = file_size() - pos_;
    int frames = max_size / (audio_bus->channels() * kBitsPerSample / 8);
    if (max_size) {
      audio_bus->FromInterleaved(
          file_->data() + pos_, frames, kBitsPerSample / 8);
      pos_ += max_size;
    }
    return frames;
  }

  virtual void OnError(AudioOutputStream* stream) {}

  int file_size() { return file_->data_size(); }

 private:
  scoped_refptr<DecoderBuffer> file_;
  scoped_ptr<int[]> delta_times_;
  int pos_;
  base::TimeTicks previous_call_time_;
  FILE* text_file_;
  size_t elements_to_write_;
};

static bool ExclusiveModeIsEnabled() {
  return (WASAPIAudioOutputStream::GetShareMode() ==
          AUDCLNT_SHAREMODE_EXCLUSIVE);
}

// Convenience method which ensures that we are not running on the build
// bots and that at least one valid output device can be found. We also
// verify that we are not running on XP since the low-latency (WASAPI-
// based) version requires Windows Vista or higher.
static bool CanRunAudioTests(AudioManager* audio_man) {
  if (!CoreAudioUtil::IsSupported()) {
    LOG(WARNING) << "This test requires Windows Vista or higher.";
    return false;
  }

  // TODO(henrika): note that we use Wave today to query the number of
  // existing output devices.
  if (!audio_man->HasAudioOutputDevices()) {
    LOG(WARNING) << "No output devices detected.";
    return false;
  }

  return true;
}

// Convenience method which creates a default AudioOutputStream object but
// also allows the user to modify the default settings.
class AudioOutputStreamWrapper {
 public:
  explicit AudioOutputStreamWrapper(AudioManager* audio_manager)
      : audio_man_(audio_manager),
        format_(AudioParameters::AUDIO_PCM_LOW_LATENCY),
        bits_per_sample_(kBitsPerSample) {
    AudioParameters preferred_params;
    EXPECT_TRUE(SUCCEEDED(CoreAudioUtil::GetPreferredAudioParameters(
        eRender, eConsole, &preferred_params)));
    channel_layout_ = preferred_params.channel_layout();
    sample_rate_ = preferred_params.sample_rate();
    samples_per_packet_ = preferred_params.frames_per_buffer();
  }

  ~AudioOutputStreamWrapper() {}

  // Creates AudioOutputStream object using default parameters.
  AudioOutputStream* Create() {
    return CreateOutputStream();
  }

  // Creates AudioOutputStream object using non-default parameters where the
  // frame size is modified.
  AudioOutputStream* Create(int samples_per_packet) {
    samples_per_packet_ = samples_per_packet;
    return CreateOutputStream();
  }

  // Creates AudioOutputStream object using non-default parameters where the
  // sample rate and frame size are modified.
  AudioOutputStream* Create(int sample_rate, int samples_per_packet) {
    sample_rate_ = sample_rate;
    samples_per_packet_ = samples_per_packet;
    return CreateOutputStream();
  }

  AudioParameters::Format format() const { return format_; }
  int channels() const { return ChannelLayoutToChannelCount(channel_layout_); }
  int bits_per_sample() const { return bits_per_sample_; }
  int sample_rate() const { return sample_rate_; }
  int samples_per_packet() const { return samples_per_packet_; }

 private:
  AudioOutputStream* CreateOutputStream() {
    AudioOutputStream* aos = audio_man_->MakeAudioOutputStream(
        AudioParameters(format_, channel_layout_, sample_rate_,
                        bits_per_sample_, samples_per_packet_),
        std::string());
    EXPECT_TRUE(aos);
    return aos;
  }

  AudioManager* audio_man_;
  AudioParameters::Format format_;
  ChannelLayout channel_layout_;
  int bits_per_sample_;
  int sample_rate_;
  int samples_per_packet_;
};

// Convenience method which creates a default AudioOutputStream object.
static AudioOutputStream* CreateDefaultAudioOutputStream(
    AudioManager* audio_manager) {
  AudioOutputStreamWrapper aosw(audio_manager);
  AudioOutputStream* aos = aosw.Create();
  return aos;
}

// Verify that we can retrieve the current hardware/mixing sample rate
// for the default audio device.
// TODO(henrika): modify this test when we support full device enumeration.
TEST(WASAPIAudioOutputStreamTest, HardwareSampleRate) {
  // Skip this test in exclusive mode since the resulting rate is only utilized
  // for shared mode streams.
  scoped_ptr<AudioManager> audio_manager(AudioManager::CreateForTesting());
  if (!CanRunAudioTests(audio_manager.get()) || ExclusiveModeIsEnabled())
    return;

  // Default device intended for games, system notification sounds,
  // and voice commands.
  int fs = static_cast<int>(
      WASAPIAudioOutputStream::HardwareSampleRate(std::string()));
  EXPECT_GE(fs, 0);
}

// Test Create(), Close() calling sequence.
TEST(WASAPIAudioOutputStreamTest, CreateAndClose) {
  scoped_ptr<AudioManager> audio_manager(AudioManager::CreateForTesting());
  if (!CanRunAudioTests(audio_manager.get()))
    return;
  AudioOutputStream* aos = CreateDefaultAudioOutputStream(audio_manager.get());
  aos->Close();
}

// Test Open(), Close() calling sequence.
TEST(WASAPIAudioOutputStreamTest, OpenAndClose) {
  scoped_ptr<AudioManager> audio_manager(AudioManager::CreateForTesting());
  if (!CanRunAudioTests(audio_manager.get()))
    return;
  AudioOutputStream* aos = CreateDefaultAudioOutputStream(audio_manager.get());
  EXPECT_TRUE(aos->Open());
  aos->Close();
}

// Test Open(), Start(), Close() calling sequence.
TEST(WASAPIAudioOutputStreamTest, OpenStartAndClose) {
  scoped_ptr<AudioManager> audio_manager(AudioManager::CreateForTesting());
  if (!CanRunAudioTests(audio_manager.get()))
    return;
  AudioOutputStream* aos = CreateDefaultAudioOutputStream(audio_manager.get());
  EXPECT_TRUE(aos->Open());
  MockAudioSourceCallback source;
  EXPECT_CALL(source, OnError(aos))
      .Times(0);
  aos->Start(&source);
  aos->Close();
}

// Test Open(), Start(), Stop(), Close() calling sequence.
TEST(WASAPIAudioOutputStreamTest, OpenStartStopAndClose) {
  scoped_ptr<AudioManager> audio_manager(AudioManager::CreateForTesting());
  if (!CanRunAudioTests(audio_manager.get()))
    return;
  AudioOutputStream* aos = CreateDefaultAudioOutputStream(audio_manager.get());
  EXPECT_TRUE(aos->Open());
  MockAudioSourceCallback source;
  EXPECT_CALL(source, OnError(aos))
      .Times(0);
  aos->Start(&source);
  aos->Stop();
  aos->Close();
}

// Test SetVolume(), GetVolume()
TEST(WASAPIAudioOutputStreamTest, Volume) {
  scoped_ptr<AudioManager> audio_manager(AudioManager::CreateForTesting());
  if (!CanRunAudioTests(audio_manager.get()))
    return;
  AudioOutputStream* aos = CreateDefaultAudioOutputStream(audio_manager.get());

  // Initial volume should be full volume (1.0).
  double volume = 0.0;
  aos->GetVolume(&volume);
  EXPECT_EQ(1.0, volume);

  // Verify some valid volume settings.
  aos->SetVolume(0.0);
  aos->GetVolume(&volume);
  EXPECT_EQ(0.0, volume);

  aos->SetVolume(0.5);
  aos->GetVolume(&volume);
  EXPECT_EQ(0.5, volume);

  aos->SetVolume(1.0);
  aos->GetVolume(&volume);
  EXPECT_EQ(1.0, volume);

  // Ensure that invalid volume setting have no effect.
  aos->SetVolume(1.5);
  aos->GetVolume(&volume);
  EXPECT_EQ(1.0, volume);

  aos->SetVolume(-0.5);
  aos->GetVolume(&volume);
  EXPECT_EQ(1.0, volume);

  aos->Close();
}

// Test some additional calling sequences.
TEST(WASAPIAudioOutputStreamTest, MiscCallingSequences) {
  scoped_ptr<AudioManager> audio_manager(AudioManager::CreateForTesting());
  if (!CanRunAudioTests(audio_manager.get()))
    return;

  AudioOutputStream* aos = CreateDefaultAudioOutputStream(audio_manager.get());
  WASAPIAudioOutputStream* waos = static_cast<WASAPIAudioOutputStream*>(aos);

  // Open(), Open() is a valid calling sequence (second call does nothing).
  EXPECT_TRUE(aos->Open());
  EXPECT_TRUE(aos->Open());

  MockAudioSourceCallback source;

  // Start(), Start() is a valid calling sequence (second call does nothing).
  aos->Start(&source);
  EXPECT_TRUE(waos->started());
  aos->Start(&source);
  EXPECT_TRUE(waos->started());

  // Stop(), Stop() is a valid calling sequence (second call does nothing).
  aos->Stop();
  EXPECT_FALSE(waos->started());
  aos->Stop();
  EXPECT_FALSE(waos->started());

  // Start(), Stop(), Start(), Stop().
  aos->Start(&source);
  EXPECT_TRUE(waos->started());
  aos->Stop();
  EXPECT_FALSE(waos->started());
  aos->Start(&source);
  EXPECT_TRUE(waos->started());
  aos->Stop();
  EXPECT_FALSE(waos->started());

  aos->Close();
}

// Use preferred packet size and verify that rendering starts.
TEST(WASAPIAudioOutputStreamTest, ValidPacketSize) {
  scoped_ptr<AudioManager> audio_manager(AudioManager::CreateForTesting());
  if (!CanRunAudioTests(audio_manager.get()))
    return;

  base::MessageLoopForUI loop;
  MockAudioSourceCallback source;

  // Create default WASAPI output stream which plays out in stereo using
  // the shared mixing rate. The default buffer size is 10ms.
  AudioOutputStreamWrapper aosw(audio_manager.get());
  AudioOutputStream* aos = aosw.Create();
  EXPECT_TRUE(aos->Open());

  // Derive the expected size in bytes of each packet.
  uint32 bytes_per_packet = aosw.channels() * aosw.samples_per_packet() *
                           (aosw.bits_per_sample() / 8);

  // Set up expected minimum delay estimation.
  AudioBuffersState state(0, bytes_per_packet);

  // Wait for the first callback and verify its parameters.
  EXPECT_CALL(source, OnMoreData(NotNull(), HasValidDelay(state)))
      .WillOnce(DoAll(
          QuitLoop(loop.message_loop_proxy()),
          Return(aosw.samples_per_packet())));

  aos->Start(&source);
  loop.PostDelayedTask(FROM_HERE, base::MessageLoop::QuitClosure(),
                       TestTimeouts::action_timeout());
  loop.Run();
  aos->Stop();
  aos->Close();
}

// This test is intended for manual tests and should only be enabled
// when it is required to play out data from a local PCM file.
// By default, GTest will print out YOU HAVE 1 DISABLED TEST.
// To include disabled tests in test execution, just invoke the test program
// with --gtest_also_run_disabled_tests or set the GTEST_ALSO_RUN_DISABLED_TESTS
// environment variable to a value greater than 0.
// The test files are approximately 20 seconds long.
TEST(WASAPIAudioOutputStreamTest, DISABLED_ReadFromStereoFile) {
  scoped_ptr<AudioManager> audio_manager(AudioManager::CreateForTesting());
  if (!CanRunAudioTests(audio_manager.get()))
    return;

  AudioOutputStreamWrapper aosw(audio_manager.get());
  AudioOutputStream* aos = aosw.Create();
  EXPECT_TRUE(aos->Open());

  std::string file_name;
  if (aosw.sample_rate() == 48000) {
    file_name = kSpeechFile_16b_s_48k;
  } else if (aosw.sample_rate() == 44100) {
    file_name = kSpeechFile_16b_s_44k;
  } else if (aosw.sample_rate() == 96000) {
    // Use 48kHz file at 96kHz as well. Will sound like Donald Duck.
    file_name = kSpeechFile_16b_s_48k;
  } else {
    FAIL() << "This test supports 44.1, 48kHz and 96kHz only.";
    return;
  }
  ReadFromFileAudioSource file_source(file_name);

  VLOG(0) << "File name      : " << file_name.c_str();
  VLOG(0) << "Sample rate    : " << aosw.sample_rate();
  VLOG(0) << "Bits per sample: " << aosw.bits_per_sample();
  VLOG(0) << "#channels      : " << aosw.channels();
  VLOG(0) << "File size      : " << file_source.file_size();
  VLOG(0) << "#file segments : " << kNumFileSegments;
  VLOG(0) << ">> Listen to the stereo file while playing...";

  for (int i = 0; i < kNumFileSegments; i++) {
    // Each segment will start with a short (~20ms) block of zeros, hence
    // some short glitches might be heard in this test if kNumFileSegments
    // is larger than one. The exact length of the silence period depends on
    // the selected sample rate.
    aos->Start(&file_source);
    base::PlatformThread::Sleep(
        base::TimeDelta::FromMilliseconds(kFileDurationMs / kNumFileSegments));
    aos->Stop();
  }

  VLOG(0) << ">> Stereo file playout has stopped.";
  aos->Close();
}

// Verify that we can open the output stream in exclusive mode using a
// certain set of audio parameters and a sample rate of 48kHz.
// The expected outcomes of each setting in this test has been derived
// manually using log outputs (--v=1).
TEST(WASAPIAudioOutputStreamTest, ExclusiveModeBufferSizesAt48kHz) {
  if (!ExclusiveModeIsEnabled())
    return;

  scoped_ptr<AudioManager> audio_manager(AudioManager::CreateForTesting());
  if (!CanRunAudioTests(audio_manager.get()))
    return;

  AudioOutputStreamWrapper aosw(audio_manager.get());

  // 10ms @ 48kHz shall work.
  // Note that, this is the same size as we can use for shared-mode streaming
  // but here the endpoint buffer delay is only 10ms instead of 20ms.
  AudioOutputStream* aos = aosw.Create(48000, 480);
  EXPECT_TRUE(aos->Open());
  aos->Close();

  // 5ms @ 48kHz does not work due to misalignment.
  // This test will propose an aligned buffer size of 5.3333ms.
  // Note that we must call Close() even is Open() fails since Close() also
  // deletes the object and we want to create a new object in the next test.
  aos = aosw.Create(48000, 240);
  EXPECT_FALSE(aos->Open());
  aos->Close();

  // 5.3333ms @ 48kHz should work (see test above).
  aos = aosw.Create(48000, 256);
  EXPECT_TRUE(aos->Open());
  aos->Close();

  // 2.6667ms is smaller than the minimum supported size (=3ms).
  aos = aosw.Create(48000, 128);
  EXPECT_FALSE(aos->Open());
  aos->Close();

  // 3ms does not correspond to an aligned buffer size.
  // This test will propose an aligned buffer size of 3.3333ms.
  aos = aosw.Create(48000, 144);
  EXPECT_FALSE(aos->Open());
  aos->Close();

  // 3.3333ms @ 48kHz <=> smallest possible buffer size we can use.
  aos = aosw.Create(48000, 160);
  EXPECT_TRUE(aos->Open());
  aos->Close();
}

// Verify that we can open the output stream in exclusive mode using a
// certain set of audio parameters and a sample rate of 44.1kHz.
// The expected outcomes of each setting in this test has been derived
// manually using log outputs (--v=1).
TEST(WASAPIAudioOutputStreamTest, ExclusiveModeBufferSizesAt44kHz) {
  if (!ExclusiveModeIsEnabled())
    return;

  scoped_ptr<AudioManager> audio_manager(AudioManager::CreateForTesting());
  if (!CanRunAudioTests(audio_manager.get()))
    return;

  AudioOutputStreamWrapper aosw(audio_manager.get());

  // 10ms @ 44.1kHz does not work due to misalignment.
  // This test will propose an aligned buffer size of 10.1587ms.
  AudioOutputStream* aos = aosw.Create(44100, 441);
  EXPECT_FALSE(aos->Open());
  aos->Close();

  // 10.1587ms @ 44.1kHz shall work (see test above).
  aos = aosw.Create(44100, 448);
  EXPECT_TRUE(aos->Open());
  aos->Close();

  // 5.8050ms @ 44.1 should work.
  aos = aosw.Create(44100, 256);
  EXPECT_TRUE(aos->Open());
  aos->Close();

  // 4.9887ms @ 44.1kHz does not work to misalignment.
  // This test will propose an aligned buffer size of 5.0794ms.
  // Note that we must call Close() even is Open() fails since Close() also
  // deletes the object and we want to create a new object in the next test.
  aos = aosw.Create(44100, 220);
  EXPECT_FALSE(aos->Open());
  aos->Close();

  // 5.0794ms @ 44.1kHz shall work (see test above).
  aos = aosw.Create(44100, 224);
  EXPECT_TRUE(aos->Open());
  aos->Close();

  // 2.9025ms is smaller than the minimum supported size (=3ms).
  aos = aosw.Create(44100, 132);
  EXPECT_FALSE(aos->Open());
  aos->Close();

  // 3.01587ms is larger than the minimum size but is not aligned.
  // This test will propose an aligned buffer size of 3.6281ms.
  aos = aosw.Create(44100, 133);
  EXPECT_FALSE(aos->Open());
  aos->Close();

  // 3.6281ms @ 44.1kHz <=> smallest possible buffer size we can use.
  aos = aosw.Create(44100, 160);
  EXPECT_TRUE(aos->Open());
  aos->Close();
}

// Verify that we can open and start the output stream in exclusive mode at
// the lowest possible delay at 48kHz.
TEST(WASAPIAudioOutputStreamTest, ExclusiveModeMinBufferSizeAt48kHz) {
  if (!ExclusiveModeIsEnabled())
    return;

  scoped_ptr<AudioManager> audio_manager(AudioManager::CreateForTesting());
  if (!CanRunAudioTests(audio_manager.get()))
    return;

  base::MessageLoopForUI loop;
  MockAudioSourceCallback source;

  // Create exclusive-mode WASAPI output stream which plays out in stereo
  // using the minimum buffer size at 48kHz sample rate.
  AudioOutputStreamWrapper aosw(audio_manager.get());
  AudioOutputStream* aos = aosw.Create(48000, 160);
  EXPECT_TRUE(aos->Open());

  // Derive the expected size in bytes of each packet.
  uint32 bytes_per_packet = aosw.channels() * aosw.samples_per_packet() *
      (aosw.bits_per_sample() / 8);

  // Set up expected minimum delay estimation.
  AudioBuffersState state(0, bytes_per_packet);

 // Wait for the first callback and verify its parameters.
  EXPECT_CALL(source, OnMoreData(NotNull(), HasValidDelay(state)))
      .WillOnce(DoAll(
          QuitLoop(loop.message_loop_proxy()),
          Return(aosw.samples_per_packet())))
      .WillRepeatedly(Return(aosw.samples_per_packet()));

  aos->Start(&source);
  loop.PostDelayedTask(FROM_HERE, base::MessageLoop::QuitClosure(),
                       TestTimeouts::action_timeout());
  loop.Run();
  aos->Stop();
  aos->Close();
}

// Verify that we can open and start the output stream in exclusive mode at
// the lowest possible delay at 44.1kHz.
TEST(WASAPIAudioOutputStreamTest, ExclusiveModeMinBufferSizeAt44kHz) {
  if (!ExclusiveModeIsEnabled())
    return;

  scoped_ptr<AudioManager> audio_manager(AudioManager::CreateForTesting());
  if (!CanRunAudioTests(audio_manager.get()))
    return;

  base::MessageLoopForUI loop;
  MockAudioSourceCallback source;

  // Create exclusive-mode WASAPI output stream which plays out in stereo
  // using the minimum buffer size at 44.1kHz sample rate.
  AudioOutputStreamWrapper aosw(audio_manager.get());
  AudioOutputStream* aos = aosw.Create(44100, 160);
  EXPECT_TRUE(aos->Open());

  // Derive the expected size in bytes of each packet.
  uint32 bytes_per_packet = aosw.channels() * aosw.samples_per_packet() *
      (aosw.bits_per_sample() / 8);

  // Set up expected minimum delay estimation.
  AudioBuffersState state(0, bytes_per_packet);

  // Wait for the first callback and verify its parameters.
  EXPECT_CALL(source, OnMoreData(NotNull(), HasValidDelay(state)))
    .WillOnce(DoAll(
        QuitLoop(loop.message_loop_proxy()),
        Return(aosw.samples_per_packet())))
    .WillRepeatedly(Return(aosw.samples_per_packet()));

  aos->Start(&source);
  loop.PostDelayedTask(FROM_HERE, base::MessageLoop::QuitClosure(),
                        TestTimeouts::action_timeout());
  loop.Run();
  aos->Stop();
  aos->Close();
}

}  // namespace media
