// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_TCP_CONNECTION_H_
#define BLIMP_NET_TCP_CONNECTION_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "blimp/net/blimp_connection.h"
#include "blimp/net/blimp_net_export.h"
#include "blimp/net/blimp_transport.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"

namespace blimp {

class BlimpMessageProcessor;
class BlimpMessagePump;
class BlimpMessageSender;
class MessagePort;

// A |BlimpConnection| implementation that passes |BlimpMessage|s using a
// |StreamSocketConnection|.
class BLIMP_NET_EXPORT TCPConnection : public BlimpConnection {
 public:
  ~TCPConnection() override;
  explicit TCPConnection(std::unique_ptr<MessagePort> message_port);

  // BlimpConnection overrides.
  void SetIncomingMessageProcessor(BlimpMessageProcessor* processor) override;
  BlimpMessageProcessor* GetOutgoingMessageProcessor() override;

 private:
  std::unique_ptr<MessagePort> message_port_;
  std::unique_ptr<BlimpMessagePump> message_pump_;
  std::unique_ptr<BlimpMessageSender> outgoing_msg_processor_;
};

}  // namespace blimp

#endif  // BLIMP_NET_TCP_CONNECTION_H_
