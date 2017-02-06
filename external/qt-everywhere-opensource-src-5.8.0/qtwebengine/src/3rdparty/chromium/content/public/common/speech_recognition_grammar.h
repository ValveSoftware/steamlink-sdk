// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_SPEECH_RECOGNITION_GRAMMAR_H_
#define CONTENT_PUBLIC_COMMON_SPEECH_RECOGNITION_GRAMMAR_H_

#include <vector>

#include "content/common/content_export.h"

namespace content {

struct CONTENT_EXPORT SpeechRecognitionGrammar {
  SpeechRecognitionGrammar()
      : weight(0.0f) {
  }
  explicit SpeechRecognitionGrammar(const std::string& url_value)
      : url(url_value),
        weight(0.0f) {
  }
  SpeechRecognitionGrammar(const std::string& url_value, double weight_value)
      : url(url_value),
        weight(weight_value) {
  }

  std::string url;
  double weight;
};

typedef std::vector<SpeechRecognitionGrammar> SpeechRecognitionGrammarArray;

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_SPEECH_RECOGNITION_GRAMMAR_H_
