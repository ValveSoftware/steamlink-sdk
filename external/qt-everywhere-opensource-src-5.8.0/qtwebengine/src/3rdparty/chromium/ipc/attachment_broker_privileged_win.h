// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_ATTACHMENT_BROKER_PRIVILEGED_WIN_H_
#define IPC_ATTACHMENT_BROKER_PRIVILEGED_WIN_H_

#include <map>
#include <vector>

#include "base/macros.h"
#include "ipc/attachment_broker_privileged.h"
#include "ipc/handle_attachment_win.h"
#include "ipc/ipc_export.h"

namespace IPC {

// This class is a concrete subclass of AttachmentBrokerPrivileged for the
// Windows platform.
class IPC_EXPORT AttachmentBrokerPrivilegedWin
    : public AttachmentBrokerPrivileged {
 public:
  AttachmentBrokerPrivilegedWin();
  ~AttachmentBrokerPrivilegedWin() override;

  // IPC::AttachmentBroker overrides.
  bool SendAttachmentToProcess(
      const scoped_refptr<IPC::BrokerableAttachment>& attachment,
      base::ProcessId destination_process) override;
  void ReceivedPeerPid(base::ProcessId peer_pid) override;

  // IPC::Listener overrides.
  bool OnMessageReceived(const Message& message) override;

 private:
  using HandleWireFormat = internal::HandleAttachmentWin::WireFormat;
  // IPC message handlers.
  void OnDuplicateWinHandle(const Message& message);

  // Duplicates |wire_Format| from |source_process| into its destination
  // process. Closes the original HANDLE.
  HandleWireFormat DuplicateWinHandle(const HandleWireFormat& wire_format,
                                      base::ProcessId source_process);

  // Copies an existing |wire_format|, but substitutes in a different handle.
  HandleWireFormat CopyWireFormat(const HandleWireFormat& wire_format,
                                  int handle);

  // If the HANDLE's destination is this process, queue it and notify the
  // observers. Otherwise, send it in an IPC to its destination.
  // If the destination process cannot be found, |store_on_failure| indicates
  // whether the |wire_format| should be stored, or an error should be emitted.
  void RouteDuplicatedHandle(const HandleWireFormat& wire_format,
                             bool store_on_failure);

  // Wire formats that cannot be immediately sent to the destination process
  // because the connection has not been established. If, for some reason, the
  // connection is never established, then the assumption is that the
  // destination process died. The resource itself will be cleaned up by the OS,
  // but the data structure HandleWireFormat will leak. If, at a later point in
  // time, a new process is created with the same process id, the WireFormats
  // will be passed to the new process. There is no security problem, since the
  // resource itself is not being sent. Furthermore, it is unlikely to affect
  // the functionality of the new process, since AttachmentBroker ids are large,
  // unguessable nonces.
  using WireFormats = std::vector<HandleWireFormat>;
  std::map<base::ProcessId, WireFormats> stored_wire_formats_;

  DISALLOW_COPY_AND_ASSIGN(AttachmentBrokerPrivilegedWin);
};

}  // namespace IPC

#endif  // IPC_ATTACHMENT_BROKER_PRIVILEGED_WIN_H_
