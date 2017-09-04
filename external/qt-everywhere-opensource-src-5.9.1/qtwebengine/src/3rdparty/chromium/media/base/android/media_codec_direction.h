// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_CODEC_DIRECTION_H_
#define MEDIA_BASE_ANDROID_MEDIA_CODEC_DIRECTION_H_

namespace media {

// Codec direction. Keep this in sync with MediaCodecUtil.java.
enum MediaCodecDirection {
  MEDIA_CODEC_DECODER,
  MEDIA_CODEC_ENCODER,
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_CODEC_DIRECTION_H_
