// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_MEDIA_CODEC_SUPPORT_SHLIB_H_
#define CHROMECAST_PUBLIC_MEDIA_CODEC_SUPPORT_SHLIB_H_

#include <string>

#include "chromecast_export.h"

namespace chromecast {
namespace media {

// Provides information on which codecs are supported by a platform.
// Note: these queries are made only in the renderer process.
class CHROMECAST_EXPORT MediaCodecSupportShlib {
 public:
  // Possible values for 'is codec supported' query
  enum CodecSupport {
    // Codec is definitely supported on this platform.
    kSupported,
    // Codec is definitely not supported on this platform.
    kNotSupported,
    // No platform-specific constraints on codec's support.
    // Support is determined by cast_shell default behaviour (which
    // may include taking current HDMI-out capabilities into account).
    kDefault
  };

  // Returns whether or not the given codec (passed in as a string
  // representation of the codec id conforming to RFC 6381) is supported.
  static CodecSupport IsSupported(const std::string& codec);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_PUBLIC_MEDIA_CODEC_SUPPORT_SHLIB_H_
