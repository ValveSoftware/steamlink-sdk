// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/placeholder_brokerable_attachment.h"

namespace IPC {

BrokerableAttachment::BrokerableType
PlaceholderBrokerableAttachment::GetBrokerableType() const {
  return PLACEHOLDER;
}

}  // namespace IPC
