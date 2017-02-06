// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_input_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Exactly;
using ::testing::InvokeWithoutArgs;
using ::testing::NotNull;

namespace media {

static const int kSampleRate = AudioParameters::kAudioCDSampleRate;
static const int kBitsPerSample = 16;
static const ChannelLayout kChannelLayout = CHANNEL_LAYOUT_STEREO;
static const int kSamplesPerPacket = kSampleRate / 10;

// Posts base::MessageLoop::QuitWhenIdleClosure() on specified message loop.
ACTION_P(QuitMessageLoop, loop_or_proxy) {
  loop_or_proxy->PostTask(FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
}

// Posts base::MessageLoop::QuitWhenIdleClosure() on specified message loop
// after a certain number of calls given by |limit|.
ACTION_P3(CheckCountAndPostQuitTask, count, limit, loop_or_proxy) {
  if (++*count >= limit) {
    loop_or_proxy->PostTask(FROM_HERE,
                            base::MessageLoop::QuitWhenIdleClosure());
  }
}

// Closes AudioOutputController synchronously.
static void CloseAudioController(AudioInputController* controller) {
  controller->Close(base::MessageLoop::QuitWhenIdleClosure());
  base::RunLoop().Run();
}

class MockAudioInputControllerEventHandler
    : public AudioInputController::EventHandler {
 public:
  MockAudioInputControllerEventHandler() {}

  MOCK_METHOD1(OnCreated, void(AudioInputController* controller));
  MOCK_METHOD1(OnRecording, void(AudioInputController* controller));
  MOCK_METHOD2(OnError, void(AudioInputController* controller,
                             AudioInputController::ErrorCode error_code));
  MOCK_METHOD2(OnData,
               void(AudioInputController* controller, const AudioBus* data));
  MOCK_METHOD2(OnLog,
               void(AudioInputController* controller,
                    const std::string& message));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioInputControllerEventHandler);
};

// Test fixture.
class AudioInputControllerTest : public testing::Test {
 public:
  AudioInputControllerTest()
      : audio_manager_(
            AudioManager::CreateForTesting(message_loop_.task_runner())) {
    // Flush the message loop to ensure that AudioManager is fully initialized.
    base::RunLoop().RunUntilIdle();
  }
  ~AudioInputControllerTest() override {
    audio_manager_.reset();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  base::MessageLoop message_loop_;
  ScopedAudioManagerPtr audio_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioInputControllerTest);
};

// Test AudioInputController for create and close without recording audio.
TEST_F(AudioInputControllerTest, CreateAndClose) {
  MockAudioInputControllerEventHandler event_handler;

  // OnCreated() will be posted once.
  EXPECT_CALL(event_handler, OnCreated(NotNull()))
      .WillOnce(QuitMessageLoop(&message_loop_));

  AudioParameters params(AudioParameters::AUDIO_FAKE, kChannelLayout,
                         kSampleRate, kBitsPerSample, kSamplesPerPacket);

  scoped_refptr<AudioInputController> controller = AudioInputController::Create(
      audio_manager_.get(), &event_handler, params,
      AudioDeviceDescription::kDefaultDeviceId, NULL);
  ASSERT_TRUE(controller.get());

  // Wait for OnCreated() to fire.
  base::RunLoop().Run();

  // Close the AudioInputController synchronously.
  CloseAudioController(controller.get());
}

// Test a normal call sequence of create, record and close.
TEST_F(AudioInputControllerTest, RecordAndClose) {
  MockAudioInputControllerEventHandler event_handler;
  int count = 0;

  // OnCreated() will be called once.
  EXPECT_CALL(event_handler, OnCreated(NotNull()))
      .Times(Exactly(1));

  // OnRecording() will be called only once.
  EXPECT_CALL(event_handler, OnRecording(NotNull()))
      .Times(Exactly(1));

  // OnData() shall be called ten times.
  EXPECT_CALL(event_handler, OnData(NotNull(), NotNull()))
      .Times(AtLeast(10))
      .WillRepeatedly(CheckCountAndPostQuitTask(
          &count, 10, message_loop_.task_runner()));

  AudioParameters params(AudioParameters::AUDIO_FAKE, kChannelLayout,
                         kSampleRate, kBitsPerSample, kSamplesPerPacket);

  // Creating the AudioInputController should render an OnCreated() call.
  scoped_refptr<AudioInputController> controller = AudioInputController::Create(
      audio_manager_.get(), &event_handler, params,
      AudioDeviceDescription::kDefaultDeviceId, NULL);
  ASSERT_TRUE(controller.get());

  // Start recording and trigger one OnRecording() call.
  controller->Record();

  // Record and wait until ten OnData() callbacks are received.
  base::RunLoop().Run();

  // Close the AudioInputController synchronously.
  CloseAudioController(controller.get());
}

// Test that the AudioInputController reports an error when the input stream
// stops. This can happen when the underlying audio layer stops feeding data as
// a result of a removed microphone device.
// Disabled due to crbug.com/357569 and crbug.com/357501.
// TODO(henrika): Remove the test when the timer workaround has been removed.
TEST_F(AudioInputControllerTest, DISABLED_RecordAndError) {
  MockAudioInputControllerEventHandler event_handler;
  int count = 0;

  // OnCreated() will be called once.
  EXPECT_CALL(event_handler, OnCreated(NotNull()))
      .Times(Exactly(1));

  // OnRecording() will be called only once.
  EXPECT_CALL(event_handler, OnRecording(NotNull()))
      .Times(Exactly(1));

  // OnData() shall be called ten times.
  EXPECT_CALL(event_handler, OnData(NotNull(), NotNull()))
      .Times(AtLeast(10))
      .WillRepeatedly(CheckCountAndPostQuitTask(
          &count, 10, message_loop_.task_runner()));

  // OnError() will be called after the data stream stops while the
  // controller is in a recording state.
  EXPECT_CALL(event_handler, OnError(NotNull(),
                                     AudioInputController::NO_DATA_ERROR))
      .Times(Exactly(1))
      .WillOnce(QuitMessageLoop(&message_loop_));

  AudioParameters params(AudioParameters::AUDIO_FAKE, kChannelLayout,
                         kSampleRate, kBitsPerSample, kSamplesPerPacket);

  // Creating the AudioInputController should render an OnCreated() call.
  scoped_refptr<AudioInputController> controller = AudioInputController::Create(
      audio_manager_.get(), &event_handler, params,
      AudioDeviceDescription::kDefaultDeviceId, NULL);
  ASSERT_TRUE(controller.get());

  // Start recording and trigger one OnRecording() call.
  controller->Record();

  // Record and wait until ten OnData() callbacks are received.
  base::RunLoop().Run();

  // Stop the stream and verify that OnError() is posted.
  AudioInputStream* stream = controller->stream_for_testing();
  stream->Stop();
  base::RunLoop().Run();

  // Close the AudioInputController synchronously.
  CloseAudioController(controller.get());
}

// Test that AudioInputController rejects insanely large packet sizes.
TEST_F(AudioInputControllerTest, SamplesPerPacketTooLarge) {
  // Create an audio device with a very large packet size.
  MockAudioInputControllerEventHandler event_handler;

  // OnCreated() shall not be called in this test.
  EXPECT_CALL(event_handler, OnCreated(NotNull()))
    .Times(Exactly(0));

  AudioParameters params(AudioParameters::AUDIO_FAKE,
                         kChannelLayout,
                         kSampleRate,
                         kBitsPerSample,
                         kSamplesPerPacket * 1000);
  scoped_refptr<AudioInputController> controller = AudioInputController::Create(
      audio_manager_.get(), &event_handler, params,
      AudioDeviceDescription::kDefaultDeviceId, NULL);
  ASSERT_FALSE(controller.get());
}

// Test calling AudioInputController::Close multiple times.
TEST_F(AudioInputControllerTest, CloseTwice) {
  MockAudioInputControllerEventHandler event_handler;

  // OnRecording() will be called only once.
  EXPECT_CALL(event_handler, OnCreated(NotNull()));

  // OnRecording() will be called only once.
  EXPECT_CALL(event_handler, OnRecording(NotNull()))
      .Times(Exactly(1));

  AudioParameters params(AudioParameters::AUDIO_FAKE,
                         kChannelLayout,
                         kSampleRate,
                         kBitsPerSample,
                         kSamplesPerPacket);
  scoped_refptr<AudioInputController> controller = AudioInputController::Create(
      audio_manager_.get(), &event_handler, params,
      AudioDeviceDescription::kDefaultDeviceId, NULL);
  ASSERT_TRUE(controller.get());

  controller->Record();

  controller->Close(base::MessageLoop::QuitWhenIdleClosure());
  base::RunLoop().Run();

  controller->Close(base::MessageLoop::QuitWhenIdleClosure());
  base::RunLoop().Run();
}

}  // namespace media
