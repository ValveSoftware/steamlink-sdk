// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_ATTACHMENT_BROKER_UNPRIVILEGED_WIN_H_
#define IPC_ATTACHMENT_BROKER_UNPRIVILEGED_WIN_H_

#include "base/macros.h"
#include "ipc/attachment_broker_unprivileged.h"
#include "ipc/handle_attachment_win.h"
#include "ipc/ipc_export.h"

namespace IPC {

class BrokerableAttachment;

// This class is an implementation of AttachmentBroker for the Windows platform
// for non-privileged processes.
class IPC_EXPORT AttachmentBrokerUnprivilegedWin
    : public IPC::AttachmentBrokerUnprivileged {
 public:
  AttachmentBrokerUnprivilegedWin();
  ~AttachmentBrokerUnprivilegedWin() override;

  // IPC::AttachmentBroker overrides.
  bool SendAttachmentToProcess(
      const scoped_refptr<BrokerableAttachment>& attachment,
      base::ProcessId destination_process) override;

  // IPC::Listener overrides.
  bool OnMessageReceived(const Message& message) override;

 private:
  // IPC message handlers.
  void OnWinHandleHasBeenDuplicated(
      const IPC::internal::HandleAttachmentWin::WireFormat& wire_format);

  DISALLOW_COPY_AND_ASSIGN(AttachmentBrokerUnprivilegedWin);
};

}  // namespace IPC

#endif  // IPC_ATTACHMENT_BROKER_UNPRIVILEGED_WIN_H_
