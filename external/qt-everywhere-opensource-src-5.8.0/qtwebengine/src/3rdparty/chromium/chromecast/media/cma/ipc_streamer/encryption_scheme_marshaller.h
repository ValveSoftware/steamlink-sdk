// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_IPC_STREAMER_ENCRYPTION_SCHEME_MARSHALLER_H_
#define CHROMECAST_MEDIA_CMA_IPC_STREAMER_ENCRYPTION_SCHEME_MARSHALLER_H_

#include "media/base/encryption_scheme.h"

namespace chromecast {
namespace media {
class MediaMessage;

class EncryptionSchemeMarshaller {
 public:
  // Writes the serialized structure of |encryption_scheme| into |msg|.
  static void Write(
      const ::media::EncryptionScheme& encryption_scheme, MediaMessage* msg);

  // Returns an EncryptionScheme from its serialized structure.
  static ::media::EncryptionScheme Read(MediaMessage* msg);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_IPC_STREAMER_ENCRYPTION_SCHEME_MARSHALLER_H_
