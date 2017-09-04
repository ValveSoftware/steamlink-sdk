// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_SPEECH_RECOGNITION_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_SPEECH_SPEECH_RECOGNITION_DISPATCHER_HOST_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/speech_recognition_event_listener.h"
#include "net/url_request/url_request_context_getter.h"

struct SpeechRecognitionHostMsg_StartRequest_Params;

namespace content {

class SpeechRecognitionManager;
struct SpeechRecognitionResult;

// SpeechRecognitionDispatcherHost is a delegate for Speech API messages used by
// RenderMessageFilter. Basically it acts as a proxy, relaying the events coming
// from the SpeechRecognitionManager to IPC messages (and vice versa).
// It's the complement of SpeechRecognitionDispatcher (owned by RenderView).
class CONTENT_EXPORT SpeechRecognitionDispatcherHost
    : public BrowserMessageFilter,
      public SpeechRecognitionEventListener {
 public:
  SpeechRecognitionDispatcherHost(
      int render_process_id,
      net::URLRequestContextGetter* context_getter);

  base::WeakPtr<SpeechRecognitionDispatcherHost> AsWeakPtr();

  // SpeechRecognitionEventListener methods.
  void OnRecognitionStart(int session_id) override;
  void OnAudioStart(int session_id) override;
  void OnEnvironmentEstimationComplete(int session_id) override;
  void OnSoundStart(int session_id) override;
  void OnSoundEnd(int session_id) override;
  void OnAudioEnd(int session_id) override;
  void OnRecognitionEnd(int session_id) override;
  void OnRecognitionResults(int session_id,
                            const SpeechRecognitionResults& results) override;
  void OnRecognitionError(int session_id,
                          const SpeechRecognitionError& error) override;
  void OnAudioLevelsChange(int session_id,
                           float volume,
                           float noise_volume) override;

  // BrowserMessageFilter implementation.
  void OnDestruct() const override;
  bool OnMessageReceived(const IPC::Message& message) override;
  void OverrideThreadForMessage(const IPC::Message& message,
                                BrowserThread::ID* thread) override;

  void OnChannelClosing() override;

 private:
  friend class base::DeleteHelper<SpeechRecognitionDispatcherHost>;
  friend class BrowserThread;

  ~SpeechRecognitionDispatcherHost() override;

  void OnStartRequest(
      const SpeechRecognitionHostMsg_StartRequest_Params& params);
  void OnStartRequestOnIO(
      int embedder_render_process_id,
      int embedder_render_view_id,
      const SpeechRecognitionHostMsg_StartRequest_Params& params,
      int params_render_frame_id,
      bool filter_profanities);
  void OnAbortRequest(int render_view_id, int request_id);
  void OnStopCaptureRequest(int render_view_id, int request_id);
  void OnAbortAllRequests(int render_view_id);

  int render_process_id_;
  scoped_refptr<net::URLRequestContextGetter> context_getter_;

  // Used for posting asynchronous tasks (on the IO thread) without worrying
  // about this class being destroyed in the meanwhile (due to browser shutdown)
  // since tasks pending on a destroyed WeakPtr are automatically discarded.
  base::WeakPtrFactory<SpeechRecognitionDispatcherHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SpeechRecognitionDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SPEECH_SPEECH_RECOGNITION_DISPATCHER_HOST_H_
