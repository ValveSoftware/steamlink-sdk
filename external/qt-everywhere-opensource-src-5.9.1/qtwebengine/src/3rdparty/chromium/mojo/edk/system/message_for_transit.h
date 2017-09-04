// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_MESSAGE_FOR_TRANSIT_H_
#define MOJO_EDK_SYSTEM_MESSAGE_FOR_TRANSIT_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "mojo/edk/system/dispatcher.h"
#include "mojo/edk/system/ports_message.h"
#include "mojo/edk/system/system_impl_export.h"

namespace mojo {
namespace edk {

// MessageForTransit holds onto a PortsMessage which may be sent via
// |MojoWriteMessage()| or which may have been received on a pipe endpoint.
// Instances of this class are exposed to Mojo system API consumers via the
// opaque pointers used with |MojoCreateMessage()|, |MojoDestroyMessage()|,
// |MojoWriteMessageNew()|, and |MojoReadMessageNew()|.
class MOJO_SYSTEM_IMPL_EXPORT MessageForTransit {
 public:
#pragma pack(push, 1)
  // Header attached to every message.
  struct MessageHeader {
    // The number of serialized dispatchers included in this header.
    uint32_t num_dispatchers;

    // Total size of the header, including serialized dispatcher data.
    uint32_t header_size;
  };

  // Header for each dispatcher in a message, immediately following the message
  // header.
  struct DispatcherHeader {
    // The type of the dispatcher, correpsonding to the Dispatcher::Type enum.
    int32_t type;

    // The size of the serialized dispatcher, not including this header.
    uint32_t num_bytes;

    // The number of ports needed to deserialize this dispatcher.
    uint32_t num_ports;

    // The number of platform handles needed to deserialize this dispatcher.
    uint32_t num_platform_handles;
  };
#pragma pack(pop)

  ~MessageForTransit();

  // A static constructor for building outbound messages.
  static MojoResult Create(
      std::unique_ptr<MessageForTransit>* message,
      uint32_t num_bytes,
      const Dispatcher::DispatcherInTransit* dispatchers,
      uint32_t num_dispatchers);

  // A static constructor for wrapping inbound messages.
  static std::unique_ptr<MessageForTransit> WrapPortsMessage(
      std::unique_ptr<PortsMessage> message) {
    return base::WrapUnique(new MessageForTransit(std::move(message)));
  }

  const void* bytes() const {
    DCHECK(message_);
    return static_cast<const void*>(
        static_cast<const char*>(message_->payload_bytes()) +
            header()->header_size);
  }

  void* mutable_bytes() {
    DCHECK(message_);
    return static_cast<void*>(
        static_cast<char*>(message_->mutable_payload_bytes()) +
            header()->header_size);
  }

  size_t num_bytes() const {
    size_t header_size = header()->header_size;
    DCHECK_GE(message_->num_payload_bytes(), header_size);
    return message_->num_payload_bytes() - header_size;
  }

  size_t num_handles() const { return header()->num_dispatchers; }

  const PortsMessage& ports_message() const { return *message_; }

  std::unique_ptr<PortsMessage> TakePortsMessage() {
    return std::move(message_);
  }

 private:
  explicit MessageForTransit(std::unique_ptr<PortsMessage> message);

  const MessageForTransit::MessageHeader* header() const {
    DCHECK(message_);
    return static_cast<const MessageForTransit::MessageHeader*>(
        message_->payload_bytes());
  }

  std::unique_ptr<PortsMessage> message_;

  DISALLOW_COPY_AND_ASSIGN(MessageForTransit);
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_MESSAGE_FOR_TRANSIT_H_
