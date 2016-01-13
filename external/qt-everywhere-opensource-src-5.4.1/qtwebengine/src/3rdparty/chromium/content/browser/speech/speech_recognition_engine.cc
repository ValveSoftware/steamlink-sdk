// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/speech_recognition_engine.h"

namespace {
const int kDefaultConfigSampleRate = 8000;
const int kDefaultConfigBitsPerSample = 16;
const uint32 kDefaultMaxHypotheses = 1;
}  // namespace

namespace content {

SpeechRecognitionEngine::Config::Config()
    : filter_profanities(false),
      continuous(true),
      interim_results(true),
      max_hypotheses(kDefaultMaxHypotheses),
      audio_sample_rate(kDefaultConfigSampleRate),
      audio_num_bits_per_sample(kDefaultConfigBitsPerSample) {
}

SpeechRecognitionEngine::Config::~Config() {
}

}  // namespace content
