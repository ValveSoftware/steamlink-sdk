// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_HANDLE_ATTACHMENT_WIN_H_
#define IPC_HANDLE_ATTACHMENT_WIN_H_

#include <stdint.h>

#include "base/process/process_handle.h"
#include "ipc/brokerable_attachment.h"
#include "ipc/handle_win.h"
#include "ipc/ipc_export.h"

namespace IPC {
namespace internal {

// This class represents a Windows HANDLE attached to a Chrome IPC message.
class IPC_EXPORT HandleAttachmentWin : public BrokerableAttachment {
 public:
  // The wire format for this handle.
  struct IPC_EXPORT WireFormat {
    // IPC translation requires that classes passed through IPC have a default
    // constructor.
    WireFormat()
        : handle(0),
          destination_process(0),
          permissions(HandleWin::INVALID) {}

    WireFormat(int32_t handle,
               const base::ProcessId& destination_process,
               HandleWin::Permissions permissions,
               const AttachmentId& attachment_id)
        : handle(handle),
          destination_process(destination_process),
          permissions(permissions),
          attachment_id(attachment_id) {}

    // The HANDLE that is intended for duplication, or the HANDLE that has been
    // duplicated, depending on context.
    // The type is int32_t instead of HANDLE because HANDLE gets typedefed to
    // void*, whose size varies between 32 and 64-bit processes. Using a
    // int32_t means that 64-bit processes will need to perform both up-casting
    // and down-casting. This is performed using the appropriate Windows APIs.
    // A value of 0 is equivalent to an invalid handle.
    int32_t handle;

    // The id of the destination process that the handle is duplicated into.
    base::ProcessId destination_process;

    // The permissions to use when duplicating the handle.
    HandleWin::Permissions permissions;

    AttachmentId attachment_id;
  };

  // This constructor makes a copy of |handle| and takes ownership of the
  // result. Should only be called by the sender of a Chrome IPC message.
  HandleAttachmentWin(const HANDLE& handle, HandleWin::Permissions permissions);

  enum FromWire {
    FROM_WIRE,
  };
  // This constructor takes ownership of |handle|. Should only be called by the
  // receiver of a Chrome IPC message.
  HandleAttachmentWin(const HANDLE& handle, FromWire from_wire);

  // This constructor takes ownership of |wire_format.handle| without making a
  // copy. Should only be called by the receiver of a Chrome IPC message.
  explicit HandleAttachmentWin(const WireFormat& wire_format);

  BrokerableType GetBrokerableType() const override;

  // Returns the wire format of this attachment.
  WireFormat GetWireFormat(const base::ProcessId& destination) const;

  HANDLE get_handle() const { return handle_; }

  // The caller of this method has taken ownership of |handle_|.
  void reset_handle_ownership() {
    owns_handle_ = false;
    handle_ = INVALID_HANDLE_VALUE;
  }

 private:
  ~HandleAttachmentWin() override;
  HANDLE handle_;
  HandleWin::Permissions permissions_;

  // In the sender process, the attachment owns the HANDLE of a newly created
  // message. The attachment broker will eventually take ownership, and set
  // this member to |false|.
  // In the destination process, the attachment owns the Mach port until a call
  // to ParamTraits<HandleWin>::Read() takes ownership.
  bool owns_handle_;
};

}  // namespace internal
}  // namespace IPC

#endif  // IPC_HANDLE_ATTACHMENT_WIN_H_
