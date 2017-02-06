// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/decoder/cast_audio_decoder.h"

#include "base/logging.h"

namespace chromecast {
namespace media {

// static
std::unique_ptr<CastAudioDecoder> CastAudioDecoder::Create(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    const media::AudioConfig& config,
    OutputFormat output_format,
    const InitializedCallback& initialized_callback) {
  return std::unique_ptr<CastAudioDecoder>();
}

// static
int CastAudioDecoder::OutputFormatSizeInBytes(
    CastAudioDecoder::OutputFormat format) {
  switch (format) {
    case CastAudioDecoder::OutputFormat::kOutputSigned16:
      return 2;
    case CastAudioDecoder::OutputFormat::kOutputPlanarFloat:
      return 4;
  }
  NOTREACHED();
  return 1;
}

}  // namespace media
}  // namespace chromecast
