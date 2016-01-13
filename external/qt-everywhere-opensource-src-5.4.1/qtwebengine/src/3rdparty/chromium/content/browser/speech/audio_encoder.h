// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_AUDIO_ENCODER_H_
#define CONTENT_BROWSER_SPEECH_AUDIO_ENCODER_H_

#include <list>
#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "content/browser/speech/audio_buffer.h"

namespace content{
class AudioChunk;

// Provides a simple interface to encode raw audio using the various speech
// codecs.
class AudioEncoder {
 public:
  enum Codec {
    CODEC_FLAC,
    CODEC_SPEEX,
  };

  static AudioEncoder* Create(Codec codec,
                              int sampling_rate,
                              int bits_per_sample);

  virtual ~AudioEncoder();

  // Encodes |raw audio| to the internal buffer. Use
  // |GetEncodedDataAndClear| to read the result after this call or when
  // audio capture completes.
  virtual void Encode(const AudioChunk& raw_audio) = 0;

  // Finish encoding and flush any pending encoded bits out.
  virtual void Flush() = 0;

  // Merges, retrieves and clears all the accumulated encoded audio chunks.
  scoped_refptr<AudioChunk> GetEncodedDataAndClear();

  const std::string& mime_type() { return mime_type_; }
  int bits_per_sample() { return bits_per_sample_; }

 protected:
  AudioEncoder(const std::string& mime_type, int bits_per_sample);
  AudioBuffer encoded_audio_buffer_;

 private:
  std::string mime_type_;
  int bits_per_sample_;

  DISALLOW_COPY_AND_ASSIGN(AudioEncoder);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SPEECH_AUDIO_ENCODER_H_
