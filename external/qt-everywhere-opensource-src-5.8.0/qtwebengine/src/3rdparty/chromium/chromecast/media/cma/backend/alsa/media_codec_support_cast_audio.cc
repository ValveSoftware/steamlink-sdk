// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "chromecast/public/media_codec_support_shlib.h"

namespace chromecast {
namespace media {

MediaCodecSupportShlib::CodecSupport MediaCodecSupportShlib::IsSupported(
    const std::string& codec) {
  // Allow audio codecs.
  if (codec == "1" /*PCM*/ || codec == "vorbis" || codec == "opus" ||
      codec == "mp3" || codec == "mp4a.66" || codec == "mp4a.67" ||
      codec == "mp4a.68" || codec == "mp4a.69" || codec == "mp4a.6B" ||
      codec == "mp4a.40.2" || codec == "mp4a.40.02" || codec == "mp4a.40.29" ||
      codec == "mp4a.40.5" || codec == "mp4a.40.05" || codec == "mp4a.40" ||
      codec == "flac")
    return kSupported;

  // Some video codecs are allowed because mixed audio/video content should
  // be able to play. The audio stream will play back and the video frames will
  // be dropped by the ALSA-based CMA backend.
  //
  // TODO(cleichner): Remove this when there is a better solution for detecting
  // audio-only device from JS and sending audio-only streams.
  if (base::StartsWith(codec, "avc1.", base::CompareCase::SENSITIVE) ||
      base::StartsWith(codec, "vp8", base::CompareCase::SENSITIVE) ||
      base::StartsWith(codec, "vp9", base::CompareCase::SENSITIVE))
    return kDefault;

  return kNotSupported;
}

}  // namespace media
}  // namespace chromecast
