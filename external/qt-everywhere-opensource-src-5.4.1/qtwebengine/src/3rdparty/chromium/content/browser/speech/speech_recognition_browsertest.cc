// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/speech/google_streaming_remote_engine.h"
#include "content/browser/speech/speech_recognition_manager_impl.h"
#include "content/browser/speech/speech_recognizer_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/mock_google_streaming_server.h"
#include "media/audio/mock_audio_manager.h"
#include "media/audio/test_audio_input_controller_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::RunLoop;

namespace content {

class SpeechRecognitionBrowserTest :
    public ContentBrowserTest,
    public MockGoogleStreamingServer::Delegate,
    public media::TestAudioInputControllerDelegate {
 public:
  enum StreamingServerState {
    kIdle,
    kTestAudioControllerOpened,
    kClientConnected,
    kClientAudioUpload,
    kClientAudioUploadComplete,
    kTestAudioControllerClosed,
    kClientDisconnected
  };

  // MockGoogleStreamingServerDelegate methods.
  virtual void OnClientConnected() OVERRIDE {
    ASSERT_EQ(kTestAudioControllerOpened, streaming_server_state_);
    streaming_server_state_ = kClientConnected;
  }

  virtual void OnClientAudioUpload() OVERRIDE {
    if (streaming_server_state_ == kClientConnected)
      streaming_server_state_ = kClientAudioUpload;
  }

  virtual void OnClientAudioUploadComplete() OVERRIDE {
    ASSERT_EQ(kTestAudioControllerClosed, streaming_server_state_);
    streaming_server_state_ = kClientAudioUploadComplete;
  }

  virtual void OnClientDisconnected() OVERRIDE {
    ASSERT_EQ(kClientAudioUploadComplete, streaming_server_state_);
    streaming_server_state_ = kClientDisconnected;
  }

  // media::TestAudioInputControllerDelegate methods.
  virtual void TestAudioControllerOpened(
      media::TestAudioInputController* controller) OVERRIDE {
    ASSERT_EQ(kIdle, streaming_server_state_);
    streaming_server_state_ = kTestAudioControllerOpened;
    const int capture_packet_interval_ms =
        (1000 * controller->audio_parameters().frames_per_buffer()) /
        controller->audio_parameters().sample_rate();
    ASSERT_EQ(GoogleStreamingRemoteEngine::kAudioPacketIntervalMs,
        capture_packet_interval_ms);
    FeedAudioController(500 /* ms */, /*noise=*/ false);
    FeedAudioController(1000 /* ms */, /*noise=*/ true);
    FeedAudioController(1000 /* ms */, /*noise=*/ false);
  }

  virtual void TestAudioControllerClosed(
      media::TestAudioInputController* controller) OVERRIDE {
    ASSERT_EQ(kClientAudioUpload, streaming_server_state_);
    streaming_server_state_ = kTestAudioControllerClosed;
    mock_streaming_server_->MockGoogleStreamingServer::SimulateResult(
        GetGoodSpeechResult());
  }

  // Helper methods used by test fixtures.
  GURL GetTestUrlFromFragment(const std::string fragment) {
    return GURL(GetTestUrl("speech", "web_speech_recognition.html").spec() +
        "#" + fragment);
  }

  std::string GetPageFragment() {
    return shell()->web_contents()->GetURL().ref();
  }

  const StreamingServerState &streaming_server_state() {
    return streaming_server_state_;
  }

 protected:
  // ContentBrowserTest methods.
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    test_audio_input_controller_factory_.set_delegate(this);
    media::AudioInputController::set_factory_for_testing(
        &test_audio_input_controller_factory_);
    mock_streaming_server_.reset(new MockGoogleStreamingServer(this));
    streaming_server_state_ = kIdle;
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    ASSERT_TRUE(SpeechRecognitionManagerImpl::GetInstance());
    SpeechRecognizerImpl::SetAudioManagerForTesting(
        new media::MockAudioManager(BrowserThread::GetMessageLoopProxyForThread(
            BrowserThread::IO)));
  }

  virtual void TearDownOnMainThread() OVERRIDE {
    SpeechRecognizerImpl::SetAudioManagerForTesting(NULL);
  }

  virtual void TearDownInProcessBrowserTestFixture() OVERRIDE {
    test_audio_input_controller_factory_.set_delegate(NULL);
    mock_streaming_server_.reset();
  }

 private:
  static void FeedSingleBufferToAudioController(
      scoped_refptr<media::TestAudioInputController> controller,
      size_t buffer_size,
      bool fill_with_noise) {
    DCHECK(controller.get());
    const media::AudioParameters& audio_params = controller->audio_parameters();
    scoped_ptr<uint8[]> audio_buffer(new uint8[buffer_size]);
    if (fill_with_noise) {
      for (size_t i = 0; i < buffer_size; ++i)
        audio_buffer[i] = static_cast<uint8>(127 * sin(i * 3.14F /
            (16 * buffer_size)));
    } else {
      memset(audio_buffer.get(), 0, buffer_size);
    }

    scoped_ptr<media::AudioBus> audio_bus =
        media::AudioBus::Create(audio_params);
    audio_bus->FromInterleaved(&audio_buffer.get()[0],
                               audio_bus->frames(),
                               audio_params.bits_per_sample() / 8);
    controller->event_handler()->OnData(controller, audio_bus.get());
  }

  void FeedAudioController(int duration_ms, bool feed_with_noise) {
    media::TestAudioInputController* controller =
        test_audio_input_controller_factory_.controller();
    ASSERT_TRUE(controller);
    const media::AudioParameters& audio_params = controller->audio_parameters();
    const size_t buffer_size = audio_params.GetBytesPerBuffer();
    const int ms_per_buffer = audio_params.frames_per_buffer() * 1000 /
                              audio_params.sample_rate();
    // We can only simulate durations that are integer multiples of the
    // buffer size. In this regard see
    // SpeechRecognitionEngine::GetDesiredAudioChunkDurationMs().
    ASSERT_EQ(0, duration_ms % ms_per_buffer);

    const int n_buffers = duration_ms / ms_per_buffer;
    for (int i = 0; i < n_buffers; ++i) {
      base::MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
          &FeedSingleBufferToAudioController,
          scoped_refptr<media::TestAudioInputController>(controller),
          buffer_size,
          feed_with_noise));
    }
  }

  SpeechRecognitionResult GetGoodSpeechResult() {
    SpeechRecognitionResult result;
    result.hypotheses.push_back(SpeechRecognitionHypothesis(
        base::UTF8ToUTF16("Pictures of the moon"), 1.0F));
    return result;
  }

  StreamingServerState streaming_server_state_;
  scoped_ptr<MockGoogleStreamingServer> mock_streaming_server_;
  media::TestAudioInputControllerFactory test_audio_input_controller_factory_;
};

// Simply loads the test page and checks if it was able to create a Speech
// Recognition object in JavaScript, to make sure the Web Speech API is enabled.
IN_PROC_BROWSER_TEST_F(SpeechRecognitionBrowserTest, Precheck) {
  NavigateToURLBlockUntilNavigationsComplete(
      shell(), GetTestUrlFromFragment("precheck"), 2);

  EXPECT_EQ(kIdle, streaming_server_state());
  EXPECT_EQ("success", GetPageFragment());
}

IN_PROC_BROWSER_TEST_F(SpeechRecognitionBrowserTest, OneShotRecognition) {
  NavigateToURLBlockUntilNavigationsComplete(
      shell(), GetTestUrlFromFragment("oneshot"), 2);

  EXPECT_EQ(kClientDisconnected, streaming_server_state());
  EXPECT_EQ("goodresult1", GetPageFragment());
}

}  // namespace content
