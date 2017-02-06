// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_ENDPOINT_H_
#define IPC_IPC_ENDPOINT_H_

#include "base/process/process_handle.h"
#include "ipc/ipc_export.h"
#include "ipc/ipc_sender.h"

namespace IPC {

// An Endpoint is an abstract base class whose interface provides sending
// functionality, and some receiving functionality. It mostly exists to provide
// a common interface to Channel and ProxyChannel.
class IPC_EXPORT Endpoint : public Sender {
 public:
  Endpoint();
  ~Endpoint() override {}

  // Get the process ID for the connected peer.
  //
  // Returns base::kNullProcessId if the peer is not connected yet. Watch out
  // for race conditions. You can easily get a channel to another process, but
  // if your process has not yet processed the "hello" message from the remote
  // side, this will fail. You should either make sure calling this is either
  // in response to a message from the remote side (which guarantees that it's
  // been connected), or you wait for the "connected" notification on the
  // listener.
  virtual base::ProcessId GetPeerPID() const = 0;

  // A callback that indicates that is_attachment_broker_endpoint() has been
  // changed.
  virtual void OnSetAttachmentBrokerEndpoint() = 0;

  // Whether this channel is used as an endpoint for sending and receiving
  // brokerable attachment messages to/from the broker process.
  void SetAttachmentBrokerEndpoint(bool is_endpoint);

 protected:
  bool is_attachment_broker_endpoint() { return attachment_broker_endpoint_; }

 private:
  // Whether this channel is used as an endpoint for sending and receiving
  // brokerable attachment messages to/from the broker process.
  bool attachment_broker_endpoint_;
};

}  // namespace IPC

#endif  // IPC_IPC_ENDPOINT_H_
