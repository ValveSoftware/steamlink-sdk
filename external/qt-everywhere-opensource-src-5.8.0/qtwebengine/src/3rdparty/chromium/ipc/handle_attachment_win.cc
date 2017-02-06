// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/handle_attachment_win.h"

#include <windows.h>

namespace IPC {
namespace internal {

HandleAttachmentWin::HandleAttachmentWin(const HANDLE& handle,
                                         HandleWin::Permissions permissions)
    : handle_(INVALID_HANDLE_VALUE),
      permissions_(HandleWin::INVALID),
      owns_handle_(true) {
  HANDLE duplicated_handle;
  BOOL result =
      ::DuplicateHandle(GetCurrentProcess(), handle, GetCurrentProcess(),
                        &duplicated_handle, 0, FALSE, DUPLICATE_SAME_ACCESS);
  if (result) {
    handle_ = duplicated_handle;
    permissions_ = permissions;
  }
}

HandleAttachmentWin::HandleAttachmentWin(const HANDLE& handle,
                                         FromWire from_wire)
    : handle_(handle), permissions_(HandleWin::INVALID), owns_handle_(true) {}

HandleAttachmentWin::HandleAttachmentWin(const WireFormat& wire_format)
    : BrokerableAttachment(wire_format.attachment_id),
      handle_(LongToHandle(wire_format.handle)),
      permissions_(wire_format.permissions),
      owns_handle_(true) {}

HandleAttachmentWin::~HandleAttachmentWin() {
  if (handle_ != INVALID_HANDLE_VALUE && owns_handle_)
    ::CloseHandle(handle_);
}

HandleAttachmentWin::BrokerableType HandleAttachmentWin::GetBrokerableType()
    const {
  return WIN_HANDLE;
}

HandleAttachmentWin::WireFormat HandleAttachmentWin::GetWireFormat(
    const base::ProcessId& destination) const {
  return WireFormat(HandleToLong(handle_), destination, permissions_,
                    GetIdentifier());
}

}  // namespace internal
}  // namespace IPC
