// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_SPEECH_RECOGNITION_ENGINE_H_
#define CONTENT_BROWSER_SPEECH_SPEECH_RECOGNITION_ENGINE_H_

#include <string>

#include "base/basictypes.h"
#include "content/common/content_export.h"
#include "content/public/common/speech_recognition_grammar.h"
#include "content/public/common/speech_recognition_result.h"

namespace content {

class AudioChunk;
struct SpeechRecognitionError;

// This interface models the basic contract that a speech recognition engine,
// either working locally or relying on a remote web-service, must obey.
// The expected call sequence for exported methods is:
// StartRecognition      Mandatory at beginning of SR.
//   TakeAudioChunk      For every audio chunk pushed.
//   AudioChunksEnded    Finalize the audio stream (omitted in case of errors).
// EndRecognition        Mandatory at end of SR (even on errors).
// No delegate callbacks are allowed before StartRecognition or after
// EndRecognition. If a recognition was started, the caller can free the
// SpeechRecognitionEngine only after calling EndRecognition.
class SpeechRecognitionEngine {
 public:
  // Interface for receiving callbacks from this object.
  class Delegate {
   public:
    // Called whenever a result is retrieved. It might be issued several times,
    // (e.g., in the case of continuous speech recognition engine
    // implementations).
    virtual void OnSpeechRecognitionEngineResults(
        const SpeechRecognitionResults& results) = 0;
    virtual void OnSpeechRecognitionEngineError(
        const SpeechRecognitionError& error) = 0;

   protected:
    virtual ~Delegate() {}
  };

  // Remote engine configuration.
  struct CONTENT_EXPORT Config {
    Config();
    ~Config();

    std::string language;
    SpeechRecognitionGrammarArray grammars;
    bool filter_profanities;
    bool continuous;
    bool interim_results;
    uint32 max_hypotheses;
    std::string hardware_info;
    std::string origin_url;
    int audio_sample_rate;
    int audio_num_bits_per_sample;
  };

  virtual ~SpeechRecognitionEngine() {}

  // Set/change the recognition engine configuration. It is not allowed to call
  // this function while a recognition is ongoing.
  virtual void SetConfig(const Config& config) = 0;

  // Called when the speech recognition begins, before any TakeAudioChunk call.
  virtual void StartRecognition() = 0;

  // End any recognition activity and don't make any further callback.
  // Must be always called to close the corresponding StartRecognition call,
  // even in case of errors.
  // No further TakeAudioChunk/AudioChunksEnded calls are allowed after this.
  virtual void EndRecognition() = 0;

  // Push a chunk of uncompressed audio data, where the chunk length agrees with
  // GetDesiredAudioChunkDurationMs().
  virtual void TakeAudioChunk(const AudioChunk& data) = 0;

  // Notifies the engine that audio capture has completed and no more chunks
  // will be pushed. The engine, however, can still provide further results
  // using the audio chunks collected so far.
  virtual void AudioChunksEnded() = 0;

  // Checks wheter recognition of pushed audio data is pending.
  virtual bool IsRecognitionPending() const = 0;

  // Retrieves the desired duration, in milliseconds, of pushed AudioChunk(s).
  virtual int GetDesiredAudioChunkDurationMs() const = 0;

  // set_delegate detached from constructor for lazy dependency injection.
  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

 protected:
  Delegate* delegate() const { return delegate_; }

 private:
  Delegate* delegate_;
};

// These typedefs are to workaround the issue with certain versions of
// Visual Studio where it gets confused between multiple Delegate
// classes and gives a C2500 error.
typedef SpeechRecognitionEngine::Delegate SpeechRecognitionEngineDelegate;
typedef SpeechRecognitionEngine::Config SpeechRecognitionEngineConfig;

}  // namespace content

#endif  // CONTENT_BROWSER_SPEECH_SPEECH_RECOGNITION_ENGINE_H_
