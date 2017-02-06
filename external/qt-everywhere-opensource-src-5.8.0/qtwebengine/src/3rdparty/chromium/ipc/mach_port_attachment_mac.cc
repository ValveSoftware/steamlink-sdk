// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/mach_port_attachment_mac.h"

#include <stdint.h>

#include "base/mac/mach_logging.h"

namespace IPC {
namespace internal {

MachPortAttachmentMac::MachPortAttachmentMac(mach_port_t mach_port)
    : mach_port_(mach_port), owns_mach_port_(true) {
  if (mach_port != MACH_PORT_NULL) {
    kern_return_t kr = mach_port_mod_refs(mach_task_self(), mach_port,
                                          MACH_PORT_RIGHT_SEND, 1);
    MACH_LOG_IF(ERROR, kr != KERN_SUCCESS, kr)
        << "MachPortAttachmentMac mach_port_mod_refs";
  }
}
MachPortAttachmentMac::MachPortAttachmentMac(mach_port_t mach_port,
                                             FromWire from_wire)
    : mach_port_(mach_port), owns_mach_port_(true) {}

MachPortAttachmentMac::MachPortAttachmentMac(const WireFormat& wire_format)
    : BrokerableAttachment(wire_format.attachment_id),
      mach_port_(static_cast<mach_port_t>(wire_format.mach_port)),
      owns_mach_port_(true) {}

MachPortAttachmentMac::~MachPortAttachmentMac() {
  if (mach_port_ != MACH_PORT_NULL && owns_mach_port_) {
    kern_return_t kr = mach_port_mod_refs(mach_task_self(), mach_port_,
                                          MACH_PORT_RIGHT_SEND, -1);
    MACH_LOG_IF(ERROR, kr != KERN_SUCCESS, kr)
        << "~MachPortAttachmentMac mach_port_mod_refs";
  }
}

MachPortAttachmentMac::BrokerableType MachPortAttachmentMac::GetBrokerableType()
    const {
  return MACH_PORT;
}

MachPortAttachmentMac::WireFormat MachPortAttachmentMac::GetWireFormat(
    const base::ProcessId& destination) const {
  return WireFormat(static_cast<uint32_t>(mach_port_), destination,
                    GetIdentifier());
}

}  // namespace internal
}  // namespace IPC
