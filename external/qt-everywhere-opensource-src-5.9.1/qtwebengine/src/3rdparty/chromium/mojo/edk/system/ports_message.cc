// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/ports_message.h"

#include "base/memory/ptr_util.h"
#include "mojo/edk/system/node_channel.h"

namespace mojo {
namespace edk {

// static
std::unique_ptr<PortsMessage> PortsMessage::NewUserMessage(
    size_t num_payload_bytes,
    size_t num_ports,
    size_t num_handles) {
  return base::WrapUnique(
      new PortsMessage(num_payload_bytes, num_ports, num_handles));
}

PortsMessage::~PortsMessage() {}

PortsMessage::PortsMessage(size_t num_payload_bytes,
                           size_t num_ports,
                           size_t num_handles)
    : ports::Message(num_payload_bytes, num_ports) {
  size_t size = num_header_bytes_ + num_ports_bytes_ + num_payload_bytes;
  void* ptr;
  channel_message_ = NodeChannel::CreatePortsMessage(size, &ptr, num_handles);
  InitializeUserMessageHeader(ptr);
}

PortsMessage::PortsMessage(size_t num_header_bytes,
                           size_t num_payload_bytes,
                           size_t num_ports_bytes,
                           Channel::MessagePtr channel_message)
    : ports::Message(num_header_bytes,
                     num_payload_bytes,
                     num_ports_bytes) {
  if (channel_message) {
    channel_message_ = std::move(channel_message);
    void* data;
    size_t num_data_bytes;
    NodeChannel::GetPortsMessageData(channel_message_.get(), &data,
                                     &num_data_bytes);
    start_ = static_cast<char*>(data);
  } else {
    // TODO: Clean this up. In practice this branch of the constructor should
    // only be reached from Node-internal calls to AllocMessage, which never
    // carry ports or non-header bytes.
    CHECK_EQ(num_payload_bytes, 0u);
    CHECK_EQ(num_ports_bytes, 0u);
    void* ptr;
    channel_message_ =
        NodeChannel::CreatePortsMessage(num_header_bytes, &ptr, 0);
    start_ = static_cast<char*>(ptr);
  }
}

}  // namespace edk
}  // namespace mojo
