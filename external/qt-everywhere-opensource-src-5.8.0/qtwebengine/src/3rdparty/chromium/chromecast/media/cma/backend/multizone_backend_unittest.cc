// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <stdlib.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chromecast/base/task_runner_impl.h"
#include "chromecast/media/cma/base/decoder_buffer_adapter.h"
#include "chromecast/media/cma/base/decoder_config_adapter.h"
#include "chromecast/public/cast_media_shlib.h"
#include "chromecast/public/media/cast_decoder_buffer.h"
#include "chromecast/public/media/decoder_config.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/public/media/media_pipeline_device_params.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {

class MultizoneBackendTest;

namespace {

const int64_t kMicrosecondsPerSecond = 1000 * 1000;
// Total length of test, in microseconds.
const int64_t kPushTimeUs = 2 * kMicrosecondsPerSecond;
const int64_t kStartPts = 0;
const int64_t kRenderingDelayGracePeriodUs = 250 * 1000;
const int64_t kMaxRenderingDelayErrorUs = 200;
const int kNumEffectsStreams = 1;

void IgnoreEos() {}

class BufferFeeder : public MediaPipelineBackend::Decoder::Delegate {
 public:
  BufferFeeder(const AudioConfig& config,
               bool effects_only,
               const base::Closure& eos_cb);
  ~BufferFeeder() override {}

  void Initialize();
  void Start();
  void Stop();

  int64_t max_rendering_delay_error_us() {
    return max_rendering_delay_error_us_;
  }

  int64_t max_positive_rendering_delay_error_us() {
    return max_positive_rendering_delay_error_us_;
  }

  int64_t max_negative_rendering_delay_error_us() {
    return max_negative_rendering_delay_error_us_;
  }

  int64_t average_rendering_delay_error_us() {
    return total_rendering_delay_error_us_ / sample_count_;
  }

 private:
  void FeedBuffer();

  // MediaPipelineBackend::Decoder::Delegate implementation:
  void OnPushBufferComplete(MediaPipelineBackend::BufferStatus status) override;
  void OnEndOfStream() override;
  void OnDecoderError() override {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (effects_only_) {
      feeding_completed_ = true;
    } else {
      ASSERT_TRUE(false);
    }
  }
  void OnKeyStatusChanged(const std::string& key_id,
                          CastKeyStatus key_status,
                          uint32_t system_code) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    ASSERT_TRUE(false);
  }
  void OnVideoResolutionChanged(const Size& size) override {
    DCHECK(thread_checker_.CalledOnValidThread());
  }

  const AudioConfig config_;
  const bool effects_only_;
  const base::Closure eos_cb_;
  int64_t max_rendering_delay_error_us_;
  int64_t max_positive_rendering_delay_error_us_;
  int64_t max_negative_rendering_delay_error_us_;
  int64_t total_rendering_delay_error_us_;
  size_t sample_count_;
  bool feeding_completed_;
  std::unique_ptr<TaskRunnerImpl> task_runner_;
  std::unique_ptr<MediaPipelineBackend> backend_;
  MediaPipelineBackend::AudioDecoder* decoder_;
  int64_t push_limit_us_;
  int64_t last_push_length_us_;
  int64_t pushed_us_;
  int64_t next_push_playback_timestamp_;
  scoped_refptr<DecoderBufferBase> pending_buffer_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(BufferFeeder);
};

}  // namespace

class MultizoneBackendTest : public testing::TestWithParam<int> {
 public:
  MultizoneBackendTest();
  ~MultizoneBackendTest() override;

  void SetUp() override {
    srand(12345);
    CastMediaShlib::Initialize(base::CommandLine::ForCurrentProcess()->argv());
  }

  void TearDown() override {
    // Pipeline must be destroyed before finalizing media shlib.
    audio_feeder_.reset();
    effects_feeders_.clear();
    CastMediaShlib::Finalize();
  }

  void AddEffectsStreams();

  void Initialize(int sample_rate);
  void Start();
  void OnEndOfStream();

 private:
  std::vector<std::unique_ptr<BufferFeeder>> effects_feeders_;
  std::unique_ptr<BufferFeeder> audio_feeder_;

  DISALLOW_COPY_AND_ASSIGN(MultizoneBackendTest);
};

namespace {

BufferFeeder::BufferFeeder(const AudioConfig& config,
                           bool effects_only,
                           const base::Closure& eos_cb)
    : config_(config),
      effects_only_(effects_only),
      eos_cb_(eos_cb),
      max_rendering_delay_error_us_(0),
      max_positive_rendering_delay_error_us_(0),
      max_negative_rendering_delay_error_us_(0),
      total_rendering_delay_error_us_(0),
      sample_count_(0),
      feeding_completed_(false),
      task_runner_(new TaskRunnerImpl()),
      decoder_(nullptr),
      push_limit_us_(effects_only_ ? 0 : kPushTimeUs),
      last_push_length_us_(0),
      pushed_us_(0),
      next_push_playback_timestamp_(0) {
  CHECK(!eos_cb_.is_null());
}

void BufferFeeder::Initialize() {
  MediaPipelineDeviceParams params(
      MediaPipelineDeviceParams::kModeIgnorePts,
      effects_only_ ? MediaPipelineDeviceParams::kAudioStreamSoundEffects
                    : MediaPipelineDeviceParams::kAudioStreamNormal,
      task_runner_.get());
  backend_.reset(CastMediaShlib::CreateMediaPipelineBackend(params));
  CHECK(backend_);

  decoder_ = backend_->CreateAudioDecoder();
  CHECK(decoder_);
  ASSERT_TRUE(decoder_->SetConfig(config_));
  decoder_->SetDelegate(this);

  ASSERT_TRUE(backend_->Initialize());
}

void BufferFeeder::Start() {
  ASSERT_TRUE(backend_->Start(kStartPts));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&BufferFeeder::FeedBuffer, base::Unretained(this)));
}

void BufferFeeder::Stop() {
  feeding_completed_ = true;
  ASSERT_TRUE(backend_->Stop());
}

void BufferFeeder::FeedBuffer() {
  CHECK(decoder_);
  if (feeding_completed_)
    return;

  if (!effects_only_ && pushed_us_ >= push_limit_us_) {
    pending_buffer_ = new media::DecoderBufferAdapter(
        ::media::DecoderBuffer::CreateEOSBuffer());
    feeding_completed_ = true;
    last_push_length_us_ = 0;
  } else {
    int size_bytes = (rand() % 128 + 16) * 16;
    int num_samples =
        size_bytes / (config_.bytes_per_channel * config_.channel_number);
    last_push_length_us_ =
        num_samples * kMicrosecondsPerSecond / config_.samples_per_second;
    scoped_refptr<::media::DecoderBuffer> silence_buffer(
        new ::media::DecoderBuffer(size_bytes));
    memset(silence_buffer->writable_data(), 0, silence_buffer->data_size());
    pending_buffer_ = new media::DecoderBufferAdapter(silence_buffer);
    pending_buffer_->set_timestamp(
        base::TimeDelta::FromMicroseconds(pushed_us_));
  }
  BufferStatus status = decoder_->PushBuffer(pending_buffer_.get());
  ASSERT_NE(status, MediaPipelineBackend::kBufferFailed);
  if (status == MediaPipelineBackend::kBufferPending)
    return;
  OnPushBufferComplete(MediaPipelineBackend::kBufferSuccess);
}

void BufferFeeder::OnEndOfStream() {
  DCHECK(thread_checker_.CalledOnValidThread());
  eos_cb_.Run();
}

void BufferFeeder::OnPushBufferComplete(BufferStatus status) {
  DCHECK(thread_checker_.CalledOnValidThread());
  pending_buffer_ = nullptr;
  ASSERT_NE(status, MediaPipelineBackend::kBufferFailed);

  if (!effects_only_) {
    MediaPipelineBackend::AudioDecoder::RenderingDelay delay =
        decoder_->GetRenderingDelay();
    int64_t expected_next_push_playback_timestamp =
        next_push_playback_timestamp_ + last_push_length_us_;
    next_push_playback_timestamp_ =
        delay.timestamp_microseconds + delay.delay_microseconds;
    // Check rendering delay accuracy only if we have pushed more than
    // kRenderingDelayGracePeriodUs of data.
    if (pushed_us_ > kRenderingDelayGracePeriodUs) {
      int64_t error =
          next_push_playback_timestamp_ - expected_next_push_playback_timestamp;
      max_rendering_delay_error_us_ =
          std::max(max_rendering_delay_error_us_, std::abs(error));
      total_rendering_delay_error_us_ += std::abs(error);
      if (error >= 0) {
        max_positive_rendering_delay_error_us_ =
            std::max(max_positive_rendering_delay_error_us_, error);
      } else {
        max_negative_rendering_delay_error_us_ =
            std::min(max_negative_rendering_delay_error_us_, error);
      }
      sample_count_++;
    }
  }
  pushed_us_ += last_push_length_us_;

  if (feeding_completed_)
    return;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&BufferFeeder::FeedBuffer, base::Unretained(this)));
}

}  // namespace

MultizoneBackendTest::MultizoneBackendTest() {}

MultizoneBackendTest::~MultizoneBackendTest() {}

void MultizoneBackendTest::Initialize(int sample_rate) {
  AudioConfig config;
  config.codec = kCodecPCM;
  config.sample_format = kSampleFormatS32;
  config.channel_number = 2;
  config.bytes_per_channel = 4;
  config.samples_per_second = sample_rate;

  audio_feeder_.reset(
      new BufferFeeder(config, false /* effects_only */,
                       base::Bind(&MultizoneBackendTest::OnEndOfStream,
                                  base::Unretained(this))));
  audio_feeder_->Initialize();
}

void MultizoneBackendTest::AddEffectsStreams() {
  AudioConfig effects_config;
  effects_config.codec = kCodecPCM;
  effects_config.sample_format = kSampleFormatS16;
  effects_config.channel_number = 2;
  effects_config.bytes_per_channel = 2;
  effects_config.samples_per_second = 48000;

  for (int i = 0; i < kNumEffectsStreams; ++i) {
    std::unique_ptr<BufferFeeder> feeder(new BufferFeeder(
        effects_config, true /* effects_only */, base::Bind(&IgnoreEos)));
    feeder->Initialize();
    effects_feeders_.push_back(std::move(feeder));
  }
}

void MultizoneBackendTest::Start() {
  for (auto& feeder : effects_feeders_)
    feeder->Start();
  CHECK(audio_feeder_);
  audio_feeder_->Start();
}

void MultizoneBackendTest::OnEndOfStream() {
  audio_feeder_->Stop();
  for (auto& feeder : effects_feeders_)
    feeder->Stop();

  base::MessageLoop::current()->QuitWhenIdle();

  EXPECT_LT(audio_feeder_->max_rendering_delay_error_us(),
            kMaxRenderingDelayErrorUs)
      << "Max positive rendering delay error: "
      << audio_feeder_->max_positive_rendering_delay_error_us()
      << "\nMax negative rendering delay error: "
      << audio_feeder_->max_negative_rendering_delay_error_us()
      << "\nAverage rendering delay error: "
      << audio_feeder_->average_rendering_delay_error_us();
}

TEST_P(MultizoneBackendTest, RenderingDelay) {
  std::unique_ptr<base::MessageLoop> message_loop(new base::MessageLoop());

  Initialize(GetParam());
  AddEffectsStreams();
  Start();
  message_loop->Run();
}

INSTANTIATE_TEST_CASE_P(Required,
                        MultizoneBackendTest,
                        ::testing::Values(8000,
                                          11025,
                                          12000,
                                          16000,
                                          22050,
                                          24000,
                                          32000,
                                          44100,
                                          48000));

INSTANTIATE_TEST_CASE_P(Optional,
                        MultizoneBackendTest,
                        ::testing::Values(64000, 88200, 96000));

}  // namespace media
}  // namespace chromecast
