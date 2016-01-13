// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_GOOGLE_ONE_SHOT_REMOTE_ENGINE_H_
#define CONTENT_BROWSER_SPEECH_GOOGLE_ONE_SHOT_REMOTE_ENGINE_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/speech/audio_encoder.h"
#include "content/browser/speech/speech_recognition_engine.h"
#include "content/common/content_export.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace net {
class URLRequestContextGetter;
}

namespace content {

// Implements a SpeechRecognitionEngine by means of remote interaction with
// Google speech recognition webservice.
class CONTENT_EXPORT GoogleOneShotRemoteEngine
    : public NON_EXPORTED_BASE(SpeechRecognitionEngine),
      public net::URLFetcherDelegate {
 public:
  // Duration of each audio packet.
  static const int kAudioPacketIntervalMs;
  // ID passed to URLFetcher::Create(). Used for testing.
  static int url_fetcher_id_for_tests;

  explicit GoogleOneShotRemoteEngine(net::URLRequestContextGetter* context);
  virtual ~GoogleOneShotRemoteEngine();

  // SpeechRecognitionEngine methods.
  virtual void SetConfig(const SpeechRecognitionEngineConfig& config) OVERRIDE;
  virtual void StartRecognition() OVERRIDE;
  virtual void EndRecognition() OVERRIDE;
  virtual void TakeAudioChunk(const AudioChunk& data) OVERRIDE;
  virtual void AudioChunksEnded() OVERRIDE;
  virtual bool IsRecognitionPending() const OVERRIDE;
  virtual int GetDesiredAudioChunkDurationMs() const OVERRIDE;

  // net::URLFetcherDelegate methods.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

 private:
  SpeechRecognitionEngineConfig config_;
  scoped_ptr<net::URLFetcher> url_fetcher_;
  scoped_refptr<net::URLRequestContextGetter> url_context_;
  scoped_ptr<AudioEncoder> encoder_;

  DISALLOW_COPY_AND_ASSIGN(GoogleOneShotRemoteEngine);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SPEECH_GOOGLE_ONE_SHOT_REMOTE_ENGINE_H_
