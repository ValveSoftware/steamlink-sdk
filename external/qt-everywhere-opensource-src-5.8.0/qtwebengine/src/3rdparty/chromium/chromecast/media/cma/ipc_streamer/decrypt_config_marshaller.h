// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_IPC_STREAMER_DECRYPT_CONFIG_MARSHALLER_H_
#define CHROMECAST_MEDIA_CMA_IPC_STREAMER_DECRYPT_CONFIG_MARSHALLER_H_

#include <memory>

namespace media {
class DecryptConfig;
}

namespace chromecast {
namespace media {
class CastDecryptConfig;
class MediaMessage;

class DecryptConfigMarshaller {
 public:
  // Writes the serialized structure of |config| into |msg|.
  static void Write(const CastDecryptConfig& config, MediaMessage* msg);

  // Returns a DecryptConfig from its serialized structure.
  static std::unique_ptr<CastDecryptConfig> Read(MediaMessage* msg);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_IPC_STREAMER_DECRYPT_CONFIG_MARSHALLER_H_
