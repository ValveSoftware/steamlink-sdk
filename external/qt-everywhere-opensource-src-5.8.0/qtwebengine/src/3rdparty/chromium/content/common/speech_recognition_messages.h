// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, hence no include guard.

#include <stdint.h>

#include <string>

#include "base/memory/shared_memory.h"
#include "base/sync_socket.h"
#include "content/public/common/speech_recognition_error.h"
#include "content/public/common/speech_recognition_grammar.h"
#include "content/public/common/speech_recognition_result.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "media/base/audio_parameters.h"
#include "ui/gfx/geometry/rect.h"

#define IPC_MESSAGE_START SpeechRecognitionMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(content::SpeechAudioErrorDetails,
                          content::SPEECH_AUDIO_ERROR_DETAILS_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(content::SpeechRecognitionErrorCode,
                          content::SPEECH_RECOGNITION_ERROR_LAST)

IPC_STRUCT_TRAITS_BEGIN(content::SpeechRecognitionError)
  IPC_STRUCT_TRAITS_MEMBER(code)
  IPC_STRUCT_TRAITS_MEMBER(details)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::SpeechRecognitionHypothesis)
  IPC_STRUCT_TRAITS_MEMBER(utterance)
  IPC_STRUCT_TRAITS_MEMBER(confidence)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::SpeechRecognitionResult)
  IPC_STRUCT_TRAITS_MEMBER(is_provisional)
  IPC_STRUCT_TRAITS_MEMBER(hypotheses)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::SpeechRecognitionGrammar)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(weight)
IPC_STRUCT_TRAITS_END()

// ------- Messages for Speech JS APIs (SpeechRecognitionDispatcher) ----------

// Renderer -> Browser messages.

// Used to start a speech recognition session.
IPC_STRUCT_BEGIN(SpeechRecognitionHostMsg_StartRequest_Params)
  // The render view requesting speech recognition.
  IPC_STRUCT_MEMBER(int, render_view_id)
  // Unique ID associated with the JS object making the calls.
  IPC_STRUCT_MEMBER(int, request_id)
  // Language to use for speech recognition.
  IPC_STRUCT_MEMBER(std::string, language)
  // Speech grammars to use.
  IPC_STRUCT_MEMBER(content::SpeechRecognitionGrammarArray, grammars)
  // URL of the page (or iframe if applicable).
  IPC_STRUCT_MEMBER(std::string, origin_url)
  // Maximum number of hypotheses allowed for each results.
  IPC_STRUCT_MEMBER(uint32_t, max_hypotheses)
  // Whether the user requested continuous recognition or not.
  IPC_STRUCT_MEMBER(bool, continuous)
  // Whether the user requested interim results or not.
  IPC_STRUCT_MEMBER(bool, interim_results)
  // Wheter the user has set an audio track as input or not.
  IPC_STRUCT_MEMBER(bool, using_audio_track)
IPC_STRUCT_END()


// Requests the speech recognition service to start speech recognition.
IPC_MESSAGE_CONTROL1(SpeechRecognitionHostMsg_StartRequest,
                     SpeechRecognitionHostMsg_StartRequest_Params)

// Requests the speech recognition service to abort speech recognition on
// behalf of the given |render_view_id| and |request_id|. If there are no
// sessions associated with the |request_id| in the render view, this call
// does nothing.
IPC_MESSAGE_CONTROL2(SpeechRecognitionHostMsg_AbortRequest,
                     int /* render_view_id */,
                     int /* request_id */)

// Requests the speech recognition service to abort all speech recognitions on
// behalf of the given |render_view_id|. If speech recognition is not happening
// or is happening on behalf of some other render view, this call does nothing.
IPC_MESSAGE_CONTROL1(SpeechRecognitionHostMsg_AbortAllRequests,
                     int /* render_view_id */)

// Requests the speech recognition service to stop audio capture on behalf of
// the given |render_view_id|. Any audio recorded so far will be fed to the
// speech recognizer. If speech recognition is not happening nor or is
// happening on behalf of some other render view, this call does nothing.
IPC_MESSAGE_CONTROL2(SpeechRecognitionHostMsg_StopCaptureRequest,
                     int /* render_view_id */,
                     int /* request_id */)

// Browser -> Renderer messages.

// The messages below follow exactly the same semantic of the corresponding
// events defined in content/public/browser/speech_recognition_event_listener.h.
IPC_MESSAGE_ROUTED2(SpeechRecognitionMsg_ResultRetrieved,
                    int /* request_id */,
                    content::SpeechRecognitionResults /* results */)

IPC_MESSAGE_ROUTED2(SpeechRecognitionMsg_ErrorOccurred,
                    int /* request_id */,
                    content::SpeechRecognitionError /* error */)

IPC_MESSAGE_ROUTED1(SpeechRecognitionMsg_Started, int /* request_id */)

IPC_MESSAGE_ROUTED1(SpeechRecognitionMsg_AudioStarted, int /* request_id */)

IPC_MESSAGE_ROUTED1(SpeechRecognitionMsg_SoundStarted, int /* request_id */)

IPC_MESSAGE_ROUTED1(SpeechRecognitionMsg_SoundEnded, int /* request_id */)

IPC_MESSAGE_ROUTED1(SpeechRecognitionMsg_AudioEnded, int /* request_id */)

IPC_MESSAGE_ROUTED1(SpeechRecognitionMsg_Ended, int /* request_id */)

IPC_MESSAGE_ROUTED4(SpeechRecognitionMsg_AudioReceiverReady,
                    int /* request_id */,
                    media::AudioParameters /* params */,
                    base::SharedMemoryHandle /* memory */,
                    base::SyncSocket::TransitDescriptor /* socket */)
