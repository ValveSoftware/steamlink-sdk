// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_SESSION_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_SESSION_CONTEXT_H_

#include <string>

#include "content/common/content_export.h"
#include "content/public/common/media_stream_request.h"
#include "ui/gfx/geometry/rect.h"

namespace content {

// The context information required by clients of the SpeechRecognitionManager
// and its delegates for mapping the recognition session to other browser
// elements involved with it (e.g., the page element that requested the
// recognition). The manager keeps this struct attached to the recognition
// session during all the session lifetime, making its contents available to
// clients (In this regard, see SpeechRecognitionManager::GetSessionContext and
// SpeechRecognitionManager::LookupSessionByContext methods).
struct CONTENT_EXPORT SpeechRecognitionSessionContext {
  SpeechRecognitionSessionContext();
  SpeechRecognitionSessionContext(const SpeechRecognitionSessionContext& other);
  ~SpeechRecognitionSessionContext();

  int render_process_id;
  int render_view_id;
  int render_frame_id;

  // Browser plugin guest's render view id, if this context represents a speech
  // recognition request from an embedder on behalf of the guest. This is used
  // for input tag where speech bubble is to be shown.
  //
  // TODO(lazyboy): Right now showing bubble from guest does not work, we fall
  // back to embedder instead, fix this and use
  // embedder_render_process_id/embedder_render_view_id similar to Web Speech
  // API below.
  int guest_render_view_id;

  // The pair (|embedder_render_process_id|, |embedder_render_view_id|)
  // represents a Browser plugin guest's embedder. This is filled in if the
  // session is from a guest Web Speech API. We use these to check if the
  // embedder (app) is permitted to use audio.
  int embedder_render_process_id;
  int embedder_render_view_id;

  int request_id;

  // A texual description of the context (website, extension name) that is
  // requesting recognition, for prompting security notifications to the user.
  std::string context_name;

  // The label for the permission request, it is used for request abortion.
  std::string label;

  // A list of devices being used by the recognition session.
  MediaStreamDevices devices;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_SESSION_CONTEXT_H_
