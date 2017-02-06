// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/speech_recognition_dispatcher.h"

#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "content/common/speech_recognition_messages.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebSpeechGrammar.h"
#include "third_party/WebKit/public/web/WebSpeechRecognitionParams.h"
#include "third_party/WebKit/public/web/WebSpeechRecognitionResult.h"
#include "third_party/WebKit/public/web/WebSpeechRecognizerClient.h"

#if defined(ENABLE_WEBRTC)
#include "content/renderer/media/speech_recognition_audio_sink.h"
#endif

using blink::WebVector;
using blink::WebString;
using blink::WebSpeechGrammar;
using blink::WebSpeechRecognitionHandle;
using blink::WebSpeechRecognitionResult;
using blink::WebSpeechRecognitionParams;
using blink::WebSpeechRecognizerClient;

namespace content {

SpeechRecognitionDispatcher::SpeechRecognitionDispatcher(
    RenderViewImpl* render_view)
    : RenderViewObserver(render_view),
      recognizer_client_(NULL),
      next_id_(1) {}

SpeechRecognitionDispatcher::~SpeechRecognitionDispatcher() {}

void SpeechRecognitionDispatcher::AbortAllRecognitions() {
  ResetAudioSink();
  Send(new SpeechRecognitionHostMsg_AbortAllRequests(
      routing_id()));
}

bool SpeechRecognitionDispatcher::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SpeechRecognitionDispatcher, message)
    IPC_MESSAGE_HANDLER(SpeechRecognitionMsg_Started, OnRecognitionStarted)
    IPC_MESSAGE_HANDLER(SpeechRecognitionMsg_AudioStarted, OnAudioStarted)
    IPC_MESSAGE_HANDLER(SpeechRecognitionMsg_SoundStarted, OnSoundStarted)
    IPC_MESSAGE_HANDLER(SpeechRecognitionMsg_SoundEnded, OnSoundEnded)
    IPC_MESSAGE_HANDLER(SpeechRecognitionMsg_AudioEnded, OnAudioEnded)
    IPC_MESSAGE_HANDLER(SpeechRecognitionMsg_ErrorOccurred, OnErrorOccurred)
    IPC_MESSAGE_HANDLER(SpeechRecognitionMsg_Ended, OnRecognitionEnded)
    IPC_MESSAGE_HANDLER(SpeechRecognitionMsg_ResultRetrieved,
                        OnResultsRetrieved)
    IPC_MESSAGE_HANDLER(SpeechRecognitionMsg_AudioReceiverReady,
                        OnAudioReceiverReady)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void SpeechRecognitionDispatcher::OnDestruct() {
  delete this;
}

void SpeechRecognitionDispatcher::start(
    const WebSpeechRecognitionHandle& handle,
    const WebSpeechRecognitionParams& params,
    WebSpeechRecognizerClient* recognizer_client) {
  DCHECK(!recognizer_client_ || recognizer_client_ == recognizer_client);
  recognizer_client_ = recognizer_client;

#if defined(ENABLE_WEBRTC)
  const blink::WebMediaStreamTrack track = params.audioTrack();
  if (!track.isNull()) {
    // Check if this type of track is allowed by implemented policy.
    if (SpeechRecognitionAudioSink::IsSupportedTrack(track)) {
      audio_track_.assign(track);
    } else {
      audio_track_.reset();
      // Notify user that the track used is not supported.
      recognizer_client_->didReceiveError(
          handle,
          WebString("Provided audioTrack is not supported."),
          WebSpeechRecognizerClient::AudioCaptureError);

      return;
    }
  }

  // Destroy any previous instance to detach from the audio track.
  // Each new session should reinstantiate the provider once the track is ready.
  ResetAudioSink();
#endif

  SpeechRecognitionHostMsg_StartRequest_Params msg_params;
  for (size_t i = 0; i < params.grammars().size(); ++i) {
    const WebSpeechGrammar& grammar = params.grammars()[i];
    msg_params.grammars.push_back(
        SpeechRecognitionGrammar(grammar.src().string().utf8(),
                                 grammar.weight()));
  }
  msg_params.language =
      base::UTF16ToUTF8(base::StringPiece16(params.language()));
  msg_params.max_hypotheses = static_cast<uint32_t>(params.maxAlternatives());
  msg_params.continuous = params.continuous();
  msg_params.interim_results = params.interimResults();
  msg_params.origin_url = params.origin().toString().utf8();
  msg_params.render_view_id = routing_id();
  msg_params.request_id = GetOrCreateIDForHandle(handle);
#if defined(ENABLE_WEBRTC)
  // Fall back to default input when the track is not allowed.
  msg_params.using_audio_track = !audio_track_.isNull();
#else
  msg_params.using_audio_track = false;
#endif
  // The handle mapping will be removed in |OnRecognitionEnd|.
  Send(new SpeechRecognitionHostMsg_StartRequest(msg_params));
}

void SpeechRecognitionDispatcher::stop(
    const WebSpeechRecognitionHandle& handle,
    WebSpeechRecognizerClient* recognizer_client) {
  ResetAudioSink();
  // Ignore a |stop| issued without a matching |start|.
  if (recognizer_client_ != recognizer_client || !HandleExists(handle))
    return;
  Send(new SpeechRecognitionHostMsg_StopCaptureRequest(
      routing_id(), GetOrCreateIDForHandle(handle)));
}

void SpeechRecognitionDispatcher::abort(
    const WebSpeechRecognitionHandle& handle,
    WebSpeechRecognizerClient* recognizer_client) {
  ResetAudioSink();
  // Ignore an |abort| issued without a matching |start|.
  if (recognizer_client_ != recognizer_client || !HandleExists(handle))
    return;
  Send(new SpeechRecognitionHostMsg_AbortRequest(
      routing_id(), GetOrCreateIDForHandle(handle)));
}

void SpeechRecognitionDispatcher::OnRecognitionStarted(int request_id) {
  recognizer_client_->didStart(GetHandleFromID(request_id));
}

void SpeechRecognitionDispatcher::OnAudioStarted(int request_id) {
  recognizer_client_->didStartAudio(GetHandleFromID(request_id));
}

void SpeechRecognitionDispatcher::OnSoundStarted(int request_id) {
  recognizer_client_->didStartSound(GetHandleFromID(request_id));
}

void SpeechRecognitionDispatcher::OnSoundEnded(int request_id) {
  recognizer_client_->didEndSound(GetHandleFromID(request_id));
}

void SpeechRecognitionDispatcher::OnAudioEnded(int request_id) {
  recognizer_client_->didEndAudio(GetHandleFromID(request_id));
}

static WebSpeechRecognizerClient::ErrorCode WebKitErrorCode(
    SpeechRecognitionErrorCode e) {
  switch (e) {
    case SPEECH_RECOGNITION_ERROR_NONE:
      NOTREACHED();
      return WebSpeechRecognizerClient::OtherError;
    case SPEECH_RECOGNITION_ERROR_NO_SPEECH:
      return WebSpeechRecognizerClient::NoSpeechError;
    case SPEECH_RECOGNITION_ERROR_ABORTED:
      return WebSpeechRecognizerClient::AbortedError;
    case SPEECH_RECOGNITION_ERROR_AUDIO_CAPTURE:
      return WebSpeechRecognizerClient::AudioCaptureError;
    case SPEECH_RECOGNITION_ERROR_NETWORK:
      return WebSpeechRecognizerClient::NetworkError;
    case SPEECH_RECOGNITION_ERROR_NOT_ALLOWED:
      return WebSpeechRecognizerClient::NotAllowedError;
    case SPEECH_RECOGNITION_ERROR_SERVICE_NOT_ALLOWED:
      return WebSpeechRecognizerClient::ServiceNotAllowedError;
    case SPEECH_RECOGNITION_ERROR_BAD_GRAMMAR:
      return WebSpeechRecognizerClient::BadGrammarError;
    case SPEECH_RECOGNITION_ERROR_LANGUAGE_NOT_SUPPORTED:
      return WebSpeechRecognizerClient::LanguageNotSupportedError;
    case SPEECH_RECOGNITION_ERROR_NO_MATCH:
      NOTREACHED();
      return WebSpeechRecognizerClient::OtherError;
  }
  NOTREACHED();
  return WebSpeechRecognizerClient::OtherError;
}

void SpeechRecognitionDispatcher::OnErrorOccurred(
    int request_id, const SpeechRecognitionError& error) {
  if (error.code == SPEECH_RECOGNITION_ERROR_NO_MATCH) {
    recognizer_client_->didReceiveNoMatch(GetHandleFromID(request_id),
                                          WebSpeechRecognitionResult());
  } else {
    ResetAudioSink();
    recognizer_client_->didReceiveError(
        GetHandleFromID(request_id),
        WebString(),  // TODO(primiano): message?
        WebKitErrorCode(error.code));
  }
}

void SpeechRecognitionDispatcher::OnRecognitionEnded(int request_id) {
  // TODO(tommi): It is possible that the handle isn't found in the array if
  // the user just refreshed the page. It seems that we then get a notification
  // for the previously loaded instance of the page.
  HandleMap::iterator iter = handle_map_.find(request_id);
  if (iter == handle_map_.end()) {
    DLOG(ERROR) << "OnRecognitionEnded called for a handle that doesn't exist";
  } else {
    WebSpeechRecognitionHandle handle = iter->second;
    // Note: we need to erase the handle from the map *before* calling didEnd.
    // didEnd may call back synchronously to start a new recognition session,
    // and we don't want to delete the handle from the map after that happens.
    handle_map_.erase(request_id);
    ResetAudioSink();
    recognizer_client_->didEnd(handle);
  }
}

void SpeechRecognitionDispatcher::OnResultsRetrieved(
    int request_id, const SpeechRecognitionResults& results) {
  size_t provisional_count = 0;
  SpeechRecognitionResults::const_iterator it = results.begin();
  for (; it != results.end(); ++it) {
    if (it->is_provisional)
      ++provisional_count;
  }

  WebVector<WebSpeechRecognitionResult> provisional(provisional_count);
  WebVector<WebSpeechRecognitionResult> final(
      results.size() - provisional_count);

  int provisional_index = 0, final_index = 0;
  for (it = results.begin(); it != results.end(); ++it) {
    const SpeechRecognitionResult& result = (*it);
    WebSpeechRecognitionResult* webkit_result = result.is_provisional ?
        &provisional[provisional_index++] : &final[final_index++];

    const size_t num_hypotheses = result.hypotheses.size();
    WebVector<WebString> transcripts(num_hypotheses);
    WebVector<float> confidences(num_hypotheses);
    for (size_t i = 0; i < num_hypotheses; ++i) {
      transcripts[i] = result.hypotheses[i].utterance;
      confidences[i] = static_cast<float>(result.hypotheses[i].confidence);
    }
    webkit_result->assign(transcripts, confidences, !result.is_provisional);
  }

  recognizer_client_->didReceiveResults(
      GetHandleFromID(request_id), final, provisional);
}

void SpeechRecognitionDispatcher::OnAudioReceiverReady(
    int request_id,
    const media::AudioParameters& params,
    const base::SharedMemoryHandle memory,
    const base::SyncSocket::TransitDescriptor descriptor) {
#if defined(ENABLE_WEBRTC)
  DCHECK(!speech_audio_sink_.get());
  if (audio_track_.isNull()) {
    ResetAudioSink();
    return;
  }

  // The instantiation and type of SyncSocket is up to the client since it
  // is dependency injected to the SpeechRecognitionAudioSink.
  std::unique_ptr<base::SyncSocket> socket(new base::CancelableSyncSocket(
      base::SyncSocket::UnwrapHandle(descriptor)));

  speech_audio_sink_.reset(new SpeechRecognitionAudioSink(
      audio_track_, params, memory, std::move(socket),
      base::Bind(&SpeechRecognitionDispatcher::ResetAudioSink,
                 base::Unretained(this))));
#endif
}

int SpeechRecognitionDispatcher::GetOrCreateIDForHandle(
    const WebSpeechRecognitionHandle& handle) {
  // Search first for an existing mapping.
  for (HandleMap::iterator iter = handle_map_.begin();
      iter != handle_map_.end();
      ++iter) {
    if (iter->second.equals(handle))
      return iter->first;
  }
  // If no existing mapping found, create a new one.
  const int new_id = next_id_;
  handle_map_[new_id] = handle;
  ++next_id_;
  return new_id;
}

bool SpeechRecognitionDispatcher::HandleExists(
    const WebSpeechRecognitionHandle& handle) {
  for (HandleMap::iterator iter = handle_map_.begin();
      iter != handle_map_.end();
      ++iter) {
    if (iter->second.equals(handle))
      return true;
  }
  return false;
}

void SpeechRecognitionDispatcher::ResetAudioSink() {
#if defined(ENABLE_WEBRTC)
  speech_audio_sink_.reset();
#endif
}

const WebSpeechRecognitionHandle& SpeechRecognitionDispatcher::GetHandleFromID(
    int request_id) {
  HandleMap::iterator iter = handle_map_.find(request_id);
  CHECK(iter != handle_map_.end());
  return iter->second;
}

}  // namespace content
