// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_BROKERABLE_ATTACHMENT_H_
#define IPC_BROKERABLE_ATTACHMENT_H_

#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "base/macros.h"
#include "build/build_config.h"
#include "ipc/ipc_export.h"
#include "ipc/ipc_message_attachment.h"

namespace IPC {

// This subclass of MessageAttachment requires an AttachmentBroker to be
// attached to a Chrome IPC message.
class IPC_EXPORT BrokerableAttachment : public MessageAttachment {
 public:
  enum BrokerableType {
    PLACEHOLDER,
    WIN_HANDLE,
    MACH_PORT,
  };

  // Whether the attachment still needs information from the broker before it
  // can be used.
  bool NeedsBrokering() const;

  // Returns TYPE_BROKERABLE_ATTACHMENT
  Type GetType() const override;

  virtual BrokerableType GetBrokerableType() const = 0;

// MessageAttachment override.
#if defined(OS_POSIX)
  base::PlatformFile TakePlatformFile() override;
#endif  // OS_POSIX

 protected:
  BrokerableAttachment();
  ~BrokerableAttachment() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrokerableAttachment);
};

}  // namespace IPC

#endif  // IPC_BROKERABLE_ATTACHMENT_H_
