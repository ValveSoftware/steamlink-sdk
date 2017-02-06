// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/brokerable_attachment.h"

#include <stddef.h>

#include "build/build_config.h"
#include "ipc/attachment_broker.h"

namespace IPC {

// BrokerableAttachment::AttachmentId ------------------------------------------
#if !USE_ATTACHMENT_BROKER
// static
BrokerableAttachment::AttachmentId
BrokerableAttachment::AttachmentId::CreateIdWithRandomNonce() {
  CHECK(false) << "Platforms that don't support attachment brokering shouldn't "
                  "be trying to generating a random nonce.";
  return AttachmentId();
}
#endif

BrokerableAttachment::AttachmentId::AttachmentId() {
  for (size_t i = 0; i < BrokerableAttachment::kNonceSize; ++i)
    nonce[i] = 0;
}

BrokerableAttachment::AttachmentId::AttachmentId(const char* start_address,
                                                 size_t size) {
  DCHECK(size == BrokerableAttachment::kNonceSize);
  for (size_t i = 0; i < BrokerableAttachment::kNonceSize; ++i)
    nonce[i] = start_address[i];
}

void BrokerableAttachment::AttachmentId::SerializeToBuffer(char* start_address,
                                                           size_t size) {
  DCHECK(size == BrokerableAttachment::kNonceSize);
  for (size_t i = 0; i < BrokerableAttachment::kNonceSize; ++i)
    start_address[i] = nonce[i];
}

// BrokerableAttachment::BrokerableAttachment ----------------------------------

BrokerableAttachment::BrokerableAttachment()
    : id_(AttachmentId::CreateIdWithRandomNonce()) {}

BrokerableAttachment::BrokerableAttachment(const AttachmentId& id) : id_(id) {}

BrokerableAttachment::~BrokerableAttachment() {}

BrokerableAttachment::AttachmentId BrokerableAttachment::GetIdentifier() const {
  return id_;
}

bool BrokerableAttachment::NeedsBrokering() const {
  return GetBrokerableType() == PLACEHOLDER;
}

BrokerableAttachment::Type BrokerableAttachment::GetType() const {
  return TYPE_BROKERABLE_ATTACHMENT;
}

#if defined(OS_POSIX)
base::PlatformFile BrokerableAttachment::TakePlatformFile() {
  NOTREACHED();
  return base::PlatformFile();
}
#endif  // OS_POSIX

}  // namespace IPC
