// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_IPC_STREAMER_AUDIO_DECODER_CONFIG_MARSHALLER_H_
#define CHROMECAST_MEDIA_CMA_IPC_STREAMER_AUDIO_DECODER_CONFIG_MARSHALLER_H_

#include "media/base/audio_decoder_config.h"

namespace chromecast {
namespace media {
class MediaMessage;

class AudioDecoderConfigMarshaller {
 public:
  // Writes the serialized structure of |config| into |msg|.
  static void Write(
      const ::media::AudioDecoderConfig& config, MediaMessage* msg);

  // Returns an AudioDecoderConfig from its serialized structure.
  static ::media::AudioDecoderConfig Read(MediaMessage* msg);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_IPC_STREAMER_AUDIO_DECODER_CONFIG_MARSHALLER_H_
