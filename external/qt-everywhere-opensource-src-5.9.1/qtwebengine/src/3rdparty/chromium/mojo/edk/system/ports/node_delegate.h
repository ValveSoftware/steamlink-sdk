// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_PORTS_NODE_DELEGATE_H_
#define MOJO_EDK_SYSTEM_PORTS_NODE_DELEGATE_H_

#include <stddef.h>

#include "mojo/edk/system/ports/message.h"
#include "mojo/edk/system/ports/name.h"
#include "mojo/edk/system/ports/port_ref.h"

namespace mojo {
namespace edk {
namespace ports {

class NodeDelegate {
 public:
  virtual ~NodeDelegate() {}

  // Port names should be difficult to guess.
  virtual void GenerateRandomPortName(PortName* port_name) = 0;

  // Allocate a message, including a header that can be used by the Node
  // implementation. |num_header_bytes| will be aligned. The newly allocated
  // memory need not be zero-filled.
  virtual void AllocMessage(size_t num_header_bytes,
                            ScopedMessage* message) = 0;

  // Forward a message asynchronously to the specified node. This method MUST
  // NOT synchronously call any methods on Node.
  virtual void ForwardMessage(const NodeName& node, ScopedMessage message) = 0;

  // Broadcast a message to all nodes.
  virtual void BroadcastMessage(ScopedMessage message) = 0;

  // Indicates that the port's status has changed recently. Use Node::GetStatus
  // to query the latest status of the port. Note, this event could be spurious
  // if another thread is simultaneously modifying the status of the port.
  virtual void PortStatusChanged(const PortRef& port_ref) = 0;
};

}  // namespace ports
}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_PORTS_NODE_DELEGATE_H_
