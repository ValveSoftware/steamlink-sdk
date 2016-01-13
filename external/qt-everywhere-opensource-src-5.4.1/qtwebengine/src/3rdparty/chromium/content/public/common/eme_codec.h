// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_EME_CODEC_H_
#define CONTENT_PUBLIC_COMMON_EME_CODEC_H_

namespace content {

// Defines bitmask values that specify codecs used in Encrypted Media Extension
// (EME). Each value represents a codec within a specific container.
// The mask values are stored in a SupportedCodecs.
enum EmeCodec {
  EME_CODEC_NONE = 0,
  EME_CODEC_WEBM_VORBIS = 1 << 0,
  EME_CODEC_WEBM_AUDIO_ALL = EME_CODEC_WEBM_VORBIS,
  EME_CODEC_WEBM_VP8 = 1 << 1,
  EME_CODEC_WEBM_VP9 = 1 << 2,
  EME_CODEC_WEBM_VIDEO_ALL = (EME_CODEC_WEBM_VP8 | EME_CODEC_WEBM_VP9),
  EME_CODEC_WEBM_ALL = (EME_CODEC_WEBM_AUDIO_ALL | EME_CODEC_WEBM_VIDEO_ALL),
#if defined(USE_PROPRIETARY_CODECS)
  EME_CODEC_MP4_AAC = 1 << 3,
  EME_CODEC_MP4_AUDIO_ALL = EME_CODEC_MP4_AAC,
  EME_CODEC_MP4_AVC1 = 1 << 4,
  EME_CODEC_MP4_VIDEO_ALL = EME_CODEC_MP4_AVC1,
  EME_CODEC_MP4_ALL = (EME_CODEC_MP4_AUDIO_ALL | EME_CODEC_MP4_VIDEO_ALL),
  EME_CODEC_ALL = (EME_CODEC_WEBM_ALL | EME_CODEC_MP4_ALL),
#else
  EME_CODEC_ALL = EME_CODEC_WEBM_ALL,
#endif  // defined(USE_PROPRIETARY_CODECS)
};

typedef uint32 SupportedCodecs;

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_EME_CODEC_H_
