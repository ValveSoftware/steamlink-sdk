// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_PLACEHOLDER_BROKERABLE_ATTACHMENT_H_
#define IPC_PLACEHOLDER_BROKERABLE_ATTACHMENT_H_

#include "base/macros.h"
#include "ipc/brokerable_attachment.h"
#include "ipc/ipc_export.h"

namespace IPC {

// This subclass of BrokerableAttachment has an AttachmentId, and nothing else.
// It is intended to be replaced by the attachment broker.
class IPC_EXPORT PlaceholderBrokerableAttachment : public BrokerableAttachment {
 public:
  PlaceholderBrokerableAttachment(const AttachmentId& id)
      : BrokerableAttachment(id){};
  BrokerableType GetBrokerableType() const override;

 protected:
  ~PlaceholderBrokerableAttachment() override{};
  DISALLOW_COPY_AND_ASSIGN(PlaceholderBrokerableAttachment);
};

}  // namespace IPC

#endif  // IPC_PLACEHOLDER_BROKERABLE_ATTACHMENT_H_
