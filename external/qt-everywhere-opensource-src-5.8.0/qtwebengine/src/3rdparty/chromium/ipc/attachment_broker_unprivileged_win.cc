// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/attachment_broker_unprivileged_win.h"

#include "base/process/process.h"
#include "ipc/attachment_broker_messages.h"
#include "ipc/brokerable_attachment.h"
#include "ipc/handle_attachment_win.h"
#include "ipc/ipc_sender.h"

namespace IPC {

AttachmentBrokerUnprivilegedWin::AttachmentBrokerUnprivilegedWin() {}

AttachmentBrokerUnprivilegedWin::~AttachmentBrokerUnprivilegedWin() {}

bool AttachmentBrokerUnprivilegedWin::SendAttachmentToProcess(
    const scoped_refptr<BrokerableAttachment>& attachment,
    base::ProcessId destination_process) {
  switch (attachment->GetBrokerableType()) {
    case BrokerableAttachment::WIN_HANDLE: {
      internal::HandleAttachmentWin* handle_attachment =
          static_cast<internal::HandleAttachmentWin*>(attachment.get());
      internal::HandleAttachmentWin::WireFormat format =
          handle_attachment->GetWireFormat(destination_process);
      bool success = get_sender()->Send(
          new AttachmentBrokerMsg_DuplicateWinHandle(format));
      if (success)
        handle_attachment->reset_handle_ownership();
      return success;
    }
    case BrokerableAttachment::MACH_PORT:
    case BrokerableAttachment::PLACEHOLDER:
      NOTREACHED();
      return false;
  }
  return false;
}

bool AttachmentBrokerUnprivilegedWin::OnMessageReceived(const Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AttachmentBrokerUnprivilegedWin, msg)
    IPC_MESSAGE_HANDLER(AttachmentBrokerMsg_WinHandleHasBeenDuplicated,
                        OnWinHandleHasBeenDuplicated)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void AttachmentBrokerUnprivilegedWin::OnWinHandleHasBeenDuplicated(
    const IPC::internal::HandleAttachmentWin::WireFormat& wire_format) {
  // The IPC message was intended for a different process. Ignore it.
  if (wire_format.destination_process != base::Process::Current().Pid()) {
    LogError(WRONG_DESTINATION);
    return;
  }

  scoped_refptr<BrokerableAttachment> attachment(
      new IPC::internal::HandleAttachmentWin(wire_format));
  HandleReceivedAttachment(attachment);
  LogError(SUCCESS);
}

}  // namespace IPC
