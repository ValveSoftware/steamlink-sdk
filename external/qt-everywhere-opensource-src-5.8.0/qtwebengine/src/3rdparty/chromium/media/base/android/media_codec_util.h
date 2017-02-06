// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_CODEC_UTIL_H_
#define MEDIA_BASE_ANDROID_MEDIA_CODEC_UTIL_H_

#include <jni.h>
#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "media/base/media_export.h"

class GURL;

namespace media {

// Helper macro to skip the test if MediaCodecBridge isn't available.
#define SKIP_TEST_IF_MEDIA_CODEC_BRIDGE_IS_NOT_AVAILABLE()        \
  do {                                                            \
    if (!MediaCodecUtil::IsMediaCodecAvailable()) {               \
      VLOG(0) << "Could not run test - not supported on device."; \
      return;                                                     \
    }                                                             \
  } while (0)

// Helper macro to skip the test if VP8 decoding isn't supported.
#define SKIP_TEST_IF_VP8_DECODER_IS_NOT_SUPPORTED()               \
  do {                                                            \
    if (!MediaCodecUtil::IsVp8DecoderAvailable()) {               \
      VLOG(0) << "Could not run test - not supported on device."; \
      return;                                                     \
    }                                                             \
  } while (0)

// Codec direction. Keep this in sync with MediaCodecUtil.java.
enum MediaCodecDirection {
  MEDIA_CODEC_DECODER,
  MEDIA_CODEC_ENCODER,
};

class MEDIA_EXPORT MediaCodecUtil {
 public:
  // Returns true if MediaCodec is available on the device.
  // All other static methods check IsAvailable() internally. There's no need
  // to check IsAvailable() explicitly before calling them.
  static bool IsMediaCodecAvailable();

  // Returns true if MediaCodec.setParameters() is available on the device.
  static bool SupportsSetParameters();

  // Returns whether MediaCodecBridge has a decoder that |is_secure| and can
  // decode |codec| type.
  static bool CanDecode(const std::string& codec, bool is_secure);

  // Get a list of encoder supported color formats for |mime_type|.
  // The mapping of color format name and its value refers to
  // MediaCodecInfo.CodecCapabilities.
  static std::set<int> GetEncoderColorFormats(const std::string& mime_type);

  // Returns true if |mime_type| is known to be unaccelerated (i.e. backed by a
  // software codec instead of a hardware one).
  static bool IsKnownUnaccelerated(const std::string& mime_type,
                                   MediaCodecDirection direction);

  // Test whether a URL contains "m3u8". (Using exactly the same logic as
  // NuPlayer does to determine if a stream is HLS.)
  static bool IsHLSURL(const GURL& url);

  // Test whether the path of a URL ends with ".m3u8".
  static bool IsHLSPath(const GURL& url);

  static bool RegisterMediaCodecUtil(JNIEnv* env);

  // Indicates if the vp8 decoder or encoder is available on this device.
  static bool IsVp8DecoderAvailable();
  static bool IsVp8EncoderAvailable();

  // Indicates if the vp9 decoder is available on this device.
  static bool IsVp9DecoderAvailable();

  // Indicates if SurfaceView and MediaCodec work well together on this device.
  static bool IsSurfaceViewOutputSupported();
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_CODEC_UTIL_H_
