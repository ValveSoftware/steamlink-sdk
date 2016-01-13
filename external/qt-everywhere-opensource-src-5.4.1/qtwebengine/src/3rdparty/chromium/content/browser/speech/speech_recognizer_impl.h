// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_SPEECH_RECOGNIZER_IMPL_H_
#define CONTENT_BROWSER_SPEECH_SPEECH_RECOGNIZER_IMPL_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/speech/endpointer/endpointer.h"
#include "content/browser/speech/speech_recognition_engine.h"
#include "content/browser/speech/speech_recognizer.h"
#include "content/public/common/speech_recognition_error.h"
#include "content/public/common/speech_recognition_result.h"
#include "media/audio/audio_input_controller.h"
#include "net/url_request/url_request_context_getter.h"

namespace media {
class AudioBus;
class AudioManager;
}

namespace content {

class SpeechRecognitionEventListener;

// Handles speech recognition for a session (identified by |session_id|), taking
// care of audio capture, silence detection/endpointer and interaction with the
// SpeechRecognitionEngine.
class CONTENT_EXPORT SpeechRecognizerImpl
    : public SpeechRecognizer,
      public media::AudioInputController::EventHandler,
      public NON_EXPORTED_BASE(SpeechRecognitionEngineDelegate) {
 public:
  static const int kAudioSampleRate;
  static const media::ChannelLayout kChannelLayout;
  static const int kNumBitsPerAudioSample;
  static const int kNoSpeechTimeoutMs;
  static const int kEndpointerEstimationTimeMs;

  static void SetAudioManagerForTesting(media::AudioManager* audio_manager);

  SpeechRecognizerImpl(SpeechRecognitionEventListener* listener,
                       int session_id,
                       bool continuous,
                       bool provisional_results,
                       SpeechRecognitionEngine* engine);

  virtual void StartRecognition(const std::string& device_id) OVERRIDE;
  virtual void AbortRecognition() OVERRIDE;
  virtual void StopAudioCapture() OVERRIDE;
  virtual bool IsActive() const OVERRIDE;
  virtual bool IsCapturingAudio() const OVERRIDE;
  const SpeechRecognitionEngine& recognition_engine() const;

 private:
  friend class SpeechRecognizerTest;

  enum FSMState {
    STATE_IDLE = 0,
    STATE_STARTING,
    STATE_ESTIMATING_ENVIRONMENT,
    STATE_WAITING_FOR_SPEECH,
    STATE_RECOGNIZING,
    STATE_WAITING_FINAL_RESULT,
    STATE_ENDED,
    STATE_MAX_VALUE = STATE_ENDED
  };

  enum FSMEvent {
    EVENT_ABORT = 0,
    EVENT_START,
    EVENT_STOP_CAPTURE,
    EVENT_AUDIO_DATA,
    EVENT_ENGINE_RESULT,
    EVENT_ENGINE_ERROR,
    EVENT_AUDIO_ERROR,
    EVENT_MAX_VALUE = EVENT_AUDIO_ERROR
  };

  struct FSMEventArgs {
    explicit FSMEventArgs(FSMEvent event_value);
    ~FSMEventArgs();

    FSMEvent event;
    scoped_refptr<AudioChunk> audio_data;
    SpeechRecognitionResults engine_results;
    SpeechRecognitionError engine_error;
  };

  virtual ~SpeechRecognizerImpl();

  // Entry point for pushing any new external event into the recognizer FSM.
  void DispatchEvent(const FSMEventArgs& event_args);

  // Defines the behavior of the recognizer FSM, selecting the appropriate
  // transition according to the current state and event.
  FSMState ExecuteTransitionAndGetNextState(const FSMEventArgs& args);

  // Process a new audio chunk in the audio pipeline (endpointer, vumeter, etc).
  void ProcessAudioPipeline(const AudioChunk& raw_audio);

  // The methods below handle transitions of the recognizer FSM.
  FSMState StartRecording(const FSMEventArgs& event_args);
  FSMState StartRecognitionEngine(const FSMEventArgs& event_args);
  FSMState WaitEnvironmentEstimationCompletion(const FSMEventArgs& event_args);
  FSMState DetectUserSpeechOrTimeout(const FSMEventArgs& event_args);
  FSMState StopCaptureAndWaitForResult(const FSMEventArgs& event_args);
  FSMState ProcessIntermediateResult(const FSMEventArgs& event_args);
  FSMState ProcessFinalResult(const FSMEventArgs& event_args);
  FSMState AbortSilently(const FSMEventArgs& event_args);
  FSMState AbortWithError(const FSMEventArgs& event_args);
  FSMState Abort(const SpeechRecognitionError& error);
  FSMState DetectEndOfSpeech(const FSMEventArgs& event_args);
  FSMState DoNothing(const FSMEventArgs& event_args) const;
  FSMState NotFeasible(const FSMEventArgs& event_args);

  // Returns the time span of captured audio samples since the start of capture.
  int GetElapsedTimeMs() const;

  // Calculates the input volume to be displayed in the UI, triggering the
  // OnAudioLevelsChange event accordingly.
  void UpdateSignalAndNoiseLevels(const float& rms, bool clip_detected);

  void CloseAudioControllerAsynchronously();

  // Callback called on IO thread by audio_controller->Close().
  void OnAudioClosed(media::AudioInputController*);

  // AudioInputController::EventHandler methods.
  virtual void OnCreated(media::AudioInputController* controller) OVERRIDE {}
  virtual void OnRecording(media::AudioInputController* controller) OVERRIDE {}
  virtual void OnError(media::AudioInputController* controller,
      media::AudioInputController::ErrorCode error_code) OVERRIDE;
  virtual void OnData(media::AudioInputController* controller,
                      const media::AudioBus* data) OVERRIDE;
  virtual void OnLog(media::AudioInputController* controller,
                     const std::string& message) OVERRIDE {}

  // SpeechRecognitionEngineDelegate methods.
  virtual void OnSpeechRecognitionEngineResults(
      const SpeechRecognitionResults& results) OVERRIDE;
  virtual void OnSpeechRecognitionEngineError(
      const SpeechRecognitionError& error) OVERRIDE;

  static media::AudioManager* audio_manager_for_tests_;

  scoped_ptr<SpeechRecognitionEngine> recognition_engine_;
  Endpointer endpointer_;
  scoped_refptr<media::AudioInputController> audio_controller_;
  int num_samples_recorded_;
  float audio_level_;
  bool is_dispatching_event_;
  bool provisional_results_;
  FSMState state_;
  std::string device_id_;

  class OnDataConverter;

  // Converts data between native input format and a WebSpeech specific
  // output format.
  scoped_ptr<SpeechRecognizerImpl::OnDataConverter> audio_converter_;

  DISALLOW_COPY_AND_ASSIGN(SpeechRecognizerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SPEECH_SPEECH_RECOGNIZER_IMPL_H_
