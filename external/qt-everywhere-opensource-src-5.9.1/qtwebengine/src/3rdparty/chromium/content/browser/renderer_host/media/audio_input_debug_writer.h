// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_DEBUG_WRITER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_DEBUG_WRITER_H_

#include <stdint.h>

#include <memory>

#include "base/files/file.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "media/audio/audio_input_writer.h"
#include "media/base/audio_parameters.h"

namespace media {

class AudioBus;

}  // namespace media

namespace content {

// Writes audio input data used for debugging purposes. All operations are
// non-blocking.
class CONTENT_EXPORT AudioInputDebugWriter
    : public NON_EXPORTED_BASE(media::AudioInputWriter) {
 public:
  explicit AudioInputDebugWriter(const media::AudioParameters& params);
  ~AudioInputDebugWriter() override;

  void Start(const base::FilePath& file) override;
  void Stop() override;
  void Write(std::unique_ptr<media::AudioBus> data) override;
  bool WillWrite() override;

 private:
  class AudioFileWriter;
  using AudioFileWriterUniquePtr =
      std::unique_ptr<AudioFileWriter, BrowserThread::DeleteOnFileThread>;
  AudioFileWriterUniquePtr file_writer_;
  const media::AudioParameters params_;
  base::SequenceChecker client_sequence_checker_;
};

}  // namspace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_DEBUG_WRITER_H_
