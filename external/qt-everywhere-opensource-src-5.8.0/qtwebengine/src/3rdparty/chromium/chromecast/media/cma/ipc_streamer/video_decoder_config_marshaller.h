// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_IPC_STREAMER_VIDEO_DECODER_CONFIG_MARSHALLER_H_
#define CHROMECAST_MEDIA_CMA_IPC_STREAMER_VIDEO_DECODER_CONFIG_MARSHALLER_H_

#include "media/base/video_decoder_config.h"

namespace chromecast {
namespace media {
class MediaMessage;

class VideoDecoderConfigMarshaller {
 public:
  // Writes the serialized structure of |config| into |msg|.
  static void Write(
      const ::media::VideoDecoderConfig& config, MediaMessage* msg);

  // Returns a VideoDecoderConfig from its serialized structure.
  static ::media::VideoDecoderConfig Read(MediaMessage* msg);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_IPC_STREAMER_VIDEO_DECODER_CONFIG_MARSHALLER_H_
