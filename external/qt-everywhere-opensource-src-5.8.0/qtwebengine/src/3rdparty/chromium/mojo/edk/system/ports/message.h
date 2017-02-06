// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_PORTS_MESSAGE_H_
#define MOJO_EDK_SYSTEM_PORTS_MESSAGE_H_

#include <stddef.h>

#include <memory>

#include "mojo/edk/system/ports/name.h"

namespace mojo {
namespace edk {
namespace ports {

// A message consists of a header (array of bytes), payload (array of bytes)
// and an array of ports. The header is used by the Node implementation.
//
// This class is designed to be subclassed, and the subclass is responsible for
// providing the underlying storage. The header size will be aligned, and it
// should be followed in memory by the array of ports and finally the payload.
//
// NOTE: This class does not manage the lifetime of the ports it references.
class Message {
 public:
  virtual ~Message() {}

  // Inspect the message at |bytes| and return the size of each section. Returns
  // |false| if the message is malformed and |true| otherwise.
  static bool Parse(const void* bytes,
                    size_t num_bytes,
                    size_t* num_header_bytes,
                    size_t* num_payload_bytes,
                    size_t* num_ports_bytes);

  void* mutable_header_bytes() { return start_; }
  const void* header_bytes() const { return start_; }
  size_t num_header_bytes() const { return num_header_bytes_; }

  void* mutable_payload_bytes() {
    return start_ + num_header_bytes_ + num_ports_bytes_;
  }
  const void* payload_bytes() const {
    return const_cast<Message*>(this)->mutable_payload_bytes();
  }
  size_t num_payload_bytes() const { return num_payload_bytes_; }

  PortName* mutable_ports() {
    return reinterpret_cast<PortName*>(start_ + num_header_bytes_);
  }
  const PortName* ports() const {
    return const_cast<Message*>(this)->mutable_ports();
  }
  size_t num_ports_bytes() const { return num_ports_bytes_; }
  size_t num_ports() const { return num_ports_bytes_ / sizeof(PortName); }

 protected:
  // Constructs a new Message base for a user message.
  //
  // Note: You MUST call InitializeUserMessageHeader() before this Message is
  // ready for transmission.
  Message(size_t num_payload_bytes, size_t num_ports);

  // Constructs a new Message base for an internal message. Do NOT call
  // InitializeUserMessageHeader() when using this constructor.
  Message(size_t num_header_bytes,
          size_t num_payload_bytes,
          size_t num_ports_bytes);

  Message(const Message& other) = delete;
  void operator=(const Message& other) = delete;

  // Initializes the header in a newly allocated message buffer to carry a
  // user message.
  void InitializeUserMessageHeader(void* start);

  // Note: storage is [header][ports][payload].
  char* start_ = nullptr;
  size_t num_ports_ = 0;
  size_t num_header_bytes_ = 0;
  size_t num_ports_bytes_ = 0;
  size_t num_payload_bytes_ = 0;
};

using ScopedMessage = std::unique_ptr<Message>;

}  // namespace ports
}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_PORTS_MESSAGE_H_
