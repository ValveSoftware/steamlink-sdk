// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_SPEECH_RECOGNITION_ERROR_H_
#define CONTENT_PUBLIC_COMMON_SPEECH_RECOGNITION_ERROR_H_

namespace content {

enum SpeechRecognitionErrorCode {
#define DEFINE_SPEECH_RECOGNITION_ERROR(x, y) SPEECH_RECOGNITION_ERROR_##x = y,
#include "content/public/common/speech_recognition_error_list.h"
#undef DEFINE_SPEECH_RECOGNITION_ERROR
};

// Error details for the SPEECH_RECOGNITION_ERROR_AUDIO error.
enum SpeechAudioErrorDetails {
  SPEECH_AUDIO_ERROR_DETAILS_NONE = 0,
  SPEECH_AUDIO_ERROR_DETAILS_NO_MIC,
  SPEECH_AUDIO_ERROR_DETAILS_LAST = SPEECH_AUDIO_ERROR_DETAILS_NO_MIC
};

struct CONTENT_EXPORT SpeechRecognitionError {
  SpeechRecognitionErrorCode code;
  SpeechAudioErrorDetails details;

  SpeechRecognitionError()
      : code(SPEECH_RECOGNITION_ERROR_NONE),
        details(SPEECH_AUDIO_ERROR_DETAILS_NONE) {
  }
  explicit SpeechRecognitionError(SpeechRecognitionErrorCode code_value)
      : code(code_value),
        details(SPEECH_AUDIO_ERROR_DETAILS_NONE) {
  }
  SpeechRecognitionError(SpeechRecognitionErrorCode code_value,
                         SpeechAudioErrorDetails details_value)
      : code(code_value),
        details(details_value) {
  }
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_SPEECH_RECOGNITION_ERROR_H_
