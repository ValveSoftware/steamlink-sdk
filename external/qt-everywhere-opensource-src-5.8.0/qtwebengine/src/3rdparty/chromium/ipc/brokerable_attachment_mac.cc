// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/brokerable_attachment.h"

#include "crypto/random.h"

namespace IPC {

// static
BrokerableAttachment::AttachmentId
BrokerableAttachment::AttachmentId::CreateIdWithRandomNonce() {
  AttachmentId id;
  // In order to prevent mutually untrusted processes from stealing resources
  // from one another, the nonce must be secret. This generates a 128-bit,
  // cryptographicaly-strong random number.
  crypto::RandBytes(id.nonce, BrokerableAttachment::kNonceSize);
  return id;
}

}  // namespace IPC
