// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/brokerable_attachment.h"

#include <stddef.h>

#include "build/build_config.h"

namespace IPC {

// BrokerableAttachment::BrokerableAttachment ----------------------------------

BrokerableAttachment::BrokerableAttachment() = default;

BrokerableAttachment::~BrokerableAttachment() = default;

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
