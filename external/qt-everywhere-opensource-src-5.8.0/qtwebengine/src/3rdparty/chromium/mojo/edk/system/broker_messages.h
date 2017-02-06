// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_BROKER_MESSAGES_H_
#define MOJO_EDK_SYSTEM_BROKER_MESSAGES_H_

#include "mojo/edk/system/channel.h"

namespace mojo {
namespace edk {

#pragma pack(push, 1)

enum BrokerMessageType : uint32_t {
  INIT,
  BUFFER_REQUEST,
  BUFFER_RESPONSE,
};

struct BrokerMessageHeader {
  BrokerMessageType type;
  uint32_t padding;
};

static_assert(sizeof(BrokerMessageHeader) % kChannelMessageAlignment == 0,
              "Invalid header size.");

struct BufferRequestData {
  uint32_t size;
};

#pragma pack(pop)

template <typename T>
inline Channel::MessagePtr CreateBrokerMessage(BrokerMessageType type,
                                               size_t num_handles,
                                               T** out_message_data) {
  const size_t message_size = sizeof(BrokerMessageHeader) + sizeof(T);
  Channel::MessagePtr message(new Channel::Message(message_size, num_handles));
  BrokerMessageHeader* header =
      reinterpret_cast<BrokerMessageHeader*>(message->mutable_payload());
  header->type = type;
  header->padding = 0;
  *out_message_data = reinterpret_cast<T*>(header + 1);
  return message;
}

inline Channel::MessagePtr CreateBrokerMessage(
    BrokerMessageType type,
    size_t num_handles,
    std::nullptr_t** dummy_out_data) {
  Channel::MessagePtr message(
      new Channel::Message(sizeof(BrokerMessageHeader), num_handles));
  BrokerMessageHeader* header =
      reinterpret_cast<BrokerMessageHeader*>(message->mutable_payload());
  header->type = type;
  header->padding = 0;
  return message;
}

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_BROKER_MESSAGES_H_
