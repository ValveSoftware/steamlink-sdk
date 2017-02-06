// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/audio_modem/audio_recorder.h"

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/aligned_memory.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/audio_modem/audio_recorder_impl.h"
#include "components/audio_modem/public/audio_modem_types.h"
#include "components/audio_modem/test/random_samples.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_manager.h"
#include "media/base/audio_bus.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const size_t kSomeNumber = 0x9999;

class TestAudioInputStream : public media::AudioInputStream {
 public:
  TestAudioInputStream(const media::AudioParameters& params,
                       const std::vector<float*> channel_data,
                       size_t samples)
      : callback_(nullptr), params_(params) {
    buffer_ = media::AudioBus::CreateWrapper(2);
    for (size_t i = 0; i < channel_data.size(); ++i)
      buffer_->SetChannelData(i, channel_data[i]);
    buffer_->set_frames(samples);
  }

  ~TestAudioInputStream() override {}

  bool Open() override { return true; }
  void Start(AudioInputCallback* callback) override {
    DCHECK(callback);
    callback_ = callback;
    media::AudioManager::Get()->GetTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&TestAudioInputStream::SimulateRecording,
                   base::Unretained(this)));
  }
  void Stop() override {}
  void Close() override {}
  double GetMaxVolume() override { return 1.0; }
  void SetVolume(double volume) override {}
  double GetVolume() override { return 1.0; }
  bool SetAutomaticGainControl(bool enabled) override { return false; }
  bool GetAutomaticGainControl() override { return true; }
  bool IsMuted() override { return false; }

 private:
  void SimulateRecording() {
    const int fpb = params_.frames_per_buffer();
    for (int i = 0; i < buffer_->frames() / fpb; ++i) {
      std::unique_ptr<media::AudioBus> source = media::AudioBus::Create(2, fpb);
      buffer_->CopyPartialFramesTo(i * fpb, fpb, 0, source.get());
      callback_->OnData(this, source.get(), fpb, 1.0);
    }
  }

  AudioInputCallback* callback_;
  media::AudioParameters params_;
  std::unique_ptr<media::AudioBus> buffer_;

  DISALLOW_COPY_AND_ASSIGN(TestAudioInputStream);
};

}  // namespace

namespace audio_modem {

class AudioRecorderTest : public testing::Test {
 public:
  AudioRecorderTest() : total_samples_(0), recorder_(nullptr) {
    audio_manager_ = media::AudioManager::CreateForTesting(
        base::ThreadTaskRunnerHandle::Get());
    base::RunLoop().RunUntilIdle();
  }

  ~AudioRecorderTest() override {
    DeleteRecorder();
    for (size_t i = 0; i < channel_data_.size(); ++i)
      base::AlignedFree(channel_data_[i]);
  }

  void CreateSimpleRecorder() {
    // If we have input devices, we'll create a recorder which uses a real
    // input stream, if not, we'll create a recorder which uses our mock input
    // stream.
    if (media::AudioManager::Get()->HasAudioInputDevices()) {
      DeleteRecorder();
      recorder_ = new AudioRecorderImpl();
      recorder_->Initialize(base::Bind(&AudioRecorderTest::DecodeSamples,
                                       base::Unretained(this)));
      base::RunLoop().RunUntilIdle();
    } else {
      CreateRecorder(kSomeNumber);
    }
  }

  void CreateRecorder(size_t samples) {
    DeleteRecorder();

    params_ = media::AudioManager::Get()->GetInputStreamParameters(
        media::AudioDeviceDescription::kDefaultDeviceId);

    channel_data_.clear();
    channel_data_.push_back(GenerateSamples(0x1337, samples));
    channel_data_.push_back(GenerateSamples(0x7331, samples));

    total_samples_ = samples;

    recorder_ = new AudioRecorderImpl();
    recorder_->set_input_stream_for_testing(
        new TestAudioInputStream(params_, channel_data_, samples));
    recorder_->set_params_for_testing(new media::AudioParameters(params_));
    recorder_->Initialize(
        base::Bind(&AudioRecorderTest::DecodeSamples, base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  void DeleteRecorder() {
    if (!recorder_)
      return;
    recorder_->Finalize();
    recorder_ = nullptr;
    base::RunLoop().RunUntilIdle();
  }

  void RecordAndVerifySamples() {
    received_samples_.clear();
    run_loop_.reset(new base::RunLoop());
    recorder_->Record();
    run_loop_->Run();
  }

  void DecodeSamples(const std::string& samples) {
    received_samples_ += samples;
    // We expect one less decode than our total samples would ideally have
    // triggered since we process data in 4k chunks. So our sample processing
    // will never rarely be perfectly aligned with 0.5s worth of samples, hence
    // we will almost always run with a buffer of leftover samples that will
    // not get sent to this callback since the recorder will be waiting for
    // more data.
    const size_t decode_buffer = params_.sample_rate() / 2;  // 0.5s
    const size_t expected_samples =
        (total_samples_ / decode_buffer - 1) * decode_buffer;
    const size_t expected_samples_size =
        expected_samples * sizeof(float) * params_.channels();
    if (received_samples_.size() == expected_samples_size) {
      VerifySamples();
      run_loop_->Quit();
    }
  }

  void VerifySamples() {
    int differences = 0;

    float* buffer_view =
        reinterpret_cast<float*>(string_as_array(&received_samples_));
    const int channels = params_.channels();
    const int frames =
        received_samples_.size() / sizeof(float) / params_.channels();
    for (int ch = 0; ch < channels; ++ch) {
      for (int si = 0, di = ch; si < frames; ++si, di += channels)
        differences += (buffer_view[di] != channel_data_[ch][si]);
    }

    ASSERT_EQ(0, differences);
  }

 protected:
  float* GenerateSamples(int random_seed, size_t size) {
    float* samples = static_cast<float*>(base::AlignedAlloc(
        size * sizeof(float), media::AudioBus::kChannelAlignment));
    PopulateSamples(0x1337, size, samples);
    return samples;
  }
  bool IsRecording() {
    base::RunLoop().RunUntilIdle();
    return recorder_->is_recording_;
  }

  content::TestBrowserThreadBundle thread_bundle_;
  media::ScopedAudioManagerPtr audio_manager_;

  std::vector<float*> channel_data_;
  media::AudioParameters params_;
  size_t total_samples_;

  // Deleted by calling Finalize() on the object.
  AudioRecorderImpl* recorder_;

  std::string received_samples_;

  std::unique_ptr<base::RunLoop> run_loop_;
};


// http://crbug.com/463854
#if defined(OS_MACOSX)
#define MAYBE_BasicRecordAndStop DISABLED_BasicRecordAndStop
#else
#define MAYBE_BasicRecordAndStop BasicRecordAndStop
#endif

TEST_F(AudioRecorderTest, MAYBE_BasicRecordAndStop) {
  CreateSimpleRecorder();

  recorder_->Record();
  EXPECT_TRUE(IsRecording());

  recorder_->Stop();
  EXPECT_FALSE(IsRecording());

  recorder_->Record();
  EXPECT_TRUE(IsRecording());

  recorder_->Stop();
  EXPECT_FALSE(IsRecording());

  recorder_->Record();
  EXPECT_TRUE(IsRecording());

  recorder_->Stop();
  EXPECT_FALSE(IsRecording());

  DeleteRecorder();
}

// http://crbug.com/460685
#if defined(OS_MACOSX)
#define MAYBE_OutOfOrderRecordAndStopMultiple \
  DISABLED_OutOfOrderRecordAndStopMultiple
#else
#define MAYBE_OutOfOrderRecordAndStopMultiple \
  OutOfOrderRecordAndStopMultiple
#endif

TEST_F(AudioRecorderTest, MAYBE_OutOfOrderRecordAndStopMultiple) {
  CreateSimpleRecorder();

  recorder_->Stop();
  recorder_->Stop();
  recorder_->Stop();
  EXPECT_FALSE(IsRecording());

  recorder_->Record();
  recorder_->Record();
  EXPECT_TRUE(IsRecording());

  recorder_->Stop();
  recorder_->Stop();
  EXPECT_FALSE(IsRecording());

  DeleteRecorder();
}

TEST_F(AudioRecorderTest, RecordingEndToEnd) {
  const int kNumSamples = 48000 * 3;
  CreateRecorder(kNumSamples);

  RecordAndVerifySamples();

  DeleteRecorder();
}

// TODO(rkc): Add tests with recording different sample rates.

}  // namespace audio_modem
