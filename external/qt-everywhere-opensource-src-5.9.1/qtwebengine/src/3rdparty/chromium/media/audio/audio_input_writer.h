// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_INPUT_WRITER_H_
#define MEDIA_AUDIO_AUDIO_INPUT_WRITER_H_

#include <memory>

namespace media {

class AudioBus;

// A writer interface used by AudioInputController for writing audio data to
// file for debugging purposes.
class AudioInputWriter {
 public:
  virtual ~AudioInputWriter() {}

  // Must be called before calling Write() for the first time after creation or
  // Stop() call. Can be called on any sequence; Write() and Stop() must be
  // called on the same sequence as Start().
  virtual void Start(const base::FilePath& file) = 0;

  // Must be called to finish recording. Each call to Start() requires a call to
  // Stop(). Will be automatically called on destruction.
  virtual void Stop() = 0;

  // Write |data| to file.
  virtual void Write(std::unique_ptr<AudioBus> data) = 0;

  // Returns true if Write() call scheduled at this point will most likely write
  // data to the file, and false if it most likely will be a no-op. The result
  // may be ambigulous if Start() or Stop() is executed at the moment. Can be
  // called from any sequence.
  virtual bool WillWrite() = 0;
};

}  // namspace media

#endif  // MEDIA_AUDIO_AUDIO_INPUT_WRITER_H_
