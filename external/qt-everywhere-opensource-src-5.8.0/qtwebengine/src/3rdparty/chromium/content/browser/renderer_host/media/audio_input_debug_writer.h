// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_DEBUG_WRITER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_DEBUG_WRITER_H_

#include <stdint.h>

#include <memory>

#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "media/audio/audio_input_writer.h"
#include "media/base/audio_parameters.h"

namespace media {

class AudioBus;
class AudioBusRefCounted;

}  // namespace media

namespace content {

// Writes audio input data used for debugging purposes. Can be created on any
// thread. Must be destroyed on the FILE thread. Write call can be made on any
// thread. This object must be unregistered in Write caller before destroyed.
// When created, it takes ownership of |file|.
class CONTENT_EXPORT AudioInputDebugWriter
    : public NON_EXPORTED_BASE(media::AudioInputWriter) {
 public:
  AudioInputDebugWriter(base::File file, const media::AudioParameters& params);

  ~AudioInputDebugWriter() override;

  // Write data from |data| to file.
  void Write(std::unique_ptr<media::AudioBus> data) override;

 private:
  // Write data from |data| to file. Called on the FILE thread.
  void DoWrite(std::unique_ptr<media::AudioBus> data);

  // Write wave header to file. Called on the FILE thread twice: on construction
  // of AudioInputDebugWriter size of the wave data is unknown, so the header is
  // written with zero sizes; then on destruction it is re-written with the
  // actual size info accumulated throughout the object lifetime.
  void WriteHeader();

  // The file to write to.
  base::File file_;

  // Number of written samples.
  uint64_t samples_;

  // Input audio parameters required to build wave header.
  media::AudioParameters params_;

  // Intermediate buffer to be written to file. Interleaved 16 bit audio data.
  std::unique_ptr<int16_t[]> interleaved_data_;
  int interleaved_data_size_;

  base::WeakPtrFactory<AudioInputDebugWriter> weak_factory_;
};

}  // namspace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_DEBUG_WRITER_H_
