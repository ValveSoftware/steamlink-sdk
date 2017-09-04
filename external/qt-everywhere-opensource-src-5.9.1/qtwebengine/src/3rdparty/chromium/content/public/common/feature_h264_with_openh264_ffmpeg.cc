// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/feature_h264_with_openh264_ffmpeg.h"

namespace content {

#if BUILDFLAG(RTC_USE_H264) && !defined(MEDIA_DISABLE_FFMPEG)

const base::Feature kWebRtcH264WithOpenH264FFmpeg {
  "WebRTC-H264WithOpenH264FFmpeg", base::FEATURE_ENABLED_BY_DEFAULT
};

#endif  // BUILDFLAG(RTC_USE_H264) && !defined(MEDIA_DISABLE_FFMPEG)

} // namespace content
