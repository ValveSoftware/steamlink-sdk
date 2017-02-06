// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_ATTACHMENT_BROKER_UNPRIVILEGED_MAC_H_
#define IPC_ATTACHMENT_BROKER_UNPRIVILEGED_MAC_H_

#include "base/macros.h"
#include "ipc/attachment_broker_unprivileged.h"
#include "ipc/ipc_export.h"
#include "ipc/mach_port_attachment_mac.h"

namespace IPC {

class BrokerableAttachment;

// This class is an implementation of AttachmentBroker for the OSX platform
// for non-privileged processes.
class IPC_EXPORT AttachmentBrokerUnprivilegedMac
    : public IPC::AttachmentBrokerUnprivileged {
 public:
  AttachmentBrokerUnprivilegedMac();
  ~AttachmentBrokerUnprivilegedMac() override;

  // IPC::AttachmentBroker overrides.
  bool SendAttachmentToProcess(
      const scoped_refptr<IPC::BrokerableAttachment>& attachment,
      base::ProcessId destination_process) override;

  // IPC::Listener overrides.
  bool OnMessageReceived(const Message& message) override;

 private:
  // IPC message handlers.
  void OnMachPortHasBeenDuplicated(
      const IPC::internal::MachPortAttachmentMac::WireFormat& wire_format);

  DISALLOW_COPY_AND_ASSIGN(AttachmentBrokerUnprivilegedMac);
};

}  // namespace IPC

#endif  // IPC_ATTACHMENT_BROKER_UNPRIVILEGED_MAC_H_
