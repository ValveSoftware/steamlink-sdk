// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/message_builder.h"

#include <stddef.h>
#include <stdint.h>

#include "mojo/public/cpp/bindings/lib/serialization_util.h"
#include "mojo/public/cpp/bindings/message.h"

namespace mojo {
namespace internal {

template <typename Header>
void Allocate(Buffer* buf, Header** header) {
  *header = static_cast<Header*>(buf->Allocate(sizeof(Header)));
  (*header)->num_bytes = sizeof(Header);
}

MessageBuilder::MessageBuilder(uint32_t name, size_t payload_size) {
  InitializeMessage(sizeof(MessageHeader) + payload_size);

  MessageHeader* header;
  Allocate(message_.buffer(), &header);
  header->version = 0;
  header->name = name;
}

MessageBuilder::~MessageBuilder() {
}

MessageBuilder::MessageBuilder() {}

void MessageBuilder::InitializeMessage(size_t size) {
  message_.Initialize(static_cast<uint32_t>(Align(size)),
                      true /* zero_initialized */);
}

MessageWithRequestIDBuilder::MessageWithRequestIDBuilder(uint32_t name,
                                                         size_t payload_size,
                                                         uint32_t flags,
                                                         uint64_t request_id) {
  InitializeMessage(sizeof(MessageHeaderWithRequestID) + payload_size);

  MessageHeaderWithRequestID* header;
  Allocate(message_.buffer(), &header);
  header->version = 1;
  header->name = name;
  header->flags = flags;
  header->request_id = request_id;
}

}  // namespace internal
}  // namespace mojo
