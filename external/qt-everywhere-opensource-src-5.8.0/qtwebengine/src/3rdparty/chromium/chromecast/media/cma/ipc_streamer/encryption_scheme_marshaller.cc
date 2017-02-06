// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/ipc_streamer/encryption_scheme_marshaller.h"

#include <stdint.h>

#include "base/logging.h"
#include "chromecast/media/cma/ipc/media_message.h"

namespace chromecast {
namespace media {

namespace {

class PatternSpecMarshaller {
 public:
  static void Write(const ::media::EncryptionScheme::Pattern& pattern,
                    MediaMessage* msg) {
    CHECK(msg->WritePod(pattern.encrypt_blocks()));
    CHECK(msg->WritePod(pattern.skip_blocks()));
  }

  static ::media::EncryptionScheme::Pattern Read(MediaMessage* msg) {
    uint32_t encrypt_blocks;
    uint32_t skip_blocks;
    CHECK(msg->ReadPod(&encrypt_blocks));
    CHECK(msg->ReadPod(&skip_blocks));
    return ::media::EncryptionScheme::Pattern(encrypt_blocks, skip_blocks);
  }
};

}  // namespace

// static
void EncryptionSchemeMarshaller::Write(
    const ::media::EncryptionScheme& encryption_scheme,
    MediaMessage* msg) {
  CHECK(msg->WritePod(encryption_scheme.mode()));
  PatternSpecMarshaller::Write(encryption_scheme.pattern(), msg);
}

// static
::media::EncryptionScheme EncryptionSchemeMarshaller::Read(MediaMessage* msg) {
  ::media::EncryptionScheme::CipherMode mode;
  ::media::EncryptionScheme::Pattern pattern;
  CHECK(msg->ReadPod(&mode));
  pattern = PatternSpecMarshaller::Read(msg);
  return ::media::EncryptionScheme(mode, pattern);
}

}  // namespace media
}  // namespace chromecast
