// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUDIO_MODEM_AUDIO_RECORDER_H_
#define COMPONENTS_AUDIO_MODEM_AUDIO_RECORDER_H_

#include <string>

#include "base/macros.h"
#include "media/audio/audio_io.h"
#include "media/base/audio_converter.h"
#include "media/base/audio_parameters.h"

namespace audio_modem {

// The AudioRecorder class will record audio until told to stop.
class AudioRecorder {
 public:
  using RecordedSamplesCallback = base::Callback<void(const std::string&)>;

  // Initializes the object. Do not use this object before calling this method.
  virtual void Initialize(const RecordedSamplesCallback& decode_callback) = 0;

  // If we are already recording, this function will do nothing.
  virtual void Record() = 0;
  // If we are already stopped, this function will do nothing.
  virtual void Stop() = 0;

  // Cleans up and deletes this object. Do not use object after this call.
  virtual void Finalize() = 0;

 protected:
  virtual ~AudioRecorder() {}
};

}  // namespace audio_modem

#endif  // COMPONENTS_AUDIO_MODEM_AUDIO_RECORDER_H_
