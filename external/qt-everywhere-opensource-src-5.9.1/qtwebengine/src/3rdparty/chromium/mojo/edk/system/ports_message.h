// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_PORTS_MESSAGE_H__
#define MOJO_EDK_SYSTEM_PORTS_MESSAGE_H__

#include <memory>
#include <utility>

#include "mojo/edk/embedder/platform_handle_vector.h"
#include "mojo/edk/system/channel.h"
#include "mojo/edk/system/ports/message.h"
#include "mojo/edk/system/ports/name.h"

namespace mojo {
namespace edk {

class NodeController;

class PortsMessage : public ports::Message {
 public:
  static std::unique_ptr<PortsMessage> NewUserMessage(size_t num_payload_bytes,
                                                      size_t num_ports,
                                                      size_t num_handles);

  ~PortsMessage() override;

  size_t num_handles() const { return channel_message_->num_handles(); }
  bool has_handles() const { return channel_message_->has_handles(); }

  void SetHandles(ScopedPlatformHandleVectorPtr handles) {
    channel_message_->SetHandles(std::move(handles));
  }

  ScopedPlatformHandleVectorPtr TakeHandles() {
    return channel_message_->TakeHandles();
  }

  Channel::MessagePtr TakeChannelMessage() {
    return std::move(channel_message_);
  }

  void set_source_node(const ports::NodeName& name) { source_node_ = name; }
  const ports::NodeName& source_node() const { return source_node_; }

 private:
  friend class NodeController;

  // Construct a new user PortsMessage backed by a new Channel::Message.
  PortsMessage(size_t num_payload_bytes, size_t num_ports, size_t num_handles);

  // Construct a new PortsMessage backed by a Channel::Message. If
  // |channel_message| is null, a new one is allocated internally.
  PortsMessage(size_t num_header_bytes,
               size_t num_payload_bytes,
               size_t num_ports_bytes,
               Channel::MessagePtr channel_message);

  Channel::MessagePtr channel_message_;

  // The node name from which this message was received, if known.
  ports::NodeName source_node_ = ports::kInvalidNodeName;
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_PORTS_MESSAGE_H__
