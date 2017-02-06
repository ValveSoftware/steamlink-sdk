// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/speech_recognition_audio_sink.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "content/renderer/media/media_stream_audio_source.h"
#include "content/renderer/media/media_stream_audio_track.h"
#include "content/renderer/media/webrtc/mock_peer_connection_dependency_factory.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebHeap.h"

namespace {

// Supported speech recognition audio parameters.
const int kSpeechRecognitionSampleRate = 16000;
const int kSpeechRecognitionFramesPerBuffer = 1600;

// Input audio format.
const media::AudioParameters::Format kInputFormat =
    media::AudioParameters::AUDIO_PCM_LOW_LATENCY;
const media::ChannelLayout kInputChannelLayout = media::CHANNEL_LAYOUT_MONO;
const int kInputChannels = 1;
const int kInputBitsPerSample = 16;

// Output audio format.
const media::AudioParameters::Format kOutputFormat =
    media::AudioParameters::AUDIO_PCM_LOW_LATENCY;
const media::ChannelLayout kOutputChannelLayout = media::CHANNEL_LAYOUT_STEREO;
const int kOutputChannels = 2;
const int kOutputBitsPerSample = 16;

// Mocked out sockets used for Send/Receive.
// Data is written and read from a shared buffer used as a FIFO and there is
// no blocking. |OnSendCB| is used to trigger a |Receive| on the other socket.
class MockSyncSocket : public base::SyncSocket {
 public:
  // This allows for 2 requests in queue between the |MockSyncSocket|s.
  static const int kSharedBufferSize = 8;

  // Buffer to be shared between two |MockSyncSocket|s. Allocated on heap.
  struct SharedBuffer {
    SharedBuffer() : data(), start(0), length(0) {}

    uint8_t data[kSharedBufferSize];
    size_t start;
    size_t length;
  };

  // Callback used for pairing an A.Send() with B.Receieve() without blocking.
  typedef base::Callback<void()> OnSendCB;

  explicit MockSyncSocket(SharedBuffer* shared_buffer)
      : buffer_(shared_buffer),
        in_failure_mode_(false) {}

  MockSyncSocket(SharedBuffer* shared_buffer, const OnSendCB& on_send_cb)
      : buffer_(shared_buffer),
        on_send_cb_(on_send_cb),
        in_failure_mode_(false) {}

  size_t Send(const void* buffer, size_t length) override;
  size_t Receive(void* buffer, size_t length) override;

  // When |in_failure_mode_| == true, the socket fails to send.
  void SetFailureMode(bool in_failure_mode) {
    in_failure_mode_ = in_failure_mode;
  }

 private:
  SharedBuffer* buffer_;
  const OnSendCB on_send_cb_;
  bool in_failure_mode_;

  DISALLOW_COPY_AND_ASSIGN(MockSyncSocket);
};

// base::SyncSocket implementation
size_t MockSyncSocket::Send(const void* buffer, size_t length) {
  if (in_failure_mode_)
    return 0;

  const uint8_t* b = static_cast<const uint8_t*>(buffer);
  for (size_t i = 0; i < length; ++i, ++buffer_->length)
    buffer_->data[buffer_->start + buffer_->length] = b[i];

  on_send_cb_.Run();
  return length;
}

size_t MockSyncSocket::Receive(void* buffer, size_t length) {
  uint8_t* b = static_cast<uint8_t*>(buffer);
  for (size_t i = buffer_->start; i < buffer_->length; ++i, ++buffer_->start)
    b[i] = buffer_->data[buffer_->start];

  // Since buffer is used sequentially, we can reset the buffer indices here.
  buffer_->start = buffer_->length = 0;
  return length;
}

// This fake class is the consumer used to verify behaviour of the producer.
// The |Initialize()| method shows what the consumer should be responsible for
// in the production code (minus the mocks).
class FakeSpeechRecognizer {
 public:
  FakeSpeechRecognizer() : is_responsive_(true) {}

  void Initialize(
      const blink::WebMediaStreamTrack& track,
      const media::AudioParameters& sink_params,
      base::SharedMemoryHandle* foreign_memory_handle) {
    // Shared memory is allocated, mapped and shared.
    const uint32_t kSharedMemorySize =
        sizeof(media::AudioInputBufferParameters) +
        media::AudioBus::CalculateMemorySize(sink_params);
    shared_memory_.reset(new base::SharedMemory());
    ASSERT_TRUE(shared_memory_->CreateAndMapAnonymous(kSharedMemorySize));
    memset(shared_memory_->memory(), 0, kSharedMemorySize);
    ASSERT_TRUE(shared_memory_->ShareToProcess(base::GetCurrentProcessHandle(),
                                               foreign_memory_handle));

    // Wrap the shared memory for the audio bus.
    media::AudioInputBuffer* buffer =
        static_cast<media::AudioInputBuffer*>(shared_memory_->memory());

    audio_track_bus_ = media::AudioBus::WrapMemory(sink_params, buffer->audio);
    audio_track_bus_->Zero();

    // Reference to the counter used to synchronize.
    buffer->params.size = 0U;

    // Create a shared buffer for the |MockSyncSocket|s.
    shared_buffer_.reset(new MockSyncSocket::SharedBuffer());

    // Local socket will receive signals from the producer.
    receiving_socket_.reset(new MockSyncSocket(shared_buffer_.get()));

    // We automatically trigger a Receive when data is sent over the socket.
    sending_socket_ = new MockSyncSocket(
        shared_buffer_.get(),
        base::Bind(&FakeSpeechRecognizer::EmulateReceiveThreadLoopIteration,
                   base::Unretained(this)));

    // This is usually done to pair the sockets. Here it's not effective.
    base::SyncSocket::CreatePair(receiving_socket_.get(), sending_socket_);
  }

  // Emulates a single iteraton of a thread receiving on the socket.
  // This would normally be done on a receiving thread's task on the browser.
  void EmulateReceiveThreadLoopIteration() {
    if (!is_responsive_)
      return;

    const int kSize = sizeof(media::AudioInputBufferParameters().size);
    receiving_socket_->Receive(&(GetAudioInputBuffer()->params.size), kSize);

    // Notify the producer that the audio buffer has been consumed.
    GetAudioInputBuffer()->params.size++;
  }

  // Used to simulate an unresponsive behaviour of the consumer.
  void SimulateResponsiveness(bool is_responsive) {
    is_responsive_ = is_responsive;
  }

  media::AudioInputBuffer * GetAudioInputBuffer() const {
    return static_cast<media::AudioInputBuffer*>(shared_memory_->memory());
  }

  MockSyncSocket* sending_socket() { return sending_socket_; }
  media::AudioBus* audio_bus() const { return audio_track_bus_.get(); }


 private:
  bool is_responsive_;

  // Shared memory for the audio and synchronization.
  std::unique_ptr<base::SharedMemory> shared_memory_;

  // Fake sockets and their shared buffer.
  std::unique_ptr<MockSyncSocket::SharedBuffer> shared_buffer_;
  std::unique_ptr<MockSyncSocket> receiving_socket_;
  MockSyncSocket* sending_socket_;

  // Audio bus wrapping the shared memory from the renderer.
  std::unique_ptr<media::AudioBus> audio_track_bus_;

  DISALLOW_COPY_AND_ASSIGN(FakeSpeechRecognizer);
};

}  // namespace

namespace content {

namespace {

class TestDrivenAudioSource : public MediaStreamAudioSource {
 public:
  TestDrivenAudioSource() : MediaStreamAudioSource(true) {}
  ~TestDrivenAudioSource() final {}

  // Expose protected methods as public for testing.
  using MediaStreamAudioSource::SetFormat;
  using MediaStreamAudioSource::DeliverDataToTracks;
};

}  // namespace

class SpeechRecognitionAudioSinkTest : public testing::Test {
 public:
  SpeechRecognitionAudioSinkTest() {}

  ~SpeechRecognitionAudioSinkTest() {
    blink_source_.reset();
    blink_track_.reset();
    speech_audio_sink_.reset();
    blink::WebHeap::collectAllGarbageForTesting();
  }

  // Initializes the producer and consumer with specified audio parameters.
  // Returns the minimal number of input audio buffers which need to be captured
  // before they get sent to the consumer.
  uint32_t Initialize(int input_sample_rate,
                      int input_frames_per_buffer,
                      int output_sample_rate,
                      int output_frames_per_buffer) {
    // Audio Environment setup.
    source_params_.Reset(kInputFormat,
                         kInputChannelLayout,
                         input_sample_rate,
                         kInputBitsPerSample,
                         input_frames_per_buffer);
    sink_params_.Reset(kOutputFormat,
                       kOutputChannelLayout,
                       output_sample_rate,
                       kOutputBitsPerSample,
                       output_frames_per_buffer);
    source_bus_ =
        media::AudioBus::Create(kInputChannels, input_frames_per_buffer);
    source_bus_->Zero();
    first_frame_capture_time_ = base::TimeTicks::Now();
    sample_frames_captured_ = 0;

    // Prepare the track and audio source.
    PrepareBlinkTrackOfType(MEDIA_DEVICE_AUDIO_CAPTURE, &blink_track_);
    blink_source_ = blink_track_.source();
    static_cast<TestDrivenAudioSource*>(
        MediaStreamAudioSource::From(blink_source_))->SetFormat(source_params_);

    // Create and initialize the consumer.
    recognizer_.reset(new FakeSpeechRecognizer());
    base::SharedMemoryHandle foreign_memory_handle;
    recognizer_->Initialize(blink_track_, sink_params_, &foreign_memory_handle);

    // Create the producer.
    std::unique_ptr<base::SyncSocket> sending_socket(
        recognizer_->sending_socket());
    speech_audio_sink_.reset(new SpeechRecognitionAudioSink(
        blink_track_, sink_params_, foreign_memory_handle,
        std::move(sending_socket),
        base::Bind(&SpeechRecognitionAudioSinkTest::StoppedCallback,
                   base::Unretained(this))));

    // Return number of buffers needed to trigger resampling and consumption.
    return static_cast<uint32_t>(std::ceil(
        static_cast<double>(output_frames_per_buffer * input_sample_rate) /
        (input_frames_per_buffer * output_sample_rate)));
  }

  // Mock callback expected to be called when the track is stopped.
  MOCK_METHOD0(StoppedCallback, void());

 protected:
  // Prepares a blink track of a given MediaStreamType and attaches the native
  // track which can be used to capture audio data and pass it to the producer.
  void PrepareBlinkTrackOfType(const MediaStreamType device_type,
                               blink::WebMediaStreamTrack* blink_track) {
    blink::WebMediaStreamSource blink_source;
    blink_source.initialize(blink::WebString::fromUTF8("dummy_source_id"),
                            blink::WebMediaStreamSource::TypeAudio,
                            blink::WebString::fromUTF8("dummy_source_name"),
                            false /* remote */);
    TestDrivenAudioSource* const audio_source = new TestDrivenAudioSource();
    audio_source->SetDeviceInfo(
        StreamDeviceInfo(device_type, "Mock device", "mock_device_id"));
    blink_source.setExtraData(audio_source);  // Takes ownership.

    blink_track->initialize(blink::WebString::fromUTF8("dummy_track"),
                            blink_source);
    ASSERT_TRUE(audio_source->ConnectToTrack(*blink_track));
  }

  // Emulates an audio capture device capturing data from the source.
  inline void CaptureAudio(const uint32_t buffers) {
    for (uint32_t i = 0; i < buffers; ++i) {
      const base::TimeTicks estimated_capture_time = first_frame_capture_time_ +
          (sample_frames_captured_ * base::TimeDelta::FromSeconds(1) /
               source_params_.sample_rate());
      static_cast<TestDrivenAudioSource*>(
          MediaStreamAudioSource::From(blink_source_))
              ->DeliverDataToTracks(*source_bus_, estimated_capture_time);
      sample_frames_captured_ += source_bus_->frames();
    }
  }

  // Used to simulate a problem with sockets.
  void SetFailureModeOnForeignSocket(bool in_failure_mode) {
    recognizer()->sending_socket()->SetFailureMode(in_failure_mode);
  }

  // Helper method for verifying captured audio data has been consumed.
  inline void AssertConsumedBuffers(const uint32_t buffer_index) {
    ASSERT_EQ(buffer_index, recognizer()->GetAudioInputBuffer()->params.size);
  }

  // Helper method for providing audio data to producer and verifying it was
  // consumed on the recognizer.
  inline void CaptureAudioAndAssertConsumedBuffers(
      const uint32_t buffers,
      const uint32_t buffer_index) {
    CaptureAudio(buffers);
    AssertConsumedBuffers(buffer_index);
  }

  // Helper method to capture and assert consumption at different sample rates
  // and audio buffer sizes.
  inline void AssertConsumptionForAudioParameters(
      const int input_sample_rate,
      const int input_frames_per_buffer,
      const int output_sample_rate,
      const int output_frames_per_buffer,
      const uint32_t consumptions) {
    const uint32_t buffers_per_notification =
        Initialize(input_sample_rate, input_frames_per_buffer,
                   output_sample_rate, output_frames_per_buffer);
    AssertConsumedBuffers(0U);

    for (uint32_t i = 1U; i <= consumptions; ++i) {
      CaptureAudio(buffers_per_notification);
      ASSERT_EQ(i, recognizer()->GetAudioInputBuffer()->params.size)
          << "Tested at rates: "
          << "In(" << input_sample_rate << ", " << input_frames_per_buffer
          << ") "
          << "Out(" << output_sample_rate << ", " << output_frames_per_buffer
          << ")";
    }
  }

  media::AudioBus* source_bus() const { return source_bus_.get(); }

  FakeSpeechRecognizer* recognizer() const { return recognizer_.get(); }

  const media::AudioParameters& sink_params() const { return sink_params_; }

  MediaStreamAudioTrack* native_track() const {
    return MediaStreamAudioTrack::From(blink_track_);
  }

 private:
  MockPeerConnectionDependencyFactory mock_dependency_factory_;

  // Producer.
  std::unique_ptr<SpeechRecognitionAudioSink> speech_audio_sink_;

  // Consumer.
  std::unique_ptr<FakeSpeechRecognizer> recognizer_;

  // Audio related members.
  std::unique_ptr<media::AudioBus> source_bus_;
  media::AudioParameters source_params_;
  media::AudioParameters sink_params_;
  blink::WebMediaStreamSource blink_source_;
  blink::WebMediaStreamTrack blink_track_;

  base::TimeTicks first_frame_capture_time_;
  int64_t sample_frames_captured_;

  DISALLOW_COPY_AND_ASSIGN(SpeechRecognitionAudioSinkTest);
};

// Not all types of tracks are supported. This test checks if that policy is
// implemented correctly.
TEST_F(SpeechRecognitionAudioSinkTest, CheckIsSupportedAudioTrack) {
  typedef std::map<MediaStreamType, bool> SupportedTrackPolicy;

  // This test must be aligned with the policy of supported tracks.
  SupportedTrackPolicy p;
  p[MEDIA_NO_SERVICE] = false;
  p[MEDIA_DEVICE_AUDIO_CAPTURE] = true;  // The only one supported for now.
  p[MEDIA_DEVICE_VIDEO_CAPTURE] = false;
  p[MEDIA_TAB_AUDIO_CAPTURE] = false;
  p[MEDIA_TAB_VIDEO_CAPTURE] = false;
  p[MEDIA_DESKTOP_VIDEO_CAPTURE] = false;
  p[MEDIA_DESKTOP_AUDIO_CAPTURE] = false;
  p[MEDIA_DEVICE_AUDIO_OUTPUT] = false;

  // Ensure this test gets updated along with |content::MediaStreamType| enum.
  EXPECT_EQ(NUM_MEDIA_TYPES, p.size());

  // Check the the entire policy.
  for (SupportedTrackPolicy::iterator it = p.begin(); it != p.end(); ++it) {
    blink::WebMediaStreamTrack blink_track;
    PrepareBlinkTrackOfType(it->first, &blink_track);
    ASSERT_EQ(
        it->second,
        SpeechRecognitionAudioSink::IsSupportedTrack(blink_track));
  }
}

// Checks if the producer can support the listed range of input sample rates
// and associated buffer sizes.
TEST_F(SpeechRecognitionAudioSinkTest, RecognizerNotifiedOnSocket) {
  const size_t kNumAudioParamTuples = 24;
  const int kAudioParams[kNumAudioParamTuples][2] = {
      {8000, 80}, {8000, 800}, {16000, 160}, {16000, 1600},
      {24000, 240}, {24000, 2400}, {32000, 320}, {32000, 3200},
      {44100, 441}, {44100, 4410}, {48000, 480}, {48000, 4800},
      {96000, 960}, {96000, 9600}, {11025, 111}, {11025, 1103},
      {22050, 221}, {22050, 2205}, {88200, 882}, {88200, 8820},
      {176400, 1764}, {176400, 17640}, {192000, 1920}, {192000, 19200}};

  // Check all listed tuples of input sample rates and buffers sizes.
  for (size_t i = 0; i < kNumAudioParamTuples; ++i) {
    AssertConsumptionForAudioParameters(
        kAudioParams[i][0], kAudioParams[i][1],
        kSpeechRecognitionSampleRate, kSpeechRecognitionFramesPerBuffer, 3U);
  }
}

// Checks that the input data is getting resampled to the target sample rate.
TEST_F(SpeechRecognitionAudioSinkTest, AudioDataIsResampledOnSink) {
  EXPECT_GE(kInputChannels, 1);
  EXPECT_GE(kOutputChannels, 1);

  // Input audio is sampled at 44.1 KHz with data chunks of 10ms. Desired output
  // is corresponding to the speech recognition engine requirements: 16 KHz with
  // 100 ms chunks (1600 frames per buffer).
  const uint32_t kSourceFrames = 441;
  const uint32_t buffers_per_notification =
      Initialize(44100, kSourceFrames, 16000, 1600);
  // Fill audio input frames with 0, 1, 2, 3, ..., 440.
  int16_t source_data[kSourceFrames * kInputChannels];
  for (uint32_t i = 0; i < kSourceFrames; ++i) {
    for (int c = 0; c < kInputChannels; ++c)
      source_data[i * kInputChannels + c] = i;
  }
  source_bus()->FromInterleaved(
      source_data, kSourceFrames, sizeof(source_data[0]));

  // Prepare sink audio bus and data for rendering.
  media::AudioBus* sink_bus = recognizer()->audio_bus();
  const uint32_t kSinkDataLength = 1600 * kOutputChannels;
  int16_t sink_data[kSinkDataLength] = {0};

  // Render the audio data from the recognizer.
  sink_bus->ToInterleaved(sink_bus->frames(),
                          sink_params().bits_per_sample() / 8, sink_data);

  // Checking only a fraction of the sink frames.
  const uint32_t kNumFramesToTest = 12;

  // Check all channels are zeroed out before we trigger resampling.
  for (uint32_t i = 0; i < kNumFramesToTest; ++i) {
    for (int c = 0; c < kOutputChannels; ++c)
      EXPECT_EQ(0, sink_data[i * kOutputChannels + c]);
  }

  // Trigger the speech sink to resample the input data.
  AssertConsumedBuffers(0U);
  CaptureAudioAndAssertConsumedBuffers(buffers_per_notification, 1U);

  // Render the audio data from the recognizer.
  sink_bus->ToInterleaved(sink_bus->frames(),
                          sink_params().bits_per_sample() / 8, sink_data);

  // Resampled data expected frames. Extracted based on |source_data|.
  const int16_t kExpectedData[kNumFramesToTest] = {0,  2,  5,  8,  11, 13,
                                                   16, 19, 22, 24, 27, 30};

  // Check all channels have the same resampled data.
  for (uint32_t i = 0; i < kNumFramesToTest; ++i) {
    for (int c = 0; c < kOutputChannels; ++c)
      EXPECT_EQ(kExpectedData[i], sink_data[i * kOutputChannels + c]);
  }
}

// Checks that the producer does not misbehave when a socket failure occurs.
TEST_F(SpeechRecognitionAudioSinkTest, SyncSocketFailsSendingData) {
  const uint32_t buffers_per_notification = Initialize(44100, 441, 16000, 1600);
  // Start with no problems on the socket.
  AssertConsumedBuffers(0U);
  CaptureAudioAndAssertConsumedBuffers(buffers_per_notification, 1U);

  // A failure occurs (socket cannot send).
  SetFailureModeOnForeignSocket(true);
  CaptureAudioAndAssertConsumedBuffers(buffers_per_notification, 1U);
}

// A very unlikely scenario in which the peer is not synchronizing for a long
// time (e.g. 300 ms) which results in dropping cached buffers and restarting.
// We check that the FIFO overflow does not occur and that the producer is able
// to resume.
TEST_F(SpeechRecognitionAudioSinkTest, RepeatedSycnhronizationLag) {
  const uint32_t buffers_per_notification = Initialize(44100, 441, 16000, 1600);

  // Start with no synchronization problems.
  AssertConsumedBuffers(0U);
  CaptureAudioAndAssertConsumedBuffers(buffers_per_notification, 1U);

  // Consumer gets out of sync.
  recognizer()->SimulateResponsiveness(false);
  CaptureAudioAndAssertConsumedBuffers(buffers_per_notification, 1U);
  CaptureAudioAndAssertConsumedBuffers(buffers_per_notification, 1U);
  CaptureAudioAndAssertConsumedBuffers(buffers_per_notification, 1U);

  // Consumer recovers.
  recognizer()->SimulateResponsiveness(true);
  CaptureAudioAndAssertConsumedBuffers(buffers_per_notification, 2U);
  CaptureAudioAndAssertConsumedBuffers(buffers_per_notification, 3U);
  CaptureAudioAndAssertConsumedBuffers(buffers_per_notification, 4U);
}

// Checks that an OnStoppedCallback is issued when the track is stopped.
TEST_F(SpeechRecognitionAudioSinkTest, OnReadyStateChangedOccured) {
  const uint32_t buffers_per_notification = Initialize(44100, 441, 16000, 1600);
  AssertConsumedBuffers(0U);
  CaptureAudioAndAssertConsumedBuffers(buffers_per_notification, 1U);
  EXPECT_CALL(*this, StoppedCallback()).Times(1);

  native_track()->Stop();
  CaptureAudioAndAssertConsumedBuffers(buffers_per_notification, 1U);
}

}  // namespace content
