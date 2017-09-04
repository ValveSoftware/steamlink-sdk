// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/aligned_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/common/media/media_stream_options.h"
#include "content/public/common/media_stream_request.h"
#include "content/renderer/media/media_stream_audio_processor.h"
#include "content/renderer/media/media_stream_audio_processor_options.h"
#include "content/renderer/media/mock_constraint_factory.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/webrtc/api/mediastreaminterface.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::Return;

using media::AudioParameters;

namespace webrtc {

bool operator==(const webrtc::Point& lhs, const webrtc::Point& rhs) {
  return lhs.x() == rhs.x() && lhs.y() == rhs.y() && lhs.z() == rhs.z();
}

}  // namespace webrtc

namespace content {

namespace {

#if defined(ANDROID)
const int kAudioProcessingSampleRate = 16000;
#else
const int kAudioProcessingSampleRate = 48000;
#endif
const int kAudioProcessingNumberOfChannel = 1;

// The number of packers used for testing.
const int kNumberOfPacketsForTest = 100;

const int kMaxNumberOfPlayoutDataChannels = 2;

void ReadDataFromSpeechFile(char* data, int length) {
  base::FilePath file;
  CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &file));
  file = file.Append(FILE_PATH_LITERAL("media"))
             .Append(FILE_PATH_LITERAL("test"))
             .Append(FILE_PATH_LITERAL("data"))
             .Append(FILE_PATH_LITERAL("speech_16b_stereo_48kHz.raw"));
  DCHECK(base::PathExists(file));
  int64_t data_file_size64 = 0;
  DCHECK(base::GetFileSize(file, &data_file_size64));
  EXPECT_EQ(length, base::ReadFile(file, data, length));
  DCHECK(data_file_size64 > length);
}

}  // namespace

class MediaStreamAudioProcessorTest : public ::testing::Test {
 public:
  MediaStreamAudioProcessorTest()
      : params_(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                media::CHANNEL_LAYOUT_STEREO, 48000, 16, 512) {
  }

 protected:
  // Helper method to save duplicated code.
  void ProcessDataAndVerifyFormat(MediaStreamAudioProcessor* audio_processor,
                                  int expected_output_sample_rate,
                                  int expected_output_channels,
                                  int expected_output_buffer_size) {
    // Read the audio data from a file.
    const media::AudioParameters& params = audio_processor->InputFormat();
    const int packet_size =
        params.frames_per_buffer() * 2 * params.channels();
    const size_t length = packet_size * kNumberOfPacketsForTest;
    std::unique_ptr<char[]> capture_data(new char[length]);
    ReadDataFromSpeechFile(capture_data.get(), length);
    const int16_t* data_ptr =
        reinterpret_cast<const int16_t*>(capture_data.get());
    std::unique_ptr<media::AudioBus> data_bus =
        media::AudioBus::Create(params.channels(), params.frames_per_buffer());

    // |data_bus_playout| is used if the number of capture channels is larger
    // that max allowed playout channels. |data_bus_playout_to_use| points to
    // the AudioBus to use, either |data_bus| or |data_bus_playout|.
    std::unique_ptr<media::AudioBus> data_bus_playout;
    media::AudioBus* data_bus_playout_to_use = data_bus.get();
    if (params.channels() > kMaxNumberOfPlayoutDataChannels) {
      data_bus_playout =
          media::AudioBus::CreateWrapper(kMaxNumberOfPlayoutDataChannels);
      data_bus_playout->set_frames(params.frames_per_buffer());
      data_bus_playout_to_use = data_bus_playout.get();
    }

    const base::TimeDelta input_capture_delay =
        base::TimeDelta::FromMilliseconds(20);
    const base::TimeDelta output_buffer_duration =
        expected_output_buffer_size * base::TimeDelta::FromSeconds(1) /
            expected_output_sample_rate;
    for (int i = 0; i < kNumberOfPacketsForTest; ++i) {
      data_bus->FromInterleaved(data_ptr, data_bus->frames(), 2);
      audio_processor->PushCaptureData(*data_bus, input_capture_delay);

      // |audio_processor| does nothing when the audio processing is off in
      // the processor.
      webrtc::AudioProcessing* ap = audio_processor->audio_processing_.get();
#if defined(OS_ANDROID)
      const bool is_aec_enabled = ap && ap->echo_control_mobile()->is_enabled();
      // AEC should be turned off for mobiles.
      DCHECK(!ap || !ap->echo_cancellation()->is_enabled());
#else
      const bool is_aec_enabled = ap && ap->echo_cancellation()->is_enabled();
#endif
      if (is_aec_enabled) {
        if (params.channels() > kMaxNumberOfPlayoutDataChannels) {
          for (int i = 0; i < kMaxNumberOfPlayoutDataChannels; ++i) {
            data_bus_playout->SetChannelData(
                i, const_cast<float*>(data_bus->channel(i)));
          }
        }
        audio_processor->OnPlayoutData(data_bus_playout_to_use,
                                       params.sample_rate(), 10);
      }

      media::AudioBus* processed_data = nullptr;
      base::TimeDelta capture_delay;
      int new_volume = 0;
      while (audio_processor->ProcessAndConsumeData(
                 255, false, &processed_data, &capture_delay, &new_volume)) {
        EXPECT_TRUE(processed_data);
        EXPECT_NEAR(input_capture_delay.InMillisecondsF(),
                    capture_delay.InMillisecondsF(),
                    output_buffer_duration.InMillisecondsF());
        EXPECT_EQ(expected_output_sample_rate,
                  audio_processor->OutputFormat().sample_rate());
        EXPECT_EQ(expected_output_channels,
                  audio_processor->OutputFormat().channels());
        EXPECT_EQ(expected_output_buffer_size,
                  audio_processor->OutputFormat().frames_per_buffer());
      }

      data_ptr += params.frames_per_buffer() * params.channels();
    }
  }

  void VerifyDefaultComponents(MediaStreamAudioProcessor* audio_processor) {
    webrtc::AudioProcessing* audio_processing =
        audio_processor->audio_processing_.get();
#if defined(OS_ANDROID)
    EXPECT_TRUE(audio_processing->echo_control_mobile()->is_enabled());
    EXPECT_TRUE(audio_processing->echo_control_mobile()->routing_mode() ==
        webrtc::EchoControlMobile::kSpeakerphone);
    EXPECT_FALSE(audio_processing->echo_cancellation()->is_enabled());
#else
    EXPECT_TRUE(audio_processing->echo_cancellation()->is_enabled());
    EXPECT_TRUE(audio_processing->echo_cancellation()->suppression_level() ==
        webrtc::EchoCancellation::kHighSuppression);
    EXPECT_TRUE(audio_processing->echo_cancellation()->are_metrics_enabled());
    EXPECT_TRUE(
        audio_processing->echo_cancellation()->is_delay_logging_enabled());
#endif

    EXPECT_TRUE(audio_processing->noise_suppression()->is_enabled());
    EXPECT_TRUE(audio_processing->noise_suppression()->level() ==
        webrtc::NoiseSuppression::kHigh);
    EXPECT_TRUE(audio_processing->high_pass_filter()->is_enabled());
    EXPECT_TRUE(audio_processing->gain_control()->is_enabled());
#if defined(OS_ANDROID)
    EXPECT_TRUE(audio_processing->gain_control()->mode() ==
        webrtc::GainControl::kFixedDigital);
    EXPECT_FALSE(audio_processing->voice_detection()->is_enabled());
#else
    EXPECT_TRUE(audio_processing->gain_control()->mode() ==
        webrtc::GainControl::kAdaptiveAnalog);
    EXPECT_TRUE(audio_processing->voice_detection()->is_enabled());
    EXPECT_TRUE(audio_processing->voice_detection()->likelihood() ==
        webrtc::VoiceDetection::kVeryLowLikelihood);
#endif
  }

  base::MessageLoop main_thread_message_loop_;
  media::AudioParameters params_;
  MediaStreamDevice::AudioDeviceParameters input_device_params_;
};

// Test crashing with ASAN on Android. crbug.com/468762
#if defined(OS_ANDROID) && defined(ADDRESS_SANITIZER)
#define MAYBE_WithAudioProcessing DISABLED_WithAudioProcessing
#else
#define MAYBE_WithAudioProcessing WithAudioProcessing
#endif
TEST_F(MediaStreamAudioProcessorTest, MAYBE_WithAudioProcessing) {
  MockConstraintFactory constraint_factory;
  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new WebRtcAudioDeviceImpl());
  scoped_refptr<MediaStreamAudioProcessor> audio_processor(
      new rtc::RefCountedObject<MediaStreamAudioProcessor>(
          constraint_factory.CreateWebMediaConstraints(), input_device_params_,
          webrtc_audio_device.get()));
  EXPECT_TRUE(audio_processor->has_audio_processing());
  audio_processor->OnCaptureFormatChanged(params_);
  VerifyDefaultComponents(audio_processor.get());

  ProcessDataAndVerifyFormat(audio_processor.get(),
                             kAudioProcessingSampleRate,
                             kAudioProcessingNumberOfChannel,
                             kAudioProcessingSampleRate / 100);

  // Stop |audio_processor| so that it removes itself from
  // |webrtc_audio_device| and clears its pointer to it.
  audio_processor->Stop();
}

TEST_F(MediaStreamAudioProcessorTest, VerifyTabCaptureWithoutAudioProcessing) {
  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new WebRtcAudioDeviceImpl());
  // Create MediaStreamAudioProcessor instance for kMediaStreamSourceTab source.
  MockConstraintFactory tab_constraint_factory;
  const std::string tab_string = kMediaStreamSourceTab;
  tab_constraint_factory.basic().mediaStreamSource.setExact(
      blink::WebString::fromUTF8(tab_string));
  scoped_refptr<MediaStreamAudioProcessor> audio_processor(
      new rtc::RefCountedObject<MediaStreamAudioProcessor>(
          tab_constraint_factory.CreateWebMediaConstraints(),
          input_device_params_, webrtc_audio_device.get()));
  EXPECT_FALSE(audio_processor->has_audio_processing());
  audio_processor->OnCaptureFormatChanged(params_);

  ProcessDataAndVerifyFormat(audio_processor.get(),
                             params_.sample_rate(),
                             params_.channels(),
                             params_.sample_rate() / 100);

  // Create MediaStreamAudioProcessor instance for kMediaStreamSourceSystem
  // source.
  MockConstraintFactory system_constraint_factory;
  const std::string system_string = kMediaStreamSourceSystem;
  system_constraint_factory.basic().mediaStreamSource.setExact(
      blink::WebString::fromUTF8(system_string));
  audio_processor = new rtc::RefCountedObject<MediaStreamAudioProcessor>(
      system_constraint_factory.CreateWebMediaConstraints(),
      input_device_params_, webrtc_audio_device.get());
  EXPECT_FALSE(audio_processor->has_audio_processing());

  // Stop |audio_processor| so that it removes itself from
  // |webrtc_audio_device| and clears its pointer to it.
  audio_processor->Stop();
}

TEST_F(MediaStreamAudioProcessorTest, TurnOffDefaultConstraints) {
  // Turn off the default constraints and pass it to MediaStreamAudioProcessor.
  MockConstraintFactory constraint_factory;
  constraint_factory.DisableDefaultAudioConstraints();
  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new WebRtcAudioDeviceImpl());
  scoped_refptr<MediaStreamAudioProcessor> audio_processor(
      new rtc::RefCountedObject<MediaStreamAudioProcessor>(
          constraint_factory.CreateWebMediaConstraints(), input_device_params_,
          webrtc_audio_device.get()));
  EXPECT_FALSE(audio_processor->has_audio_processing());
  audio_processor->OnCaptureFormatChanged(params_);

  ProcessDataAndVerifyFormat(audio_processor.get(),
                             params_.sample_rate(),
                             params_.channels(),
                             params_.sample_rate() / 100);

  // Stop |audio_processor| so that it removes itself from
  // |webrtc_audio_device| and clears its pointer to it.
  audio_processor->Stop();
}

TEST_F(MediaStreamAudioProcessorTest, VerifyConstraints) {
  {
    // Verify that echo cancellation is off when platform aec effect is on.
    MockConstraintFactory constraint_factory;
    MediaAudioConstraints audio_constraints(
        constraint_factory.CreateWebMediaConstraints(),
        media::AudioParameters::ECHO_CANCELLER);
    EXPECT_FALSE(audio_constraints.GetEchoCancellationProperty());
  }

  {
    // Verify |kEchoCancellation| overwrite |kGoogEchoCancellation|.
    MockConstraintFactory constraint_factory_1;
    constraint_factory_1.AddAdvanced().echoCancellation.setExact(true);
    constraint_factory_1.AddAdvanced().googEchoCancellation.setExact(false);
    blink::WebMediaConstraints constraints_1 =
        constraint_factory_1.CreateWebMediaConstraints();
    MediaAudioConstraints audio_constraints_1(constraints_1, 0);
    EXPECT_TRUE(audio_constraints_1.GetEchoCancellationProperty());

    MockConstraintFactory constraint_factory_2;
    constraint_factory_2.AddAdvanced().echoCancellation.setExact(false);
    constraint_factory_2.AddAdvanced().googEchoCancellation.setExact(true);
    blink::WebMediaConstraints constraints_2 =
        constraint_factory_2.CreateWebMediaConstraints();
    MediaAudioConstraints audio_constraints_2(constraints_2, 0);
    EXPECT_FALSE(audio_constraints_2.GetEchoCancellationProperty());
  }
  {
    // When |kEchoCancellation| is explicitly set to false, the default values
    // for all the constraints are false.
    MockConstraintFactory constraint_factory;
    constraint_factory.AddAdvanced().echoCancellation.setExact(false);
    blink::WebMediaConstraints constraints =
        constraint_factory.CreateWebMediaConstraints();
    MediaAudioConstraints audio_constraints(constraints, 0);
  }
}

TEST_F(MediaStreamAudioProcessorTest, ValidateBadConstraints) {
  MockConstraintFactory constraint_factory;
  // Add a constraint that is not valid for audio.
  constraint_factory.basic().width.setExact(240);
  MediaAudioConstraints audio_constraints(
      constraint_factory.CreateWebMediaConstraints(), 0);
  EXPECT_FALSE(audio_constraints.IsValid());
}

TEST_F(MediaStreamAudioProcessorTest, ValidateGoodConstraints) {
  MockConstraintFactory constraint_factory;
  // Check that the renderToAssociatedSink constraint is considered valid.
  constraint_factory.basic().renderToAssociatedSink.setExact(true);
  MediaAudioConstraints audio_constraints(
      constraint_factory.CreateWebMediaConstraints(), 0);
  EXPECT_TRUE(audio_constraints.IsValid());
}

TEST_F(MediaStreamAudioProcessorTest, NoEchoTurnsOffProcessing) {
  {
    MockConstraintFactory constraint_factory;
    MediaAudioConstraints audio_constraints(
        constraint_factory.CreateWebMediaConstraints(), 0);
    // The default value for echo cancellation is true, except when all
    // audio processing has been turned off.
    EXPECT_TRUE(audio_constraints.default_audio_processing_constraint_value());
  }
  // Turning off audio processing via a mandatory constraint.
  {
    MockConstraintFactory constraint_factory;
    constraint_factory.basic().echoCancellation.setExact(false);
    MediaAudioConstraints audio_constraints(
        constraint_factory.CreateWebMediaConstraints(), 0);
    // The default value for echo cancellation is true, except when all
    // audio processing has been turned off.
    EXPECT_FALSE(audio_constraints.default_audio_processing_constraint_value());
  }
  // Turning off audio processing via an optional constraint.
  {
    MockConstraintFactory constraint_factory;
    constraint_factory.AddAdvanced().echoCancellation.setExact(false);
    MediaAudioConstraints audio_constraints(
        constraint_factory.CreateWebMediaConstraints(), 0);
    EXPECT_FALSE(audio_constraints.default_audio_processing_constraint_value());
  }
}

MediaAudioConstraints MakeMediaAudioConstraints(
    const MockConstraintFactory& constraint_factory) {
  return MediaAudioConstraints(constraint_factory.CreateWebMediaConstraints(),
                               AudioParameters::NO_EFFECTS);
}

TEST_F(MediaStreamAudioProcessorTest, SelectsConstraintsArrayGeometryIfExists) {
  std::vector<webrtc::Point> constraints_geometry(1,
                                                  webrtc::Point(-0.02f, 0, 0));
  constraints_geometry.push_back(webrtc::Point(0.02f, 0, 0));

  std::vector<webrtc::Point> input_device_geometry(1, webrtc::Point(0, 0, 0));
  input_device_geometry.push_back(webrtc::Point(0, 0.05f, 0));

  {
    // Both geometries empty.
    MockConstraintFactory constraint_factory;
    MediaStreamDevice::AudioDeviceParameters input_params;

    const auto& actual_geometry = GetArrayGeometryPreferringConstraints(
        MakeMediaAudioConstraints(constraint_factory), input_params);
    EXPECT_EQ(std::vector<webrtc::Point>(), actual_geometry);
  }
  {
    // Constraints geometry empty.
    MockConstraintFactory constraint_factory;
    MediaStreamDevice::AudioDeviceParameters input_params;
    input_params.mic_positions.push_back(media::Point(0, 0, 0));
    input_params.mic_positions.push_back(media::Point(0, 0.05f, 0));

    const auto& actual_geometry = GetArrayGeometryPreferringConstraints(
        MakeMediaAudioConstraints(constraint_factory), input_params);
    EXPECT_EQ(input_device_geometry, actual_geometry);
  }
  {
    // Input device geometry empty.
    MockConstraintFactory constraint_factory;
    constraint_factory.AddAdvanced().googArrayGeometry.setExact(
        blink::WebString::fromUTF8("-0.02 0 0 0.02 0 0"));
    MediaStreamDevice::AudioDeviceParameters input_params;

    const auto& actual_geometry = GetArrayGeometryPreferringConstraints(
        MakeMediaAudioConstraints(constraint_factory), input_params);
    EXPECT_EQ(constraints_geometry, actual_geometry);
  }
  {
    // Both geometries existing.
    MockConstraintFactory constraint_factory;
    constraint_factory.AddAdvanced().googArrayGeometry.setExact(
        blink::WebString::fromUTF8("-0.02 0 0 0.02 0 0"));
    MediaStreamDevice::AudioDeviceParameters input_params;
    input_params.mic_positions.push_back(media::Point(0, 0, 0));
    input_params.mic_positions.push_back(media::Point(0, 0.05f, 0));

    // Constraints geometry is preferred.
    const auto& actual_geometry = GetArrayGeometryPreferringConstraints(
        MakeMediaAudioConstraints(constraint_factory), input_params);
    EXPECT_EQ(constraints_geometry, actual_geometry);
  }
}

// Test crashing with ASAN on Android. crbug.com/468762
#if defined(OS_ANDROID) && defined(ADDRESS_SANITIZER)
#define MAYBE_TestAllSampleRates DISABLED_TestAllSampleRates
#else
#define MAYBE_TestAllSampleRates TestAllSampleRates
#endif
TEST_F(MediaStreamAudioProcessorTest, MAYBE_TestAllSampleRates) {
  MockConstraintFactory constraint_factory;
  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new WebRtcAudioDeviceImpl());
  scoped_refptr<MediaStreamAudioProcessor> audio_processor(
      new rtc::RefCountedObject<MediaStreamAudioProcessor>(
          constraint_factory.CreateWebMediaConstraints(), input_device_params_,
          webrtc_audio_device.get()));
  EXPECT_TRUE(audio_processor->has_audio_processing());

  static const int kSupportedSampleRates[] =
      { 8000, 16000, 22050, 32000, 44100, 48000 };
  for (size_t i = 0; i < arraysize(kSupportedSampleRates); ++i) {
    int buffer_size = (kSupportedSampleRates[i] / 100)  < 128 ?
        kSupportedSampleRates[i] / 100 : 128;
    media::AudioParameters params(
        media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
        media::CHANNEL_LAYOUT_STEREO, kSupportedSampleRates[i], 16,
        buffer_size);
    audio_processor->OnCaptureFormatChanged(params);
    VerifyDefaultComponents(audio_processor.get());

    ProcessDataAndVerifyFormat(audio_processor.get(),
                               kAudioProcessingSampleRate,
                               kAudioProcessingNumberOfChannel,
                               kAudioProcessingSampleRate / 100);
  }

  // Stop |audio_processor| so that it removes itself from
  // |webrtc_audio_device| and clears its pointer to it.
  audio_processor->Stop();
}

// Test that if we have an AEC dump message filter created, we are getting it
// correctly in MSAP. Any IPC messages will be deleted since no sender in the
// filter will be created.
TEST_F(MediaStreamAudioProcessorTest, GetAecDumpMessageFilter) {
  scoped_refptr<AecDumpMessageFilter> aec_dump_message_filter_(
      new AecDumpMessageFilter(base::ThreadTaskRunnerHandle::Get(),
                               base::ThreadTaskRunnerHandle::Get()));

  MockConstraintFactory constraint_factory;
  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new WebRtcAudioDeviceImpl());
  scoped_refptr<MediaStreamAudioProcessor> audio_processor(
      new rtc::RefCountedObject<MediaStreamAudioProcessor>(
          constraint_factory.CreateWebMediaConstraints(), input_device_params_,
          webrtc_audio_device.get()));

  EXPECT_TRUE(audio_processor->aec_dump_message_filter_.get());

  // Stop |audio_processor| so that it removes itself from
  // |webrtc_audio_device| and clears its pointer to it.
  audio_processor->Stop();
}

TEST_F(MediaStreamAudioProcessorTest, TestStereoAudio) {
  // Set up the correct constraints to turn off the audio processing and turn
  // on the stereo channels mirroring.
  MockConstraintFactory constraint_factory;
  constraint_factory.basic().echoCancellation.setExact(false);
  constraint_factory.basic().googAudioMirroring.setExact(true);
  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new WebRtcAudioDeviceImpl());
  scoped_refptr<MediaStreamAudioProcessor> audio_processor(
      new rtc::RefCountedObject<MediaStreamAudioProcessor>(
          constraint_factory.CreateWebMediaConstraints(), input_device_params_,
          webrtc_audio_device.get()));
  EXPECT_FALSE(audio_processor->has_audio_processing());
  const media::AudioParameters source_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::CHANNEL_LAYOUT_STEREO, 48000, 16, 480);
  audio_processor->OnCaptureFormatChanged(source_params);
  // There's no sense in continuing if this fails.
  ASSERT_EQ(2, audio_processor->OutputFormat().channels());

  // Construct left and right channels, and assign different values to the
  // first data of the left channel and right channel.
  const int size = media::AudioBus::CalculateMemorySize(source_params);
  std::unique_ptr<float, base::AlignedFreeDeleter> left_channel(
      static_cast<float*>(base::AlignedAlloc(size, 32)));
  std::unique_ptr<float, base::AlignedFreeDeleter> right_channel(
      static_cast<float*>(base::AlignedAlloc(size, 32)));
  std::unique_ptr<media::AudioBus> wrapper =
      media::AudioBus::CreateWrapper(source_params.channels());
  wrapper->set_frames(source_params.frames_per_buffer());
  wrapper->SetChannelData(0, left_channel.get());
  wrapper->SetChannelData(1, right_channel.get());
  wrapper->Zero();
  float* left_channel_ptr = left_channel.get();
  left_channel_ptr[0] = 1.0f;

  // Run the test consecutively to make sure the stereo channels are not
  // flipped back and forth.
  static const int kNumberOfPacketsForTest = 100;
  const base::TimeDelta pushed_capture_delay =
      base::TimeDelta::FromMilliseconds(42);
  for (int i = 0; i < kNumberOfPacketsForTest; ++i) {
    audio_processor->PushCaptureData(*wrapper, pushed_capture_delay);

    media::AudioBus* processed_data = nullptr;
    base::TimeDelta capture_delay;
    int new_volume = 0;
    EXPECT_TRUE(audio_processor->ProcessAndConsumeData(
        0, false, &processed_data, &capture_delay, &new_volume));
    EXPECT_TRUE(processed_data);
    EXPECT_EQ(processed_data->channel(0)[0], 0);
    EXPECT_NE(processed_data->channel(1)[0], 0);
    EXPECT_EQ(pushed_capture_delay, capture_delay);
  }

  // Stop |audio_processor| so that it removes itself from
  // |webrtc_audio_device| and clears its pointer to it.
  audio_processor->Stop();
}

// Disabled on android clang builds due to crbug.com/470499
#if defined(__clang__) && defined(OS_ANDROID)
#define MAYBE_TestWithKeyboardMicChannel DISABLED_TestWithKeyboardMicChannel
#else
#define MAYBE_TestWithKeyboardMicChannel TestWithKeyboardMicChannel
#endif
TEST_F(MediaStreamAudioProcessorTest, MAYBE_TestWithKeyboardMicChannel) {
  MockConstraintFactory constraint_factory;
  constraint_factory.basic().googExperimentalNoiseSuppression.setExact(true);
  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new WebRtcAudioDeviceImpl());
  scoped_refptr<MediaStreamAudioProcessor> audio_processor(
      new rtc::RefCountedObject<MediaStreamAudioProcessor>(
          constraint_factory.CreateWebMediaConstraints(), input_device_params_,
          webrtc_audio_device.get()));
  EXPECT_TRUE(audio_processor->has_audio_processing());

  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC,
                                48000, 16, 512);
  audio_processor->OnCaptureFormatChanged(params);

  ProcessDataAndVerifyFormat(audio_processor.get(),
                             kAudioProcessingSampleRate,
                             kAudioProcessingNumberOfChannel,
                             kAudioProcessingSampleRate / 100);

  // Stop |audio_processor| so that it removes itself from
  // |webrtc_audio_device| and clears its pointer to it.
  audio_processor->Stop();
}

}  // namespace content
