// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "media/base/android/audio_media_codec_decoder.h"
#include "media/base/android/media_codec_util.h"
#include "media/base/android/media_statistics.h"
#include "media/base/android/sdk_media_codec_bridge.h"
#include "media/base/android/test_data_factory.h"
#include "media/base/android/test_statistics.h"
#include "media/base/android/video_media_codec_decoder.h"
#include "media/base/timestamp_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/android/surface_texture.h"

namespace media {

namespace {

const base::TimeDelta kDefaultTimeout = base::TimeDelta::FromMilliseconds(200);
const base::TimeDelta kAudioFramePeriod =
    base::TimeDelta::FromSecondsD(1024.0 / 44100);  // 1024 samples @ 44100 Hz
const base::TimeDelta kVideoFramePeriod = base::TimeDelta::FromMilliseconds(20);

// A helper function to calculate the expected number of frames.
int GetFrameCount(base::TimeDelta duration, base::TimeDelta frame_period) {
  // A chunk has 4 access units. The last unit timestamp must exceed the
  // duration. Last chunk has 3 regular access units and one stand-alone EOS
  // unit that we do not count.

  // Number of time intervals to exceed duration.
  int num_intervals = duration / frame_period + 1.0;

  // To cover these intervals we need one extra unit at the beginning.
  int num_units = num_intervals + 1;

  // Number of 4-unit chunks that hold these units:
  int num_chunks = (num_units + 3) / 4;

  // Altogether these chunks hold 4*num_chunks units, but we do not count
  // the last EOS as a frame.
  return 4 * num_chunks - 1;
}

class AudioFactory : public TestDataFactory {
 public:
  AudioFactory(base::TimeDelta duration);
  DemuxerConfigs GetConfigs() const override;

 protected:
  void ModifyChunk(DemuxerData* chunk) override;
};

class VideoFactory : public TestDataFactory {
 public:
  VideoFactory(base::TimeDelta duration);
  DemuxerConfigs GetConfigs() const override;

 protected:
  void ModifyChunk(DemuxerData* chunk) override;
};

AudioFactory::AudioFactory(base::TimeDelta duration)
    : TestDataFactory("aac-44100-packet-%d", duration, kAudioFramePeriod) {
}

DemuxerConfigs AudioFactory::GetConfigs() const {
  return TestDataFactory::CreateAudioConfigs(kCodecAAC, duration_);
}

void AudioFactory::ModifyChunk(DemuxerData* chunk) {
  DCHECK(chunk);
  for (AccessUnit& unit : chunk->access_units) {
    if (!unit.data.empty())
      unit.is_key_frame = true;
  }
}

VideoFactory::VideoFactory(base::TimeDelta duration)
    : TestDataFactory("h264-320x180-frame-%d", duration, kVideoFramePeriod) {
}

DemuxerConfigs VideoFactory::GetConfigs() const {
  return TestDataFactory::CreateVideoConfigs(kCodecH264, duration_,
                                             gfx::Size(320, 180));
}

void VideoFactory::ModifyChunk(DemuxerData* chunk) {
  // The frames are taken from High profile and some are B-frames.
  // The first 4 frames appear in the file in the following order:
  //
  // Frames:             I P B P
  // Decoding order:     0 1 2 3
  // Presentation order: 0 2 1 4(3)
  //
  // I keep the last PTS to be 3 for simplicity.

  // If the chunk contains EOS, it should not break the presentation order.
  // For instance, the following chunk is ok:
  //
  // Frames:             I P B EOS
  // Decoding order:     0 1 2 -
  // Presentation order: 0 2 1 -
  //
  // while this one might cause decoder to block:
  //
  // Frames:             I P EOS
  // Decoding order:     0 1 -
  // Presentation order: 0 2 -  <------- might wait for the B frame forever
  //
  // With current base class implementation that always has EOS at the 4th
  // place we are covered (http://crbug.com/526755)

  DCHECK(chunk);
  DCHECK(chunk->access_units.size() == 4);

  // Swap pts for second and third frames. Make first frame a key frame.
  base::TimeDelta tmp = chunk->access_units[1].timestamp;
  chunk->access_units[1].timestamp = chunk->access_units[2].timestamp;
  chunk->access_units[2].timestamp = tmp;

  chunk->access_units[0].is_key_frame = true;
}

}  // namespace (anonymous)

// The test fixture for MediaCodecDecoder

class MediaCodecDecoderTest : public testing::Test {
 public:
  MediaCodecDecoderTest();
  ~MediaCodecDecoderTest() override;

  // Conditions we wait for.
  bool is_prefetched() const { return is_prefetched_; }
  bool is_stopped() const { return is_stopped_; }
  bool is_starved() const { return is_starved_; }

  void SetPrefetched(bool value) { is_prefetched_ = value; }
  void SetStopped(bool value) { is_stopped_ = value; }
  void SetStarved(bool value) { is_starved_ = value; }

 protected:
  typedef base::Callback<bool()> Predicate;

  typedef base::Callback<void(const DemuxerData&)> DataAvailableCallback;

  // Waits for condition to become true or for timeout to expire.
  // Returns true if the condition becomes true.
  bool WaitForCondition(const Predicate& condition,
                        const base::TimeDelta& timeout = kDefaultTimeout);

  void SetDataFactory(std::unique_ptr<TestDataFactory> factory) {
    data_factory_ = std::move(factory);
  }

  DemuxerConfigs GetConfigs() const {
    // ASSERT_NE does not compile here because it expects void return value.
    EXPECT_NE(nullptr, data_factory_.get());
    return data_factory_->GetConfigs();
  }

  void CreateAudioDecoder();
  void CreateVideoDecoder();
  void SetVideoSurface();
  void SetStopRequestAtTime(const base::TimeDelta& time) {
    stop_request_time_ = time;
  }

  // Decoder callbacks.
  void OnDataRequested();
  void OnStarvation() { is_starved_ = true; }
  void OnDecoderDrained() {}
  void OnStopDone() { is_stopped_ = true; }
  void OnKeyRequired() {}
  void OnError() { DVLOG(0) << "MediaCodecDecoderTest::" << __FUNCTION__; }
  void OnUpdateCurrentTime(base::TimeDelta now_playing,
                           base::TimeDelta last_buffered,
                           bool postpone) {
    // Add the |last_buffered| value for PTS. For video it is the same as
    // |now_playing| and is equal to PTS, for audio |last_buffered| should
    // exceed PTS.
    if (postpone)
      return;

    pts_stat_.AddValue(last_buffered);

    if (stop_request_time_ != kNoTimestamp() &&
        now_playing >= stop_request_time_) {
      stop_request_time_ = kNoTimestamp();
      decoder_->RequestToStop();
    }
  }

  void OnVideoSizeChanged(const gfx::Size& video_size) {
    video_size_ = video_size;
  }

  void OnVideoCodecCreated() {}

  std::unique_ptr<MediaCodecDecoder> decoder_;
  std::unique_ptr<TestDataFactory> data_factory_;
  Minimax<base::TimeDelta> pts_stat_;
  gfx::Size video_size_;

 private:
  bool is_timeout_expired() const { return is_timeout_expired_; }
  void SetTimeoutExpired(bool value) { is_timeout_expired_ = value; }

  base::MessageLoop message_loop_;
  bool is_timeout_expired_;

  bool is_prefetched_;
  bool is_stopped_;
  bool is_starved_;
  base::TimeDelta stop_request_time_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  FrameStatistics frame_statistics_;
  DataAvailableCallback data_available_cb_;
  scoped_refptr<gl::SurfaceTexture> surface_texture_;

  DISALLOW_COPY_AND_ASSIGN(MediaCodecDecoderTest);
};

MediaCodecDecoderTest::MediaCodecDecoderTest()
    : is_timeout_expired_(false),
      is_prefetched_(false),
      is_stopped_(false),
      is_starved_(false),
      stop_request_time_(kNoTimestamp()),
      task_runner_(base::ThreadTaskRunnerHandle::Get()) {
}

MediaCodecDecoderTest::~MediaCodecDecoderTest() {}

bool MediaCodecDecoderTest::WaitForCondition(const Predicate& condition,
                                             const base::TimeDelta& timeout) {
  // Let the message_loop_ process events.
  // We start the timer and RunUntilIdle() until it signals.

  SetTimeoutExpired(false);

  base::Timer timer(false, false);
  timer.Start(FROM_HERE, timeout,
              base::Bind(&MediaCodecDecoderTest::SetTimeoutExpired,
                         base::Unretained(this), true));

  do {
    if (condition.Run()) {
      timer.Stop();
      return true;
    }
    message_loop_.RunUntilIdle();
  } while (!is_timeout_expired());

  DCHECK(!timer.IsRunning());
  return false;
}

void MediaCodecDecoderTest::CreateAudioDecoder() {
  decoder_ = base::WrapUnique(new AudioMediaCodecDecoder(
      task_runner_, &frame_statistics_,
      base::Bind(&MediaCodecDecoderTest::OnDataRequested,
                 base::Unretained(this)),
      base::Bind(&MediaCodecDecoderTest::OnStarvation, base::Unretained(this)),
      base::Bind(&MediaCodecDecoderTest::OnDecoderDrained,
                 base::Unretained(this)),
      base::Bind(&MediaCodecDecoderTest::OnStopDone, base::Unretained(this)),
      base::Bind(&MediaCodecDecoderTest::OnKeyRequired, base::Unretained(this)),
      base::Bind(&MediaCodecDecoderTest::OnError, base::Unretained(this)),
      base::Bind(&MediaCodecDecoderTest::OnUpdateCurrentTime,
                 base::Unretained(this))));

  data_available_cb_ = base::Bind(&MediaCodecDecoder::OnDemuxerDataAvailable,
                                  base::Unretained(decoder_.get()));
}

void MediaCodecDecoderTest::CreateVideoDecoder() {
  decoder_ = base::WrapUnique(new VideoMediaCodecDecoder(
      task_runner_, &frame_statistics_,
      base::Bind(&MediaCodecDecoderTest::OnDataRequested,
                 base::Unretained(this)),
      base::Bind(&MediaCodecDecoderTest::OnStarvation, base::Unretained(this)),
      base::Bind(&MediaCodecDecoderTest::OnDecoderDrained,
                 base::Unretained(this)),
      base::Bind(&MediaCodecDecoderTest::OnStopDone, base::Unretained(this)),
      base::Bind(&MediaCodecDecoderTest::OnKeyRequired, base::Unretained(this)),
      base::Bind(&MediaCodecDecoderTest::OnError, base::Unretained(this)),
      base::Bind(&MediaCodecDecoderTest::OnUpdateCurrentTime,
                 base::Unretained(this)),
      base::Bind(&MediaCodecDecoderTest::OnVideoSizeChanged,
                 base::Unretained(this))));

  data_available_cb_ = base::Bind(&MediaCodecDecoder::OnDemuxerDataAvailable,
                                  base::Unretained(decoder_.get()));
}

void MediaCodecDecoderTest::OnDataRequested() {
  if (!data_factory_)
    return;

  DemuxerData data;
  base::TimeDelta delay;
  if (!data_factory_->CreateChunk(&data, &delay))
    return;

  task_runner_->PostDelayedTask(FROM_HERE, base::Bind(data_available_cb_, data),
                                delay);
}

void MediaCodecDecoderTest::SetVideoSurface() {
  surface_texture_ = gl::SurfaceTexture::Create(0);
  gl::ScopedJavaSurface surface(surface_texture_.get());
  ASSERT_NE(nullptr, decoder_.get());
  VideoMediaCodecDecoder* video_decoder =
      static_cast<VideoMediaCodecDecoder*>(decoder_.get());
  video_decoder->SetVideoSurface(std::move(surface));
}

TEST_F(MediaCodecDecoderTest, AudioPrefetch) {
  CreateAudioDecoder();

  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(500);
  SetDataFactory(base::WrapUnique(new AudioFactory(duration)));

  decoder_->Prefetch(base::Bind(&MediaCodecDecoderTest::SetPrefetched,
                                base::Unretained(this), true));

  EXPECT_TRUE(WaitForCondition(base::Bind(&MediaCodecDecoderTest::is_prefetched,
                                          base::Unretained(this))));
}

TEST_F(MediaCodecDecoderTest, VideoPrefetch) {
  CreateVideoDecoder();

  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(500);
  SetDataFactory(base::WrapUnique(new VideoFactory(duration)));

  decoder_->Prefetch(base::Bind(&MediaCodecDecoderTest::SetPrefetched,
                                base::Unretained(this), true));

  EXPECT_TRUE(WaitForCondition(base::Bind(&MediaCodecDecoderTest::is_prefetched,
                                          base::Unretained(this))));
}

TEST_F(MediaCodecDecoderTest, AudioConfigureNoParams) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  CreateAudioDecoder();

  // Cannot configure without config parameters.
  EXPECT_EQ(MediaCodecDecoder::kConfigFailure, decoder_->Configure(nullptr));
}

TEST_F(MediaCodecDecoderTest, AudioConfigureValidParams) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  CreateAudioDecoder();

  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(500);
  std::unique_ptr<AudioFactory> factory(new AudioFactory(duration));
  decoder_->SetDemuxerConfigs(factory->GetConfigs());

  EXPECT_EQ(MediaCodecDecoder::kConfigOk, decoder_->Configure(nullptr));
}

TEST_F(MediaCodecDecoderTest, VideoConfigureNoParams) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  CreateVideoDecoder();

  // decoder_->Configure() searches back for the key frame.
  // We have to prefetch decoder.

  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(500);
  SetDataFactory(base::WrapUnique(new VideoFactory(duration)));

  decoder_->Prefetch(base::Bind(&MediaCodecDecoderTest::SetPrefetched,
                                base::Unretained(this), true));

  EXPECT_TRUE(WaitForCondition(base::Bind(&MediaCodecDecoderTest::is_prefetched,
                                          base::Unretained(this))));

  SetVideoSurface();

  // Cannot configure without config parameters.
  EXPECT_EQ(MediaCodecDecoder::kConfigFailure, decoder_->Configure(nullptr));
}

TEST_F(MediaCodecDecoderTest, VideoConfigureNoSurface) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  CreateVideoDecoder();

  // decoder_->Configure() searches back for the key frame.
  // We have to prefetch decoder.

  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(500);
  SetDataFactory(base::WrapUnique(new VideoFactory(duration)));

  decoder_->Prefetch(base::Bind(&MediaCodecDecoderTest::SetPrefetched,
                                base::Unretained(this), true));

  EXPECT_TRUE(WaitForCondition(base::Bind(&MediaCodecDecoderTest::is_prefetched,
                                          base::Unretained(this))));

  decoder_->SetDemuxerConfigs(GetConfigs());

  // Surface is not set, Configure() should fail.

  EXPECT_EQ(MediaCodecDecoder::kConfigFailure, decoder_->Configure(nullptr));
}

TEST_F(MediaCodecDecoderTest, VideoConfigureInvalidSurface) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  CreateVideoDecoder();

  // decoder_->Configure() searches back for the key frame.
  // We have to prefetch decoder.

  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(500);
  SetDataFactory(base::WrapUnique(new VideoFactory(duration)));

  decoder_->Prefetch(base::Bind(&MediaCodecDecoderTest::SetPrefetched,
                                base::Unretained(this), true));

  EXPECT_TRUE(WaitForCondition(base::Bind(&MediaCodecDecoderTest::is_prefetched,
                                          base::Unretained(this))));

  decoder_->SetDemuxerConfigs(GetConfigs());

  // Prepare the surface.
  scoped_refptr<gl::SurfaceTexture> surface_texture(
      gl::SurfaceTexture::Create(0));
  gl::ScopedJavaSurface surface(surface_texture.get());

  // Release the surface texture.
  surface_texture = NULL;

  VideoMediaCodecDecoder* video_decoder =
      static_cast<VideoMediaCodecDecoder*>(decoder_.get());
  video_decoder->SetVideoSurface(std::move(surface));

  EXPECT_EQ(MediaCodecDecoder::kConfigFailure, decoder_->Configure(nullptr));
}

TEST_F(MediaCodecDecoderTest, VideoConfigureValidParams) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  CreateVideoDecoder();

  // decoder_->Configure() searches back for the key frame.
  // We have to prefetch decoder.

  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(500);
  SetDataFactory(base::WrapUnique(new VideoFactory(duration)));

  decoder_->Prefetch(base::Bind(&MediaCodecDecoderTest::SetPrefetched,
                                base::Unretained(this), true));

  EXPECT_TRUE(WaitForCondition(base::Bind(&MediaCodecDecoderTest::is_prefetched,
                                          base::Unretained(this))));

  decoder_->SetDemuxerConfigs(GetConfigs());

  SetVideoSurface();

  // Now we can expect Configure() to succeed.

  EXPECT_EQ(MediaCodecDecoder::kConfigOk, decoder_->Configure(nullptr));
}

TEST_F(MediaCodecDecoderTest, AudioStartWithoutConfigure) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  CreateAudioDecoder();

  // Decoder has to be prefetched and configured before the start.

  // Wrong state: not prefetched
  EXPECT_FALSE(decoder_->Start(base::TimeDelta::FromMilliseconds(0)));

  // Do the prefetch.
  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(500);
  SetDataFactory(base::WrapUnique(new AudioFactory(duration)));

  // Prefetch to avoid starvation at the beginning of playback.
  decoder_->Prefetch(base::Bind(&MediaCodecDecoderTest::SetPrefetched,
                                base::Unretained(this), true));

  EXPECT_TRUE(WaitForCondition(base::Bind(&MediaCodecDecoderTest::is_prefetched,
                                          base::Unretained(this))));

  // Still, decoder is not configured.
  EXPECT_FALSE(decoder_->Start(base::TimeDelta::FromMilliseconds(0)));
}

// http://crbug.com/518900
TEST_F(MediaCodecDecoderTest, DISABLED_AudioPlayTillCompletion) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  DVLOG(0) << "AudioPlayTillCompletion started";

  CreateAudioDecoder();

  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(500);
  base::TimeDelta timeout = base::TimeDelta::FromMilliseconds(1500);

  SetDataFactory(base::WrapUnique(new AudioFactory(duration)));

  // Prefetch to avoid starvation at the beginning of playback.
  decoder_->Prefetch(base::Bind(&MediaCodecDecoderTest::SetPrefetched,
                                base::Unretained(this), true));

  EXPECT_TRUE(WaitForCondition(base::Bind(&MediaCodecDecoderTest::is_prefetched,
                                          base::Unretained(this))));

  decoder_->SetDemuxerConfigs(GetConfigs());

  EXPECT_EQ(MediaCodecDecoder::kConfigOk, decoder_->Configure(nullptr));

  EXPECT_TRUE(decoder_->Start(base::TimeDelta::FromMilliseconds(0)));

  EXPECT_TRUE(WaitForCondition(
      base::Bind(&MediaCodecDecoderTest::is_stopped, base::Unretained(this)),
      timeout));

  EXPECT_TRUE(decoder_->IsStopped());
  EXPECT_TRUE(decoder_->IsCompleted());

  // Last buffered timestamp should be no less than PTS.
  // The number of hits in pts_stat_ depends on the preroll implementation.
  // We might not report the time for the first buffer after preroll that
  // is written to the audio track. pts_stat_.num_values() is either 21 or 22.
  EXPECT_LE(21, pts_stat_.num_values());
  EXPECT_LE(data_factory_->last_pts(), pts_stat_.max());

  DVLOG(0) << "AudioPlayTillCompletion stopping";
}

// crbug.com/618274
#if defined(OS_ANDROID)
#define MAYBE_VideoPlayTillCompletion DISABLED_VideoPlayTillCompletion
#else
#define MAYBE_VideoPlayTillCompletion VideoPlayTillCompletion
#endif
TEST_F(MediaCodecDecoderTest, MAYBE_VideoPlayTillCompletion) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  CreateVideoDecoder();

  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(500);
  // The first output frame might come out with significant delay. Apparently
  // the codec does initial configuration at this time. We increase the timeout
  // to leave a room of 1 second for this initial configuration.
  base::TimeDelta timeout = base::TimeDelta::FromMilliseconds(1500);
  SetDataFactory(base::WrapUnique(new VideoFactory(duration)));

  // Prefetch
  decoder_->Prefetch(base::Bind(&MediaCodecDecoderTest::SetPrefetched,
                                base::Unretained(this), true));

  EXPECT_TRUE(WaitForCondition(base::Bind(&MediaCodecDecoderTest::is_prefetched,
                                          base::Unretained(this))));

  decoder_->SetDemuxerConfigs(GetConfigs());

  SetVideoSurface();

  EXPECT_EQ(MediaCodecDecoder::kConfigOk, decoder_->Configure(nullptr));

  EXPECT_TRUE(decoder_->Start(base::TimeDelta::FromMilliseconds(0)));

  EXPECT_TRUE(WaitForCondition(
      base::Bind(&MediaCodecDecoderTest::is_stopped, base::Unretained(this)),
      timeout));

  EXPECT_TRUE(decoder_->IsStopped());
  EXPECT_TRUE(decoder_->IsCompleted());

  int expected_video_frames = GetFrameCount(duration, kVideoFramePeriod);
  EXPECT_EQ(expected_video_frames, pts_stat_.num_values());
  EXPECT_EQ(data_factory_->last_pts(), pts_stat_.max());
}

// Disabled per http://crbug.com/611489.
TEST_F(MediaCodecDecoderTest, DISABLED_VideoStopAndResume) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  CreateVideoDecoder();

  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(500);
  base::TimeDelta stop_request_time = base::TimeDelta::FromMilliseconds(200);
  base::TimeDelta timeout = base::TimeDelta::FromMilliseconds(1000);

  SetDataFactory(base::WrapUnique(new VideoFactory(duration)));

  // Prefetch
  decoder_->Prefetch(base::Bind(&MediaCodecDecoderTest::SetPrefetched,
                                base::Unretained(this), true));

  EXPECT_TRUE(WaitForCondition(base::Bind(&MediaCodecDecoderTest::is_prefetched,
                                          base::Unretained(this))));

  decoder_->SetDemuxerConfigs(GetConfigs());

  SetVideoSurface();

  EXPECT_EQ(MediaCodecDecoder::kConfigOk, decoder_->Configure(nullptr));

  SetStopRequestAtTime(stop_request_time);

  // Start from the beginning.
  EXPECT_TRUE(decoder_->Start(base::TimeDelta::FromMilliseconds(0)));

  EXPECT_TRUE(WaitForCondition(
      base::Bind(&MediaCodecDecoderTest::is_stopped, base::Unretained(this)),
      timeout));

  EXPECT_TRUE(decoder_->IsStopped());
  EXPECT_FALSE(decoder_->IsCompleted());

  base::TimeDelta last_pts = pts_stat_.max();

  EXPECT_GE(last_pts, stop_request_time);

  // Resume playback from last_pts:

  SetPrefetched(false);
  SetStopped(false);

  // Prefetch again.
  decoder_->Prefetch(base::Bind(&MediaCodecDecoderTest::SetPrefetched,
                                base::Unretained(this), true));

  EXPECT_TRUE(WaitForCondition(base::Bind(&MediaCodecDecoderTest::is_prefetched,
                                          base::Unretained(this))));

  // Then start.
  EXPECT_TRUE(decoder_->Start(last_pts));

  // Wait till completion.
  EXPECT_TRUE(WaitForCondition(
      base::Bind(&MediaCodecDecoderTest::is_stopped, base::Unretained(this)),
      timeout));

  EXPECT_TRUE(decoder_->IsStopped());
  EXPECT_TRUE(decoder_->IsCompleted());

  // We should not skip frames in this process.
  int expected_video_frames = GetFrameCount(duration, kVideoFramePeriod);
  EXPECT_EQ(expected_video_frames, pts_stat_.num_values());
  EXPECT_EQ(data_factory_->last_pts(), pts_stat_.max());
}

// http://crbug.com/518900
TEST_F(MediaCodecDecoderTest, DISABLED_AudioStarvationAndStop) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  CreateAudioDecoder();

  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(200);
  base::TimeDelta timeout = base::TimeDelta::FromMilliseconds(400);

  AudioFactory* factory = new AudioFactory(duration);
  factory->SetStarvationMode(true);
  SetDataFactory(base::WrapUnique(factory));

  // Prefetch.
  decoder_->Prefetch(base::Bind(&MediaCodecDecoderTest::SetPrefetched,
                                base::Unretained(this), true));

  EXPECT_TRUE(WaitForCondition(base::Bind(&MediaCodecDecoderTest::is_prefetched,
                                          base::Unretained(this))));

  // Configure.
  decoder_->SetDemuxerConfigs(GetConfigs());

  EXPECT_EQ(MediaCodecDecoder::kConfigOk, decoder_->Configure(nullptr));

  // Start.
  EXPECT_TRUE(decoder_->Start(base::TimeDelta::FromMilliseconds(0)));

  // Wait for starvation.
  EXPECT_TRUE(WaitForCondition(
      base::Bind(&MediaCodecDecoderTest::is_starved, base::Unretained(this)),
      timeout));

  EXPECT_FALSE(decoder_->IsStopped());
  EXPECT_FALSE(decoder_->IsCompleted());

  EXPECT_GT(pts_stat_.num_values(), 0);

  // After starvation we should be able to stop decoder.
  decoder_->RequestToStop();

  EXPECT_TRUE(WaitForCondition(
      base::Bind(&MediaCodecDecoderTest::is_stopped, base::Unretained(this))));

  EXPECT_TRUE(decoder_->IsStopped());
  EXPECT_FALSE(decoder_->IsCompleted());
}

// Disabled per http://crbug.com/611489.
TEST_F(MediaCodecDecoderTest, DISABLED_VideoFirstUnitIsReconfig) {
  SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE();

  // Test that the kConfigChanged unit that comes before the first data unit
  // gets processed, i.e. is not lost.

  CreateVideoDecoder();

  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(200);
  base::TimeDelta timeout = base::TimeDelta::FromMilliseconds(1000);
  SetDataFactory(base::WrapUnique(new VideoFactory(duration)));

  // Ask factory to produce initial configuration unit. The configuraton will
  // be factory.GetConfigs().
  data_factory_->RequestInitialConfigs();

  // Create am alternative configuration (we just alter video size).
  DemuxerConfigs alt_configs = data_factory_->GetConfigs();
  alt_configs.video_size = gfx::Size(100, 100);

  // Pass the alternative configuration to decoder.
  decoder_->SetDemuxerConfigs(alt_configs);

  // Prefetch.
  decoder_->Prefetch(base::Bind(&MediaCodecDecoderTest::SetPrefetched,
                                base::Unretained(this), true));

  EXPECT_TRUE(WaitForCondition(base::Bind(&MediaCodecDecoderTest::is_prefetched,
                                          base::Unretained(this))));

  // Current implementation reports the new video size after
  // SetDemuxerConfigs(), verify that it is alt size.
  EXPECT_EQ(alt_configs.video_size, video_size_);

  SetVideoSurface();

  // Configure.
  EXPECT_EQ(MediaCodecDecoder::kConfigOk, decoder_->Configure(nullptr));

  // Start.
  EXPECT_TRUE(decoder_->Start(base::TimeDelta::FromMilliseconds(0)));

  // Wait for completion.
  EXPECT_TRUE(WaitForCondition(
      base::Bind(&MediaCodecDecoderTest::is_stopped, base::Unretained(this)),
      timeout));

  EXPECT_TRUE(decoder_->IsStopped());
  EXPECT_TRUE(decoder_->IsCompleted());
  EXPECT_EQ(data_factory_->last_pts(), pts_stat_.max());

  // Check that the reported video size is the one from the in-stream configs.
  EXPECT_EQ(data_factory_->GetConfigs().video_size, video_size_);
}

}  // namespace media
