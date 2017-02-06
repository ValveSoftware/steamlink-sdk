// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_MESSAGE_BUILDER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_MESSAGE_BUILDER_H_

#include <stddef.h>
#include <stdint.h>

#include "mojo/public/cpp/bindings/lib/message_internal.h"
#include "mojo/public/cpp/bindings/message.h"

namespace mojo {
class Message;

namespace internal {

class MessageBuilder {
 public:
  MessageBuilder(uint32_t name, size_t payload_size);
  ~MessageBuilder();

  Buffer* buffer() { return message_.buffer(); }
  Message* message() { return &message_; }

 protected:
  MessageBuilder();
  void InitializeMessage(size_t size);

  Message message_;

  DISALLOW_COPY_AND_ASSIGN(MessageBuilder);
};

class MessageWithRequestIDBuilder : public MessageBuilder {
 public:
  MessageWithRequestIDBuilder(uint32_t name,
                              size_t payload_size,
                              uint32_t flags,
                              uint64_t request_id);
};

class RequestMessageBuilder : public MessageWithRequestIDBuilder {
 public:
  RequestMessageBuilder(uint32_t name, size_t payload_size)
      : MessageWithRequestIDBuilder(name,
                                    payload_size,
                                    Message::kFlagExpectsResponse,
                                    0) {}

  RequestMessageBuilder(uint32_t name,
                        size_t payload_size,
                        uint32_t extra_flags)
      : MessageWithRequestIDBuilder(name,
                                    payload_size,
                                    Message::kFlagExpectsResponse | extra_flags,
                                    0) {}
};

class ResponseMessageBuilder : public MessageWithRequestIDBuilder {
 public:
  ResponseMessageBuilder(uint32_t name,
                         size_t payload_size,
                         uint64_t request_id)
      : MessageWithRequestIDBuilder(name,
                                    payload_size,
                                    Message::kFlagIsResponse,
                                    request_id) {}

  ResponseMessageBuilder(uint32_t name,
                         size_t payload_size,
                         uint64_t request_id,
                         uint32_t extra_flags)
      : MessageWithRequestIDBuilder(name,
                                    payload_size,
                                    Message::kFlagIsResponse | extra_flags,
                                    request_id) {}
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_MESSAGE_BUILDER_H_
