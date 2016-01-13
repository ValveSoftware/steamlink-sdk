// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/logging.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/media/mock_media_constraint_factory.h"
#include "content/renderer/media/webrtc/webrtc_local_audio_track_adapter.h"
#include "content/renderer/media/webrtc_audio_capturer.h"
#include "content/renderer/media/webrtc_local_audio_track.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_bus.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"

using ::testing::_;
using ::testing::AtLeast;

namespace content {

namespace {

class MockCapturerSource : public media::AudioCapturerSource {
 public:
  MockCapturerSource() {}
  MOCK_METHOD3(Initialize, void(const media::AudioParameters& params,
                                CaptureCallback* callback,
                                int session_id));
  MOCK_METHOD0(Start, void());
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD1(SetVolume, void(double volume));
  MOCK_METHOD1(SetAutomaticGainControl, void(bool enable));

 protected:
  virtual ~MockCapturerSource() {}
};

class MockPeerConnectionAudioSink : public PeerConnectionAudioSink {
 public:
  MockPeerConnectionAudioSink() {}
  ~MockPeerConnectionAudioSink() {}
  virtual int OnData(const int16* audio_data, int sample_rate,
                     int number_of_channels, int number_of_frames,
                     const std::vector<int>& channels,
                     int audio_delay_milliseconds, int current_volume,
                     bool need_audio_processing, bool key_pressed) OVERRIDE {
    EXPECT_EQ(sample_rate, params_.sample_rate());
    EXPECT_EQ(number_of_channels, params_.channels());
    EXPECT_EQ(number_of_frames, params_.frames_per_buffer());
    OnDataCallback(audio_data, channels, audio_delay_milliseconds,
                   current_volume, need_audio_processing, key_pressed);
    return 0;
  }
  MOCK_METHOD6(OnDataCallback, void(const int16* audio_data,
                                    const std::vector<int>& channels,
                                    int audio_delay_milliseconds,
                                    int current_volume,
                                    bool need_audio_processing,
                                    bool key_pressed));
  virtual void OnSetFormat(const media::AudioParameters& params) OVERRIDE {
    params_ = params;
    FormatIsSet();
  }
  MOCK_METHOD0(FormatIsSet, void());

 private:
  media::AudioParameters params_;
};

}  // namespace

class WebRtcAudioCapturerTest : public testing::Test {
 protected:
  WebRtcAudioCapturerTest()
#if defined(OS_ANDROID)
      : params_(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                media::CHANNEL_LAYOUT_STEREO, 48000, 16, 960) {
    // Android works with a buffer size bigger than 20ms.
#else
      : params_(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                media::CHANNEL_LAYOUT_STEREO, 48000, 16, 128) {
#endif
  }

  void DisableAudioTrackProcessing() {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableAudioTrackProcessing);
  }

  void VerifyAudioParams(const blink::WebMediaConstraints& constraints,
                         bool need_audio_processing) {
    capturer_ = WebRtcAudioCapturer::CreateCapturer(
        -1, StreamDeviceInfo(MEDIA_DEVICE_AUDIO_CAPTURE,
                             "", "", params_.sample_rate(),
                             params_.channel_layout(),
                             params_.frames_per_buffer()),
        constraints, NULL, NULL);
    capturer_source_ = new MockCapturerSource();
    EXPECT_CALL(*capturer_source_.get(), Initialize(_, capturer_.get(), -1));
    EXPECT_CALL(*capturer_source_.get(), SetAutomaticGainControl(true));
    EXPECT_CALL(*capturer_source_.get(), Start());
    capturer_->SetCapturerSourceForTesting(capturer_source_, params_);

    scoped_refptr<WebRtcLocalAudioTrackAdapter> adapter(
        WebRtcLocalAudioTrackAdapter::Create(std::string(), NULL));
    track_.reset(new WebRtcLocalAudioTrack(adapter, capturer_, NULL));
    track_->Start();

    // Connect a mock sink to the track.
    scoped_ptr<MockPeerConnectionAudioSink> sink(
        new MockPeerConnectionAudioSink());
    track_->AddSink(sink.get());

    int delay_ms = 65;
    bool key_pressed = true;
    double volume = 0.9;

    // MaxVolume() in WebRtcAudioCapturer is hard-coded to return 255, we add
    // 0.5 to do the correct truncation like the production code does.
    int expected_volume_value = volume * capturer_->MaxVolume() + 0.5;
    scoped_ptr<media::AudioBus> audio_bus = media::AudioBus::Create(params_);
    audio_bus->Zero();

    media::AudioCapturerSource::CaptureCallback* callback =
        static_cast<media::AudioCapturerSource::CaptureCallback*>(capturer_);

    // Verify the sink is getting the correct values.
    EXPECT_CALL(*sink, FormatIsSet());
    EXPECT_CALL(*sink,
                OnDataCallback(_, _, delay_ms, expected_volume_value,
                               need_audio_processing, key_pressed));
    callback->Capture(audio_bus.get(), delay_ms, volume, key_pressed);

    // Verify the cached values in the capturer fits what we expect.
    base::TimeDelta cached_delay;
    int cached_volume = !expected_volume_value;
    bool cached_key_pressed = !key_pressed;
    capturer_->GetAudioProcessingParams(&cached_delay, &cached_volume,
                                        &cached_key_pressed);
    EXPECT_EQ(cached_delay.InMilliseconds(), delay_ms);
    EXPECT_EQ(cached_volume, expected_volume_value);
    EXPECT_EQ(cached_key_pressed, key_pressed);

    track_->RemoveSink(sink.get());
    EXPECT_CALL(*capturer_source_.get(), Stop());
    capturer_->Stop();
  }

  media::AudioParameters params_;
  scoped_refptr<MockCapturerSource> capturer_source_;
  scoped_refptr<WebRtcAudioCapturer> capturer_;
  scoped_ptr<WebRtcLocalAudioTrack> track_;
};

// Pass the delay value, volume and key_pressed info via capture callback, and
// those values should be correctly stored and passed to the track.
TEST_F(WebRtcAudioCapturerTest, VerifyAudioParamsWithoutAudioProcessing) {
  DisableAudioTrackProcessing();
  // Use constraints with default settings.
  MockMediaConstraintFactory constraint_factory;
  VerifyAudioParams(constraint_factory.CreateWebMediaConstraints(), true);
}

TEST_F(WebRtcAudioCapturerTest, VerifyAudioParamsWithAudioProcessing) {
  // Turn off the default constraints to verify that the sink will get packets
  // with a buffer size smaller than 10ms.
  MockMediaConstraintFactory constraint_factory;
  constraint_factory.DisableDefaultAudioConstraints();
  VerifyAudioParams(constraint_factory.CreateWebMediaConstraints(), false);
}

TEST_F(WebRtcAudioCapturerTest, FailToCreateCapturerWithWrongConstraints) {
  MockMediaConstraintFactory constraint_factory;
  const std::string dummy_constraint = "dummy";
  constraint_factory.AddMandatory(dummy_constraint, true);

  scoped_refptr<WebRtcAudioCapturer> capturer(
      WebRtcAudioCapturer::CreateCapturer(
          0, StreamDeviceInfo(MEDIA_DEVICE_AUDIO_CAPTURE,
                               "", "", params_.sample_rate(),
                               params_.channel_layout(),
                               params_.frames_per_buffer()),
          constraint_factory.CreateWebMediaConstraints(), NULL, NULL)
  );
  EXPECT_TRUE(capturer == NULL);
}


}  // namespace content
